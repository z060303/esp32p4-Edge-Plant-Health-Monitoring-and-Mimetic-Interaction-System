# Plant Pet AI

## 第一版交接入口

第一版的最终接线、关键参数、源码位置、验收步骤和第二版计划已经整理在：

- `docs/first_version_handoff.md`

当前第一版定义为：电脑端 AI 识别 + ESP32-P4 板端传感器采集 + RGB 中文界面 + TTP223 触摸心情反馈。

植物健康监测 + 桌宠表情的端侧 AI 原型。

当前框架暂定为 **状态融合框架 v0.1**：传感器状态判断不使用 AI 模型，而是使用固定规则；AI 训练重点放在叶片图像分类。

```text
传感器数据 -> 固定规则引擎
叶片图片 -> 叶片 AI 分类模型
规则状态 + 叶片状态 -> 健康分 -> 心情 -> 表情 -> 问题和养护反馈
```

## 当前已固定的部分

1. 使用规则引擎判断土壤、光照、温度、空气湿度。
2. 输入叶片状态：`healthy`、`yellow_leaf`、`leaf_spot`、`pest_damage`、`wilted`、`unknown`。
3. 输出健康分、心情、emoji 表情、异常项和养护建议。
4. 支持“摸摸它”互动：健康时随机撒娇、亲密、乖巧；亚健康时随机委屈、宽慰；生病时固定奄奄一息并求助。
5. 支持测试版表情缓冲：数据变化后先维持旧表情，等待稳定后再切换基础表情。
6. 记录最近 `12` 条状态历史，方便观察为什么变脸。

## 文件结构

- `src/plant_pet_ai/state_engine.py`：状态融合规则引擎。
- `src/plant_pet_ai/demo.py`：命令行演示。
- `tools/plant_pet_app.py`：__本地黑匣子可视化交互界面，运行可以测试当前项目的功能。__
- `tools/tune_state_engine_app.py`：旧入口兼容包装。
- `tools/train_leaf_classifier.py`：__叶片图像分类训练骨架。__
- `docs/framework_v0_1.md`：当前框架冻结说明。
- `requirements.txt`：基础运行依赖。
- `requirements-train.txt`：训练叶片模型时再安装的依赖。

## 快速运行

命令行演示：

```powershell
python -m src.plant_pet_ai.demo
```

启动黑匣子交互界面：

```powershell
python tools/plant_pet_app.py
```

旧入口仍可用：

```powershell
python tools/tune_state_engine_app.py
```

## 当前固定参数

- 土壤偏干：`soil_moisture < 30`
- 土壤过湿：`soil_moisture > 80`
- 光照不足：`light < 150`
- 光照过强：`light > 1200`
- 温度偏低：`temperature < 14`
- 温度偏高：`temperature > 32`
- 空气偏干：`air_humidity < 35`
- 空气偏湿：`air_humidity > 75`
- 舒适但偏强光：`light >= 850` 时倾向“悠闲”
- 舒适但湿度较高：`air_humidity >= 60` 时倾向“滋润”
- 舒适但温度较高：`temperature >= 28` 时倾向“暖洋洋”

当前测试版缓冲时间为 `5` 秒，后续接入真实外设时可调整为 `10-20` 分钟。

## 下一阶段：叶片 AI

状态判断暂时不训练模型。接下来训练叶片图像分类模型，目标类别：

```text
dataset/
  healthy/
  yellow_leaf/
  leaf_spot/
  pest_damage/
  wilted/
```

第一版建议每类先收集 `50-100` 张图，先验证训练、推理、接入流程，再逐步提高数据质量。
