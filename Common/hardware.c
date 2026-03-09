/********************************** (C) COPYRIGHT  *******************************
* File Name          : hardware.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2025/03/01
* Description        : USB 音频主机示例的硬件初始化入口。
*********************************************************************************
* Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/
/*
 * 本文件只做两类事情：
 * 1. 建立 Type-C 主机侧电气角色
 * 2. 初始化 USBFS Host，并进入应用层主循环
 *
 * 如果 Type-C 角色没有先建立成功，数字耳机不会真正挂到 D+/D- 上，
 * 这时应用层再怎么写，也不会收到 USB 插入事件。
 */
/*******************************************************************************/
#include "hardware.h"
#include "usb_host_config.h"

#define AUDIO_HOST_REVISION        "AUDIO_HOST_R15"

/*
 * 让板载 Type-C 口以 Source 身份工作。
 * 这一动作的本质是在 CC1/CC2 上提供 Rp，告诉耳机“我是主机”。
 */
static void USBFS_TypeC_SourceInit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = { 0 };

    /* 使能 GPIO、AFIO 和 USBPD 时钟。 */
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOB | RCC_HB2Periph_AFIO, ENABLE);
    RCC_HBPeriphClockCmd(RCC_HBPeriph_USBPD, ENABLE);

    /* 将 PB3/PB4 复用为 USBPD 的 CC1/CC2。 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource3, GPIO_AF4);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource4, GPIO_AF4);

    AFIO->PCFR1 |= (1 << 20);

    /* 清除状态，并把 CC1/CC2 配置为 Rp 上拉。 */
    USBPD->CONFIG = 0;
    USBPD->STATUS = BUF_ERR | IF_RX_BIT | IF_RX_BYTE | IF_RX_ACT | IF_RX_RESET | IF_TX_END;
    USBPD->PORT_CC1 = CC_CMP_66 | CC_PU_330;
    USBPD->PORT_CC2 = CC_CMP_66 | CC_PU_330;

    DUG_PRINTF("Type-C SRC Init: CC1/CC2 Rp enabled.\r\n");
}

/*********************************************************************
 * @fn      Hardware
 *
 * @brief   初始化 USB 音频主机工程所需的硬件，并进入主循环。
 *
 * @return  none
 */
void Hardware(void)
{
    printf("Build Time: %s %s\n", __DATE__, __TIME__);
    printf("GCC Version: %d.%d.%d\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
    printf("Audio Host Rev: %s\n", AUDIO_HOST_REVISION);
    DUG_PRINTF("USBFS HOST AUDIO Test\r\n");

    /* 初始化 1ms 软件时基。 */
    TIM3_Init();
    DUG_PRINTF("TIM3 Init OK!\r\n");

#if DEF_USBFS_PORT_EN
    DUG_PRINTF("USBFS Host Init\r\n");

    /*
     * 初始化顺序必须固定：
     * 1. 先建立 Type-C 主机角色
     * 2. 再打开 USBFS Host 控制器
     */
    USBFS_TypeC_SourceInit();
    Delay_Ms(20);
    USBFS_RCC_Init();
    USBFS_Host_Init(ENABLE);

    memset(&RootHubDev.bStatus, 0, sizeof(ROOT_HUB_DEVICE));
    memset(&HostCtl[DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL].InterfaceNum,
           0,
           DEF_ONE_USB_SUP_DEV_TOTAL * sizeof(HOST_CTL));
#endif

    while(1)
    {
        USBH_MainDeal();
    }
}
