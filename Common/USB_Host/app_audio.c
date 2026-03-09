/*
 * USB 音频专用例程。
 *
 * 本文件基于 CH32H417 USBFS Host 栈实现一个便于学习的 USB 音频主机示例。
 * 代码包含设备枚举、音频流控制、线控按键处理、录音回放和 HUB 下级设备支持。
 */
#include <stddef.h>
#include "usb_host_config.h"
#include "ch32h417_tim.h"



/* 标准设备描述符缓存。 */
uint8_t  DevDesc_Buf[ 18 ];
/* 枚举和控制传输共用缓冲区。 */
uint8_t  Com_Buf[ DEF_COM_BUF_LEN ];
/* 单个播放包缓冲区。 */
static uint8_t Audio_PlayBuf[ 512 ];
/* 单个录音包缓冲区。 */
static uint8_t Audio_RecBuf[ 512 ];
/* 线控 HID 报告缓冲区。 */
static uint8_t Audio_HidBuf[ 64 ];
/* ADPCM 录音存储区，最长支持约 10 秒录音。 */
static uint8_t Audio_RecordStore[ 240000 ];
struct   _ROOT_HUB_DEVICE RootHubDev;
struct   __HOST_CTL HostCtl[ DEF_TOTAL_ROOT_HUB * DEF_ONE_USB_SUP_DEV_TOTAL ];

/* 音频测试状态机。 */
#define AUDIO_TEST_IDLE                0
#define AUDIO_TEST_RECORDING           1
#define AUDIO_TEST_READY               2
#define AUDIO_TEST_PLAYBACK            3
#define AUDIO_TEST_BOOT_MELODY         4
/* USB Audio Class 请求常量。 */
#define AUDIO_REQ_SET_CUR              0x01
#define AUDIO_REQ_EP_OUT               0x22
#define AUDIO_REQ_IF_OUT               0x21
#define AUDIO_CS_SAM_FREQ              0x0100
#define AUDIO_FU_MUTE                  0x0100
#define AUDIO_FU_VOLUME                0x0200
/* 音频测试参数。 */
#define AUDIO_BOOT_MELODY_MS           5000
#define AUDIO_RECORD_MAX_MS            10000
#define AUDIO_RECORD_PLAY_GAIN_MAX     24
#define AUDIO_RECORD_GAIN_PEAK_CAP     12000
#define AUDIO_RECORD_INPUT_SHIFT       1
/* TIM3 用作 1ms 周期中断，为轮询逻辑提供时间基准。 */
void TIM3_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

/* 内部辅助函数声明。 */
static void USBH_PrintDeviceSummary( uint8_t *pdev_buf, uint8_t *pcfg_buf );
static void USBH_PrintAudioDeviceInfo( uint8_t *pdev_buf, uint8_t *pcfg_buf );
static uint8_t USBH_EnumAudioDevice( uint8_t index, uint8_t ep0_size );
static void USBH_ServiceAudioDevice( uint8_t index, uint8_t dev_addr, uint8_t dev_speed );
static uint8_t USBH_AudioSetStreamState( uint8_t dev_addr, uint8_t dev_speed, uint8_t ep0_size, PAUDIO_STREAM_CTL pstream, uint8_t enable );
static void USBH_HandleAudioButton( uint8_t dev_addr, uint8_t dev_speed, PAUDIO_CTL paudio );
static uint8_t USBH_AudioSetSampleFreq( uint8_t ep0_size, uint8_t endp_addr, uint32_t sample_rate );
static uint8_t USBH_AudioSetFeatureMute( uint8_t ep0_size, uint8_t ac_intf, uint8_t unit_id, uint8_t mute );
static uint8_t USBH_AudioSetFeatureVolume( uint8_t ep0_size, uint8_t ac_intf, uint8_t unit_id, int16_t volume );
static int16_t USBH_GetBootMelodySample( PAUDIO_CTL paudio );
static int16_t USBH_ApplySampleGain( int16_t sample, uint8_t gain );
static uint32_t USBH_GetRecordDurationMs( PAUDIO_CTL paudio );
static uint8_t USBH_CalcPlaybackGain( PAUDIO_CTL paudio );
static uint32_t USBH_GetRecordTargetLen( PAUDIO_CTL paudio );
static void USBH_FinishRecording( uint8_t dev_addr, uint8_t dev_speed, PAUDIO_CTL paudio );
static void USBH_AudioTick( uint8_t index );
static uint8_t USBH_ADPCMEncodeSample( int16_t sample, int16_t *ppredictor, uint8_t *pindex );
static int16_t USBH_ADPCMDecodeSample( uint8_t code, int16_t *ppredictor, uint8_t *pindex );


/* IMA ADPCM 索引调整表。 */
static const int8_t Audio_AdpcmIndexTable[ 16 ] =
{
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

/* IMA ADPCM 步长表。 */
static const int16_t Audio_AdpcmStepTable[ 89 ] =
{
       7,    8,    9,   10,   11,   12,   13,   14,   16,   17,   19,
      21,   23,   25,   28,   31,   34,   37,   41,   45,   50,   55,
      60,   66,   73,   80,   88,   97,  107,  118,  130,  143,  157,
     173,  190,  209,  230,  253,  279,  307,  337,  371,  408,  449,
     494,  544,  598,  658,  724,  796,  876,  963, 1060, 1166, 1282,
    1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660,
    4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493,10442,
   11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,
   32767
};
















/*
 * 初始化 TIM3，产生 1ms 周期中断。
 * 这里不直接发起 USB 事务，而是给主循环中的音频轮询逻辑提供统一节拍。
 */
void TIM3_Init()
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = { 0 };
    RCC_ClocksTypeDef ClockStructure = { 0 };
    /* 读取当前系统时钟，后续用它换算 TIM3 分频。 */
    RCC_GetClocksFreq(&ClockStructure);
    /* 打开 TIM3 时钟。 */
    RCC_HB1PeriphClockCmd( RCC_HB1Periph_TIM3, ENABLE );

    /* 配置成 1ms 产生一次更新事件。 */
    TIM_TimeBaseStructure.TIM_Period = 10 - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = ClockStructure.HCLK_Frequency / 10000 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit( TIM3, &TIM_TimeBaseStructure );
    TIM_ITConfig( TIM3, TIM_IT_Update, ENABLE );
    NVIC_SetPriority(TIM3_IRQn,0x80);
    NVIC_EnableIRQ(TIM3_IRQn);

    /* 启动 TIM3。 */
    TIM_Cmd( TIM3, ENABLE );
}
/*
 * TIM3 中断服务函数。
 * 每进入一次中断，就给音频流、HID 轮询和 HUB 中断端点的时间计数器加 1。
 */
void TIM3_IRQHandler( void )
{
    uint8_t index;
    uint8_t hub_port;

    if( TIM_GetITStatus( TIM3, TIM_IT_Update ) != RESET )
    {
        TIM_ClearITPendingBit( TIM3, TIM_IT_Update );

        /* 只有设备已经完成枚举后，时间计数才有意义。 */
        if( RootHubDev.bStatus >= ROOT_DEV_SUCCESS )
        {
            index = RootHubDev.DeviceIndex;
            if( RootHubDev.bType == USB_DEV_CLASS_AUDIO )
            {
                USBH_AudioTick( index );
            }
            else if( RootHubDev.bType == USB_DEV_CLASS_HUB )
            {
                HostCtl[ index ].Interface[ 0 ].InEndpTimeCount[ 0 ]++;
                for( hub_port = 0; hub_port < RootHubDev.bPortNum; hub_port++ )
                {
                    if( RootHubDev.Device[ hub_port ].bStatus >= ROOT_DEV_SUCCESS )
                    {
                        index = RootHubDev.Device[ hub_port ].DeviceIndex;

                        if( RootHubDev.Device[ hub_port ].bType == USB_DEV_CLASS_AUDIO )
                        {
                            USBH_AudioTick( index );
                        }
                    }
                }
            }
        }
    }
}










/* 打印设备描述符和接口描述符摘要，便于观察枚举结果。 */
static void USBH_PrintDeviceSummary( uint8_t *pdev_buf, uint8_t *pcfg_buf )
{
    uint16_t i;
    uint16_t total_len;
    PUSB_DEV_DESCR pdev;
    PUSB_ITF_DESCR pitf;

    pdev = (PUSB_DEV_DESCR)pdev_buf;
    DUG_PRINTF( "Dev VID:%04x PID:%04x Class:%02x Sub:%02x Proto:%02x\r\n",
                pdev->idVendor,
                pdev->idProduct,
                pdev->bDeviceClass,
                pdev->bDeviceSubClass,
                pdev->bDeviceProtocol );

    total_len = ( (PUSB_CFG_DESCR)pcfg_buf )->wTotalLength;
    if( total_len > DEF_COM_BUF_LEN )
    {
        total_len = DEF_COM_BUF_LEN;
    }

    for( i = sizeof( USB_CFG_DESCR ); ( i + 2 ) <= total_len; )
    {
        if( pcfg_buf[ i ] < 2 )
        {
            break;
        }
        if( ( i + pcfg_buf[ i ] ) > total_len )
        {
            break;
        }

        if( pcfg_buf[ i + 1 ] == DEF_DECR_INTERFACE )
        {
            pitf = (PUSB_ITF_DESCR)( &pcfg_buf[ i ] );
            DUG_PRINTF( "IF:%u Alt:%u Class:%02x Sub:%02x Proto:%02x EP:%u\r\n",
                        pitf->bInterfaceNumber,
                        pitf->bAlternateSetting,
                        pitf->bInterfaceClass,
                        pitf->bInterfaceSubClass,
                        pitf->bInterfaceProtocol,
                        pitf->bNumEndpoints );
        }

        i += pcfg_buf[ i ];
    }
}










/* 打印音频相关接口信息，确认播放流、录音流和线控接口是否被识别到。 */
static void USBH_PrintAudioDeviceInfo( uint8_t *pdev_buf, uint8_t *pcfg_buf )
{
    uint16_t i;
    uint16_t total_len;
    PUSB_DEV_DESCR pdev;
    PUSB_ITF_DESCR pitf;

    pdev = (PUSB_DEV_DESCR)pdev_buf;
    DUG_PRINTF( "Audio Dev VID:%04x PID:%04x\r\n", pdev->idVendor, pdev->idProduct );

    total_len = ( (PUSB_CFG_DESCR)pcfg_buf )->wTotalLength;
    if( total_len > DEF_COM_BUF_LEN )
    {
        total_len = DEF_COM_BUF_LEN;
    }

    for( i = sizeof( USB_CFG_DESCR ); ( i + 2 ) <= total_len; )
    {
        if( pcfg_buf[ i ] < 2 )
        {
            break;
        }
        if( ( i + pcfg_buf[ i ] ) > total_len )
        {
            break;
        }

        if( pcfg_buf[ i + 1 ] == DEF_DECR_INTERFACE )
        {
            pitf = (PUSB_ITF_DESCR)( &pcfg_buf[ i ] );
            if( pitf->bInterfaceClass == USB_DEV_CLASS_AUDIO )
            {
                DUG_PRINTF( "Audio IF:%u Alt:%u Sub:%02x Proto:%02x EP:%u\r\n",
                            pitf->bInterfaceNumber,
                            pitf->bAlternateSetting,
                            pitf->bInterfaceSubClass,
                            pitf->bInterfaceProtocol,
                            pitf->bNumEndpoints );
            }
        }

        i += pcfg_buf[ i ];
    }
}

