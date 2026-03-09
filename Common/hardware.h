/********************************** (C) COPYRIGHT  *******************************
* File Name          : hardware.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2025/03/01
* Description        : 硬件初始化对外接口声明。
*********************************************************************************
* Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/
#ifndef __HARDWARE_H
#define __HARDWARE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ch32h417.h"
#include "debug.h"

/* 初始化 USB 音频主机工程所需的全部硬件资源，并进入主循环。 */
void Hardware(void);

#ifdef __cplusplus
}
#endif

#endif
