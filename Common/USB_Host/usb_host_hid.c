/********************************** (C) COPYRIGHT  *******************************
 * File Name          : usb_host_hid.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2022/08/29
 * Description        : USB HID 类辅助请求实现。
 *********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#include "usb_host_config.h"

/*********************************************************************
 * @fn      HID_GetHidDesr
 *
 * @brief   读取 USB 设备的 HID 报告描述符。
 *
 * @para    ep0_size: 设备端点 0 最大包长
 *          intf_num: HID 接口号
 *          pbuf: 保存描述符的缓冲区
 *          plen: 期望读取长度，返回实际读取长度
 *
 * @return  传输结果
 */
uint8_t HID_GetHidDesr(uint8_t ep0_size, uint8_t intf_num, uint8_t *pbuf, uint16_t *plen)
{
    memcpy(pUSBFS_SetupRequest, SetupGetHidDes, sizeof(USB_SETUP_REQ));
    pUSBFS_SetupRequest->wIndex = (uint16_t)intf_num;
    pUSBFS_SetupRequest->wLength = *plen;
    return USBFSH_CtrlTransfer(ep0_size, pbuf, plen);
}

/*********************************************************************
 * @fn      HID_SetIdle
 *
 * @brief   设置 HID 设备的 Idle 时间。
 *
 * @para    ep0_size: 设备端点 0 最大包长
 *          intf_num: HID 接口号
 *          duration: Idle 持续时间
 *          reportid: 目标 Report ID
 *
 * @return  传输结果
 */
uint8_t HID_SetIdle(uint8_t ep0_size, uint8_t intf_num, uint8_t duration, uint8_t reportid)
{
    memcpy(pUSBFS_SetupRequest, SetupSetidle, sizeof(USB_SETUP_REQ));
    pUSBFS_SetupRequest->wValue = ((uint16_t)duration << 8) | reportid;
    pUSBFS_SetupRequest->wIndex = (uint16_t)intf_num;
    return USBFSH_CtrlTransfer(ep0_size, NULL, NULL);
}
