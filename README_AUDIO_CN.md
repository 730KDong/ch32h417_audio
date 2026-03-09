# CH32H417 USB Audio Host 阅读说明

这套工程已经从原来的通用 Host 示例整理成 USB 音频专用工程。
应用层核心文件已经统一为 [app_audio.c](c:\Users\dongk\Downloads\CH32H417EVT\EVT\EXAM\USBFS\HOST\Host_KM\Common\USB_Host\app_audio.c) 和 [app_audio.h](c:\Users\dongk\Downloads\CH32H417EVT\EVT\EXAM\USBFS\HOST\Host_KM\Common\USB_Host\app_audio.h)。

## 推荐阅读顺序

1. [hardware.c](c:\Users\dongk\Downloads\CH32H417EVT\EVT\EXAM\USBFS\HOST\Host_KM\Common\hardware.c)
2. [usb_host_config.h](c:\Users\dongk\Downloads\CH32H417EVT\EVT\EXAM\USBFS\HOST\Host_KM\Common\USB_Host\usb_host_config.h)
3. [ch32h417_usbfs_host.h](c:\Users\dongk\Downloads\CH32H417EVT\EVT\EXAM\USBFS\HOST\Host_KM\Common\USB_Host\ch32h417_usbfs_host.h)
4. [ch32h417_usbfs_host.c](c:\Users\dongk\Downloads\CH32H417EVT\EVT\EXAM\USBFS\HOST\Host_KM\Common\USB_Host\ch32h417_usbfs_host.c)
5. [app_audio.c](c:\Users\dongk\Downloads\CH32H417EVT\EVT\EXAM\USBFS\HOST\Host_KM\Common\USB_Host\app_audio.c)

## 先看什么

### 1. Type-C 主机角色建立

先看 `USBFS_TypeC_SourceInit()`。

这一步负责：
- 使能 USBPD 相关时钟
- 把 `PB3/PB4` 复用为 `CC1/CC2`
- 在 `CC1/CC2` 上提供 `Rp`

如果这一步没有完成，Type-C 数字耳机不会真正挂到 `D+/D-`，USB 枚举代码也就不会工作。

### 2. 根口枚举

再看 `USBH_MainDeal()` 和 `USBH_EnumRootDevice()`。

这里负责：
- 检测设备插入和拔出
- 读取设备描述符
- 分配设备地址
- 读取配置描述符
- 设置配置值
- 调用 `USBH_AnalyseType()` 判断设备类型

当前工程只把两种设备当作有效目标：
- `USB Audio`
- `USB HUB`

其他设备会被直接标记为不支持，不进入后续运行态。

### 3. 音频枚举

再看 `USBH_EnumAudioDevice()`。

这个函数会把音频运行所需的静态信息全部提取出来，包括：
- Audio Control 接口
- 扬声器流接口
- 麦克风流接口
- 线控 HID 接口
- Feature Unit
- 音频端点参数

读完这一段，你就能知道工程如何识别一只 USB 耳机的播放、录音、线控三条主链路。

### 4. 音频流控制

再看 `USBH_AudioSetStreamState()`。

它统一处理：
- `SET_INTERFACE` 切换 `Alt0/Alt1`
- `SET_CUR Sampling Frequency`
- Feature Unit 的静音和音量初始化
- 流开启时的统计量清零

可以把它理解成“打开或关闭一条音频流”的统一入口。

### 5. 运行期服务

最后看 `USBH_ServiceAudioDevice()`。

它负责三个周期性任务：
- 按播放端点周期发送等时 `OUT` 数据
- 按录音端点周期接收等时 `IN` 数据
- 按 HID 中断端点周期轮询线控按键

这是整个应用层的运行核心。

### 6. 线控按键状态机

`USBH_HandleAudioButton()` 负责按键状态机。

当前逻辑是：
- 空闲时按一下，开始录音
- 录音中按一下，停止录音
- 已有录音时按一下，播放录音

为了在有限 RAM 内支持更长录音，录音数据采用 `IMA ADPCM 4-bit` 压缩保存，回放时再实时解码。

## 这套工程保留了什么，删掉了什么

### 保留

- USBFS Host 底层传输逻辑
- HUB 枚举和下级端口管理
- USB Audio 设备枚举
- 音频录音、回放、启动旋律和线控测试

### 删除或停用

- 键盘、鼠标输入解析路径
- 与音频功能无关的示例逻辑
- 未使用的 HID `SetReport` 路径
- 非音频设备的持续业务处理逻辑

## 调试时先看哪些日志

### 枚举阶段

- `USB Port Dev In.`
- `Get DevDesc`
- `Get CfgDesc`
- `DevType: 01`

### 音频枚举

- `Play IF:...`
- `Mic IF:...`
- `Ctrl IF:...`

### 流控制

- `Audio Stream On ...`
- `Audio Stream Off ...`

### 测试状态机

- `Audio Demo: start boot melody 5000ms.`
- `Audio Test: start record ...`
- `Audio Test: record done ...`
- `Audio Test: start playback ...`
- `Audio Test: playback done.`

### 线控按键

- `Headset HID init: ...`
- `Headset HID: ...`

## 建议怎样学这套代码

1. 先看 `USBH_MainDeal()`，把设备插入后会经过哪些阶段串起来。
2. 再看 `USBH_EnumAudioDevice()`，理解接口号、端点号、格式参数是怎么被记录下来的。
3. 再看 `USBH_AudioSetStreamState()`，理解为什么播放和录音都要先切换备用接口。
4. 最后看 `USBH_ServiceAudioDevice()`，把播放、录音和 HID 三条运行时路径对应起来。
