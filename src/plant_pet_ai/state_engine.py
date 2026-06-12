import random
from dataclasses import dataclass, field
from typing import Mapping


ENV_LABELS = {
    "normal": "环境舒适",
    "thirsty": "土壤偏干",
    "overwatered": "土壤过湿",
    "too_dark": "光照不足",
    "too_bright": "光照过强",
    "too_hot": "温度偏高",
    "too_cold": "温度偏低",
    "air_too_dry": "空气偏干",
    "air_too_humid": "空气偏湿",
}

LEAF_LABELS = {
    "healthy": "叶片健康",
    "yellow_leaf": "黄叶",
    "leaf_spot": "病斑",
    "pest_damage": "虫咬",
    "wilted": "萎蔫",
    "unknown": "叶片状态未知",
}

LEAF_SCOPE_LABELS = {
    "local": "局部叶片",
    "widespread": "大规模叶片异常",
    "unknown": "范围未知",
}

MOOD_LABELS = {
    "happy": "开心",
    "leisurely": "悠闲",
    "hydrated": "滋润",
    "warm": "暖洋洋",
    "upset": "郁闷",
    "wilted_mood": "萎靡",
    "dizzy": "晕眩",
    "gloomy": "阴霾",
    "flushed": "红温",
    "shivering": "打颤",
    "sticky": "黏腻",
    "dry": "干枯",
    "sad": "悲伤",
    "unwell": "难受",
    "help": "求助",
    "clingy": "撒娇",
    "close": "亲密",
    "well_behaved": "乖巧",
    "wronged": "委屈",
    "comforted": "宽慰",
    "fading": "奄奄一息",
}

FACES = {
    "happy": "😄",
    "leisurely": "😎",
    "hydrated": "🧃",
    "warm": "😌",
    "upset": "😞",
    "wilted_mood": "🥀",
    "dizzy": "😵‍💫",
    "gloomy": "🌚",
    "flushed": "🥵",
    "shivering": "🥶",
    "sticky": "😶‍🌫️",
    "dry": "😵",
    "sad": "😢",
    "unwell": "🤒",
    "help": "🆘",
    "clingy": "🥰",
    "close": "😘",
    "well_behaved": "😊",
    "wronged": "🥺",
    "comforted": "☺️",
    "fading": "😰",
}

TOUCH_MOODS_HEALTHY = ("clingy", "close", "well_behaved")
TOUCH_MOODS_UNCOMFORTABLE = ("wronged", "comforted")

ENV_PENALTY = {
    "thirsty": 25,
    "overwatered": 30,
    "too_dark": 25,
    "too_bright": 25,
    "too_hot": 25,
    "too_cold": 25,
    "air_too_dry": 25,
    "air_too_humid": 25,
}

LEAF_PENALTY = {
    "yellow_leaf": 0,
    "leaf_spot": 0,
    "pest_damage": 0,
    "wilted": 0,
}

SEVERE_LEAF_PENALTY = {
    "yellow_leaf": 25,
    "leaf_spot": 35,
    "pest_damage": 30,
    "wilted": 40,
}

ENV_PRIORITY = (
    "thirsty",
    "overwatered",
    "too_dark",
    "too_bright",
    "too_hot",
    "too_cold",
    "air_too_dry",
    "air_too_humid",
)


@dataclass(frozen=True)
class RuleConfig:
    soil_dry_threshold: float = 30
    soil_wet_threshold: float = 80
    light_dark_threshold: float = 150
    light_bright_threshold: float = 1200
    temp_hot_threshold: float = 32
    temp_cold_threshold: float = 14
    air_dry_threshold: float = 35
    air_humid_threshold: float = 75
    light_relaxed_threshold: float = 850
    air_comfy_humid_threshold: float = 60
    temp_warm_threshold: float = 28
    env_penalty: Mapping[str, int] = field(default_factory=lambda: ENV_PENALTY)
    leaf_penalty: Mapping[str, int] = field(default_factory=lambda: LEAF_PENALTY)
    severe_leaf_penalty: Mapping[str, int] = field(default_factory=lambda: SEVERE_LEAF_PENALTY)


DEFAULT_CONFIG = RuleConfig()


@dataclass(frozen=True)
class SensorReading:
    soil_moisture: float
    temperature: float
    air_humidity: float
    light: float
    touched: bool = False


@dataclass(frozen=True)
class PlantPetState:
    env_status: str
    leaf_status: str
    leaf_scope: str
    comfort_score: int
    health_score: int
    mood: str
    face: str
    summary: str
    advice: tuple[str, ...]
    issues: tuple[str, ...]
    care_notes: tuple[str, ...]


def infer_env_conditions(
    reading: SensorReading,
    config: RuleConfig = DEFAULT_CONFIG,
) -> tuple[str, ...]:
    conditions = []
    if reading.soil_moisture < config.soil_dry_threshold:
        conditions.append("thirsty")
    if reading.soil_moisture > config.soil_wet_threshold:
        conditions.append("overwatered")
    if reading.light < config.light_dark_threshold:
        conditions.append("too_dark")
    if reading.light > config.light_bright_threshold:
        conditions.append("too_bright")
    if reading.temperature > config.temp_hot_threshold:
        conditions.append("too_hot")
    if reading.temperature < config.temp_cold_threshold:
        conditions.append("too_cold")
    if reading.air_humidity < config.air_dry_threshold:
        conditions.append("air_too_dry")
    if reading.air_humidity > config.air_humid_threshold:
        conditions.append("air_too_humid")
    return tuple(conditions)


def infer_env_status(
    reading: SensorReading,
    config: RuleConfig = DEFAULT_CONFIG,
) -> str:
    conditions = infer_env_conditions(reading, config)
    if not conditions:
        return "normal"
    return min(conditions, key=ENV_PRIORITY.index)


def analyze_plant(
    soil_moisture: float,
    temperature: float,
    air_humidity: float,
    light: float,
    touched: bool = False,
    leaf_status: str = "healthy",
    leaf_scope: str = "local",
    config: RuleConfig = DEFAULT_CONFIG,
) -> PlantPetState:
    reading = SensorReading(
        soil_moisture=soil_moisture,
        temperature=temperature,
        air_humidity=air_humidity,
        light=light,
        touched=touched,
    )
    conditions = infer_env_conditions(reading, config)
    env_status = "normal" if not conditions else min(conditions, key=ENV_PRIORITY.index)
    leaf_status = leaf_status if leaf_status in LEAF_LABELS else "unknown"
    leaf_scope = normalize_leaf_scope(leaf_scope, leaf_status)
    comfort_score = calculate_comfort_score(conditions, config)
    health_score = calculate_health_score(conditions, leaf_status, leaf_scope, config)
    mood = infer_mood(reading, conditions, leaf_status, leaf_scope, comfort_score, health_score, config)
    issues = tuple(build_issues(reading, conditions, leaf_status, leaf_scope, config))
    care_notes = tuple(build_care_notes(leaf_status, leaf_scope))
    advice = tuple(build_advice(conditions, leaf_status, leaf_scope, comfort_score, reading.touched))
    summary = (
        f"{ENV_LABELS[env_status]}，{LEAF_LABELS[leaf_status]}，{LEAF_SCOPE_LABELS[leaf_scope]}，"
        f"舒适分 {comfort_score}，健康分 {health_score}，心情：{MOOD_LABELS[mood]}。"
    )

    return PlantPetState(
        env_status=env_status,
        leaf_status=leaf_status,
        leaf_scope=leaf_scope,
        comfort_score=comfort_score,
        health_score=health_score,
        mood=mood,
        face=FACES[mood],
        summary=summary,
        advice=advice,
        issues=issues,
        care_notes=care_notes,
    )


def normalize_leaf_scope(leaf_scope: str, leaf_status: str) -> str:
    if leaf_status in {"healthy", "unknown"}:
        return "local"
    return leaf_scope if leaf_scope in LEAF_SCOPE_LABELS else "local"


def is_widespread_leaf_issue(leaf_status: str, leaf_scope: str) -> bool:
    return leaf_scope == "widespread" and leaf_status not in {"healthy", "unknown"}


def calculate_comfort_score(
    conditions: str | tuple[str, ...],
    config: RuleConfig = DEFAULT_CONFIG,
) -> int:
    if isinstance(conditions, str):
        conditions = () if conditions == "normal" else (conditions,)
    score = 100
    for condition in conditions:
        score -= config.env_penalty.get(condition, 0)
    return max(0, min(100, score))


