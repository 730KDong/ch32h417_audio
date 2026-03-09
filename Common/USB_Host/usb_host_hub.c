/********************************** (C) COPYRIGHT  *******************************
 * File Name          : usb_host_hub.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2022/08/29
 * Description        : USB HUB 类辅助请求实现。
 *********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#include "usb_host_config.h"

/*********************************************************************
 * @fn      HUB_GetPortStatus
 *
 * @brief   读取指定 HUB 端口的状态。
 *
 * @para    hub_ep0_size: HUB 设备端点 0 最大包长
 *          hub_port: HUB 端口号
 *          pbuf: 接收端口状态的缓冲区
 *
 * @return  传输结果
 */
uint8_t HUB_GetPortStatus(uint8_t hub_ep0_size, uint8_t hub_port, uint8_t *pbuf)
{
    uint16_t len;

    memcpy(pUSBFS_SetupRequest, GetPortStatus, sizeof(USB_SETUP_REQ));
    pUSBFS_SetupRequest->wIndex = (uint16_t)hub_port;
    return USBFSH_CtrlTransfer(hub_ep0_size, pbuf, &len);
}

/*********************************************************************
 * @fn      HUB_ClearPortFeature
 *
 * @brief   清除指定 HUB 端口的某个特性位。
 *
 * @para    hub_ep0_size: HUB 设备端点 0 最大包长
 *          hub_port: HUB 端口号
 *          selector: 需要清除的特性选择子
 *
 * @return  传输结果
 */
uint8_t HUB_ClearPortFeature(uint8_t hub_ep0_size, uint8_t hub_port, uint8_t selector)
{
    memcpy(pUSBFS_SetupRequest, ClearPortFeature, sizeof(USB_SETUP_REQ));
    pUSBFS_SetupRequest->wValue = (uint16_t)selector;
    pUSBFS_SetupRequest->wIndex = (uint16_t)hub_port;
    return USBFSH_CtrlTransfer(hub_ep0_size, NULL, NULL);
}

/*********************************************************************
 * @fn      HUB_SetPortFeature
 *
 * @brief   设置指定 HUB 端口的某个特性位。
 *
 * @para    hub_ep0_size: HUB 设备端点 0 最大包长
 *          hub_port: HUB 端口号
 *          selector: 需要设置的特性选择子
 *
 * @return  传输结果
 */
uint8_t HUB_SetPortFeature(uint8_t hub_ep0_size, uint8_t hub_port, uint8_t selector)
{
    memcpy(pUSBFS_SetupRequest, SetPortFeature, sizeof(USB_SETUP_REQ));
    pUSBFS_SetupRequest->wValue = (uint16_t)selector;
    pUSBFS_SetupRequest->wIndex = (uint16_t)hub_port;
    return USBFSH_CtrlTransfer(hub_ep0_size, NULL, NULL);
}

/*********************************************************************
 * @fn      HUB_GetClassDevDescr
 *
 * @brief   读取 HUB 类描述符。
 *
 * @para    hub_ep0_size: HUB 设备端点 0 最大包长
 *          pbuf: 描述符缓冲区
 *          plen: 输入期望长度，返回实际长度
 *
 * @return  传输结果
 */
uint8_t HUB_GetClassDevDescr(uint8_t hub_ep0_size, uint8_t *pbuf, uint16_t *plen)
{
    uint8_t s;

    memcpy(pUSBFS_SetupRequest, GetHubDescr, sizeof(USB_SETUP_REQ));
    s = USBFSH_CtrlTransfer(hub_ep0_size, pbuf, plen);
    if(s != ERR_SUCCESS)
    {
        return s;
    }

    pUSBFS_SetupRequest->wLength = *plen = (uint16_t)pbuf[0];
    s = USBFSH_CtrlTransfer(hub_ep0_size, pbuf, plen);

    return s;
}
