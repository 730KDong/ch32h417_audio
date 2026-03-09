/********************************** (C) COPYRIGHT *******************************
* File Name          : ch32h417_usbfs_host.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2022/09/01
* Description        : USBFS 全速主机底层操作函数。
*********************************************************************************
* Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for 
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/


/*******************************************************************************/
/* 头文件 */
#include "usb_host_config.h"

/*******************************************************************************/
/* 全局变量定义 */
__attribute__((aligned(4))) uint8_t  USBFS_RX_Buf[ USBFS_MAX_PACKET_SIZE ];     // IN 方向 DMA 缓冲，地址必须按偶地址对齐。
__attribute__((aligned(4))) uint8_t  USBFS_TX_Buf[ USBFS_MAX_PACKET_SIZE ];     // OUT 方向 DMA 缓冲，地址必须按偶地址对齐。

/*********************************************************************
 * @fn      USBFS_RCC_Init
 *
 * @brief   初始化 USBFS 主机所需的时钟。
 *
 * @return  none
 */
void USBFS_RCC_Init(void)
{
    if((RCC->PLLCFGR & RCC_SYSPLL_SEL) != RCC_SYSPLL_USBHS)
    {
        /* 初始化 USBHS 480MHz PLL。 */
        RCC_USBHS_PLLCmd(DISABLE);
        RCC_USBHSPLLCLKConfig((RCC->CTLR & RCC_HSERDY) ? RCC_USBHSPLLSource_HSE : RCC_USBHSPLLSource_HSI);
        RCC_USBHSPLLReferConfig(RCC_USBHSPLLRefer_25M);
        RCC_USBHSPLLClockSourceDivConfig(RCC_USBHSPLL_IN_Div1);
        RCC_USBHS_PLLCmd(ENABLE);
        while (!(RCC->CTLR & RCC_USBHS_PLLRDY));
    }
    RCC_USBFSCLKConfig(RCC_USBFSCLKSource_USBHSPLL);
    RCC_USBFS48ClockSourceDivConfig(RCC_USBFS_Div10);
    RCC_HBPeriphClockCmd(RCC_HBPeriph_OTG_FS, ENABLE);
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOA, ENABLE);
}

/*********************************************************************
 * @fn      USBFS_Host_Init
 *
 * @brief   初始化 USB 主机端口配置。
 *
 * @param   sta - 使能或关闭主机端口
 *
 * @return  none
 */
void USBFS_Host_Init( FunctionalState sta )
{
    if( sta == ENABLE )
    {
        USBFSH->BASE_CTRL = USBFS_UC_HOST_MODE;
        while(!(USBFSH->BASE_CTRL & USBFS_UC_HOST_MODE));
        USBFSH->HOST_CTRL = 0;
        USBFSH->DEV_ADDR = 0;
        USBFSH->HOST_EP_MOD = USBFS_UH_EP_TX_EN | USBFS_UH_EP_RX_EN;

        USBFSH->HOST_RX_DMA = (uint32_t)USBFS_RX_Buf;
        USBFSH->HOST_TX_DMA = (uint32_t)USBFS_TX_Buf;

        USBFSH->HOST_RX_CTRL = 0;
        USBFSH->HOST_TX_CTRL = 0;
        USBFSH->BASE_CTRL = USBFS_UC_HOST_MODE | USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN;

        USBFSH->INT_FG = 0xFF;
        USBFSH->INT_EN = USBFS_UIE_TRANSFER | USBFS_UIE_DETECT;
    }
    else
    {
        USBFSH->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;
        Delay_Us( 10 );
        USBFSH->BASE_CTRL = 0;
    }
}

/*********************************************************************
 * @fn      USBFSH_CheckRootHubPortStatus
 *
 * @brief   结合保存的根口设备状态，检查当前 USB 端口连接状态。
 *
 * @para    dev_sta: 当前根口已记录的设备状态
 *
 * @return  当前端口状态
 */
uint8_t USBFSH_CheckRootHubPortStatus( uint8_t dev_sta )
{
    /* Check USB device connection or disconnection */
    if( USBFSH->INT_FG & USBFS_UIF_DETECT )
    {
        USBFSH->INT_FG = USBFS_UIF_DETECT; // 清除检测标志。
        if( USBFSH->MIS_ST & USBFS_UMS_DEV_ATTACH ) // 端口上检测到设备接入。
        {
            if( ( dev_sta == ROOT_DEV_DISCONNECT ) || ( ( dev_sta != ROOT_DEV_FAILED ) && ( USBFSH_CheckRootHubPortEnable( ) == 0x00 ) ) )
            {
                return ROOT_DEV_CONNECTED;
            }
            else
            {
                return ROOT_DEV_FAILED;
            }
        }
        else // 端口上没有设备。
        {
            return ROOT_DEV_DISCONNECT;
        }
    }
    else
    {
        return ROOT_DEV_FAILED;
    }
}

/*********************************************************************
 * @fn      USBFSH_CheckRootHubPortEnable
 *
 * @brief   检查 USB 端口是否已经使能。
 *          注意：设备断开时该位会被硬件自动清零。
 *
 * @return  当前端口使能状态
 */
uint8_t USBFSH_CheckRootHubPortEnable( void )
{
    return ( USBFSH->HOST_CTRL & USBFS_UH_PORT_EN );
}

/*********************************************************************
 * @fn      USBFSH_CheckRootHubPortSpeed
 *
 * @brief   检查 USB 端口速率。
 *
 * @return  当前端口速率
 */
uint8_t USBFSH_CheckRootHubPortSpeed( void )
{
    return ( USBFSH->MIS_ST & USBFS_UMS_DM_LEVEL? USB_LOW_SPEED: USB_FULL_SPEED );
}

/*********************************************************************
 * @fn      USBFSH_SetSelfAddr
 *
 * @brief   设置当前访问设备的 USB 地址。
 *
 * @para    addr: USB 设备地址
 *
 * @return  none
 */
void USBFSH_SetSelfAddr( uint8_t addr )
{
    USBFSH->DEV_ADDR = ( USBFSH->DEV_ADDR & USBFS_UDA_GP_BIT ) | ( addr & USBFS_USB_ADDR_MASK );
}

/*********************************************************************
 * @fn      USBFSH_SetSelfSpeed
 *
 * @brief   设置当前主机事务的 USB 速率。
 *
 * @para    speed: USB 速率
 *
 * @return  none
 */
void USBFSH_SetSelfSpeed( uint8_t speed )
{
    if( speed == USB_FULL_SPEED )
    {
        USBFSH->BASE_CTRL &= ~USBFS_UC_LOW_SPEED;
        USBFSH->HOST_CTRL &= ~USBFS_UH_LOW_SPEED;
        USBFSH->HOST_SETUP &= ~USBFS_UH_PRE_PID_EN;
    }
    else
    {
        USBFSH->BASE_CTRL |= USBFS_UC_LOW_SPEED;
        USBFSH->HOST_CTRL |= USBFS_UH_LOW_SPEED;
        USBFSH->HOST_SETUP |= USBFS_UH_PRE_PID_EN;
    }
}

/*********************************************************************
 * @fn      USBFSH_ResetRootHubPort
 *
 * @brief   复位 USB 主机端口。
 *
 * @para    mod: 端口复位模式
 *               0 -> 执行完整复位并等待结束
 *               1 -> 开始复位
 *               2 -> 结束复位
 *
 * @return  none
 */
void USBFSH_ResetRootHubPort( uint8_t mode )
{
    USBFSH_SetSelfAddr( 0x00 );
    USBFSH_SetSelfSpeed( USB_FULL_SPEED );

    if( mode <= 1 )
    {
        USBFSH->HOST_CTRL |= USBFS_UH_BUS_RESET; // 开始复位。
    }
    if( mode == 0 )
    {
        Delay_Ms( DEF_BUS_RESET_TIME ); // 复位保持时间，典型值 10ms 到 20ms。
    }
    if( mode != 1 )
    {
        USBFSH->HOST_CTRL &= ~USBFS_UH_BUS_RESET; // 结束复位。
    }
    Delay_Ms( 2 );

    if( USBFSH->INT_FG & USBFS_UIF_DETECT )
    {
        if( USBFSH->MIS_ST & USBFS_UMS_DEV_ATTACH )
        {
            USBFSH->INT_FG = USBFS_UIF_DETECT;
        }
    }
}

/*********************************************************************
 * @fn      USBFSH_EnableRootHubPort
 *
 * @brief   使能 USB 主机端口。
 *
 * @para    *pspeed: 返回检测到的设备速率
 *
 * @return  端口使能结果
 */
uint8_t USBFSH_EnableRootHubPort( uint8_t *pspeed )
{
    if( USBFSH->MIS_ST & USBFS_UMS_DEV_ATTACH )
    {
        if( USBFSH_CheckRootHubPortEnable( ) == 0x00 )
        { 
            *pspeed = USBFSH_CheckRootHubPortSpeed( );
            if( *pspeed == USB_LOW_SPEED )
            {
                USBFSH_SetSelfSpeed( USB_LOW_SPEED );
            }
        }
        USBFSH->HOST_CTRL |= USBFS_UH_PORT_EN;
        USBFSH->HOST_SETUP |= USBFS_UH_SOF_EN;

        return ERR_SUCCESS;
    }

    return ERR_USB_DISCON;
}

/*********************************************************************
 * @fn      USBFSH_Transact
 *
 * @brief   执行一次 USB 事务。
 *
 * @para    endp_pid: 事务 Token PID
 *          endp_tog: Toggle 或等时事务模式
 *          timeout: 超时时间
 *
 * @return  USB 传输结果
 */
uint8_t USBFSH_Transact( uint8_t endp_pid, uint8_t endp_tog, uint16_t timeout )
{
    uint8_t  r, trans_retry;
    uint16_t i;

    USBFSH->HOST_TX_CTRL = USBFSH->HOST_RX_CTRL = endp_tog;

    trans_retry = 0;
    do
    {

        USBFSH->HOST_EP_PID = endp_pid;       // 指定 Token PID 和端点号。
        USBFSH->INT_FG = USBFS_UIF_TRANSFER;  // 允许开始传输。
        for( i = DEF_WAIT_USB_TRANSFER_CNT; ( i != 0 ) && ( ( USBFSH->INT_FG & USBFS_UIF_TRANSFER ) == 0 ); i-- )
        {
            Delay_Us( 1 ); // 等待 USB 传输完成。
        }
        USBFSH->HOST_EP_PID = 0x00;  // 停止当前 USB 传输。
        if( ( USBFSH->INT_FG & USBFS_UIF_TRANSFER ) == 0 )
        {
            return ERR_USB_UNKNOWN;
        }
        else // 传输完成
        {

            if( USBFSH->INT_ST & USBFS_UIS_TOG_OK )
            {
                return ERR_SUCCESS;
            }
            r = USBFSH->INT_ST & USBFS_UIS_H_RES_MASK; // 当前 USB 事务的握手结果。
            if( r == USB_PID_STALL )
            {
                return ( r | ERR_USB_TRANSFER );
            }
            if( r == USB_PID_NAK )
            {
                if( ( ( endp_pid >> 4 ) == USB_PID_OUT ) && ( ( endp_tog & USBFS_UH_T_RES ) != 0 ) )
                {
                    return ERR_SUCCESS;
                }
                if( timeout == 0 )
                {
                    return ( r | ERR_USB_TRANSFER );
                }
                if( timeout < 0xFFFF )
                {
                    timeout--;
                }
                --trans_retry;
            }
            else switch ( endp_pid >> 4 )
            {
                case USB_PID_SETUP:
                case USB_PID_OUT:
                    if( ( ( endp_tog & USBFS_UH_T_RES ) != 0 ) && ( r == 0 ) )
                    {
                        return ERR_SUCCESS;
                    }
                    if( ( r == USB_PID_DATA0 ) || ( r == USB_PID_DATA1 ) )
                    {
                        return ERR_SUCCESS;
                    }
                    if( r )
                    {
                        return ( r | ERR_USB_TRANSFER );
                    }
                    break;
                case USB_PID_IN:
                    if( ( ( endp_tog & USBFS_UH_R_RES ) != 0 ) && ( r == 0 ) )
                    {
                        return ERR_SUCCESS;
                    }
                    if( ( r == USB_PID_DATA0 ) || ( r == USB_PID_DATA1 ) )
                    {
                        ;
                    }
                    else if( r )
                    {
                        return ( r | ERR_USB_TRANSFER );
                    }
                    break;
                default:
                    return ERR_USB_UNKNOWN;
            }
        }
        Delay_Us( 20 );

        if( USBFSH->INT_FG & USBFS_UIF_DETECT )
        {
            Delay_Us( 200 );

            if( USBFSH_CheckRootHubPortEnable( ) == 0x00 )
            {
                return ERR_USB_DISCON;  // 检测到设备断开事件。
            }
            else
            {
                USBFSH->INT_FG = USBFS_UIF_DETECT;
            }
        }
    }while( ++trans_retry < 10 );

    return ERR_USB_TRANSFER; // 事务应答超时。
}

/*********************************************************************
 * @fn      USBFSH_CtrlTransfer
 *
 * @brief   执行一次 USB 主机控制传输。
 *
 * @para    ep0_size: 设备端点 0 最大包长
 *          pbuf: 数据缓冲区
 *          plen: 数据长度
 *
 * @return  控制传输结果
 */
uint8_t USBFSH_CtrlTransfer( uint8_t ep0_size, uint8_t *pbuf, uint16_t *plen )
{
    uint8_t  s;
    uint16_t rem_len, rx_len, rx_cnt, tx_cnt;

    if( plen )
    {
        *plen = 0;
    }
    USBFSH->HOST_TX_LEN = sizeof( USB_SETUP_REQ );
    s = USBFSH_Transact( ( USB_PID_SETUP << 4 ) | 0x00, 0x00, DEF_CTRL_TRANS_TIMEOVER_CNT );  // SETUP stage
    if( s != ERR_SUCCESS )
    {
        return s;
    }
    USBFSH->HOST_TX_CTRL = USBFSH->HOST_RX_CTRL = USBFS_UH_T_TOG | USBFS_UH_R_TOG; // Default DATA1
    rem_len = pUSBFS_SetupRequest->wLength;
    if( rem_len && pbuf )
    {
        if( pUSBFS_SetupRequest->bRequestType & USB_REQ_TYP_IN )
        {
            /* Receive data */
            while( rem_len )
            {

                Delay_Us( 100 );
                s = USBFSH_Transact( ( USB_PID_IN << 4 ) | 0x00, USBFSH->HOST_RX_CTRL, DEF_CTRL_TRANS_TIMEOVER_CNT );  // IN
                if( s != ERR_SUCCESS )
                {
                    return s;
                }
                USBFSH->HOST_RX_CTRL ^= USBFS_UH_R_TOG;

                rx_len = ( USBFSH->RX_LEN < rem_len )? USBFSH->RX_LEN : rem_len;
                rem_len -= rx_len;
                if( plen )
                {
                    *plen += rx_len; // The total length of the actual successful transmission and reception
                }
                for( rx_cnt = 0; rx_cnt != rx_len; rx_cnt++ )
                {
                    *pbuf = USBFS_RX_Buf[ rx_cnt ];
                    pbuf++;
                }

                if( ( USBFSH->RX_LEN == 0 ) || ( USBFSH->RX_LEN & ( ep0_size - 1 ) ) )
                {
                    break; // Short package
                }
            }
            USBFSH->HOST_TX_LEN = 0; // Status stage is OUT
        }
        else
        {
            /* Send data */
            while( rem_len )
            {
                Delay_Us( 100 );
                USBFSH->HOST_TX_LEN = ( rem_len >= ep0_size )? ep0_size : rem_len;
                for( tx_cnt = 0; tx_cnt != USBFSH->HOST_TX_LEN; tx_cnt++ )
                {
                    USBFS_TX_Buf[ tx_cnt ] = *pbuf;
                    pbuf++;
                }
                s = USBFSH_Transact( USB_PID_OUT << 4 | 0x00, USBFSH->HOST_TX_CTRL, DEF_CTRL_TRANS_TIMEOVER_CNT ); // OUT
                if( s != ERR_SUCCESS )
                {
                    return s;
                }
                USBFSH->HOST_TX_CTRL ^= USBFS_UH_T_TOG;

                rem_len -= USBFSH->HOST_TX_LEN;
                if( plen )
                {
                    *plen += USBFSH->HOST_TX_LEN; // The total length of the actual successful transmission and reception
                }
            }
        }
    }
    Delay_Us( 100 );
    s = USBFSH_Transact( ( USBFSH->HOST_TX_LEN )? ( USB_PID_IN << 4 | 0x00 ) : ( USB_PID_OUT << 4 | 0x00 ), USBFS_UH_R_TOG | USBFS_UH_T_TOG, DEF_CTRL_TRANS_TIMEOVER_CNT ); // STATUS stage
    if( s != ERR_SUCCESS )
    {
        return s;
    }
    if( USBFSH->HOST_TX_LEN == 0 )
    {
        return ERR_SUCCESS;
    }
    if( USBFSH->RX_LEN == 0 )
    {
        return ERR_SUCCESS;
    }
    return ERR_USB_BUF_OVER;
}


/*********************************************************************
 * @fn      USBFSH_GetDeviceDescr
 *
 * @brief   读取 USB 设备描述符。
 *
 * @para    pep0_size: Device endpoint 0 size
 *          pbuf: Data buffer
 *
 * @return  The result of getting the device descriptor.
 */
uint8_t USBFSH_GetDeviceDescr( uint8_t *pep0_size, uint8_t *pbuf )
{
    uint8_t  s;
    uint16_t len;

    *pep0_size = DEFAULT_ENDP0_SIZE;
    memcpy( pUSBFS_SetupRequest, SetupGetDevDesc, sizeof( USB_SETUP_REQ ) );
    s = USBFSH_CtrlTransfer( *pep0_size, pbuf, &len );
    if( s != ERR_SUCCESS )
    {
        return s;
    }

    *pep0_size = ( (PUSB_DEV_DESCR)pbuf )->bMaxPacketSize0;
    if( len < ( (PUSB_SETUP_REQ)SetupGetDevDesc )->wLength )
    {
        return ERR_USB_BUF_OVER;
    }
    return ERR_SUCCESS;
}

/*********************************************************************
 * @fn      USBFSH_GetConfigDescr
 *
 * @brief   读取 USB 配置描述符。
 *
 * @para    ep0_size: Device endpoint 0 size
 *          pbuf: Data buffer
 *          buf_len: Data buffer length
 *          pcfg_len: The length of the device configuration descriptor
 *
 * @return  The result of getting the configuration descriptor.
 */
