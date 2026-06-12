"""Train the Plant Pet AI leaf image classifier.

Expected dataset layout:

dataset/
  healthy/
  yellow_leaf/
  leaf_spot/
  pest_damage/
  wilted/
"""

from __future__ import annotations

from argparse import ArgumentParser
from pathlib import Path
import random


EXPECTED_CLASSES = ("healthy", "yellow_leaf", "leaf_spot", "pest_damage", "wilted")
IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp", ".webp"}


def count_images(dataset_dir: Path) -> dict[str, int]:
    counts: dict[str, int] = {}
    for class_name in EXPECTED_CLASSES:
        class_dir = dataset_dir / class_name
        if not class_dir.exists():
            counts[class_name] = -1
            continue
        counts[class_name] = sum(
            1
            for path in class_dir.rglob("*")
            if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS
        )
    return counts


def validate_dataset(dataset_dir: Path, min_images_per_class: int) -> None:
    if not dataset_dir.exists():
        raise SystemExit(f"Missing dataset folder: {dataset_dir}")

    counts = count_images(dataset_dir)
    missing = [name for name, count in counts.items() if count < 0]
    if missing:
        raise SystemExit(f"Missing class folders: {', '.join(missing)}")

    print("Dataset image counts:")
    for class_name in EXPECTED_CLASSES:
        print(f"  {class_name}: {counts[class_name]}")

    empty = [name for name, count in counts.items() if count == 0]
    if empty:
        raise SystemExit(
            "Cannot train yet because these classes have no images: "
            + ", ".join(empty)
        )

    too_small = [
        name for name, count in counts.items() if 0 < count < min_images_per_class
    ]
    if too_small:
        raise SystemExit(
            f"Need at least {min_images_per_class} images per class for this run. "
            f"Too small: {', '.join(too_small)}"
        )


def collect_stratified_files(
    dataset_dir: Path,
    validation_split: float,
    seed: int,
) -> tuple[list[str], list[int], list[str], list[int]]:
    rng = random.Random(seed)
    train_paths: list[str] = []
    train_labels: list[int] = []
    val_paths: list[str] = []
    val_labels: list[int] = []

    for label, class_name in enumerate(EXPECTED_CLASSES):
        class_dir = dataset_dir / class_name
        paths = sorted(
            path
            for path in class_dir.rglob("*")
            if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS
        )
        rng.shuffle(paths)
        val_count = max(1, int(round(len(paths) * validation_split)))
        val_class_paths = paths[:val_count]
        train_class_paths = paths[val_count:]
        train_paths.extend(str(path) for path in train_class_paths)
        train_labels.extend([label] * len(train_class_paths))
        val_paths.extend(str(path) for path in val_class_paths)
        val_labels.extend([label] * len(val_class_paths))

    train_items = list(zip(train_paths, train_labels, strict=True))
    val_items = list(zip(val_paths, val_labels, strict=True))
    rng.shuffle(train_items)
    rng.shuffle(val_items)
    train_paths, train_labels = map(list, zip(*train_items, strict=True))
    val_paths, val_labels = map(list, zip(*val_items, strict=True))
    return train_paths, train_labels, val_paths, val_labels


def make_dataset(
    tf,
    image_paths: list[str],
    labels: list[int],
    image_size: tuple[int, int],
    batch_size: int,
    shuffle: bool,
    seed: int,
):
    path_ds = tf.data.Dataset.from_tensor_slices((image_paths, labels))

    def load_image(path, label):
        image = tf.io.read_file(path)
        image = tf.io.decode_image(image, channels=3, expand_animations=False)
        image = tf.image.resize(image, image_size)
        image.set_shape(image_size + (3,))
        return image, label

    ds = path_ds.map(load_image, num_parallel_calls=tf.data.AUTOTUNE)
    if shuffle:
        ds = ds.shuffle(min(len(image_paths), 1024), seed=seed)
    return ds.batch(batch_size).prefetch(tf.data.AUTOTUNE)


