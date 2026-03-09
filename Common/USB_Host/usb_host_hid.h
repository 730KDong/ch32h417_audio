/********************************** (C) COPYRIGHT  *******************************
 * File Name          : usb_host_hid.h
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2022/08/29
 * Description        : USB HID 类辅助请求定义。
 *********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef __USB_HOST_HID_H
#define __USB_HOST_HID_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

/*
 * HID 类标准请求模板。
 * 这些模板会在运行时复制到 Setup 包中，再补齐接口号和长度等字段。
 */
#ifndef DEF_HID_DED_CMD
#define DEF_HID_DED_CMD

/* 设置 HID 协议类型的请求模板。 */
__attribute__((aligned(4))) static const uint8_t SetupSetprotocol[] =
{
    0x21, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* 设置 HID Idle 时间的请求模板。 */
__attribute__((aligned(4))) static const uint8_t SetupSetidle[] =
{
    0x21, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* 读取 HID Report Descriptor 的请求模板。 */
__attribute__((aligned(4))) static const uint8_t SetupGetHidDes[] =
{
    0x81, 0x06, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00
};
#endif

/* HID 类辅助函数声明。 */
extern uint8_t HID_GetHidDesr(uint8_t ep0_size, uint8_t intf_num, uint8_t *pbuf, uint16_t *plen);
extern uint8_t HID_SetIdle(uint8_t ep0_size, uint8_t intf_num, uint8_t duration, uint8_t reportid);

#ifdef __cplusplus
}
#endif

#endif