def calculate_health_score(
    conditions: str | tuple[str, ...],
    leaf_status: str,
    leaf_scope: str = "local",
    config: RuleConfig = DEFAULT_CONFIG,
) -> int:
    score = calculate_comfort_score(conditions, config)
    penalty_map = config.severe_leaf_penalty if is_widespread_leaf_issue(leaf_status, leaf_scope) else config.leaf_penalty
    score -= penalty_map.get(leaf_status, 0)
    return max(0, min(100, score))


def infer_mood(
    reading: SensorReading,
    conditions: tuple[str, ...],
    leaf_status: str,
    leaf_scope: str,
    comfort_score: int,
    health_score: int,
    config: RuleConfig = DEFAULT_CONFIG,
) -> str:
    condition_set = set(conditions)
    widespread_leaf_issue = is_widespread_leaf_issue(leaf_status, leaf_scope)

    if reading.touched:
        if widespread_leaf_issue and health_score < 70:
            return "fading"
        if comfort_score >= 80:
            return random.choice(TOUCH_MOODS_HEALTHY)
        if comfort_score >= 60:
            return random.choice(TOUCH_MOODS_UNCOMFORTABLE)
        return "fading"

    if widespread_leaf_issue:
        if health_score >= 70:
            return "wilted_mood"
        if health_score >= 40:
            return "sad"
        if health_score >= 25:
            return "unwell"
        return "help"

    if comfort_score >= 80:
        if "too_bright" in condition_set or reading.light >= config.light_relaxed_threshold:
            return "leisurely"
        if "air_too_humid" in condition_set or reading.air_humidity >= config.air_comfy_humid_threshold:
            return "hydrated"
        if "too_hot" in condition_set or reading.temperature >= config.temp_warm_threshold:
            return "warm"
        return "happy"

    if comfort_score >= 60:
        if "too_bright" in condition_set:
            return "dizzy"
        if "too_dark" in condition_set:
            return "gloomy"
        if "too_hot" in condition_set:
            return "flushed"
        if "too_cold" in condition_set:
            return "shivering"
        if "overwatered" in condition_set or "air_too_humid" in condition_set:
            return "sticky"
        if "thirsty" in condition_set or "air_too_dry" in condition_set:
            return "dry"
        return "upset" if comfort_score >= 70 else "wilted_mood"

    if comfort_score >= 40:
        return "sad"
    if comfort_score >= 25:
        return "unwell"
    return "help"


def build_issues(
    reading: SensorReading,
    conditions: tuple[str, ...],
    leaf_status: str,
    leaf_scope: str = "local",
    config: RuleConfig = DEFAULT_CONFIG,
) -> list[str]:
    issues = []
    for condition in conditions:
        if condition == "thirsty":
            issues.append(f"土壤湿度偏低：{reading.soil_moisture} < {config.soil_dry_threshold}")
        elif condition == "overwatered":
            issues.append(f"土壤湿度偏高：{reading.soil_moisture} > {config.soil_wet_threshold}")
        elif condition == "too_dark":
            issues.append(f"光照偏低：{reading.light} < {config.light_dark_threshold}")
        elif condition == "too_bright":
            issues.append(f"光照偏高：{reading.light} > {config.light_bright_threshold}")
        elif condition == "too_hot":
            issues.append(f"温度偏高：{reading.temperature} > {config.temp_hot_threshold}")
        elif condition == "too_cold":
            issues.append(f"温度偏低：{reading.temperature} < {config.temp_cold_threshold}")
        elif condition == "air_too_dry":
            issues.append(f"空气湿度偏低：{reading.air_humidity} < {config.air_dry_threshold}")
        elif condition == "air_too_humid":
            issues.append(f"空气湿度偏高：{reading.air_humidity} > {config.air_humid_threshold}")
    if is_widespread_leaf_issue(leaf_status, leaf_scope):
        issues.append(f"大规模叶片异常警告：{LEAF_LABELS[leaf_status]}")
    elif leaf_status not in {"healthy", "unknown"}:
        issues.append(f"叶片护理提醒：{LEAF_LABELS[leaf_status]}")
    return issues


