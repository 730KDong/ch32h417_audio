/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2025/05/26
 * Description        : V3F 核启动入口。
 *********************************************************************************
 * Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "debug.h"
#include "hardware.h"

/*********************************************************************
 * @fn      main
 *
 * @brief   V3F 核主函数。
 *          在双核模式下负责唤醒 V5F；在单核运行模式下直接进入硬件初始化。
 *
 * @return  none
 */
int main(void)
{
    SystemInit();
    SystemAndCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(921600);
    printf("SystemClk:%d\r\n", SystemClock);
    printf("V3F SystemCoreClk:%d\r\n", SystemCoreClock);

#if (Run_Core == Run_Core_V3FandV5F)
    NVIC_WakeUp_V5F(Core_V5F_StartAddr); /* 唤醒 V5F 内核。 */
    HSEM_ITConfig(HSEM_ID0, ENABLE);
    NVIC->SCTLR |= 1 << 4;
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_PWR, ENABLE);
    PWR_EnterSTOPMode(PWR_Regulator_ON, PWR_STOPEntry_WFE);
    HSEM_ClearFlag(HSEM_ID0);
    printf("V3F wake up\r\n");

    Hardware();

#elif (Run_Core == Run_Core_V3F)
    Hardware();

#elif (Run_Core == Run_Core_V5F)
    NVIC_WakeUp_V5F(Core_V5F_StartAddr); /* 唤醒 V5F 内核。 */
    PWR_EnterSTOPMode(PWR_Regulator_ON, PWR_STOPEntry_WFE);
    printf("V3F wake up\r\n");
#endif

    while(1)
    {
        ;
    }
}