def build_model(tf, image_size: tuple[int, int], class_count: int):
    data_augmentation = tf.keras.Sequential(
        [
            tf.keras.layers.RandomFlip("horizontal"),
            tf.keras.layers.RandomRotation(0.08),
            tf.keras.layers.RandomZoom(0.12),
            tf.keras.layers.RandomContrast(0.12),
        ],
        name="data_augmentation",
    )

    base_model = tf.keras.applications.MobileNetV2(
        input_shape=image_size + (3,),
        include_top=False,
        weights="imagenet",
    )
    base_model.trainable = False

    inputs = tf.keras.Input(shape=image_size + (3,))
    x = data_augmentation(inputs)
    x = tf.keras.applications.mobilenet_v2.preprocess_input(x)
    x = base_model(x, training=False)
    x = tf.keras.layers.GlobalAveragePooling2D()(x)
    x = tf.keras.layers.Dropout(0.25)(x)
    outputs = tf.keras.layers.Dense(class_count, activation="softmax")(x)
    model = tf.keras.Model(inputs, outputs, name="plant_pet_leaf_classifier")
    return model, base_model


def parse_args():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("--dataset", default="dataset", type=Path)
    parser.add_argument("--output-dir", default="models/leaf_classifier", type=Path)
    parser.add_argument("--image-size", default=224, type=int)
    parser.add_argument("--batch-size", default=16, type=int)
    parser.add_argument("--epochs", default=12, type=int)
    parser.add_argument("--fine-tune-epochs", default=0, type=int)
    parser.add_argument("--fine-tune-at", default=100, type=int)
    parser.add_argument("--learning-rate", default=5e-4, type=float)
    parser.add_argument("--fine-tune-learning-rate", default=5e-5, type=float)
    parser.add_argument("--validation-split", default=0.2, type=float)
    parser.add_argument("--seed", default=42, type=int)
    parser.add_argument("--min-images-per-class", default=5, type=int)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    image_size = (args.image_size, args.image_size)

    validate_dataset(args.dataset, args.min_images_per_class)

    try:
        import tensorflow as tf
    except ImportError as exc:
        raise SystemExit(
            "TensorFlow is not installed. Run: pip install -r requirements-train.txt"
        ) from exc

    args.output_dir.mkdir(parents=True, exist_ok=True)

    train_paths, train_labels, val_paths, val_labels = collect_stratified_files(
        args.dataset,
        args.validation_split,
        args.seed,
    )
    class_names = list(EXPECTED_CLASSES)
    print(f"Using {len(train_paths)} files for training.")
    print(f"Using {len(val_paths)} files for validation.")

    train_ds = make_dataset(
        tf,
        train_paths,
        train_labels,
        image_size,
        args.batch_size,
        shuffle=True,
        seed=args.seed,
    )
    val_ds = make_dataset(
        tf,
        val_paths,
        val_labels,
        image_size,
        args.batch_size,
        shuffle=False,
        seed=args.seed,
    )

    model, base_model = build_model(tf, image_size, len(class_names))
    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=args.learning_rate),
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )

    callbacks = [
        tf.keras.callbacks.ModelCheckpoint(
            args.output_dir / "best_leaf_model.keras",
            monitor="val_accuracy",
            save_best_only=True,
        ),
        tf.keras.callbacks.CSVLogger(args.output_dir / "training_log.csv"),
        tf.keras.callbacks.EarlyStopping(
            monitor="val_accuracy",
            patience=5,
            restore_best_weights=True,
        ),
    ]

    model.fit(
        train_ds,
        validation_data=val_ds,
        epochs=args.epochs,
        callbacks=callbacks,
    )

    if args.fine_tune_epochs > 0:
        base_model.trainable = True
        for layer in base_model.layers[: args.fine_tune_at]:
            layer.trainable = False
        model.compile(
            optimizer=tf.keras.optimizers.Adam(
                learning_rate=args.fine_tune_learning_rate
            ),
            loss="sparse_categorical_crossentropy",
            metrics=["accuracy"],
        )
        model.fit(
            train_ds,
            validation_data=val_ds,
            epochs=args.epochs + args.fine_tune_epochs,
            initial_epoch=args.epochs,
            callbacks=callbacks,
        )

    final_model_path = args.output_dir / "leaf_model.keras"
    labels_path = args.output_dir / "leaf_labels.txt"
    model.save(final_model_path)
    labels_path.write_text("\n".join(class_names), encoding="utf-8")

    print(f"Saved model: {final_model_path}")
    print(f"Saved labels: {labels_path}")


if __name__ == "__main__":
    main()
