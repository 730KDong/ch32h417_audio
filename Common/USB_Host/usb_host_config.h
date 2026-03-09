/********************************** (C) COPYRIGHT  *******************************
 * File Name          : usb_host_config.h
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2022/08/29
 * Description        : USB 主机示例的公共配置头文件。
 *                      本工程已整理为 USB 音频专用例程，本文件主要定义
 *                      主机枚举状态、设备管理结构体以及音频运行时状态。
 *********************************************************************************
 * Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#ifndef __USB_HOST_CONFIG_H
#define __USB_HOST_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "string.h"
#include "debug.h"
#include "ch32h417_usb.h"
#include "ch32h417_usbfs_host.h"
#include "usb_host_hid.h"
#include "usb_host_hub.h"
#include "app_audio.h"

/* 调试打印开关。关闭后 DUG_PRINTF 会被编译成空操作。 */
#define DEF_DEBUG_PRINTF            1
#if ( DEF_DEBUG_PRINTF == 1 )
#define DUG_PRINTF( format, arg... )    printf( format, ##arg )
#else
#define DUG_PRINTF( format, arg... )    do{ if( 0 )printf( format, ##arg ); }while( 0 );
#endif

/* 根口与下级 HUB 的规模配置。当前工程只启用一个 USBFS 根口。 */
#define DEF_TOTAL_ROOT_HUB          1
#define DEF_USBFS_PORT_EN           1
#define DEF_USBFS_PORT_INDEX        0x00
#define DEF_ONE_USB_SUP_DEV_TOTAL   5
#define DEF_NEXT_HUB_PORT_NUM_MAX   4
#define DEF_INTERFACE_NUM_MAX       4

/* 根口设备状态。 */
#define ROOT_DEV_DISCONNECT         0
#define ROOT_DEV_CONNECTED          1
#define ROOT_DEV_FAILED             2
#define ROOT_DEV_SUCCESS            3

/* 枚举时分配给设备的地址。 */
#define USB_DEVICE_ADDR             0x02

/* USB 速率定义。 */
#define USB_LOW_SPEED               0x00
#define USB_FULL_SPEED              0x01
#define USB_HIGH_SPEED              0x02
#define USB_SPEED_CHECK_ERR         0xFF

/* 描述符类型。 */
#define DEF_DECR_CONFIG             0x02
#define DEF_DECR_INTERFACE          0x04
#define DEF_DECR_ENDPOINT           0x05
#define DEF_DECR_HID                0x21

/* 主机栈内部使用的错误码。 */
#define ERR_SUCCESS                 0x00
#define ERR_USB_CONNECT             0x15
#define ERR_USB_DISCON              0x16
#define ERR_USB_BUF_OVER            0x17
#define ERR_USB_DISK_ERR            0x1F
#define ERR_USB_TRANSFER            0x20
#define ERR_USB_UNSUPPORT           0xFB
#define ERR_USB_UNAVAILABLE         0xFC
#define ERR_USB_UNKNOWN             0xFE

/* 枚举阶段的失败原因。 */
#define DEF_DEV_DESCR_GETFAIL       0x45
#define DEF_DEV_ADDR_SETFAIL        0x46
#define DEF_CFG_DESCR_GETFAIL       0x47
#define DEF_REP_DESCR_GETFAIL       0x48
#define DEF_CFG_VALUE_SETFAIL       0x49
#define DEF_DEV_TYPE_UNKNOWN        0xFF

/* 总线复位、重连等待和控制传输超时参数。 */
#define DEF_BUS_RESET_TIME          11
#define DEF_RE_ATTACH_TIMEOUT       100
#define DEF_WAIT_USB_TRANSFER_CNT   1000
#define DEF_CTRL_TRANS_TIMEOVER_CNT 60000

/* HUB 下级端口上的设备状态。 */
typedef struct _HUB_DEVICE
{
    uint8_t  bStatus;
    uint8_t  bType;
    uint8_t  bAddress;
    uint8_t  bSpeed;
    uint8_t  bEp0MaxPks;
    uint8_t  DeviceIndex;
} HUB_DEVICE, *PHUB_DEVICE;

/* 根口设备状态。如果根口接的是 HUB，则 Device[] 保存各下级端口设备。 */
typedef struct _ROOT_HUB_DEVICE
{
    uint8_t  bStatus;
    uint8_t  bType;
    uint8_t  bAddress;
    uint8_t  bSpeed;
    uint8_t  bEp0MaxPks;
    uint8_t  DeviceIndex;
    uint8_t  bPortNum;
    HUB_DEVICE Device[ DEF_NEXT_HUB_PORT_NUM_MAX ];
} ROOT_HUB_DEVICE, *PROOT_HUB_DEVICE;