/* 按小端格式读取 16 位字段。 */
static uint16_t USBH_ReadLe16( const uint8_t *pbuf )
{
    return (uint16_t)pbuf[ 0 ] | ( (uint16_t)pbuf[ 1 ] << 8 );
}

/* 按小端格式读取 24 位字段，UAC1 采样率常用这种表示。 */
static uint32_t USBH_ReadLe24( const uint8_t *pbuf )
{
    return (uint32_t)pbuf[ 0 ] | ( (uint32_t)pbuf[ 1 ] << 8 ) | ( (uint32_t)pbuf[ 2 ] << 16 );
}

/* 切换当前主机事务的目标设备地址和速率。 */
static void USBH_SelectDevice( uint8_t dev_addr, uint8_t dev_speed )
{
    USBFSH_SetSelfAddr( dev_addr );
    USBFSH_SetSelfSpeed( dev_speed );
    if( dev_speed != USB_LOW_SPEED )
    {
        USBFSH->HOST_CTRL &= ~USBFS_UH_LOW_SPEED;
    }
}

/* 1ms 时基推进函数，递增各类端点的轮询计数器。 */
static void USBH_AudioTick( uint8_t index )
{
    if( HostCtl[ index ].Audio.HidDebounceMs )
    {
        HostCtl[ index ].Audio.HidDebounceMs--;
    }
    if( HostCtl[ index ].Audio.Play.Valid )
    {
        HostCtl[ index ].Audio.Play.TimeCount++;
    }
    if( HostCtl[ index ].Audio.Record.Valid )
    {
        HostCtl[ index ].Audio.Record.TimeCount++;
    }
    if( HostCtl[ index ].Audio.Hid.Valid )
    {
        HostCtl[ index ].Audio.Hid.InEndpTimeCount++;
    }
}

/* 根据采样率、声道数和样本字节数估算每 1ms 需要传输的字节数。 */
static uint16_t USBH_AudioCalcPacketSize( PAUDIO_STREAM_CTL pstream )
{
    uint32_t bytes_per_frame;
    uint32_t packet_size;

    if( ( pstream->SampleRate == 0 ) || ( pstream->Channels == 0 ) || ( pstream->SubFrameSize == 0 ) )
    {
        return pstream->MaxPacketSize;
    }

    bytes_per_frame = (uint32_t)pstream->Channels * pstream->SubFrameSize;
    packet_size = ( pstream->SampleRate * bytes_per_frame + 999 ) / 1000;
    if( ( packet_size == 0 ) || ( packet_size > pstream->MaxPacketSize ) )
    {
        packet_size = pstream->MaxPacketSize;
    }

    return (uint16_t)packet_size;
}

/* 打印某个音频流的关键参数，用于核对描述符解析结果。 */
static void USBH_PrintAudioStream( const char *name, PAUDIO_STREAM_CTL pstream )
{
    if( pstream->Valid == 0 )
    {
        return;
    }

    DUG_PRINTF( "%s IF:%u Alt:%u EP:%02x Max:%u Packet:%u %uch %ubit %luHz\r\n",
                name,
                pstream->InterfaceNum,
                pstream->AltSetting,
                pstream->EndpointAddr,
                pstream->MaxPacketSize,
                pstream->PacketSize,
                pstream->Channels,
                pstream->BitResolution,
                (unsigned long)pstream->SampleRate );
}

/* 生成开机旋律的单个采样点，用来单独验证播放链路。 */
static int16_t USBH_GetBootMelodySample( PAUDIO_CTL paudio )
{
    static const uint16_t note_freq[] =
    {
        523, 659, 784, 659, 880, 784, 659, 587, 659, 698, 784, 698, 659, 587
    };
    static const uint16_t note_ms[] =
    {
        350, 350, 350, 350, 350, 350, 350, 350, 350, 350, 350, 350, 400, 400
    };
    uint32_t frame_size;
    uint32_t sample_rate;
    uint32_t sample_index;
    uint32_t elapsed_ms;
    uint32_t acc_ms;
    uint16_t note_idx;
    uint32_t freq;
    int16_t sample;

    frame_size = (uint32_t)paudio->Play.Channels * paudio->Play.SubFrameSize;
    if( frame_size == 0 || paudio->Play.SampleRate == 0 )
    {
        return 0;
    }

    sample_rate = paudio->Play.SampleRate;
    sample_index = paudio->PlayDataPos;
    elapsed_ms = ( sample_index * 1000UL ) / sample_rate;
    acc_ms = 0;
    freq = 0;
    for( note_idx = 0; note_idx < ( sizeof( note_ms ) / sizeof( note_ms[ 0 ] ) ); note_idx++ )
    {
        acc_ms += note_ms[ note_idx ];
        if( elapsed_ms < acc_ms )
        {
            freq = note_freq[ note_idx ];
            break;
        }
    }

    if( freq == 0 )
    {
        return 0;
    }

    paudio->TonePhase += freq;
    while( paudio->TonePhase >= sample_rate )
    {
        paudio->TonePhase -= sample_rate;
    }

    sample = ( paudio->TonePhase < ( sample_rate / 2 ) ) ? 9000 : -9000;
    if( ( ( elapsed_ms / 175 ) & 0x01 ) != 0 )
    {
        sample = (int16_t)( sample / 2 );
    }

    return sample;
}

/* 对采样值施加软件增益，并做饱和裁剪。 */
static int16_t USBH_ApplySampleGain( int16_t sample, uint8_t gain )
{
    int32_t scaled;

    scaled = (int32_t)sample * gain;
    if( scaled > 32767 )
    {
        return 32767;
    }
    if( scaled < -32768 )
    {
        return -32768;
    }

    return (int16_t)scaled;
}

/* 根据已录样本数和采样率换算录音时长。 */
static uint32_t USBH_GetRecordDurationMs( PAUDIO_CTL paudio )
{
    

    if( paudio->Record.SampleRate == 0 )
    {
        return 0;
    }

    return ( paudio->RecordSampleCount * 1000UL ) / paudio->Record.SampleRate;
}

/* 按录音峰值估算回放增益，让测试回放更容易听清。 */
static uint8_t USBH_CalcPlaybackGain( PAUDIO_CTL paudio )
{
    uint32_t gain;
    uint32_t effective_peak;

    if( paudio->RecordPeak == 0 )
    {
        return 1;
    }

    
    effective_peak = paudio->RecordPeak;
    if( effective_peak > AUDIO_RECORD_GAIN_PEAK_CAP )
    {
        effective_peak = AUDIO_RECORD_GAIN_PEAK_CAP;
    }

    gain = 24000UL / effective_peak;
    if( gain == 0 )
    {
        gain = 1;
    }
    if( gain > AUDIO_RECORD_PLAY_GAIN_MAX )
    {
        gain = AUDIO_RECORD_PLAY_GAIN_MAX;
    }

    return (uint8_t)gain;
}

/* 计算当前录音流在最大录音时长下对应的样本上限。 */
static uint32_t USBH_GetRecordTargetLen( PAUDIO_CTL paudio )
{
    uint32_t sample_count;
    uint32_t max_samples;

    if( paudio->Record.SampleRate == 0 )
    {
        return 0;
    }

    


    sample_count = ( paudio->Record.SampleRate * AUDIO_RECORD_MAX_MS ) / 1000UL;
    max_samples = (uint32_t)sizeof( Audio_RecordStore ) * 2UL;
    if( sample_count > max_samples )
    {
        sample_count = max_samples;
    }

    return sample_count;
}

/* 统一结束录音流程，处理停流、统计和日志输出。 */
static void USBH_FinishRecording( uint8_t dev_addr, uint8_t dev_speed, PAUDIO_CTL paudio )
{
    



    if( paudio->Record.Running )
    {
        USBH_AudioSetStreamState( dev_addr, dev_speed, paudio->Ep0Size, &paudio->Record, 0 );
    }

    paudio->RecordDataLen = ( paudio->RecordSampleCount + 1UL ) >> 1;
    paudio->PlaybackGain = USBH_CalcPlaybackGain( paudio );
    paudio->TestState = AUDIO_TEST_READY;
    DUG_PRINTF( "Audio Test: record done, %lums, %lu samples, %lu bytes, %lu packets, peak:%u, gain:%u.\r\n",
                (unsigned long)USBH_GetRecordDurationMs( paudio ),
                (unsigned long)paudio->RecordSampleCount,
                (unsigned long)paudio->RecordDataLen,
                (unsigned long)paudio->Record.PacketCount,
                paudio->RecordPeak,
                paudio->PlaybackGain );
}

/* IMA ADPCM 编码单个 PCM 样本，降低录音占用的 RAM。 */
static uint8_t USBH_ADPCMEncodeSample( int16_t sample, int16_t *ppredictor, uint8_t *pindex )
{
    

    int32_t diff;
    int32_t delta;
    int32_t predictor;
    int32_t step;
    uint8_t code;
    int32_t new_index;

    predictor = *ppredictor;
    step = Audio_AdpcmStepTable[ *pindex ];
    diff = (int32_t)sample - predictor;
    code = 0;
    if( diff < 0 )
    {
        code = 0x08;
        diff = -diff;
    }

    delta = step >> 3;
    if( diff >= step )
    {
        code |= 0x04;
        diff -= step;
        delta += step;
    }
    step >>= 1;
    if( diff >= step )
    {
        code |= 0x02;
        diff -= step;
        delta += step;
    }
    step >>= 1;
    if( diff >= step )
    {
        code |= 0x01;
        delta += step;
    }

    if( code & 0x08 )
    {
        predictor -= delta;
    }
    else
    {
        predictor += delta;
    }

    if( predictor > 32767 )
    {
        predictor = 32767;
    }
    else if( predictor < -32768 )
    {
        predictor = -32768;
    }

    new_index = (int32_t)*pindex + Audio_AdpcmIndexTable[ code & 0x0F ];
    if( new_index < 0 )
    {
        new_index = 0;
    }
    else if( new_index > 88 )
    {
        new_index = 88;
    }

    *ppredictor = (int16_t)predictor;
    *pindex = (uint8_t)new_index;

    return ( code & 0x0F );
}

/* IMA ADPCM 解码单个 4bit 码字，回放时恢复 PCM。 */
static int16_t USBH_ADPCMDecodeSample( uint8_t code, int16_t *ppredictor, uint8_t *pindex )
{
    

    int32_t predictor;
    int32_t step;
    int32_t delta;
    int32_t new_index;

    predictor = *ppredictor;
    step = Audio_AdpcmStepTable[ *pindex ];
    delta = step >> 3;
    if( code & 0x04 )
    {
        delta += step;
    }
    if( code & 0x02 )
    {
        delta += step >> 1;
    }
    if( code & 0x01 )
    {
        delta += step >> 2;
    }

    if( code & 0x08 )
    {
        predictor -= delta;
    }
    else
    {
        predictor += delta;
    }

    if( predictor > 32767 )
    {
        predictor = 32767;
    }
    else if( predictor < -32768 )
    {
        predictor = -32768;
    }

    new_index = (int32_t)*pindex + Audio_AdpcmIndexTable[ code & 0x0F ];
    if( new_index < 0 )
    {
        new_index = 0;
    }
    else if( new_index > 88 )
    {
        new_index = 88;
    }

    *ppredictor = (int16_t)predictor;
    *pindex = (uint8_t)new_index;

    return (int16_t)predictor;
}

