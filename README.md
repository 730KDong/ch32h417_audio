# CH32H417 USB Audio Host

这是一个基于 `CH32H417` 的 USB 音频主机示例工程，目标是让开发板作为 `USB Host` 识别并驱动 USB 音频设备，完成播放、录音、线控按键识别等功能。

当前仓库已经从通用 Host 示例中整理出音频专用路径，核心应用层集中在 `Common/USB_Host/app_audio.c` 与 `Common/USB_Host/app_audio.h`。

## 主要功能

- 支持 USB Audio 设备枚举
- 支持播放流与录音流识别
- 支持耳机线控 HID 按键轮询
- 支持录音、回放、启动旋律测试
- 录音数据使用 `IMA ADPCM 4-bit` 压缩存储，降低 RAM 占用
- 支持 HUB 下挂音频设备的枚举与运行

## 工程结构

```text
Host_audio/
|- Common/
|  |- hardware.c/h
|  |- USB_Host/
|     |- app_audio.c/h
|     |- ch32h417_usbfs_host.c/h
|     |- usb_host_config.h
|     |- usb_host_hid.c/h
|     |- usb_host_hub.c/h
|- V3F/
|  |- User/
|  |- Host_Audio_V3F.wvproj
|- V5F/
|  |- User/
|  |- Host_Audio_V5F.wvproj
|- README_AUDIO_CN.md
|- USB_AUDIO_RUNTIME_FLOW_CN.md
|- USB_AUDIO_MIGRATION_HISTORY_CN.md
```

## 当前双核说明

CH32H417 是双核器件，工程中同时提供了 `V3F` 和 `V5F` 两套工程入口。

结合当前代码实际行为：

- `V3F` 是工程配置中的主核
- `V5F` 是工程配置中的从核
- 双核模式下，`V3F` 负责唤醒 `V5F`
- 当前代码中，真正进入 `Hardware()` 并持续执行 `USBH_MainDeal()` 主循环的是 `V3F`
- `V5F` 在当前双核路径下主要完成一次 `HSEM` 握手

也就是说，当前仓库已经具备双核工程骨架，但 USB 音频业务还没有完全拆分成 “V3F 一部分 / V5F 一部分” 的并行结构。

## 主要代码入口

建议按下面顺序阅读：

1. `V3F/User/main.c`
2. `Common/hardware.c`
3. `Common/USB_Host/usb_host_config.h`
4. `Common/USB_Host/ch32h417_usbfs_host.c`
5. `Common/USB_Host/app_audio.c`

各文件作用大致如下：

- `V3F/User/main.c`
  当前主入口，负责系统初始化、双核唤醒和进入业务路径
- `V5F/User/main.c`
  当前用于双核握手；在 `V5F-only` 模式下也可直接进入 `Hardware()`
- `Common/hardware.c`
  初始化 Type-C Source、USBFS Host，并进入主循环
- `Common/USB_Host/app_audio.c`
  音频设备枚举、流控制、录音回放、HID 按键处理的核心实现
- `Common/USB_Host/ch32h417_usbfs_host.c`
  USBFS Host 底层传输实现

## 运行流程概览

程序主线可以概括为：

1. 系统上电后进入 `main()`
2. 初始化时钟、延时和串口打印
3. 建立 Type-C 主机角色
4. 初始化 USBFS Host
5. 持续执行 `USBH_MainDeal()`
6. 发现音频设备后完成枚举
7. 解析出播放流、录音流和 HID 线控接口
8. 进入录音、回放、线控测试主循环

更详细的说明可参考：

- [README_AUDIO_CN.md](./README_AUDIO_CN.md)
- [USB_AUDIO_RUNTIME_FLOW_CN.md](./USB_AUDIO_RUNTIME_FLOW_CN.md)
- [USB_AUDIO_MIGRATION_HISTORY_CN.md](./USB_AUDIO_MIGRATION_HISTORY_CN.md)

## 当前音频测试逻辑

当前工程内置了一套便于验证链路的测试状态机：

- 启动后先尝试播放一段内置旋律
- 通过耳机线控按键触发录音
- 再次按键停止录音
- 已有录音数据时，再次按键触发回放

录音数据在 RAM 中以 `IMA ADPCM 4-bit` 形式保存，回放时实时解码为 PCM 输出。

## 调试时建议关注的日志

- `USBFS HOST AUDIO Test`
- `USB Port Dev In.`
- `Get DevDesc`
- `Get CfgDesc`
- `Play IF:`
- `Mic IF:`
- `Ctrl IF:`
- `Audio Demo: start boot melody`
- `Audio Test: start record`
- `Audio Test: record done`
- `Audio Test: start playback`
- `Audio Test: playback done`

## 适合进一步扩展的方向

- 将录音数据通过 UART 转发到另一块开发板
- 将 USB 音频业务真正迁移到 `V5F`，由 `V3F` 负责通信或控制
- 增加串口音频传输协议和缓冲区管理
- 对录音链路加入更明确的采样率/声道协商日志
- 将示例从本地录音回放扩展为板间实时音频传输

## 说明

本仓库中的代码和工程文件基于 WCH CH32H417 双核工程结构整理而来，当前重点是保留 USB 音频主机主链路，并让代码结构更便于阅读、调试和二次开发。