/*
 * 单条音频数据流的运行时控制块。
 * Play 和 Record 都使用这个结构，只是方向不同。
 * - Valid: 枚举阶段已经找到该流对应的接口和端点。
 * - Running: 当前是否已经通过 SET_INTERFACE 打开该备用接口。
 * - IsIn: 1 表示录音流，0 表示播放流。
 * - PacketSize: 按 1ms USB 帧换算后的实际收发字节数。
 * - Toggle / TimeCount: 主机轮询时用到的软状态。
 */
typedef struct _AUDIO_STREAM_CTL
{
    uint8_t  Valid;
    uint8_t  Running;
    uint8_t  IsIn;
    uint8_t  InterfaceNum;
    uint8_t  AltSetting;
    uint8_t  EndpointAddr;
    uint8_t  EndpointType;
    uint16_t MaxPacketSize;
    uint16_t PacketSize;
    uint8_t  Interval;
    uint8_t  Toggle;
    uint8_t  Channels;
    uint8_t  SubFrameSize;
    uint8_t  BitResolution;
    uint32_t SampleRate;
    uint8_t  TimeCount;
    uint32_t PacketCount;
    uint32_t ErrorCount;
    uint8_t  LastError;
} AUDIO_STREAM_CTL, *PAUDIO_STREAM_CTL;

/* 线控按键使用的 HID 中断输入端点信息。 */
typedef struct _AUDIO_HID_CTL
{
    uint8_t  Valid;
    uint8_t  InterfaceNum;
    uint8_t  InEndpAddr;
    uint16_t InEndpSize;
    uint8_t  InEndpInterval;
    uint8_t  InEndpTimeCount;
    uint8_t  InEndpTog;
    uint16_t HidDescLen;
} AUDIO_HID_CTL, *PAUDIO_HID_CTL;

/*
 * 单个 USB 音频设备的完整运行时状态。
 * 这里集中保存枚举结果、录放音状态机、HID 线控状态，以及录音缓存
 * 对应的 ADPCM 编码器/解码器状态。
 */
typedef struct _AUDIO_CTL
{
    uint8_t  Active;
    uint8_t  AcInterfaceNum;
    uint8_t  Ep0Size;
    uint8_t  PlayFeatureUnitId;
    uint8_t  RecordFeatureUnitId;
    AUDIO_STREAM_CTL Play;
    AUDIO_STREAM_CTL Record;
    AUDIO_HID_CTL    Hid;
    uint32_t TonePhase;
    uint8_t  TestState;
    uint8_t  HidPrimed;
    uint8_t  HidLastPressed;
    uint8_t  HidLastLen;
    uint8_t  HidLastBuf[ 8 ];
    uint16_t HidDebounceMs;
    uint8_t  AutoPlayPending;
    uint16_t RecordPeak;
    uint8_t  PlaybackGain;
    int16_t  RecordAdpcmPred;
    uint8_t  RecordAdpcmIndex;
    int16_t  PlayAdpcmPred;
    uint8_t  PlayAdpcmIndex;
    uint32_t RecordDataLen;
    uint32_t RecordSampleCount;
    uint32_t RecordTargetLen;
    uint32_t PlayDataPos;
    uint32_t PlayTargetLen;
} AUDIO_CTL, *PAUDIO_CTL;

/*
 * 主机控制块。
 * Interface[] 保留了通用 USB Host 例程的接口/端点资源描述能力，
 * Audio 字段则保存本工程真正关心的 USB 音频设备状态。
 */
typedef struct __HOST_CTL
{
    uint8_t  InterfaceNum;
    uint8_t  ErrorCount;

    struct interface
    {
        uint8_t  Type;
        uint16_t HidDescLen;

        uint8_t  InEndpNum;
        uint8_t  InEndpAddr[ 4 ];
        uint8_t  InEndpType[ 4 ];
        uint16_t InEndpSize[ 4 ];
        uint8_t  InEndpTog[ 4 ];
        uint8_t  InEndpInterval[ 4 ];
        uint8_t  InEndpTimeCount[ 4 ];

        uint8_t  OutEndpNum;
        uint8_t  OutEndpAddr[ 4 ];
        uint8_t  OutEndpType[ 4 ];
        uint16_t OutEndpSize[ 4 ];
        uint8_t  OutEndpTog[ 4 ];
    }Interface[ DEF_INTERFACE_NUM_MAX ];

    AUDIO_CTL Audio;
} HOST_CTL, *PHOST_CTL;

extern struct _ROOT_HUB_DEVICE RootHubDev;
extern struct __HOST_CTL HostCtl[ ];

#ifdef __cplusplus
}
#endif

#endif
