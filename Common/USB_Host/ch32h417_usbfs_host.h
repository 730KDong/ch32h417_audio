/********************************** (C) COPYRIGHT *******************************
* File Name          : ch32h417_usbfs_host.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2025/06/06
* Description        : USBFS 全速主机底层接口头文件。
*********************************************************************************
* Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef __CH32H417_USBFS_HOST_H__
#define __CH32H417_USBFS_HOST_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "ch32h417.h"
#include "ch32h417_usb.h"

/* 便于访问 Setup 包的快捷宏。 */
#define pUSBFS_SetupRequest        ( (PUSB_SETUP_REQ)USBFS_TX_Buf )

/* USBFS 主机读写缓冲区大小。 */
#ifndef USBFS_MAX_PACKET_SIZE
#define USBFS_MAX_PACKET_SIZE      512
#endif

/* USB 标准请求模板。 */
#ifndef DEF_USB_GEN_ENUM_CMD
#define DEF_USB_GEN_ENUM_CMD

/* 获取设备描述符请求模板。 */
__attribute__((aligned(4))) static const uint8_t SetupGetDevDesc[] =
{
    USB_REQ_TYP_IN, USB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_DEVICE, 0x00, 0x00, sizeof(USB_DEV_DESCR), 0x00
};

/* 获取配置描述符请求模板。 */
__attribute__((aligned(4))) static const uint8_t SetupGetCfgDesc[] =
{
    USB_REQ_TYP_IN, USB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_CONFIG, 0x00, 0x00, 0x04, 0x00
};

/* 获取字符串描述符请求模板。 */
__attribute__((aligned(4))) static const uint8_t SetupGetStrDesc[] =
{
    USB_REQ_TYP_IN, USB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_STRING, 0x09, 0x04, 0x04, 0x00
};

/* 设置设备地址请求模板。 */
__attribute__((aligned(4))) static const uint8_t SetupSetAddr[] =
{
    USB_REQ_TYP_OUT, USB_SET_ADDRESS, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* 设置配置值请求模板。 */
__attribute__((aligned(4))) static const uint8_t SetupSetConfig[] =
{
    USB_REQ_TYP_OUT, USB_SET_CONFIGURATION, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* 清除端点 STALL 请求模板。 */
__attribute__((aligned(4))) static const uint8_t SetupClearEndpStall[] =
{
    USB_REQ_TYP_OUT | USB_REQ_RECIP_ENDP, USB_CLEAR_FEATURE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* 设置接口备用设置请求模板。 */
__attribute__((aligned(4))) static const uint8_t SetupSetInterface[] =
{
    USB_REQ_RECIP_INTERF, USB_SET_INTERFACE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
#endif

/* 底层收发缓冲区声明。 */
extern __attribute__((aligned(4))) uint8_t USBFS_RX_Buf[];
extern __attribute__((aligned(4))) uint8_t USBFS_TX_Buf[];

/* USBFS 主机底层接口声明。 */
extern void USBFS_RCC_Init(void);
extern void USBFS_Host_Init(FunctionalState sta);
extern uint8_t USBFSH_CheckRootHubPortStatus(uint8_t dev_sta);
extern uint8_t USBFSH_CheckRootHubPortEnable(void);
extern uint8_t USBFSH_CheckRootHubPortSpeed(void);
extern void USBFSH_SetSelfAddr(uint8_t addr);
extern void USBFSH_SetSelfSpeed(uint8_t speed);
extern void USBFSH_ResetRootHubPort(uint8_t mode);
extern uint8_t USBFSH_EnableRootHubPort(uint8_t *pspeed);
extern uint8_t USBFSH_Transact(uint8_t endp_pid, uint8_t endp_tog, uint16_t timeout);
extern uint8_t USBFSH_CtrlTransfer(uint8_t ep0_size, uint8_t *pbuf, uint16_t *plen);
extern uint8_t USBFSH_GetDeviceDescr(uint8_t *pep0_size, uint8_t *pbuf);
extern uint8_t USBFSH_GetConfigDescr(uint8_t ep0_size, uint8_t *pbuf, uint16_t buf_len, uint16_t *pcfg_len);
extern uint8_t USBFSH_GetStrDescr(uint8_t ep0_size, uint8_t str_num, uint8_t *pbuf);
extern uint8_t USBFSH_SetUsbAddress(uint8_t ep0_size, uint8_t addr);
extern uint8_t USBFSH_SetUsbConfig(uint8_t ep0_size, uint8_t cfg_val);
extern uint8_t USBFSH_SetInterface(uint8_t ep0_size, uint8_t intf_num, uint8_t alt_value);
extern uint8_t USBFSH_ClearEndpStall(uint8_t ep0_size, uint8_t endp_num);
extern uint8_t USBFSH_GetEndpData(uint8_t endp_num, uint8_t *pendp_tog, uint8_t *pbuf, uint16_t *plen);
extern uint8_t USBFSH_SendEndpData(uint8_t endp_num, uint8_t *pendp_tog, uint8_t *pbuf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif
