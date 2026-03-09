/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2025/05/26
 * Description        : V5F core startup entry.
 * Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "debug.h"
#include "hardware.h"

/*********************************************************************
 * @fn      main
 *
 * @brief   V5F core entry.
 *          In the current code, V5F only completes the HSEM handshake in
 *          dual-core mode. The USB audio Hardware() path runs here only
 *          when building in V5F-only mode.
 * @return  none
 */
int main(void)
{
    SystemAndCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(921600);
    printf("V5F SystemCoreClk:%d\r\n", SystemCoreClock);

#if (Run_Core == Run_Core_V3FandV5F)
    HSEM_FastTake(HSEM_ID0);
    HSEM_ReleaseOneSem(HSEM_ID0, 0);

#elif (Run_Core == Run_Core_V3F)

#elif (Run_Core == Run_Core_V5F)
    Hardware();
#endif

    while(1)
    {
        ;
    }
}
