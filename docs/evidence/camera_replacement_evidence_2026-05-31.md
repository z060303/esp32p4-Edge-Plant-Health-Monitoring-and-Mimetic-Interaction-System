# ATK-MCOV5645 摄像头无法稳定识别：换货证据

日期：2026-05-31

## 设备

- 主控板：ESP32-P4，USB MAC `30:ed:a0:e2:10:38`
- 摄像头：ATK-MCOV5645，OV5645 MIPI CSI
- 摄像头 SCCB/I2C 地址：`0x3C`
- 正确底板接线：SDA `GPIO33`，SCL `GPIO32`，CSI_RST `GPIO34`，CSI_PWDN `GPIO54`

## 判定标准

OV5645 正常工作时，地址 `0x3C` 必须应答，产品 ID 寄存器必须返回：

- `0x300A = 0x56`
- `0x300B = 0x45`
- 合并产品 ID：`0x5645`

## 已排除的软件原因

专用探测固件只使用底板真实接线，不再测试会扰动 `CSI_RST` 的通用示例引脚。源码位于：

`camera_csi_probe/main/camera_csi_probe.c`

固件执行了：

- 固定使用 SCCB/I2C `GPIO33/GPIO32`
- 冷启动后保持 `GPIO34/GPIO54 = 1/1`
- 三轮控制恢复序列：`RESET/PWDN = 0/0`、`0/1`、`1/0`，每次恢复到 `1/1`
- 随后保持 `1/1`，继续探测 60 秒
- 直接读取 OV5645 产品 ID 寄存器 `0x300A/0x300B`

## 关键结果

### 1. 热态曾经可以正确识别

在一次偶发恢复后，同一套底板和摄像头曾稳定返回：

`addr=0x3C ESP_OK ... PID=0x5645 OV5645 FOUND`

随后硬复位验证 5 次，5/5 均成功。这说明探测地址、寄存器和 GPIO33/GPIO32 总线映射正确。

证据日志：

`camera_csi_probe_after_vendor_cycle2_2026-05-31.log`

### 2. 真实断电重启后稳定失败

断电 10 秒再上电后，摄像头地址 `0x3C` 连续约 78 秒无应答：

`addr=0x3C ESP_ERR_NOT_FOUND ... not OV5645`

三轮控制脚恢复序列和 60 次等待探测均无效。

证据日志：

`camera_csi_recovery_cold_boot_2026-05-31.log`

### 3. 软件硬复位无法恢复

冷启动失败状态下，执行 3 次硬复位，3/3 仍然无应答。

证据日志：

`camera_csi_recovery_cold_failed_hard_resets_2026-05-31.log`

### 4. 重烧同一探测固件无法恢复

不经过其他应用，仅重新烧录同一探测固件，摄像头仍然无应答。

证据日志：

`camera_csi_recovery_after_same_firmware_reflash_2026-05-31.log`

### 5. 重新插拔 USB 供电后仍失败

重新插拔开发板 USB 供电后，`COM8` 恢复枚举，但摄像头从启动开始持续无应答。三轮控制脚恢复序列仍然无效。

证据日志：

`camera_csi_recovery_after_usb_replug_minimal_2026-05-31.log`

### 6. USB 链路也出现板级掉线

在一次不修改 flash 内容的完整 flash 读取对照实验中，读取约 63.3 秒、进度约 81% 时，USB-Serial/JTAG 端口 `COM8` 写超时并从 Windows 中完全消失。等待后未自动恢复，必须重新插拔 USB 才能重新枚举。

此前烧录测试中也出现过一次 USB 中途掉线。

## 结论

同一固件、同一引脚映射下，摄像头会在热态偶发可识别，但真实冷启动后长期完全无 ACK。GPIO 时序、持续等待、软件硬复位、重新烧录均无法稳定恢复。

这不是普通摄像头驱动初始化错误。现象符合摄像头模块、FPC 排线、底板摄像头供电/时钟路径或底板本身存在间歇性硬件问题。由于 USB 链路也发生过掉线，建议更换整套设备或至少同时更换底板、摄像头模块和 FPC 排线进行交叉验证。

## 日志 SHA-256

```text
32D2D4C191A878F9A02E69514F684E1215D943B15D44AB9D2903F617404C3A5F  camera_csi_recovery_cold_boot_2026-05-31.log
507C91E90EE8DD03075B0ACE2319E4D5155B82F6CDD12EA7A27C7404E6C6A975  camera_csi_recovery_cold_failed_hard_resets_2026-05-31.log
DA41BBE7015DBC502D79B0530286BC25F86691459FBD75BA4B3853880054D986  camera_csi_recovery_after_same_firmware_reflash_2026-05-31.log
95033F9C09E247E5011623FD5CE41B463F9BBCF284A84BED5522C326BF5A7D13  camera_csi_recovery_after_usb_replug_minimal_2026-05-31.log
B1F43DDA5389E54FCD3E5380063E1A2B43AAD858ED23F57BF4A8F3145F9B6DA5  camera_csi_probe_after_vendor_cycle2_2026-05-31.log
```