/*
 * 填充一个播放包。
 * 在开机旋律阶段，数据来自内部波形合成。
 * 在录音回放阶段，数据来自 ADPCM 解码后的 PCM。
 */
static uint32_t USBH_FillAudioPlaybackPacket( PAUDIO_CTL paudio, uint8_t *pbuf, uint16_t len )
{
    uint16_t frame_size;
    uint16_t frame_count;
    uint16_t frame_idx;
    uint16_t ch_idx;
    int16_t  sample;
    uint32_t play_pos;

    memset( pbuf, 0, len );
    if( paudio->Play.Valid == 0 )
    {
        return paudio->PlayDataPos;
    }

    frame_size = (uint16_t)paudio->Play.Channels * paudio->Play.SubFrameSize;
    if( ( frame_size == 0 ) || ( paudio->Play.SubFrameSize != 2 ) || ( paudio->Play.BitResolution != 16 ) )
    {
        return paudio->PlayDataPos;
    }

    

    frame_count = len / frame_size;
    play_pos = paudio->PlayDataPos;
    if( ( paudio->TestState == AUDIO_TEST_PLAYBACK ) || ( paudio->TestState == AUDIO_TEST_BOOT_MELODY ) )
    {
        for( frame_idx = 0; frame_idx < frame_count; frame_idx++ )
        {
            if( paudio->TestState == AUDIO_TEST_BOOT_MELODY )
            {
                sample = USBH_GetBootMelodySample( paudio );
                play_pos++;
            }
            else if( play_pos < paudio->RecordSampleCount )
            {
                uint8_t adpcm_code;

                
                adpcm_code = Audio_RecordStore[ play_pos >> 1 ];
                if( play_pos & 0x01 )
                {
                    adpcm_code >>= 4;
                }
                sample = USBH_ADPCMDecodeSample( adpcm_code & 0x0F, &paudio->PlayAdpcmPred, &paudio->PlayAdpcmIndex );
                sample = USBH_ApplySampleGain( sample, paudio->PlaybackGain );
                play_pos++;
            }
            else
            {
                sample = 0;
            }

            for( ch_idx = 0; ch_idx < paudio->Play.Channels; ch_idx++ )
            {
                *pbuf++ = (uint8_t)( sample & 0xFF );
                *pbuf++ = (uint8_t)( ( sample >> 8 ) & 0xFF );
            }
        }
    }

    return play_pos;
}

/*
 * 打开或关闭一个音频流。
 * enable=1 时会切到对应 AltSetting，并按需要设置采样率、静音和音量。
 * enable=0 时会切回 Alt0，表示停流。
 */
static uint8_t USBH_AudioSetStreamState( uint8_t dev_addr, uint8_t dev_speed, uint8_t ep0_size, PAUDIO_STREAM_CTL pstream, uint8_t enable )
{
    uint8_t s;

    if( pstream->Valid == 0 )
    {
        return ERR_SUCCESS;
    }

    if( pstream->Running == enable )
    {
        return ERR_SUCCESS;
    }

    USBH_SelectDevice( dev_addr, dev_speed );
    if( enable )
    {
        




        DUG_PRINTF( "Audio Stream On IF:%u Alt:%u EP:%02x %s\r\n",
                    pstream->InterfaceNum,
                    pstream->AltSetting,
                    pstream->EndpointAddr,
                    pstream->IsIn ? "IN" : "OUT" );
        s = USBFSH_SetInterface( ep0_size, pstream->InterfaceNum, pstream->AltSetting );
        if( s != ERR_SUCCESS )
        {
            return s;
        }

        if( pstream->SampleRate )
        {
            s = USBH_AudioSetSampleFreq( ep0_size,
                                         pstream->IsIn ? (uint8_t)( pstream->EndpointAddr | 0x80 ) : pstream->EndpointAddr,
                                         pstream->SampleRate );
            if( s != ERR_SUCCESS )
            {
                return s;
            }
        }

        if( pstream->IsIn == 0 )
        {
            PAUDIO_CTL paudio;

            paudio = (PAUDIO_CTL)( (uint8_t *)pstream - offsetof( AUDIO_CTL, Play ) );
            if( paudio->PlayFeatureUnitId && paudio->AcInterfaceNum != 0xFF )
            {
                DUG_PRINTF( "Play FU Init: AC:%u FU:%u\r\n", paudio->AcInterfaceNum, paudio->PlayFeatureUnitId );
                USBH_AudioSetFeatureMute( ep0_size, paudio->AcInterfaceNum, paudio->PlayFeatureUnitId, 0 );
                USBH_AudioSetFeatureVolume( ep0_size, paudio->AcInterfaceNum, paudio->PlayFeatureUnitId, 0 );
            }
        }
        else
        {
            PAUDIO_CTL paudio;

            paudio = (PAUDIO_CTL)( (uint8_t *)pstream - offsetof( AUDIO_CTL, Record ) );
            if( paudio->RecordFeatureUnitId && paudio->AcInterfaceNum != 0xFF )
            {
                DUG_PRINTF( "Mic FU Init: AC:%u FU:%u\r\n", paudio->AcInterfaceNum, paudio->RecordFeatureUnitId );
                USBH_AudioSetFeatureMute( ep0_size, paudio->AcInterfaceNum, paudio->RecordFeatureUnitId, 0 );
                USBH_AudioSetFeatureVolume( ep0_size, paudio->AcInterfaceNum, paudio->RecordFeatureUnitId, 0 );
            }
        }

        pstream->Toggle = ( ( pstream->EndpointType & 0x03 ) == 0x01 ) ?
                          ( pstream->IsIn ? USBFS_UH_R_RES : USBFS_UH_T_RES ) : 0x00;
        pstream->TimeCount = 0;
        pstream->PacketCount = 0;
        pstream->ErrorCount = 0;
        pstream->LastError = ERR_SUCCESS;
        pstream->Running = 1;
    }
    else
    {
        
        DUG_PRINTF( "Audio Stream Off IF:%u Alt:0 EP:%02x %s\r\n",
                    pstream->InterfaceNum,
                    pstream->EndpointAddr,
                    pstream->IsIn ? "IN" : "OUT" );
        s = USBFSH_SetInterface( ep0_size, pstream->InterfaceNum, 0 );
        if( s != ERR_SUCCESS )
        {
            return s;
        }
        pstream->Running = 0;
    }

    return ERR_SUCCESS;
}

/* 线控按键状态机：开始录音、停止录音、开始回放。 */
static void USBH_HandleAudioButton( uint8_t dev_addr, uint8_t dev_speed, PAUDIO_CTL paudio )
{
    uint8_t s;

    



    if( paudio->TestState == AUDIO_TEST_RECORDING )
    {
        USBH_FinishRecording( dev_addr, dev_speed, paudio );
    }
    else if( ( paudio->TestState == AUDIO_TEST_READY ) && paudio->RecordDataLen )
    {
        paudio->PlayDataPos = 0;
        paudio->PlayTargetLen = paudio->RecordSampleCount;
        paudio->PlayAdpcmPred = 0;
        paudio->PlayAdpcmIndex = 0;
        paudio->Play.PacketCount = 0;
        paudio->Play.ErrorCount = 0;
        paudio->Play.LastError = ERR_SUCCESS;
        s = USBH_AudioSetStreamState( dev_addr, dev_speed, paudio->Ep0Size, &paudio->Record, 0 );
        if( s != ERR_SUCCESS )
        {
            DUG_PRINTF( "Audio Test: stop mic err(%02x).\r\n", s );
            return;
        }
        s = USBH_AudioSetStreamState( dev_addr, dev_speed, paudio->Ep0Size, &paudio->Play, 1 );
        if( s != ERR_SUCCESS )
        {
            DUG_PRINTF( "Audio Test: start spk err(%02x).\r\n", s );
            return;
        }
        paudio->TestState = AUDIO_TEST_PLAYBACK;
        DUG_PRINTF( "Audio Test: start playback, %lums, %lu bytes.\r\n",
                    (unsigned long)USBH_GetRecordDurationMs( paudio ),
                    (unsigned long)paudio->RecordDataLen );
    }
    else
    {
        s = USBH_AudioSetStreamState( dev_addr, dev_speed, paudio->Ep0Size, &paudio->Play, 0 );
        if( s != ERR_SUCCESS )
        {
            DUG_PRINTF( "Audio Test: stop spk err(%02x).\r\n", s );
            return;
        }
        s = USBH_AudioSetStreamState( dev_addr, dev_speed, paudio->Ep0Size, &paudio->Record, 1 );
        if( s != ERR_SUCCESS )
        {
            DUG_PRINTF( "Audio Test: start mic err(%02x).\r\n", s );
            return;
        }
        paudio->RecordDataLen = 0;
        paudio->RecordSampleCount = 0;
        paudio->RecordTargetLen = USBH_GetRecordTargetLen( paudio );
        paudio->PlayDataPos = 0;
        paudio->PlayTargetLen = 0;
        paudio->RecordPeak = 0;
        paudio->PlaybackGain = 1;
        paudio->RecordAdpcmPred = 0;
        paudio->RecordAdpcmIndex = 0;
        paudio->PlayAdpcmPred = 0;
        paudio->PlayAdpcmIndex = 0;
        paudio->Record.PacketCount = 0;
        paudio->Record.ErrorCount = 0;
        paudio->Record.LastError = ERR_SUCCESS;
        paudio->TestState = AUDIO_TEST_RECORDING;
        DUG_PRINTF( "Audio Test: start record, max %ums.\r\n", AUDIO_RECORD_MAX_MS );
    }
}

/* 通过 UAC1 端点控制请求设置采样率。 */
static uint8_t USBH_AudioSetSampleFreq( uint8_t ep0_size, uint8_t endp_addr, uint32_t sample_rate )
{
    uint8_t  freq[ 3 ];
    uint16_t len;

    freq[ 0 ] = (uint8_t)( sample_rate & 0xFF );
    freq[ 1 ] = (uint8_t)( ( sample_rate >> 8 ) & 0xFF );
    freq[ 2 ] = (uint8_t)( ( sample_rate >> 16 ) & 0xFF );
    len = sizeof( freq );

    pUSBFS_SetupRequest->bRequestType = AUDIO_REQ_EP_OUT;
    pUSBFS_SetupRequest->bRequest = AUDIO_REQ_SET_CUR;
    pUSBFS_SetupRequest->wValue = AUDIO_CS_SAM_FREQ;
    pUSBFS_SetupRequest->wIndex = endp_addr;
    pUSBFS_SetupRequest->wLength = len;

    return USBFSH_CtrlTransfer( ep0_size, freq, &len );
}

/* 通过 Feature Unit 设置静音状态。 */
static uint8_t USBH_AudioSetFeatureMute( uint8_t ep0_size, uint8_t ac_intf, uint8_t unit_id, uint8_t mute )
{
    uint16_t len;

    len = 1;
    pUSBFS_SetupRequest->bRequestType = AUDIO_REQ_IF_OUT;
    pUSBFS_SetupRequest->bRequest = AUDIO_REQ_SET_CUR;
    pUSBFS_SetupRequest->wValue = AUDIO_FU_MUTE;
    pUSBFS_SetupRequest->wIndex = ( (uint16_t)unit_id << 8 ) | ac_intf;
    pUSBFS_SetupRequest->wLength = len;

    return USBFSH_CtrlTransfer( ep0_size, &mute, &len );
}

/* 通过 Feature Unit 设置音量。 */
static uint8_t USBH_AudioSetFeatureVolume( uint8_t ep0_size, uint8_t ac_intf, uint8_t unit_id, int16_t volume )
{
    uint8_t  vol[ 2 ];
    uint16_t len;

    vol[ 0 ] = (uint8_t)( volume & 0xFF );
    vol[ 1 ] = (uint8_t)( ( volume >> 8 ) & 0xFF );
    len = sizeof( vol );
    pUSBFS_SetupRequest->bRequestType = AUDIO_REQ_IF_OUT;
    pUSBFS_SetupRequest->bRequest = AUDIO_REQ_SET_CUR;
    pUSBFS_SetupRequest->wValue = AUDIO_FU_VOLUME;
    pUSBFS_SetupRequest->wIndex = ( (uint16_t)unit_id << 8 ) | ac_intf;
    pUSBFS_SetupRequest->wLength = len;

    return USBFSH_CtrlTransfer( ep0_size, vol, &len );
}

/*
 * 解析音频设备配置描述符。
 * 这里会找出 Audio Control、播放流、录音流、HID 线控和 Feature Unit。
 * 解析结果会写入 HostCtl[index].Audio，供后续运行态直接使用。
 */
static uint8_t USBH_EnumAudioDevice( uint8_t index, uint8_t ep0_size )
{
    uint16_t i;
    uint16_t total_len;
    uint16_t hid_rep_len;
    uint16_t term_type;
    uint8_t  cur_class;
    uint8_t  cur_subclass;
    uint8_t  cur_intf;
    uint8_t  cur_alt;
    uint8_t  fmt_channels;
    uint8_t  fmt_subframe;
    uint8_t  fmt_bits;
    uint32_t fmt_rate;
    PUSB_ITF_DESCR    pitf;
    PUSB_ENDP_DESCR   pendp;
    PAUDIO_CTL        paudio;
    PAUDIO_STREAM_CTL pstream;
    uint8_t  feature_src[ 16 ];
    uint8_t  s;
#if DEF_DEBUG_PRINTF
    uint16_t dump_i;
#endif

    paudio = &HostCtl[ index ].Audio;
    /* 每次重新枚举音频设备时，先把上一轮解析结果全部清空。 */
    memset( paudio, 0, sizeof( AUDIO_CTL ) );
    paudio->Ep0Size = ep0_size;
    memset( feature_src, 0, sizeof( feature_src ) );

    /* 配置描述符可能比通用缓冲区大，这里只在缓冲区范围内解析。 */
    total_len = ( (PUSB_CFG_DESCR)Com_Buf )->wTotalLength;
    if( total_len > DEF_COM_BUF_LEN )
    {
        total_len = DEF_COM_BUF_LEN;
    }

    cur_class = 0xFF;
    cur_subclass = 0xFF;
    cur_intf = 0xFF;
    cur_alt = 0;
    fmt_channels = 0;
    fmt_subframe = 0;
    fmt_bits = 0;
    fmt_rate = 0;
    hid_rep_len = 0;

    for( i = 0; ( i + 2 ) <= total_len; )
    {
        /* 每个描述符至少要有 bLength 和 bDescriptorType 两个字节。 */
        if( Com_Buf[ i ] < 2 )
        {
            break;
        }
        if( ( i + Com_Buf[ i ] ) > total_len )
        {
            break;
        }

        if( Com_Buf[ i + 1 ] == DEF_DECR_INTERFACE )
        {
            /* 遇到新的接口描述符，就刷新“当前接口上下文”。 */
            pitf = (PUSB_ITF_DESCR)( &Com_Buf[ i ] );
            cur_class = pitf->bInterfaceClass;
            cur_subclass = pitf->bInterfaceSubClass;
            cur_intf = pitf->bInterfaceNumber;
            cur_alt = pitf->bAlternateSetting;
            fmt_channels = 0;
            fmt_subframe = 0;
            fmt_bits = 0;
            fmt_rate = 0;

            if( ( cur_class == USB_DEV_CLASS_AUDIO ) && ( cur_subclass == 0x01 ) )
            {
                /* Audio Control 接口本身不传音频数据，但后续 Feature Unit 控制要用到它。 */
                paudio->AcInterfaceNum = cur_intf;
            }
        }
        else if( ( cur_class == USB_DEV_CLASS_AUDIO ) && ( cur_subclass == 0x01 ) &&
                 ( Com_Buf[ i + 1 ] == 0x24 ) && ( Com_Buf[ i ] >= 5 ) )
        {
            /* Audio Control 类专用描述符：这里重点关心输入端、输出端和 Feature Unit 的连接关系。 */
            if( ( Com_Buf[ i + 2 ] == 0x06 ) && ( Com_Buf[ i ] >= 5 ) )
            {
                if( Com_Buf[ i + 3 ] < sizeof( feature_src ) )
                {
                    feature_src[ Com_Buf[ i + 3 ] ] = Com_Buf[ i + 4 ];
                }
            }
            else if( ( Com_Buf[ i + 2 ] == 0x03 ) && ( Com_Buf[ i ] >= 8 ) )
            {
                term_type = USBH_ReadLe16( &Com_Buf[ i + 4 ] );
                if( Com_Buf[ i + 7 ] < sizeof( feature_src ) )
                {
                    if( ( term_type == 0x0301 ) || ( term_type == 0x0302 ) || ( term_type == 0x0303 ) )
                    {
                        paudio->PlayFeatureUnitId = Com_Buf[ i + 7 ];
                    }
                    else if( term_type == 0x0101 )
                    {
                        paudio->RecordFeatureUnitId = Com_Buf[ i + 7 ];
                    }
                }
            }
        }
        else if( ( cur_class == USB_DEV_CLASS_AUDIO ) && ( cur_subclass == 0x02 ) && ( cur_alt != 0 ) )
        {
            /* Audio Streaming 的 Alt0 一般是不带端点的停流状态，这里只解析真正带音频数据的 Alt。 */
            if( ( Com_Buf[ i + 1 ] == 0x24 ) && ( Com_Buf[ i ] >= 8 ) && ( Com_Buf[ i + 2 ] == 0x02 ) )
            {
                /* FORMAT_TYPE 描述符给出声道数、位宽和采样率。 */
                fmt_channels = Com_Buf[ i + 4 ];
                fmt_subframe = Com_Buf[ i + 5 ];
                fmt_bits = Com_Buf[ i + 6 ];
                if( Com_Buf[ i + 7 ] != 0 )
                {
                    if( Com_Buf[ i ] >= 11 )
                    {
                        fmt_rate = USBH_ReadLe24( &Com_Buf[ i + 8 ] );
                    }
                }
                else if( Com_Buf[ i ] >= 14 )
                {
                    fmt_rate = USBH_ReadLe24( &Com_Buf[ i + 8 ] );
                }
            }
            else if( ( Com_Buf[ i + 1 ] == DEF_DECR_ENDPOINT ) && ( Com_Buf[ i ] >= 7 ) )
            {
                /* 找到等时端点后，就把当前接口上下文固化为播放流或录音流。 */
                pendp = (PUSB_ENDP_DESCR)( &Com_Buf[ i ] );
                if( ( pendp->bmAttributes & 0x03 ) == 0x01 )
                {
                    if( pendp->bEndpointAddress & 0x80 )
                    {
                        pstream = &paudio->Record;
                        pstream->Toggle = USBFS_UH_R_RES;
                        pstream->IsIn = 1;
                    }
                    else
                    {
                        pstream = &paudio->Play;
                        pstream->Toggle = 0x00;
                        pstream->IsIn = 0;
                    }

                    if( pstream->Valid == 0 )
                    {
                        pstream->Valid = 1;
                        pstream->InterfaceNum = cur_intf;
                        pstream->AltSetting = cur_alt;
                        pstream->EndpointAddr = pendp->bEndpointAddress & 0x0F;
                        pstream->EndpointType = pendp->bmAttributes;
                        pstream->MaxPacketSize = USBH_ReadLe16( &Com_Buf[ i + 4 ] );
                        pstream->Interval = pendp->bInterval ? pendp->bInterval : 1;
                        pstream->Channels = fmt_channels;
                        pstream->SubFrameSize = fmt_subframe;
                        pstream->BitResolution = fmt_bits;
                        pstream->SampleRate = fmt_rate;
                        pstream->PacketSize = USBH_AudioCalcPacketSize( pstream );
                    }
                }
            }
        }
        else if( ( cur_class == USB_DEV_CLASS_HID ) && ( Com_Buf[ i + 1 ] == DEF_DECR_ENDPOINT ) &&
                 ( Com_Buf[ i ] >= 7 ) )
        {
            /* 线控按键通常通过 HID 中断输入端点上报。 */
            pendp = (PUSB_ENDP_DESCR)( &Com_Buf[ i ] );
            if( ( paudio->Hid.Valid == 0 ) && ( pendp->bEndpointAddress & 0x80 ) )
            {
                paudio->Hid.Valid = 1;
                paudio->Hid.InterfaceNum = cur_intf;
                paudio->Hid.InEndpAddr = pendp->bEndpointAddress & 0x0F;
                paudio->Hid.InEndpSize = USBH_ReadLe16( &Com_Buf[ i + 4 ] );
                paudio->Hid.InEndpInterval = pendp->bInterval ? pendp->bInterval : 1;
                paudio->Hid.HidDescLen = hid_rep_len;
            }
        }
        else if( ( cur_class == USB_DEV_CLASS_HID ) && ( Com_Buf[ i + 1 ] == DEF_DECR_HID ) && ( Com_Buf[ i ] >= 9 ) )
        {
            hid_rep_len = USBH_ReadLe16( &Com_Buf[ i + 7 ] );
            if( paudio->Hid.Valid && ( paudio->Hid.InterfaceNum == cur_intf ) )
            {
                paudio->Hid.HidDescLen = hid_rep_len;
            }
        }

        i += Com_Buf[ i ];
    }

    if( ( paudio->Play.Valid == 0 ) && ( paudio->Record.Valid == 0 ) )
    {
        /* 没有找到任何音频流，说明这不是当前例程支持的 USB 音频设备。 */
        DUG_PRINTF( "Audio stream not found.\r\n" );
        return ERR_USB_UNSUPPORT;
    }

    paudio->Active = 1;
    paudio->AutoPlayPending = paudio->Play.Valid ? 1 : 0;
    if( paudio->PlayFeatureUnitId &&
        ( paudio->PlayFeatureUnitId < sizeof( feature_src ) ) &&
        feature_src[ paudio->PlayFeatureUnitId ] )
    {
        DUG_PRINTF( "Play FU:%u Src:%u\r\n", paudio->PlayFeatureUnitId, feature_src[ paudio->PlayFeatureUnitId ] );
    }
    if( paudio->RecordFeatureUnitId &&
        ( paudio->RecordFeatureUnitId < sizeof( feature_src ) ) &&
        feature_src[ paudio->RecordFeatureUnitId ] )
    {
        DUG_PRINTF( "Mic FU:%u Src:%u\r\n", paudio->RecordFeatureUnitId, feature_src[ paudio->RecordFeatureUnitId ] );
    }
    USBH_PrintAudioStream( "Play", &paudio->Play );
    USBH_PrintAudioStream( "Mic", &paudio->Record );
    if( paudio->Hid.Valid )
    {
        /* HID 报告描述符只在调试和学习时打印，便于确认线控按键布局。 */
        if( paudio->Hid.HidDescLen )
        {
            i = paudio->Hid.HidDescLen;
            if( i > DEF_COM_BUF_LEN )
            {
                i = DEF_COM_BUF_LEN;
            }

            DUG_PRINTF( "Get Audio HID RepDesc(IF%u): ", paudio->Hid.InterfaceNum );
            s = HID_GetHidDesr( ep0_size, paudio->Hid.InterfaceNum, Com_Buf, &i );
            if( s == ERR_SUCCESS )
            {
#if DEF_DEBUG_PRINTF
                for( dump_i = 0; dump_i < i; dump_i++ )
                {
                    DUG_PRINTF( "%02x ", Com_Buf[ dump_i ] );
                }
                DUG_PRINTF( "\r\n" );
#endif
            }
            else
            {
                DUG_PRINTF( "Err(%02x)\r\n", s );
            }
        }

        DUG_PRINTF( "Ctrl IF:%u EP:%02x Interval:%u\r\n",
                    paudio->Hid.InterfaceNum,
                    paudio->Hid.InEndpAddr,
                    paudio->Hid.InEndpInterval );
    }
    DUG_PRINTF( "Audio Demo: boot melody pending.\r\n" );
    DUG_PRINTF( "Audio Host Base Ready.\r\n" );

    return ERR_SUCCESS;
}

/*
 * 音频设备运行态服务函数。
 * 这里周期性完成三类工作：
 * 1. 播放端点发包。
 * 2. 麦克风端点收包并写入录音缓存。
 * 3. 轮询 HID 线控端点，并把按键变化交给状态机处理。
 */
static void USBH_ServiceAudioDevice( uint8_t index, uint8_t dev_addr, uint8_t dev_speed )
{
    uint8_t  s;
    uint16_t len;
    uint16_t dump_len;
    uint16_t dump_idx;
    uint8_t  changed;
    uint8_t  pressed;
    uint16_t sample_count;
    uint32_t next_play_pos;
    uint32_t sample_idx;
    int16_t  mic_sample;
    uint16_t abs_sample;
    PAUDIO_CTL paudio;

    paudio = &HostCtl[ index ].Audio;
    /* 设备尚未完成音频枚举时，不做任何运行态处理。 */
    if( paudio->Active == 0 )
    {
        return;
    }

    USBH_SelectDevice( dev_addr, dev_speed );

    if( paudio->AutoPlayPending && paudio->Play.Valid && ( paudio->Play.Running == 0 ) )
    {
        /* 枚举完成后的第一次进入服务函数，先自动播放 5 秒旋律验证播放链路。 */
        paudio->AutoPlayPending = 0;
        paudio->PlayDataPos = 0;
        paudio->PlayTargetLen = (uint32_t)paudio->Play.SampleRate * AUDIO_BOOT_MELODY_MS / 1000UL;
        paudio->TonePhase = 0;
        paudio->Play.PacketCount = 0;
        paudio->Play.ErrorCount = 0;
        paudio->Play.LastError = ERR_SUCCESS;
        if( USBH_AudioSetStreamState( dev_addr, dev_speed, paudio->Ep0Size, &paudio->Play, 1 ) == ERR_SUCCESS )
        {
            paudio->TestState = AUDIO_TEST_BOOT_MELODY;
            DUG_PRINTF( "Audio Demo: start boot melody %ums.\r\n", AUDIO_BOOT_MELODY_MS );
        }
    }

    if( paudio->Play.Valid && paudio->Play.Running && ( paudio->Play.TimeCount >= paudio->Play.Interval ) )
    {
        /* 到达播放端点轮询周期后，准备一包数据并发送。 */
        paudio->Play.TimeCount %= paudio->Play.Interval;
        len = paudio->Play.PacketSize;
        if( len > sizeof( Audio_PlayBuf ) )
        {
            len = sizeof( Audio_PlayBuf );
        }

        next_play_pos = USBH_FillAudioPlaybackPacket( paudio, Audio_PlayBuf, len );
        s = USBFSH_SendEndpData( paudio->Play.EndpointAddr, &paudio->Play.Toggle, Audio_PlayBuf, len );
        if( s == ERR_SUCCESS )
        {
            /* 发包成功后再提交播放位置，避免“包没发出去但位置已前进”的错误。 */
            paudio->Play.PacketCount++;
            paudio->Play.LastError = ERR_SUCCESS;
            paudio->PlayDataPos = next_play_pos;
            if( ( paudio->TestState == AUDIO_TEST_PLAYBACK ) && ( paudio->PlayDataPos >= paudio->PlayTargetLen ) )
            {
                USBH_AudioSetStreamState( dev_addr, dev_speed, paudio->Ep0Size, &paudio->Play, 0 );
                paudio->TestState = AUDIO_TEST_READY;
                paudio->PlayDataPos = 0;
                paudio->PlayTargetLen = 0;
                DUG_PRINTF( "Audio Test: playback done.\r\n" );
            }
            else if( ( paudio->TestState == AUDIO_TEST_BOOT_MELODY ) && ( paudio->PlayDataPos >= paudio->PlayTargetLen ) )
            {
                USBH_AudioSetStreamState( dev_addr, dev_speed, paudio->Ep0Size, &paudio->Play, 0 );
                paudio->TestState = AUDIO_TEST_READY;
                paudio->PlayDataPos = 0;
                paudio->PlayTargetLen = 0;
                DUG_PRINTF( "Audio Demo: boot melody done.\r\n" );
                DUG_PRINTF( "Audio Test: press headset button to record/stop/play, max 10s.\r\n" );
            }
        }
        else
        {
            paudio->Play.LastError = s;
            paudio->Play.ErrorCount++;
            if( ( ( paudio->TestState == AUDIO_TEST_PLAYBACK ) || ( paudio->TestState == AUDIO_TEST_BOOT_MELODY ) ) &&
                ( ( paudio->Play.ErrorCount <= 8 ) || ( ( paudio->Play.ErrorCount % 500 ) == 0 ) ) )
            {
                DUG_PRINTF( "Spk Tx Err:%02x Cnt:%lu Pos:%lu Len:%u\r\n",
                            s,
                            (unsigned long)paudio->Play.ErrorCount,
                            (unsigned long)paudio->PlayDataPos,
                            len );
            }
        }
    }

    if( paudio->Record.Valid && paudio->Record.Running && ( paudio->Record.TimeCount >= paudio->Record.Interval ) )
    {
        /* 到达录音端点轮询周期后，从麦克风取一包数据。 */
        paudio->Record.TimeCount %= paudio->Record.Interval;
        len = 0;
        s = USBFSH_GetEndpData( paudio->Record.EndpointAddr, &paudio->Record.Toggle, Audio_RecBuf, &len );
        if( s == ERR_SUCCESS )
        {
            paudio->Record.PacketCount++;
            paudio->Record.LastError = ERR_SUCCESS;
            if( ( paudio->TestState == AUDIO_TEST_RECORDING ) && len )
            {
                /* 录音阶段把 PCM 转成 ADPCM 存入缓冲区，同时统计峰值。 */
                sample_count = len / 2;
                for( sample_idx = 0; sample_idx < sample_count; sample_idx++ )
                {
                    mic_sample = (int16_t)( (uint16_t)Audio_RecBuf[ sample_idx * 2 ] |
                                            ( (uint16_t)Audio_RecBuf[ sample_idx * 2 + 1 ] << 8 ) );
#if AUDIO_RECORD_INPUT_SHIFT > 0
                    mic_sample = (int16_t)( mic_sample >> AUDIO_RECORD_INPUT_SHIFT );
#endif
                    abs_sample = (uint16_t)( mic_sample < 0 ? -mic_sample : mic_sample );
                    if( abs_sample > paudio->RecordPeak )
                    {
                        paudio->RecordPeak = abs_sample;
                    }
                    if( paudio->RecordTargetLen && ( paudio->RecordSampleCount < paudio->RecordTargetLen ) )
                    {
                        uint8_t adpcm_code;
                        uint32_t byte_index;

                        adpcm_code = USBH_ADPCMEncodeSample( mic_sample, &paudio->RecordAdpcmPred, &paudio->RecordAdpcmIndex );
                        byte_index = paudio->RecordSampleCount >> 1;
                        if( ( paudio->RecordSampleCount & 0x01 ) == 0 )
                        {
                            Audio_RecordStore[ byte_index ] = adpcm_code;
                        }
                        else
                        {
                            Audio_RecordStore[ byte_index ] |= (uint8_t)( adpcm_code << 4 );
                        }
                        paudio->RecordSampleCount++;
                        paudio->RecordDataLen = ( paudio->RecordSampleCount + 1UL ) >> 1;
                    }
                }

                if( paudio->RecordTargetLen && ( paudio->RecordSampleCount >= paudio->RecordTargetLen ) )
                {
                    USBH_FinishRecording( dev_addr, dev_speed, paudio );
                }
            }
        }
        else
        {
            paudio->Record.LastError = s;
            paudio->Record.ErrorCount++;
            if( ( paudio->TestState == AUDIO_TEST_RECORDING ) &&
                ( ( paudio->Record.ErrorCount <= 8 ) || ( ( paudio->Record.ErrorCount % 500 ) == 0 ) ) )
            {
                DUG_PRINTF( "Mic Rx Err:%02x Cnt:%lu\r\n", s, (unsigned long)paudio->Record.ErrorCount );
            }
        }
    }

    if( paudio->Hid.Valid && ( paudio->Hid.InEndpTimeCount >= paudio->Hid.InEndpInterval ) )
    {
        /* 线控 HID 是中断输入端点，按照自己的轮询周期读取。 */
        paudio->Hid.InEndpTimeCount %= paudio->Hid.InEndpInterval;
        len = 0;
        s = USBFSH_GetEndpData( paudio->Hid.InEndpAddr, &paudio->Hid.InEndpTog, Audio_HidBuf, &len );
        if( ( s == ERR_SUCCESS ) && len )
        {
            dump_len = ( len > sizeof( paudio->HidLastBuf ) ) ? sizeof( paudio->HidLastBuf ) : len;
            changed = ( paudio->HidLastLen != dump_len );
            pressed = 0;
            if( changed == 0 )
            {
                if( memcmp( paudio->HidLastBuf, Audio_HidBuf, dump_len ) )
                {
                    changed = 1;
                }
            }

            for( dump_idx = 0; dump_idx < dump_len; dump_idx++ )
            {
                if( Audio_HidBuf[ dump_idx ] != 0 )
                {
                    pressed = 1;
                    break;
                }
            }

            if( paudio->HidPrimed == 0 )
            {
                /* 第一包只作为基线，不触发按键动作。 */
                paudio->HidPrimed = 1;
                paudio->HidLastLen = (uint8_t)dump_len;
                memcpy( paudio->HidLastBuf, Audio_HidBuf, paudio->HidLastLen );
                paudio->HidLastPressed = pressed;
                DUG_PRINTF( "Headset HID init:" );
                dump_len = ( len > 8 ) ? 8 : len;
                for( dump_idx = 0; dump_idx < dump_len; dump_idx++ )
                {
                    DUG_PRINTF( " %02x", Audio_HidBuf[ dump_idx ] );
                }
                DUG_PRINTF( "\r\n" );
                return;
            }

            if( changed )
            {
                dump_len = ( len > 8 ) ? 8 : len;
                DUG_PRINTF( "Headset HID:" );
                for( dump_idx = 0; dump_idx < dump_len; dump_idx++ )
                {
                    DUG_PRINTF( " %02x", Audio_HidBuf[ dump_idx ] );
                }
                DUG_PRINTF( "\r\n" );

                if( ( paudio->TestState != AUDIO_TEST_BOOT_MELODY ) &&
                    pressed && ( paudio->HidLastPressed == 0 ) && ( paudio->HidDebounceMs == 0 ) )
                {
                    /* 只在“按下沿”触发，并做简单去抖。 */
                    paudio->HidDebounceMs = 300;
                    USBH_HandleAudioButton( dev_addr, dev_speed, paudio );
                }
            }

            paudio->HidLastLen = (uint8_t)dump_len;
            memcpy( paudio->HidLastBuf, Audio_HidBuf, paudio->HidLastLen );
            paudio->HidLastPressed = pressed;
        }
        else if( s == ERR_SUCCESS )
        {
            if( paudio->HidLastLen )
            {
                paudio->HidLastLen = 0;
                memset( paudio->HidLastBuf, 0, sizeof( paudio->HidLastBuf ) );
            }
            paudio->HidLastPressed = 0;
        }
    }
}













/* 根据设备描述符和配置描述符判断当前设备类型。 */
void USBH_AnalyseType( uint8_t *pdev_buf, uint8_t *pcfg_buf, uint8_t *ptype )
{
    uint8_t  dv_cls;
    uint8_t  if_cls;
    uint16_t i;
    uint16_t total_len;
    uint8_t  if_audio = 0;
    uint8_t  if_hub = 0;

    dv_cls = ( (PUSB_DEV_DESCR)pdev_buf )->bDeviceClass;

    
    total_len = ( (PUSB_CFG_DESCR)pcfg_buf )->wTotalLength;
    if( total_len > DEF_COM_BUF_LEN )
    {
        total_len = DEF_COM_BUF_LEN;
    }
    for( i = sizeof( USB_CFG_DESCR ); ( i + 2 ) <= total_len; )
    {
        if( pcfg_buf[ i ] < 2 )
        {
            break;
        }
        if( ( i + pcfg_buf[ i ] ) > total_len )
        {
            break;
        }
        if( pcfg_buf[ i + 1 ] == DEF_DECR_INTERFACE )
        {
            if_cls = ( (PUSB_ITF_DESCR)( &pcfg_buf[ i ] ) )->bInterfaceClass;
            if( if_cls == USB_DEV_CLASS_AUDIO )
            {
                if_audio = 1;
            }
            else if( if_cls == USB_DEV_CLASS_HUB )
            {
                if_hub = 1;
            }
        }
        i += pcfg_buf[ i ];
    }

    if( ( dv_cls == USB_DEV_CLASS_HUB ) || if_hub )
    {
        *ptype = USB_DEV_CLASS_HUB;
    }
    else if( ( dv_cls == USB_DEV_CLASS_AUDIO ) || if_audio )
    {
        *ptype = USB_DEV_CLASS_AUDIO;
    }
    else
    {
        *ptype = DEF_DEV_TYPE_UNKNOWN;
    }
}









/*
 * 枚举根口设备。
 * 流程与标准 USB 主机枚举一致：读设备描述符、设地址、读配置描述符、判定类型、设配置值。
 */
uint8_t USBH_EnumRootDevice( void )
{
    uint8_t  s;
    uint8_t  enum_cnt;
    uint8_t  cfg_val;
    uint16_t i;
    uint16_t len;

    DUG_PRINTF( "Enum:\r\n" );

    enum_cnt = 0;
ENUM_START:
    
    Delay_Ms( 100 );
    enum_cnt++;
    Delay_Ms( 8 << enum_cnt );

    
    USBFSH_ResetRootHubPort( 0 );
    for( i = 0, s = 0; i < DEF_RE_ATTACH_TIMEOUT; i++ )
    {
        if( USBFSH_EnableRootHubPort( &RootHubDev.bSpeed ) == ERR_SUCCESS )
        {
            i = 0;
            s++;
            if( s > 6 )
            {
                break;
            }
        }
        Delay_Ms( 1 );
    }
    if( i )
    {
        
        if( enum_cnt <= 5 )
        {
            goto ENUM_START;
        }
        return ERR_USB_DISCON;
    }

    
    DUG_PRINTF("Get DevDesc: ");
    s = USBFSH_GetDeviceDescr( &RootHubDev.bEp0MaxPks, DevDesc_Buf );
    if( s == ERR_SUCCESS )
    {
        
#if DEF_DEBUG_PRINTF
        for( i = 0; i < 18; i++ )
        {
            DUG_PRINTF( "%02x ", DevDesc_Buf[ i ] );
        }
        DUG_PRINTF("\r\n"); 
#endif
    }
    else
    {
        
        DUG_PRINTF( "Err(%02x)\r\n", s );
        if( enum_cnt <= 5 )
        {
            goto ENUM_START;
        }
        return DEF_DEV_DESCR_GETFAIL;
    }

    
    DUG_PRINTF("Set DevAddr: ");
    RootHubDev.bAddress = (uint8_t)( USB_DEVICE_ADDR );
    s = USBFSH_SetUsbAddress( RootHubDev.bEp0MaxPks, RootHubDev.bAddress );
    if( s == ERR_SUCCESS )
    {
        DUG_PRINTF( "OK\r\n" );

        RootHubDev.bAddress = USB_DEVICE_ADDR;
    }
    else
    {
        
        DUG_PRINTF( "Err(%02x)\r\n", s );
        if( enum_cnt <= 5 )
        {
            goto ENUM_START;
        }
        return DEF_DEV_ADDR_SETFAIL;
    }
    Delay_Ms( 5 );

    
    DUG_PRINTF("Get CfgDesc: ");
    s = USBFSH_GetConfigDescr( RootHubDev.bEp0MaxPks, Com_Buf, DEF_COM_BUF_LEN, &len );
    if( s == ERR_SUCCESS )
    {
        cfg_val = ( (PUSB_CFG_DESCR)Com_Buf )->bConfigurationValue;
        
        
#if DEF_DEBUG_PRINTF
        for( i = 0; i < len; i++ )
        {
            DUG_PRINTF( "%02x ", Com_Buf[ i ] );
        }
        DUG_PRINTF("\r\n");
#endif
        USBH_PrintDeviceSummary( DevDesc_Buf, Com_Buf );

        
        USBH_AnalyseType( DevDesc_Buf, Com_Buf, &RootHubDev.bType );
        DUG_PRINTF( "DevType: %02x\r\n", RootHubDev.bType );
    }
    else
    {
        
        DUG_PRINTF( "Err(%02x)\r\n", s );
        if( enum_cnt <= 5 )
        {
            goto ENUM_START;
        }
        return DEF_CFG_DESCR_GETFAIL;
    }

    
    DUG_PRINTF("Set Cfg: ");
    s = USBFSH_SetUsbConfig( RootHubDev.bEp0MaxPks, cfg_val );
    if( s == ERR_SUCCESS )
    {
        DUG_PRINTF( "OK\r\n" );
    }
    else
    {
        
        DUG_PRINTF( "Err(%02x)\r\n", s );
        if( enum_cnt <= 5 )
        {
            goto ENUM_START;
        }
        return ERR_USB_UNSUPPORT;
    }

    return ERR_SUCCESS;
}

/* 解析 HUB 设备的配置描述符，保存中断输入端点等参数。 */
uint8_t HUB_AnalyzeConfigDesc( uint8_t index )
{
    uint8_t  s = ERR_SUCCESS;
    uint16_t i;

    for( i = 0; i < ( Com_Buf[ 2 ] + ( (uint16_t)Com_Buf[ 3 ] << 8 ) ); )
    {
        if( Com_Buf[ i + 1 ] == DEF_DECR_CONFIG )
        {
            
            if( ( (PUSB_CFG_DESCR)( &Com_Buf[ i ] ) )->bNumInterfaces > 1 )
            {
                HostCtl[ index ].InterfaceNum = 1;
            }
            else
            {
                HostCtl[ index ].InterfaceNum = ( (PUSB_CFG_DESCR)( &Com_Buf[ i ] ) )->bNumInterfaces;
            }
            i += Com_Buf[ i ];
        }
        else if( Com_Buf[ i + 1 ] == DEF_DECR_INTERFACE )
        {
            if( ( (PUSB_ITF_DESCR)( &Com_Buf[ i ] ) )->bInterfaceClass == 0x09 )
            {
                i += Com_Buf[ i ];
                while( 1 )
                {
                    if( ( Com_Buf[ i + 1 ] == DEF_DECR_INTERFACE ) ||
                        ( i >= ( Com_Buf[ 2 ] + ( (uint16_t)Com_Buf[ 3 ] << 8 ) ) ) )
                    {
                        break;
                    }
                    else
                    {
                        
                        if( Com_Buf[ i + 1 ] == DEF_DECR_ENDPOINT )
                        {
                            if( ( (PUSB_ENDP_DESCR)( &Com_Buf[ i ] ) )->bEndpointAddress & 0x80 )
                            {
                                HostCtl[ index ].Interface[ 0 ].InEndpAddr[ 0 ] = ( (PUSB_ENDP_DESCR)( &Com_Buf[ i ] ) )->bEndpointAddress;
                                HostCtl[ index ].Interface[ 0 ].InEndpType[ 0 ] = ( (PUSB_ENDP_DESCR)( &Com_Buf[ i ] ) )->bmAttributes;
                                HostCtl[ index ].Interface[ 0 ].InEndpSize[ 0 ] = USBH_ReadLe16( &Com_Buf[ i + 4 ] ) & 0x07ff;
                                HostCtl[ index ].Interface[ 0 ].InEndpInterval[ 0 ] = ( (PUSB_ENDP_DESCR)( &Com_Buf[ i ] ) )->bInterval;
                                HostCtl[ index ].Interface[ 0 ].InEndpNum++;
                            }

                            i += Com_Buf[ i ];
                        }
                        else
                        {
                            i += Com_Buf[ i ];
                        }
                    }
                }
            }
            else
            {
                
                i += Com_Buf[ i ];
            }
        }
        else
        {
            i += Com_Buf[ i ];
        }
    }
    return s;
}









