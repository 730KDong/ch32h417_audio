/********************************** (C) COPYRIGHT  *******************************
 * File Name          : app_audio.h
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2020/04/30
 * Description        : USB 音频应用层对外接口声明。
 *********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef __APP_AUDIO_H
#define __APP_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

/*
 * 通用控制传输缓冲区长度。
 * 设备描述符、配置描述符以及若干类请求都复用这块缓冲区。
 */
#define DEF_COM_BUF_LEN                 1024

/*
 * 全局缓冲区由 app_audio.c 定义。
 * 其他模块只能把它们当作临时数据区使用，不应长期保存其地址。
 */
extern uint8_t DevDesc_Buf[];
extern uint8_t Com_Buf[];

/*
 * 应用层对外入口：
 * 1. TIM3_Init：建立 1ms 软件时基。
 * 2. USBH_MainDeal：主循环入口，负责枚举、录音、回放和线控按键测试。
 */
extern void TIM3_Init(void);
extern void USBH_AnalyseType(uint8_t *pdev_buf, uint8_t *pcfg_buf, uint8_t *ptype);
extern uint8_t USBH_EnumRootDevice(void);
extern uint8_t HUB_AnalyzeConfigDesc(uint8_t index);
extern uint8_t HUB_Port_PreEnum1(uint8_t hub_port, uint8_t *pbuf);
extern uint8_t HUB_Port_PreEnum2(uint8_t hub_port, uint8_t *pbuf);
extern uint8_t HUB_CheckPortSpeed(uint8_t hub_port, uint8_t *pbuf);
extern uint8_t USBH_EnumHubPortDevice(uint8_t hub_port, uint8_t *paddr, uint8_t *ptype);
extern void USBH_MainDeal(void);

#ifdef __cplusplus
}
#endif

#endif