uint8_t USBFSH_GetConfigDescr( uint8_t ep0_size, uint8_t *pbuf, uint16_t buf_len, uint16_t *pcfg_len )
{
    uint8_t  s;
    
    memcpy( pUSBFS_SetupRequest, SetupGetCfgDesc, sizeof( USB_SETUP_REQ ) );
    s = USBFSH_CtrlTransfer( ep0_size, pbuf, pcfg_len );
    if( s != ERR_SUCCESS )
    {
        return s;
    }
    if( *pcfg_len < ( (PUSB_SETUP_REQ)SetupGetCfgDesc )->wLength )
    {
        return ERR_USB_BUF_OVER;
    }

    *pcfg_len = ( (PUSB_CFG_DESCR)pbuf )->wTotalLength;
    if( *pcfg_len > buf_len  )
    {
        *pcfg_len = buf_len;
    }
    memcpy( pUSBFS_SetupRequest, SetupGetCfgDesc, sizeof( USB_SETUP_REQ ) );
    pUSBFS_SetupRequest->wLength = *pcfg_len;
    s = USBFSH_CtrlTransfer( ep0_size, pbuf, pcfg_len );
    return s;
}

/*********************************************************************
 * @fn      USBFSH_GetStrDescr
 *
 * @brief   读取 USB 字符串描述符。
 *
 * @para    ep0_size: Device endpoint 0 size
 *          str_num: Index of string descriptor  
 *          pbuf: Data buffer
 *
 * @return  The result of getting the string descriptor.
 */
uint8_t USBFSH_GetStrDescr( uint8_t ep0_size, uint8_t str_num, uint8_t *pbuf )
{
    uint8_t  s;
    uint16_t len;

    /* Get the string descriptor of the first 4 bytes */
    memcpy( pUSBFS_SetupRequest, SetupGetStrDesc, sizeof( USB_SETUP_REQ ) );
    pUSBFS_SetupRequest->wValue = ( (uint16_t)USB_DESCR_TYP_STRING << 8 ) | str_num;
    s = USBFSH_CtrlTransfer( ep0_size, pbuf, &len );
    if( s != ERR_SUCCESS )
    {
        return s;
    }

    /* Get the complete string descriptor */
    len = pbuf[ 0 ];
    memcpy( pUSBFS_SetupRequest, SetupGetStrDesc, sizeof( USB_SETUP_REQ ) );
    pUSBFS_SetupRequest->wValue = ( (uint16_t)USB_DESCR_TYP_STRING << 8 ) | str_num;
    pUSBFS_SetupRequest->wLength = len;
    s = USBFSH_CtrlTransfer( ep0_size, pbuf, &len );
    if( s != ERR_SUCCESS )
    {
        return s;
    }
    return ERR_SUCCESS;
}

/*********************************************************************
 * @fn      USBFSH_SetUsbAddress
 *
 * @brief   给设备设置 USB 地址。
 *
 * @para    ep0_size: Device endpoint 0 size
 *          addr: Device address
 *
 * @return  The result of setting device address.
 */
uint8_t USBFSH_SetUsbAddress( uint8_t ep0_size, uint8_t addr )
{
    uint8_t  s;

    memcpy( pUSBFS_SetupRequest, SetupSetAddr, sizeof( USB_SETUP_REQ ) );
    pUSBFS_SetupRequest->wValue = (uint16_t)addr;
    s = USBFSH_CtrlTransfer( ep0_size, NULL, NULL );
    if( s != ERR_SUCCESS )
    {
        return s;
    }
    USBFSH_SetSelfAddr( addr );
    Delay_Ms( DEF_BUS_RESET_TIME >> 1 ); // 等待设备内部完成地址切换。
    return ERR_SUCCESS;
}

/*********************************************************************
 * @fn      USBFSH_SetUsbConfig
 *
 * @brief   设置 USB 配置值。
 *
 * @para    ep0_size: Device endpoint 0 size
 *          cfg: Device configuration value
 *
 * @return  The result of setting device configuration.
 */