/* 枚举 HUB 本体，读取 HUB 描述符并给各端口上电。 */
uint8_t USBH_EnumHubDevice( void )
{
    uint8_t  s, retry;
    uint16_t len;
    uint16_t  i;

    DUG_PRINTF( "Enum Hub:\r\n" );

    
    DUG_PRINTF("Analyze CfgDesc: ");
    s = HUB_AnalyzeConfigDesc( RootHubDev.DeviceIndex );
    if( s == ERR_SUCCESS )
    {
        DUG_PRINTF( "OK\r\n" );
    }
    else
    {
        DUG_PRINTF( "Err(%02x)\r\n", s );
        return s;
    }

    
    if( Com_Buf[ 6 ] )
    {
        DUG_PRINTF("Get StringDesc4: ");
        s = USBFSH_GetStrDescr( RootHubDev.bEp0MaxPks, Com_Buf[ 6 ], Com_Buf );
        if( s == ERR_SUCCESS )
        {
            
#if DEF_DEBUG_PRINTF
            for( i = 0; i < Com_Buf[ 0 ]; i++ )
            {
                DUG_PRINTF( "%02x ", Com_Buf[ i ] );
            }
            DUG_PRINTF("\r\n");
#endif
        }
        else
        {
            DUG_PRINTF( "Err(%02x)\r\n", s );
        }
    }

    
    if( DevDesc_Buf[ 14 ] )
    {
        DUG_PRINTF("Get StringDesc1: ");
        s = USBFSH_GetStrDescr( RootHubDev.bEp0MaxPks, DevDesc_Buf[ 14 ], Com_Buf );
        if( s == ERR_SUCCESS )
        {
            
#if DEF_DEBUG_PRINTF
            for( i = 0; i < Com_Buf[ 0 ]; i++ )
            {
                DUG_PRINTF( "%02x ", Com_Buf[ i ]);
            }
            DUG_PRINTF("\r\n");
#endif
        }
        else
        {
            DUG_PRINTF( "Err(%02x)\r\n", s );
        }
    }

    
    if( DevDesc_Buf[ 15 ] )
    {
        DUG_PRINTF("Get StringDesc2: ");
        s = USBFSH_GetStrDescr( RootHubDev.bEp0MaxPks, DevDesc_Buf[ 15 ], Com_Buf );
        if( s == ERR_SUCCESS )
        {
            
#if DEF_DEBUG_PRINTF
            for( i = 0; i < Com_Buf[ 0 ]; i++ )
            {
                DUG_PRINTF( "%02x ", Com_Buf[ i ] );
            }
            DUG_PRINTF("\r\n");
#endif
        }
        else
        {
            DUG_PRINTF( "Err(%02x)\r\n", s );
        }
    }

    
    if( DevDesc_Buf[ 16 ] )
    {
        DUG_PRINTF("Get StringDesc3: ");
        s = USBFSH_GetStrDescr( RootHubDev.bEp0MaxPks, DevDesc_Buf[ 16 ], Com_Buf );
        if( s == ERR_SUCCESS )
        {
            
#if DEF_DEBUG_PRINTF
            for( i = 0; i < Com_Buf[ 0 ]; i++ )
            {
                DUG_PRINTF( "%02x ", Com_Buf[ i ] );
            }
            DUG_PRINTF("\r\n");
#endif
        }
        else
        {
            DUG_PRINTF( "Err(%02x)\r\n", s );
        }
    }

    
    DUG_PRINTF("Get Hub Desc: ");
    for( retry = 0; retry < 5; retry++ )
    {
        s = HUB_GetClassDevDescr( RootHubDev.bEp0MaxPks, Com_Buf, &len );
        if( s == ERR_SUCCESS )
        {
            
#if DEF_DEBUG_PRINTF
            for( i = 0; i < len; i++ )
            {
                DUG_PRINTF( "%02x ", Com_Buf[ i ] );
            }
            DUG_PRINTF("\r\n");
#endif

            RootHubDev.bPortNum = ( (PUSB_HUB_DESCR)Com_Buf)->bNbrPorts;
            if( RootHubDev.bPortNum > DEF_NEXT_HUB_PORT_NUM_MAX )
            {
                RootHubDev.bPortNum = DEF_NEXT_HUB_PORT_NUM_MAX;
            }
            DUG_PRINTF( "RootHubDev.bPortNum: %02x\r\n", RootHubDev.bPortNum );
            break;
        }
        else
        {
            
            DUG_PRINTF( "Err(%02x)\r\n", s );

            if( retry == 4 )
            {
                return ERR_USB_UNKNOWN;
            }
        }
    }

    
    for( retry = 0, i = 1; i <= RootHubDev.bPortNum; i++ )
    {
        s = HUB_SetPortFeature( RootHubDev.bEp0MaxPks, i, HUB_PORT_POWER );
        if( s == ERR_SUCCESS )
        {
            continue;
        }
        else
        {
            Delay_Ms( 5 );

            i--;
            retry++;
            if( retry >= 5 )
            {
                return ERR_USB_UNKNOWN;
            }
        }
    }

    return ERR_SUCCESS;
}










/* HUB 端口预枚举阶段 1：检测插拔、复位和基础状态。 */
uint8_t HUB_Port_PreEnum1( uint8_t hub_port, uint8_t *pbuf )
{
    uint8_t  s;
    uint8_t  buf[ 4 ];
    uint8_t  retry;

    if( ( *pbuf ) & ( 1 << hub_port ) )
    {
        s = HUB_GetPortStatus( RootHubDev.bEp0MaxPks, hub_port, &buf[ 0 ] );
        if( s != ERR_SUCCESS )
        {
            DUG_PRINTF( "HUB_PE1_ERR1:%x\r\n", s );
            return s;
        }
        else
        {
            if( buf[ 2 ] & 0x01 )
            {
                s = HUB_ClearPortFeature( RootHubDev.bEp0MaxPks, hub_port, HUB_C_PORT_CONNECTION );
                if( s != ERR_SUCCESS )
                {
                    DUG_PRINTF( "HUB_PE1_ERR2:%x\r\n", s );
                    return s;
                }

                retry = 0;
                do
                {
                    s = HUB_GetPortStatus( RootHubDev.bEp0MaxPks, hub_port, &buf[ 0 ] );
                    if( s != ERR_SUCCESS )
                    {
                        DUG_PRINTF( "HUB_PE1_ERR3:%x\r\n", s );
                        return s;
                    }
                    retry++;
                }while( ( buf[ 2 ] & 0x01 ) && ( retry < 10 ) );

                if( retry != 10 )
                {
                    if( !( buf[ 0 ] & 0x01 ) )
                    {
                        DUG_PRINTF( "Hub Port%x Out\r\n", hub_port );
                        return ERR_USB_DISCON;
                    }
                }
            }
        }
    }

    return ERR_USB_UNKNOWN;
}










/* HUB 端口预枚举阶段 2：确认设备已稳定接入。 */
uint8_t HUB_Port_PreEnum2( uint8_t hub_port, uint8_t *pbuf )
{
    uint8_t  s;
    uint8_t  buf[ 4 ];
    uint8_t  retry = 0;

    if( ( *pbuf ) & ( 1 << hub_port ) )
    {
        s = HUB_SetPortFeature( RootHubDev.bEp0MaxPks, hub_port, HUB_PORT_RESET );
        if( s != ERR_SUCCESS )
        {
            DUG_PRINTF( "HUB_PE2_ERR1:%x\r\n", s );
            return s;
        }

        Delay_Ms( 10 );
        do
        {
            s = HUB_GetPortStatus( RootHubDev.bEp0MaxPks, hub_port, &buf[ 0 ] );
            if( s != ERR_SUCCESS )
            {
                DUG_PRINTF( "HUB_PE2_ERR2:%x\r\n", s );
                return s;
            }
            retry++;
        }while( ( !( buf[ 2 ] & 0x10 ) ) && ( retry <= 10 ) );

        if( retry != 10 )
        {
            retry = 0;
            s = HUB_ClearPortFeature( RootHubDev.bEp0MaxPks, hub_port, HUB_C_PORT_RESET  );

            do
            {
                s = HUB_GetPortStatus( RootHubDev.bEp0MaxPks, hub_port, &buf[ 0 ] );
                if( s != ERR_SUCCESS )
                {
                    DUG_PRINTF( "HUB_PE2_ERR3:%x\r\n", s );
                    return s;
                }
                retry++;
            }while( ( buf[ 2 ] & 0x10 ) && ( retry <= 10 ) );

            if( retry != 10 )
            {
                if( buf[ 0 ] & 0x01 )
                {
                    DUG_PRINTF( "Hub Port%x In\r\n", hub_port );
                    return ERR_USB_CONNECT;
                }
            }
        }
    }

    return ERR_USB_UNKNOWN;
}










/* 读取 HUB 端口状态，判断下级设备是全速还是低速。 */
uint8_t HUB_CheckPortSpeed( uint8_t hub_port, uint8_t *pbuf )
{
    uint8_t  s;

    s = HUB_GetPortStatus( RootHubDev.bEp0MaxPks, hub_port, pbuf );
    if( s )
    {
        return s;
    }

    if( pbuf[ 1 ] & 0x02 )
    {
        return USB_LOW_SPEED;
    }
    else
    {
        if( pbuf[ 1 ] & 0x04 )
        {
            return USB_HIGH_SPEED;
        }
        else
        {
            return USB_FULL_SPEED;
        }
    }
}










/* 枚举 HUB 某个下级端口上的设备。 */
uint8_t USBH_EnumHubPortDevice( uint8_t hub_port, uint8_t *paddr, uint8_t *ptype )
{
    uint8_t  s;
    uint8_t  enum_cnt;
    uint16_t len;
    uint8_t  cfg_val;
#if DEF_DEBUG_PRINTF
    uint16_t i;
#endif

    
    DUG_PRINTF("(S1)Get DevDesc: \r\n");
    enum_cnt = 0;
    do
    {
        enum_cnt++;
        s = USBFSH_GetDeviceDescr( &RootHubDev.Device[ hub_port ].bEp0MaxPks, DevDesc_Buf );
        if( s == ERR_SUCCESS )
        {
#if DEF_DEBUG_PRINTF
            for( i = 0; i < 18; i++ )
            {
                DUG_PRINTF( "%02x ", DevDesc_Buf[ i ] );
            }
            DUG_PRINTF( "\r\n" );
#endif
        }
        else
        {
            DUG_PRINTF( "Err(%02x)\r\n", s );
            if( enum_cnt >= 10 )
            {
                return DEF_DEV_DESCR_GETFAIL;
            }
        }
    }while( ( s != ERR_SUCCESS ) && ( enum_cnt < 10 ) );

    
    DUG_PRINTF( "Set DevAddr: \r\n" );
    enum_cnt = 0;
    do
    {
        enum_cnt++;
        s = USBFSH_SetUsbAddress( RootHubDev.Device[ hub_port ].bEp0MaxPks, \
                                  RootHubDev.Device[ hub_port ].DeviceIndex + USB_DEVICE_ADDR );
        if( s == ERR_SUCCESS )
        {
            
            *paddr = RootHubDev.Device[ hub_port ].DeviceIndex + USB_DEVICE_ADDR;
        }
        else
        {
            DUG_PRINTF( "Err(%02x)\r\n", s );
            if( enum_cnt >= 10 )
            {
                return DEF_DEV_ADDR_SETFAIL;
            }
        }
    }while( ( s != ERR_SUCCESS ) && ( enum_cnt < 10 ) );
    Delay_Ms( 5 );

    
    DUG_PRINTF( "Get DevCfgDesc: \r\n" );
    enum_cnt = 0;
    do
    {
        enum_cnt++;
        s = USBFSH_GetConfigDescr( RootHubDev.Device[ hub_port ].bEp0MaxPks, Com_Buf, DEF_COM_BUF_LEN, &len );
        if( s == ERR_SUCCESS )
        {
#if DEF_DEBUG_PRINTF
            for( i = 0; i < len; i++ )
            {
                DUG_PRINTF( "%02x ", Com_Buf[ i ] );
            }
            DUG_PRINTF( "\r\n" );
#endif
            USBH_PrintDeviceSummary( DevDesc_Buf, Com_Buf );

            
            cfg_val = ( (PUSB_CFG_DESCR)Com_Buf )->bConfigurationValue;

            
            USBH_AnalyseType( DevDesc_Buf, Com_Buf, ptype );
            DUG_PRINTF( "DevType: %02x\r\n", *ptype );
        }
        else
        {
            DUG_PRINTF( "Err(%02x)\r\n", s );
            if( enum_cnt >= 10 )
            {
                return DEF_DEV_DESCR_GETFAIL;
            }
        }
    }while( ( s != ERR_SUCCESS ) && ( enum_cnt < 10 ) );

    
    DUG_PRINTF( "Set CfgValue: \r\n" );
    enum_cnt = 0;
    do
    {
        enum_cnt++;
        s = USBFSH_SetUsbConfig( RootHubDev.Device[ hub_port ].bEp0MaxPks, cfg_val );
        if( s != ERR_SUCCESS )
        {
            DUG_PRINTF( "Err(%02x)\r\n", s );
            if( enum_cnt >= 10 )
            {
                return DEF_CFG_VALUE_SETFAIL;
            }
        }
    }while( ( s != ERR_SUCCESS ) && ( enum_cnt < 10 ) );

    return( ERR_SUCCESS );
}