def build_care_notes(leaf_status: str, leaf_scope: str = "local") -> list[str]:
    if is_widespread_leaf_issue(leaf_status, leaf_scope):
        if leaf_status == "yellow_leaf":
            return ["多处黄叶说明问题可能已影响整株，需要优先排查水分、光照、根系和近期环境变化。"]
        if leaf_status == "leaf_spot":
            return ["多处病斑属于整株级风险，需要减少叶面潮湿、加强通风，并观察是否继续扩散。"]
        if leaf_status == "pest_damage":
            return ["多处虫咬属于虫害警报，需要系统检查叶背、嫩芽和盆土周边，并及时清理虫体。"]
        if leaf_status == "wilted":
            return ["多片叶片萎蔫说明整株可能处于压力中，需要优先检查缺水、烂根、温度和通风。"]
    if leaf_status == "yellow_leaf":
        return ["黄叶可能是老叶代谢，也可能来自过去的水分或光照波动；先观察是否继续扩散。"]
    if leaf_status == "leaf_spot":
        return ["病斑更适合作为长期护理项处理：减少叶面潮湿，观察是否出现在更多叶片上。"]
    if leaf_status == "pest_damage":
        return ["虫咬通常是局部线索：重点检查叶背、嫩芽和新叶，确认是否还有活动虫体。"]
    if leaf_status == "wilted":
        return ["单片或局部萎蔫不等于整株状态很差；如果扩散，再优先检查根部、水分和温度。"]
    return []


def build_advice(
    conditions: tuple[str, ...],
    leaf_status: str,
    leaf_scope: str,
    comfort_score: int,
    touched: bool = False,
) -> list[str]:
    advice = []
    condition_set = set(conditions)

    if "thirsty" in condition_set:
        advice.append("主人，我有点干，请少量补水，并观察土壤湿度是否回升。")
    if "overwatered" in condition_set:
        advice.append("我现在有点闷，请暂停浇水，保持通风，检查盆底是否积水。")
    if "too_dark" in condition_set:
        advice.append("这里太暗了，请把我移动到更明亮的位置，避免突然暴晒。")
    if "too_bright" in condition_set:
        advice.append("光有点刺眼，请适当遮阴，避开强直射光。")
    if "too_hot" in condition_set:
        advice.append("我有点热，请降低环境温度，避开热源或正午暴晒。")
    if "too_cold" in condition_set:
        advice.append("我在打颤，请移到更温暖的位置，避免冷风直吹。")
    if "air_too_dry" in condition_set:
        advice.append("空气太干了，请提高环境湿度，或把我远离空调直吹。")
    if "air_too_humid" in condition_set:
        advice.append("空气太潮了，请加强通风，避免叶面长期潮湿。")

    if is_widespread_leaf_issue(leaf_status, leaf_scope):
        advice.append("检测到可能的大规模叶片异常，请优先当作整株风险处理：先隔离观察、检查根系和环境，并持续复拍确认是否扩散。")
    elif leaf_status == "yellow_leaf":
        advice.append("发现黄叶先当作护理记录：严重或影响观感时再修剪，并观察新叶是否正常。")
    elif leaf_status == "leaf_spot":
        advice.append("病斑先温和处理：减少叶面喷水、保持通风，只有扩散时再修剪或隔离。")
    elif leaf_status == "pest_damage":
        advice.append("有虫咬痕迹时检查叶背和嫩芽；如果没有活动虫体，就先记录观察。")
    elif leaf_status == "wilted":
        advice.append("局部萎蔫先不要直接判定整株难受；如果多片叶子同时变软，再检查根部、水分和温度。")

    if not advice:
        advice.append("现在整体很舒服，维持稳定浇水、光照和通风即可。")
    elif comfort_score < 60:
        advice.append("我现在状态比较差，请主人尽快帮我缓解上面这些问题。")
    elif not condition_set and leaf_status not in {"healthy", "unknown"} and leaf_scope == "local":
        advice.append("当前环境是舒服的，叶片问题先作为长期护理项，不会影响我现在的心情。")

    if touched and comfort_score < 60:
        advice.append("如果正在摸摸我，我现在只能微弱回应，请先帮我恢复环境。")

    return advice
