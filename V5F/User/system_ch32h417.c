/********************************** (C) COPYRIGHT *******************************
 * File Name          : system_ch32h417.c
 * Author             : WCH
 * Version            : V1.0.1
 * Date               : 2025/10/16
 * Description        : CH32H417 系统时钟辅助文件。
 *                      本文件根据寄存器当前配置，回填 SystemClock、
 *                      HCLKClock 和 SystemCoreClock 三个全局变量。
 *                      适用于外部高速时钟 HSE = 25MHz 的配置。
 *********************************************************************************
 * Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "ch32h417.h"

uint32_t HCLKClock;
uint32_t SystemClock;
uint32_t SystemCoreClock;

/* 寄存器编码到实际倍频/分频值的查找表。 */
static __I uint8_t PLLMULTB[ 32 ] = { 4, 6, 7, 8, 17, 9, 19, 10, 21, 11, 23, 12, 25, 13, 14, 15, 16, 17, 18, 19, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 59 };
static __I uint8_t HBPrescTB[ 16 ] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9 };
static __I uint8_t SERDESPLLMULTB[ 16 ] = { 25, 28, 30, 32, 35, 38, 40, 45, 50, 56, 60, 64, 70, 76, 80, 90 };
static __I uint8_t FPRETB[ 4 ] = { 0, 1, 2, 2 };

/*********************************************************************
 * @fn      SystemAndCoreClockUpdate
 *
 * @brief   根据当前时钟寄存器配置，更新系统时钟相关全局变量。
 *          该函数不会修改硬件，只负责把寄存器状态换算为可读频率值。
 *
 * @return  none
 */
void SystemAndCoreClockUpdate( void )
{
    uint32_t tmp = 0;
    uint32_t tmp1 = 0;
    uint32_t tmp2 = 0;
    uint32_t tmp3 = 0;
    uint32_t pllmull = 0;
    uint32_t pllsource = 0;
    uint32_t presc = 0;
    uint32_t presc1 = 0;

    tmp = RCC->CFGR0 & RCC_SWS;
    tmp2 = RCC->PLLCFGR & RCC_SYSPLL_SEL;

    switch( tmp )
    {
        case 0x00:
            SystemClock = HSI_VALUE;
            break;

        case 0x04:
            SystemClock = HSE_VALUE;
            break;

        case 0x08:
            switch( tmp2 )
            {
                case RCC_SYSPLL_PLL:
                    pllmull = RCC->PLLCFGR & RCC_PLLMUL;
                    pllsource = RCC->PLLCFGR & RCC_PLLSRC;
                    presc = ( ( ( RCC->PLLCFGR & RCC_PLL_SRC_DIV ) >> 8 ) + 1 );

                    if( pllsource == 0xA0 )
                    {
                        tmp1 = 500000000 / presc;
                    }
                    else if( pllsource == 0xE0 )
                    {
                        tmp1 = HSE_VALUE * SERDESPLLMULTB[ RCC->PLLCFGR2 >> 16 ] / 2 / presc;
                    }
                    else if( pllsource == 0x80 )
                    {
                        tmp1 = 480000000 / presc;
                    }
                    else if( pllsource == 0xC0 )
                    {
                        tmp1 = 125000000 / presc;
                    }
                    else if( pllsource == 0x20 )
                    {
                        tmp1 = HSE_VALUE / presc;
                    }
                    else
                    {
                        tmp1 = HSI_VALUE / presc;
                    }

                    if( ( pllmull == 4 ) || ( pllmull == 6 ) || ( pllmull == 8 ) || ( pllmull == 10 ) || ( pllmull == 12 ) )
                    {
                        SystemClock = ( tmp1 * PLLMULTB[ pllmull ] ) >> 1;
                    }
                    else
                    {
                        SystemClock = tmp1 * PLLMULTB[ pllmull ];
                    }
                    break;

                case RCC_SYSPLL_USBHS:
                    SystemClock = 480000000;
                    break;

                case RCC_SYSPLL_ETH:
                    SystemClock = 500000000;
                    break;

                case RCC_SYSPLL_SERDES:
                    SystemClock = HSE_VALUE * SERDESPLLMULTB[ RCC->PLLCFGR2 >> 16 ] / 2;
                    break;

                case RCC_SYSPLL_USBSS:
                    SystemClock = 125000000;
                    break;

                default:
                    SystemClock = HSI_VALUE;
                    break;
            }
            break;

        default:
            SystemClock = HSI_VALUE;
            break;
    }

    /* 先计算系统总线时钟，再进一步折算出 HCLK。 */
    tmp = ( RCC->CFGR0 & RCC_HPRE ) >> 4;
    presc1 = HBPrescTB[ tmp ];
    tmp3 = SystemClock >> presc1;

    tmp = ( RCC->CFGR0 & RCC_FPRE ) >> 16;
    presc1 = FPRETB[ tmp ];
    HCLKClock = tmp3 >> presc1;

    /* 双核器件中，V3F 与 V5F 看到的核心时钟不同。 */
    if( NVIC_GetCurrentCoreID( ) == 0 )
    {
        SystemCoreClock = HCLKClock;
    }
    else
    {
        SystemCoreClock = tmp3;
    }
}