/*
 * USB 主机主状态机。
 * 负责监视根口插拔、驱动根口设备枚举、驱动 HUB 及其下级设备枚举，
 * 并在设备成功进入运行态后持续调用音频服务函数。
 */
void USBH_MainDeal( void )
{
    uint8_t  s;
    uint8_t  index;
    uint8_t  hub_port;
    uint8_t  hub_dat;
    uint16_t len;

    
    s = USBFSH_CheckRootHubPortStatus( RootHubDev.bStatus );
    if( s == ROOT_DEV_CONNECTED )
    {
        /* 根口检测到新设备接入后，先走标准枚举流程。 */
        DUG_PRINTF( "USB Port Dev In.\r\n" );

        RootHubDev.bStatus = ROOT_DEV_CONNECTED;
        RootHubDev.DeviceIndex = DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL;

        s = USBH_EnumRootDevice( );
        if( s == ERR_SUCCESS )
        {
            if( RootHubDev.bType == USB_DEV_CLASS_HUB )
            {
                /* 若根口是 HUB，则先枚举 HUB 本体，后续再管理它的各个下级端口。 */
                DUG_PRINTF( "Root Device Is HUB. " );
                s = USBH_EnumHubDevice( );
                DUG_PRINTF( "Further Enum Result: " );
                if( s == ERR_SUCCESS )
                {
                    DUG_PRINTF( "OK\r\n" );
                    RootHubDev.bStatus = ROOT_DEV_SUCCESS;
                }
                else if( s != ERR_USB_DISCON )
                {
                    DUG_PRINTF( "Err(%02x)\r\n", s );
                    RootHubDev.bStatus = ROOT_DEV_FAILED;
                }
            }
            else if( RootHubDev.bType == USB_DEV_CLASS_AUDIO )
            {
                /* 若根口直接接入音频耳机，则进入音频专用枚举。 */
                DUG_PRINTF( "Root Device Is Audio(Headset). Further Enum:\r\n" );
                s = USBH_EnumAudioDevice( RootHubDev.DeviceIndex, RootHubDev.bEp0MaxPks );
                if( s == ERR_SUCCESS )
                {
                    USBH_PrintAudioDeviceInfo( DevDesc_Buf, Com_Buf );
                    RootHubDev.bStatus = ROOT_DEV_SUCCESS;
                    DUG_PRINTF( "End Enum.\r\n" );
                }
                else if( s != ERR_USB_DISCON )
                {
                    DUG_PRINTF( "Audio Enum Err(%02x)\r\n", s );
                    RootHubDev.bStatus = ROOT_DEV_FAILED;
                }
            }
            else
            {
                DUG_PRINTF( "Root Device Unsupported Type:%02x\r\n", RootHubDev.bType );
                RootHubDev.bStatus = ROOT_DEV_FAILED;
            }
        }
        else if( s != ERR_USB_DISCON )
        {
            DUG_PRINTF( "Enum Fail with Error Code:%x\r\n", s );
            RootHubDev.bStatus = ROOT_DEV_FAILED;
        }
    }
    else if( s == ROOT_DEV_DISCONNECT )
    {
        DUG_PRINTF( "USB Port Dev Out.\r\n" );
        index = RootHubDev.DeviceIndex;
        memset( &RootHubDev.bStatus, 0, sizeof( ROOT_HUB_DEVICE ) );
        memset( &HostCtl[ index ].InterfaceNum, 0, sizeof( HOST_CTL ) );
    }

    
    if( RootHubDev.bStatus >= ROOT_DEV_SUCCESS )
    {
        /* 枚举成功后进入运行态：要么直接服务根口音频设备，要么服务 HUB 与其下级设备。 */
        index = RootHubDev.DeviceIndex;

        if( RootHubDev.bType == USB_DEV_CLASS_AUDIO )
        {
            USBH_ServiceAudioDevice( index, RootHubDev.bAddress, RootHubDev.bSpeed );
        }
        else if( RootHubDev.bType == USB_DEV_CLASS_HUB )
        {
            /* HUB 自身通过中断端点上报端口状态变化。 */
            if( HostCtl[ index ].Interface[ 0 ].InEndpTimeCount[ 0 ] >= HostCtl[ index ].Interface[ 0 ].InEndpInterval[ 0 ] )
            {
                HostCtl[ index ].Interface[ 0 ].InEndpTimeCount[ 0 ] %= HostCtl[ index ].Interface[ 0 ].InEndpInterval[ 0 ];

                USBFSH_SetSelfAddr( RootHubDev.bAddress );
                USBFSH_SetSelfSpeed( RootHubDev.bSpeed );
                s = USBFSH_GetEndpData( HostCtl[ index ].Interface[ 0 ].InEndpAddr[ 0 ],
                                        &HostCtl[ index ].Interface[ 0 ].InEndpTog[ 0 ],
                                        Com_Buf,
                                        &len );
                if( s == ERR_SUCCESS )
                {
                    hub_dat = Com_Buf[ 0 ];
                    DUG_PRINTF( "Hub Int Data:%02x\r\n", hub_dat );

                    for( hub_port = 0; hub_port < RootHubDev.bPortNum; hub_port++ )
                    {
                        /* 先处理端口插拔与复位，再决定是否需要发起下级设备枚举。 */
                        s = HUB_Port_PreEnum1( ( hub_port + 1 ), &hub_dat );
                        if( s == ERR_USB_DISCON )
                        {
                            hub_dat &= ~( 1 << ( hub_port + 1 ) );
                            memset( &HostCtl[ RootHubDev.Device[ hub_port ].DeviceIndex ], 0, sizeof( HOST_CTL ) );
                            memset( &RootHubDev.Device[ hub_port ], 0, sizeof( HUB_DEVICE ) );
                            continue;
                        }

                        Delay_Ms( 100 );
                        s = HUB_Port_PreEnum2( ( hub_port + 1 ), &hub_dat );
                        if( s == ERR_USB_CONNECT )
                        {
                            RootHubDev.Device[ hub_port ].bStatus = ROOT_DEV_CONNECTED;
                            RootHubDev.Device[ hub_port ].bEp0MaxPks = DEFAULT_ENDP0_SIZE;
                            RootHubDev.Device[ hub_port ].DeviceIndex = DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL + hub_port + 1;
                        }
                        else
                        {
                            hub_dat &= ~( 1 << ( hub_port + 1 ) );
                        }

                        if( RootHubDev.Device[ hub_port ].bStatus == ROOT_DEV_CONNECTED )
                        {
                            /* 下级端口稳定接入后，再执行标准枚举。 */
                            RootHubDev.Device[ hub_port ].bSpeed = HUB_CheckPortSpeed( ( hub_port + 1 ), &hub_dat );
                            DUG_PRINTF( "Dev Speed:%x\r\n", RootHubDev.Device[ hub_port ].bSpeed );

                            USBFSH_SetSelfAddr( RootHubDev.Device[ hub_port ].bAddress );
                            USBFSH_SetSelfSpeed( RootHubDev.Device[ hub_port ].bSpeed );
                            if( RootHubDev.bSpeed != USB_LOW_SPEED )
                            {
                                USBFSH->HOST_CTRL &= ~USBFS_UH_LOW_SPEED;
                            }

                            DUG_PRINTF( "Enum_HubDevice\r\n" );
                            s = USBH_EnumHubPortDevice( hub_port,
                                                        &RootHubDev.Device[ hub_port ].bAddress,
                                                        &RootHubDev.Device[ hub_port ].bType );
                            if( s == ERR_SUCCESS )
                            {
                                if( RootHubDev.Device[ hub_port ].bType == USB_DEV_CLASS_AUDIO )
                                {
                                    DUG_PRINTF( "HUB port%x device is audio(headset)! Further Enum:\r\n", hub_port + 1 );
                                    s = USBH_EnumAudioDevice( RootHubDev.Device[ hub_port ].DeviceIndex,
                                                              RootHubDev.Device[ hub_port ].bEp0MaxPks );
                                    if( s == ERR_SUCCESS )
                                    {
                                        USBH_PrintAudioDeviceInfo( DevDesc_Buf, Com_Buf );
                                        RootHubDev.Device[ hub_port ].bStatus = ROOT_DEV_SUCCESS;
                                    }
                                    else
                                    {
                                        DUG_PRINTF( "Audio Enum Err(%02x)\r\n", s );
                                        RootHubDev.Device[ hub_port ].bStatus = ROOT_DEV_FAILED;
                                    }
                                }
                                else
                                {
                                    DUG_PRINTF( "HUB port%x unsupported type:%02x\r\n",
                                                hub_port,
                                                RootHubDev.Device[ hub_port ].bType );
                                    RootHubDev.Device[ hub_port ].bStatus = ROOT_DEV_FAILED;
                                }
                            }
                            else
                            {
                                RootHubDev.Device[ hub_port ].bStatus = ROOT_DEV_FAILED;
                                DUG_PRINTF( "HUB Port%x Enum Err!\r\n", hub_port );
                            }
                        }
                    }
                }
            }

            for( hub_port = 0; hub_port < RootHubDev.bPortNum; hub_port++ )
            {
                if( RootHubDev.Device[ hub_port ].bStatus == ROOT_DEV_SUCCESS )
                {
                    index = RootHubDev.Device[ hub_port ].DeviceIndex;
                    if( RootHubDev.Device[ hub_port ].bType == USB_DEV_CLASS_AUDIO )
                    {
                        /* HUB 下的音频设备与根口音频设备共用同一套运行态服务逻辑。 */
                        USBH_ServiceAudioDevice( index,
                                                 RootHubDev.Device[ hub_port ].bAddress,
                                                 RootHubDev.Device[ hub_port ].bSpeed );
                    }
                }
            }
        }
    }
}































































































































































































































































































































































































































































































































































































































































































































































































































































































