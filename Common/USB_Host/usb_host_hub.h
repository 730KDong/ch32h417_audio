/********************************** (C) COPYRIGHT  *******************************
 * File Name          : usb_host_hub.h
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2022/08/29
 * Description        : USB HUB 类请求定义。
 *********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef __USB_HOST_HUB_H
#define __USB_HOST_HUB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

/* HUB 类标准请求模板。 */
#ifndef DEF_HUB_DED_CMD
#define DEF_HUB_DED_CMD

/* 读取端口状态请求模板。 */
__attribute__((aligned(4))) static const uint8_t GetPortStatus[] =
{
    0xA3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00
};

/* 清除端口特性请求模板。 */
__attribute__((aligned(4))) static const uint8_t ClearPortFeature[] =
{
    0x23, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* 设置端口特性请求模板。 */
__attribute__((aligned(4))) static const uint8_t SetPortFeature[] =
{
    0x23, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* 读取 HUB 类描述符请求模板。 */
__attribute__((aligned(4))) static const uint8_t GetHubDescr[] =
{
    0xA0, 0x06, 0x00, 0x29, 0x00, 0x00, 0x02, 0x00
};
#endif

/* HUB 类辅助函数声明。 */
extern uint8_t HUB_GetPortStatus(uint8_t hub_ep0_size, uint8_t hub_port, uint8_t *pbuf);
extern uint8_t HUB_ClearPortFeature(uint8_t hub_ep0_size, uint8_t hub_port, uint8_t selector);
extern uint8_t HUB_SetPortFeature(uint8_t hub_ep0_size, uint8_t hub_port, uint8_t selector);
extern uint8_t HUB_GetClassDevDescr(uint8_t hub_ep0_size, uint8_t *pbuf, uint16_t *plen);

#ifdef __cplusplus
}
#endif

#endif