## 官方 `28_mipicamera` 源码补充实验

已从正点原子官方资料包提取同名源码：

`vendor_docs/ATK-DNESP32P4/official_sources/28_mipicamera`

官方源码确认底板初始化顺序为：

1. 初始化共享 I2C 总线，实际使用 SDA `GPIO33`、SCL `GPIO32`
2. 初始化 LCD
3. 初始化 OV5645 和 MIPI CSI

### 卖家成品镜像不兼容本板

卖家提供的合并镜像直接刷入后，ESP32-P4 `revision v1.0` 在二级 bootloader 阶段持续触发：

`Guru Meditation Error: Core 0 panic'ed (Store/AMO access fault)`

将卖家镜像的应用区单独拆出并刷到已知可启动 bootloader 后，仍在加载应用前触发相同错误。此时尚未访问摄像头。

证据日志：

`official_28_mipicamera_vendor_bin_boot_2026-05-31.log`

`official_28_mipicamera_app_only_over_known_good_bootloader_2026-05-31.log`

### 官方例程默认会被 LCD 初始化挡住

使用匹配 ESP32-P4 `revision v1.0` 的完整构建后，官方例程可以进入应用，但在 LCD 的 MIPI DSI 初始化阶段持续触发任务看门狗。摄像头初始化尚未执行。

证据日志：

`official_28_mipicamera_rev1_matching_full_build_boot_2026-05-31.log`

### 无屏版已完成真实 MIPI CSI 抓帧

基于官方例程制作无屏验证版，仅跳过 LCD 初始化，并保留官方 OV5645、`esp_video`、MIPI CSI 和 V4L2 抓帧路径。

一次连续运行中，日志记录：

```text
ov5645: Detected Camera sensor PID=0x5645
app_video: bus:     esp32p4:MIPI-CSI
app_video: width=1280 height=960
app_video: Capture RGB 5-6-5 format
mipi_cam: frame=1020 index=0 bytes=2457600
```

随后使用 `esptool` 执行 5 次硬复位，`5/5` 次均重新识别 `PID=0x5645`，并抓到第 60 帧。

证据日志：

`official_28_mipicamera_rev1_headless_frame_capture_2026-05-31.log`

`official_28_mipicamera_rev1_headless_esptool_hard_reset_5_cycles_2026-05-31.log`

当前可复用工程：

`camera_mipi_headless`

可直接刷写的合并固件：

`docs/evidence/firmware/28_mipicamera_rev1_headless_merged.bin`

合并固件 SHA-256：

```text
A31707860E2B48380ABFC7DB8D7A0F39D5B43E1FE392A6190B0CB165A78F4D9F
```

### 冷启动验收通过

2026-06-01 将开发板彻底断电约 10 秒后重新上电。串口连接时程序已经连续运行约 18 秒，帧计数为 `540`；随后持续抓帧至 `1710`，未中断。

断电启动后又执行一次受控硬复位，完整初始化链再次通过：

```text
ov5645: Detected Camera sensor PID=0x5645
app_video: bus:     esp32p4:MIPI-CSI
app_video: width=1280 height=960
app_video: Capture RGB 5-6-5 format
mipi_cam: frame=270 index=0 bytes=2457600
```

证据日志：

`official_28_mipicamera_rev1_headless_cold_boot_after_10s_poweroff_2026-06-01.log`

`official_28_mipicamera_rev1_headless_after_cold_boot_controlled_reset_2026-06-01.log`

SHA-256：

```text
339B0AAD31579A37176DBEEA69D0223C34B9551991D6FCF0BE82858073893429  official_28_mipicamera_rev1_headless_cold_boot_after_10s_poweroff_2026-06-01.log
8C6725836CC472C76A5AA937E904BFA3B68A33CC76EB4767B218A369469DCA9A  official_28_mipicamera_rev1_headless_after_cold_boot_controlled_reset_2026-06-01.log
```

## 最终判定

当前摄像头模块、FPC 排线和底板已经通过真实 MIPI CSI 抓帧验收，不需要立即换货。

稳定驱动基线是 `camera_mipi_headless`：使用 ESP32-P4 `revision v1.0` 兼容构建，跳过会阻塞摄像头初始化的 LCD 探测流程，并通过底板共享 SCCB 总线 SDA `GPIO33`、SCL `GPIO32` 初始化 OV5645。

此前无法识别和不稳定现象并非单一原因：

1. 卖家成品镜像面向不匹配的芯片版本，在本板上会崩溃。
2. 官方原始例程先初始化 LCD；没有匹配 LCD 时会停在 MIPI DSI 阶段，摄像头尚未执行。
3. 旧诊断固件只能证明 SCCB 是否应答，不能替代真实 CSI 抓帧验收。