uint8_t USBFSH_SetUsbConfig( uint8_t ep0_size, uint8_t cfg_val )
{
    memcpy( pUSBFS_SetupRequest, SetupSetConfig, sizeof( USB_SETUP_REQ ) );
    pUSBFS_SetupRequest->wValue = (uint16_t)cfg_val;
    return USBFSH_CtrlTransfer( ep0_size, NULL, NULL );
}

/*********************************************************************
 * @fn      USBFSH_SetInterface
 *
 * @brief   选择 USB 设备的备用接口设置。
 *
 * @para    ep0_size: Device endpoint 0 size
 *          intf_num: Interface number
 *          alt_value: Alternate setting value
 *
 * @return  The result of setting device interface.
 */
uint8_t USBFSH_SetInterface( uint8_t ep0_size, uint8_t intf_num, uint8_t alt_value )
{
    memcpy( pUSBFS_SetupRequest, SetupSetInterface, sizeof( USB_SETUP_REQ ) );
    pUSBFS_SetupRequest->wIndex = (uint16_t)intf_num;
    pUSBFS_SetupRequest->wValue = (uint16_t)alt_value;
    return USBFSH_CtrlTransfer( ep0_size, NULL, NULL );
}

/*********************************************************************
 * @fn      USBFSH_ClearEndpStall
 *
 * @brief   清除端点 STALL 状态。
 *
 * @para    ep0_size: Device endpoint 0 size
 *          endp_num: Endpoint number.
 *
 * @return  The result of clearing endpoint stall.
 */
uint8_t USBFSH_ClearEndpStall( uint8_t ep0_size, uint8_t endp_num )
{
    memcpy( pUSBFS_SetupRequest, SetupClearEndpStall, sizeof( USB_SETUP_REQ ) );
    pUSBFS_SetupRequest->wIndex = (uint16_t)endp_num;
    return USBFSH_CtrlTransfer( ep0_size, NULL, NULL );
}

/*********************************************************************
 * @fn      USBFSH_GetEndpData
 *
 * @brief   从 USB 输入端点读取数据。
 *
 * @para    endp_num: Endpoint number
 *          endp_tog: Endpoint toggle
 *          pbuf: Data Buffer
 *          plen: Data length
 *
 * @return  The result of getting data.
 */
uint8_t USBFSH_GetEndpData( uint8_t endp_num, uint8_t *pendp_tog, uint8_t *pbuf, uint16_t *plen )
{
    uint8_t  s;
    
    s = USBFSH_Transact( ( USB_PID_IN << 4 ) | endp_num, *pendp_tog, 0 );
    if( s == ERR_SUCCESS )
    {
        *plen = USBFSH->RX_LEN;
        memcpy( pbuf, USBFS_RX_Buf, *plen );
        if( ( *pendp_tog & USBFS_UH_R_RES ) == 0 )
        {
            *pendp_tog ^= USBFS_UH_R_TOG;
        }
    }
    
    return s;
}

/*********************************************************************
 * @fn      USBFSH_SendEndpData
 *
 * @brief   向 USB 输出端点发送数据。
 *
 * @para    endp_num: Endpoint number
 *          endp_tog: Endpoint toggle
 *          pbuf: Data Buffer
 *          len: Data length
 *
 * @return  The result of sending data.
 */
uint8_t USBFSH_SendEndpData( uint8_t endp_num, uint8_t *pendp_tog, uint8_t *pbuf, uint16_t len )
{
    uint8_t  s;
    
    memcpy( USBFS_TX_Buf, pbuf, len );
    USBFSH->HOST_TX_LEN = len;
    s = USBFSH_Transact( ( USB_PID_OUT << 4 ) | endp_num, *pendp_tog, 0 );
    if( s == ERR_SUCCESS )
    {
        if( ( *pendp_tog & USBFS_UH_T_TOG ) != 0 )
        {
            *pendp_tog &= (uint8_t)~USBFS_UH_T_TOG;
        }
        else
        {
            *pendp_tog |= USBFS_UH_T_TOG;
        }
    }

    return s;
}
