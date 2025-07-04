// #include <common.h>
#include <dm.h>
#include <errno.h>
#include <i2c.h>
#include <log.h>
#include <video_bridge.h>
#include <display.h>
#include <edid.h>
#include <fdtdec.h>

#define SII902X_TPI_VIDEO_DATA			0x0

#define SII902X_TPI_PIXEL_REPETITION		0x8
#define SII902X_TPI_AVI_PIXEL_REP_BUS_24BIT     BIT(5)
#define SII902X_TPI_AVI_PIXEL_REP_RISING_EDGE   BIT(4)
#define SII902X_TPI_AVI_PIXEL_REP_4X		3
#define SII902X_TPI_AVI_PIXEL_REP_2X		1
#define SII902X_TPI_AVI_PIXEL_REP_NONE		0
#define SII902X_TPI_CLK_RATIO_HALF		(0 << 6)
#define SII902X_TPI_CLK_RATIO_1X		(1 << 6)
#define SII902X_TPI_CLK_RATIO_2X		(2 << 6)
#define SII902X_TPI_CLK_RATIO_4X		(3 << 6)

#define SII902X_TPI_AVI_IN_FORMAT		0x9
#define SII902X_TPI_AVI_INPUT_BITMODE_12BIT	BIT(7)
#define SII902X_TPI_AVI_INPUT_DITHER		BIT(6)
#define SII902X_TPI_AVI_INPUT_RANGE_LIMITED	(2 << 2)
#define SII902X_TPI_AVI_INPUT_RANGE_FULL	(1 << 2)
#define SII902X_TPI_AVI_INPUT_RANGE_AUTO	(0 << 2)
#define SII902X_TPI_AVI_INPUT_COLORSPACE_BLACK	(3 << 0)
#define SII902X_TPI_AVI_INPUT_COLORSPACE_YUV422	(2 << 0)
#define SII902X_TPI_AVI_INPUT_COLORSPACE_YUV444	(1 << 0)
#define SII902X_TPI_AVI_INPUT_COLORSPACE_RGB	(0 << 0)

#define SII902X_TPI_AVI_INFOFRAME		0x0c

#define SII902X_SYS_CTRL_DATA			0x1a
#define SII902X_SYS_CTRL_PWR_DWN		BIT(4)
#define SII902X_SYS_CTRL_AV_MUTE		BIT(3)
#define SII902X_SYS_CTRL_DDC_BUS_REQ		BIT(2)
#define SII902X_SYS_CTRL_DDC_BUS_GRTD		BIT(1)
#define SII902X_SYS_CTRL_OUTPUT_MODE		BIT(0)
#define SII902X_SYS_CTRL_OUTPUT_HDMI		1
#define SII902X_SYS_CTRL_OUTPUT_DVI		0

#define SII902X_REG_CHIPID(n)			(0x1b + (n))

#define SII902X_PWR_STATE_CTRL			0x1e
#define SII902X_AVI_POWER_STATE_MSK		GENMASK(1, 0)
#define SII902X_AVI_POWER_STATE_D(l)		((l) & SII902X_AVI_POWER_STATE_MSK)

/* Audio  */
#define SII902X_TPI_I2S_ENABLE_MAPPING_REG	0x1f
#define SII902X_TPI_I2S_CONFIG_FIFO0			(0 << 0)
#define SII902X_TPI_I2S_CONFIG_FIFO1			(1 << 0)
#define SII902X_TPI_I2S_CONFIG_FIFO2			(2 << 0)
#define SII902X_TPI_I2S_CONFIG_FIFO3			(3 << 0)
#define SII902X_TPI_I2S_LEFT_RIGHT_SWAP			(1 << 2)
#define SII902X_TPI_I2S_AUTO_DOWNSAMPLE			(1 << 3)
#define SII902X_TPI_I2S_SELECT_SD0			(0 << 4)
#define SII902X_TPI_I2S_SELECT_SD1			(1 << 4)
#define SII902X_TPI_I2S_SELECT_SD2			(2 << 4)
#define SII902X_TPI_I2S_SELECT_SD3			(3 << 4)
#define SII902X_TPI_I2S_FIFO_ENABLE			(1 << 7)

#define SII902X_TPI_I2S_INPUT_CONFIG_REG	0x20
#define SII902X_TPI_I2S_FIRST_BIT_SHIFT_YES		(0 << 0)
#define SII902X_TPI_I2S_FIRST_BIT_SHIFT_NO		(1 << 0)
#define SII902X_TPI_I2S_SD_DIRECTION_MSB_FIRST		(0 << 1)
#define SII902X_TPI_I2S_SD_DIRECTION_LSB_FIRST		(1 << 1)
#define SII902X_TPI_I2S_SD_JUSTIFY_LEFT			(0 << 2)
#define SII902X_TPI_I2S_SD_JUSTIFY_RIGHT		(1 << 2)
#define SII902X_TPI_I2S_WS_POLARITY_LOW			(0 << 3)
#define SII902X_TPI_I2S_WS_POLARITY_HIGH		(1 << 3)
#define SII902X_TPI_I2S_MCLK_MULTIPLIER_128		(0 << 4)
#define SII902X_TPI_I2S_MCLK_MULTIPLIER_256		(1 << 4)
#define SII902X_TPI_I2S_MCLK_MULTIPLIER_384		(2 << 4)
#define SII902X_TPI_I2S_MCLK_MULTIPLIER_512		(3 << 4)
#define SII902X_TPI_I2S_MCLK_MULTIPLIER_768		(4 << 4)
#define SII902X_TPI_I2S_MCLK_MULTIPLIER_1024		(5 << 4)
#define SII902X_TPI_I2S_MCLK_MULTIPLIER_1152		(6 << 4)
#define SII902X_TPI_I2S_MCLK_MULTIPLIER_192		(7 << 4)
#define SII902X_TPI_I2S_SCK_EDGE_FALLING		(0 << 7)
#define SII902X_TPI_I2S_SCK_EDGE_RISING			(1 << 7)

#define SII902X_TPI_I2S_STRM_HDR_BASE	0x21
#define SII902X_TPI_I2S_STRM_HDR_SIZE	5

#define SII902X_TPI_AUDIO_CONFIG_BYTE2_REG	0x26
#define SII902X_TPI_AUDIO_CODING_STREAM_HEADER		(0 << 0)
#define SII902X_TPI_AUDIO_CODING_PCM			(1 << 0)
#define SII902X_TPI_AUDIO_CODING_AC3			(2 << 0)
#define SII902X_TPI_AUDIO_CODING_MPEG1			(3 << 0)
#define SII902X_TPI_AUDIO_CODING_MP3			(4 << 0)
#define SII902X_TPI_AUDIO_CODING_MPEG2			(5 << 0)
#define SII902X_TPI_AUDIO_CODING_AAC			(6 << 0)
#define SII902X_TPI_AUDIO_CODING_DTS			(7 << 0)
#define SII902X_TPI_AUDIO_CODING_ATRAC			(8 << 0)
#define SII902X_TPI_AUDIO_MUTE_DISABLE			(0 << 4)
#define SII902X_TPI_AUDIO_MUTE_ENABLE			(1 << 4)
#define SII902X_TPI_AUDIO_LAYOUT_2_CHANNELS		(0 << 5)
#define SII902X_TPI_AUDIO_LAYOUT_8_CHANNELS		(1 << 5)
#define SII902X_TPI_AUDIO_INTERFACE_DISABLE		(0 << 6)
#define SII902X_TPI_AUDIO_INTERFACE_SPDIF		(1 << 6)
#define SII902X_TPI_AUDIO_INTERFACE_I2S			(2 << 6)

#define SII902X_TPI_AUDIO_CONFIG_BYTE3_REG	0x27
#define SII902X_TPI_AUDIO_FREQ_STREAM			(0 << 3)
#define SII902X_TPI_AUDIO_FREQ_32KHZ			(1 << 3)
#define SII902X_TPI_AUDIO_FREQ_44KHZ			(2 << 3)
#define SII902X_TPI_AUDIO_FREQ_48KHZ			(3 << 3)
#define SII902X_TPI_AUDIO_FREQ_88KHZ			(4 << 3)
#define SII902X_TPI_AUDIO_FREQ_96KHZ			(5 << 3)
#define SII902X_TPI_AUDIO_FREQ_176KHZ			(6 << 3)
#define SII902X_TPI_AUDIO_FREQ_192KHZ			(7 << 3)
#define SII902X_TPI_AUDIO_SAMPLE_SIZE_STREAM		(0 << 6)
#define SII902X_TPI_AUDIO_SAMPLE_SIZE_16		(1 << 6)
#define SII902X_TPI_AUDIO_SAMPLE_SIZE_20		(2 << 6)
#define SII902X_TPI_AUDIO_SAMPLE_SIZE_24		(3 << 6)

#define SII902X_TPI_AUDIO_CONFIG_BYTE4_REG	0x28

#define SII902X_INT_ENABLE			0x3c
#define SII902X_INT_STATUS			0x3d
#define SII902X_HOTPLUG_EVENT			BIT(0)
#define SII902X_PLUGGED_STATUS			BIT(2)

#define SII902X_REG_TPI_RQB			0xc7

/* Indirect internal register access */
#define SII902X_IND_SET_PAGE			0xbc
#define SII902X_IND_OFFSET			0xbd
#define SII902X_IND_VALUE			0xbe

#define SII902X_TPI_MISC_INFOFRAME_BASE		0xbf
#define SII902X_TPI_MISC_INFOFRAME_END		0xde
#define SII902X_TPI_MISC_INFOFRAME_SIZE	\
	(SII902X_TPI_MISC_INFOFRAME_END - SII902X_TPI_MISC_INFOFRAME_BASE)

#define SII902X_I2C_BUS_ACQUISITION_TIMEOUT_MS	500

#define SII902X_AUDIO_PORT_INDEX		3

/*
 * The maximum resolution supported by the HDMI bridge is 1080p@60Hz
 * and 1920x1200 requiring a pixel clock of 165MHz and the minimum
 * resolution supported is 480p@60Hz requiring a pixel clock of 25MHz
 */
#define SII902X_MIN_PIXEL_CLOCK_KHZ		25000
#define SII902X_MAX_PIXEL_CLOCK_KHZ		165000

static int sii902x_write(struct udevice *dev,
			uint reg_addr, uint8_t value)
{
    const uint8_t *buffer = &value;
	return dm_i2c_write(dev, reg_addr, buffer, 1);
}

static int sii902x_read(struct udevice *dev,
			uint reg_addr, uint8_t *value)
{
    // const uint8_t *buffer = &value;
	return dm_i2c_read(dev, reg_addr, value, 1);
}

static int sii902x_bulk_read(struct udevice *dev,
			uint reg_addr, uint8_t *val, uint val_count)
{
    uint8_t i;
    int ret = 0;
    
    for (i = 0; i < val_count; i++) {
        ret = sii902x_read(dev, reg_addr + i, val + i);
        if(ret)
            return ret;
    }

    return ret;
}

static int sii902x_bulk_write(struct udevice *dev,
			uint reg_addr, uint8_t *val, uint val_count)
{
    uint8_t i;
    int ret = 0;

    for (i = 0; i < val_count; i++) {
        ret = sii902x_write(dev, reg_addr + i, *(val + i));
        if(ret)
            return ret;
    }

    return ret;
}

// static int sii902x_reset(struct udevice *dev)
// {
//     return sii902x_write(dev, 0xC7, 0x00);
// }

static int sii902x_power_up_transmitter(struct udevice *dev){
    u8 regVal;
    sii902x_read(dev, 0x1E, &regVal);
    return sii902x_write(dev, 0x1E, regVal & 0xFC);
}

static int sii902x_enable_device(struct udevice *dev){
    u8 regVal;
    sii902x_write(dev, 0xBC, 0x01);
    sii902x_write(dev, 0xBD, 0x82);
    sii902x_read(dev, 0xBE, &regVal);
    return sii902x_write(dev, 0xBE,regVal | 0x01);
}

static int sii902x_configure_input_bus(struct udevice *dev){
    return sii902x_write(dev, 0x08, 0x70);
}

static int sii902x_configure_YCMux_mode(struct udevice *dev){
    return sii902x_write(dev, 0x0B, 0x00);
}

static int sii902x_configure_sync_mode(struct udevice *dev){
    //values can be 0x04/0xA4/0x84
    return sii902x_write(dev, 0x60, 0x04);
}


static int32_t sii902x_prgm_emb_sync_timing_info(struct udevice *dev){
    int32_t status = 0;
    uint8_t regValue;
    uint8_t regAddr;
    regAddr = 0x62U;
    regValue = (uint8_t)(88 & 0xFFU);
    sii902x_write(dev, regAddr, regValue);

    regAddr = 0x63U;
    regValue = 0x40;
    regValue &= ((uint8_t) ~(0x03U));
    regValue |= ((88 & 0x300U) >> 8);
    regValue = (regValue & 0xFFU);
    sii902x_write(dev, regAddr, regValue);

    regAddr = 0x64U;
    regValue = (uint8_t)(0 & 0xFFU);
    sii902x_write(dev, regAddr, regValue);

    regAddr = 0x65U;
    regValue = (uint8_t)((0 & 0xF00U) >> 8);
    sii902x_write(dev, regAddr, regValue);

    regAddr = 0x66U;
    regValue = (uint8_t)(44 & 0xFFU);
    sii902x_write(dev, regAddr, regValue);

    regAddr = 0x67U;
    regValue = (uint8_t)((44 & 0x300U) >> 8);
    sii902x_write(dev, regAddr, regValue);

    regAddr = 0x68U;
    regValue = (uint8_t)(4 & 0x3FU);
    sii902x_write(dev, regAddr, regValue);

    regAddr = 0x69U;
    regValue = (uint8_t)(5 & 0x3FU);
    sii902x_write(dev, regAddr, regValue);

    regAddr = 0x00U;
    regValue = (uint8_t)(14850 & 0xFFU);
    sii902x_write(dev, regAddr, regValue);

    regAddr = 0x01U;
    regValue = (uint8_t)((14850 & 0xFF00U) >> 8);
    sii902x_write(dev, regAddr, regValue);

    regAddr = 0x02U;
    regValue = (uint8_t)(60 & 0xFFU);
    sii902x_write(dev, regAddr, regValue);

    regAddr = 0x03U;
    regValue = (uint8_t)((60 & 0xFF00U) >> 8);
    sii902x_write(dev, regAddr, regValue);

    regAddr = 0x04U;
    regValue = (uint8_t)(2200 & 0xFFU);
    sii902x_write(dev, regAddr, regValue);

    regAddr = 0x05U;
    regValue = (uint8_t)((2200 & 0xFF00U) >> 8);
    sii902x_write(dev, regAddr, regValue);

    regAddr = 0x06U;
    regValue = (uint8_t)(1125 & 0xFFU);
    sii902x_write(dev, regAddr, regValue);

    regAddr = 0x07U;
    regValue = (uint8_t)((1125 & 0xFF00U) >> 8);
    sii902x_write(dev, regAddr, regValue);

    regAddr = 0x08U;
    regValue = (uint8_t) 0x70;
    sii902x_write(dev, regAddr, regValue);

    regAddr = 0x09U;
    regValue = 0x02U;
    sii902x_write(dev, regAddr, regValue);

    regAddr = 0x0AU;
    regValue = 0x10U;
    // switch (object->outputFormat)
    // {
    //     case BRIDGE_SII9022A_HDMI_RGB:
    //         regValue = 0x10U;
    //         object->isRgbOutput = 1;
    //         break;

    //     case BRIDGE_SII9022A_HDMI_YUV444:
    //         regValue = 0x11U;
    //         object->isRgbOutput = 0;
    //         break;

    //     case BRIDGE_SII9022A_HDMI_YUV422:
    //         regValue = 0x12U;
    //         object->isRgbOutput = 0;
    //         break;

    //     case BRIDGE_SII9022A_DVI_RGB:
    //         regValue = 0x13U;
    //         object->isRgbOutput = 1;
    //         break;

    //     default:
    //         break;
    // }
    sii902x_write(dev, regAddr, regValue);
    return 0;
}

static void BridgeSii9022a_calculateCRC(uint8_t *regAddr,
                                        uint8_t *regValue,
                                        uint32_t *numRegs)
{
    uint32_t sum = 0U;

    for (uint32_t count = 0U; count < *numRegs; count++)
    {
        sum += regValue[count];
    }

    sum += (0x82U + 0x02U + 13U);
    sum &= 0xFFU;
    regValue[*numRegs] = (uint8_t)(0x100U - sum);
    regAddr[*numRegs] = 0x0CU;
    (*numRegs)++;

    return;
}

// static int sii902x_bulk_write(struct udevice *dev, uint8_t *regAddr, uint8_t* regVal, uint32_t numRegs){
//     int i;
//     int status = 0;
//     for(i = 0; i < numRegs; i++){
//         status = sii902x_write(dev, regAddr[i], regVal[i]);
//         if(status)
//             return status;
//     }
//     return status;
// }

static int32_t sii902x_prgm_AvInfo_Frame(struct udevice *dev){
    int32_t status = 0;
    uint8_t regValue[15];
    uint8_t regAddr[15];
    uint32_t numRegs = 0;

    regAddr[numRegs] = 0x0D;
    regValue[numRegs] = 0x01;
    numRegs++;

    regAddr[numRegs] = 0x0E;
    regValue[numRegs] = 0xA0;
    numRegs++;

    regAddr[numRegs] = 0x0F;
    regValue[numRegs] = 0x00;
    numRegs++;

    regAddr[numRegs] = 0x10;
    regValue[numRegs] = (uint8_t)(16 & 0x7F);
    numRegs++;

    regAddr[numRegs] = 0x11;
    regValue[numRegs] = 0x00;
    numRegs++;

    regAddr[numRegs] = 0x12;
    regValue[numRegs] = 0x00;
    numRegs++;

    regAddr[numRegs] = 0x13;
    regValue[numRegs] = 0x00;
    numRegs++;

    regAddr[numRegs] = 0x14;
    regValue[numRegs] = 0x00;
    numRegs++;

    regAddr[numRegs] = 0x15;
    regValue[numRegs] = 0x00;
    numRegs++;

    regAddr[numRegs] = 0x16;
    regValue[numRegs] = 0x00;
    numRegs++;

    regAddr[numRegs] = 0x17;
    regValue[numRegs] = 0x00;
    numRegs++;

    regAddr[numRegs] = 0x18;
    regValue[numRegs] = 0x00;
    numRegs++;

    regAddr[numRegs] = 0x19;
    regValue[numRegs] = 0x00;
    numRegs++;

    if(status == 0)
    {
        BridgeSii9022a_calculateCRC(regAddr, regValue, &numRegs);
        status = sii902x_bulk_write(dev, regAddr, regValue, numRegs);
    }
    regAddr[0] = 0x19U;
    regValue[0] = 0x00U;
    status += sii902x_bulk_write(dev, regAddr, regValue, 1);
    return status;
}

int sii902x_prgm_mode_reset_regs(struct udevice *dev){
    int32_t status = 0;
    uint8_t regAddr;
    uint8_t regValue;
    uint32_t numRegs = 1U;

    regAddr = 0x63U;
    regValue = 0x40;
    status = sii902x_write(dev, regAddr, regValue);

    regAddr = 0x60U;
    regValue = 0xA4; //0x04,A4,84
    status += sii902x_write(dev, regAddr, regValue);
    if (status == 0)
    {
        /* Sleep to be added for 5 ms if requried. */
        regAddr = 0x61U;
        status = sii902x_write(dev, regAddr, regValue);

        if (status == 0)
        {
            uint32_t tempSyncPolarityReg;
            uint32_t regValue32;
            /* Set the same sync polarity in 0x63U register */
            regValue32 = (uint32_t)regValue;
            tempSyncPolarityReg = (uint32_t)0x40;
            tempSyncPolarityReg &= (~(0x30U));
            tempSyncPolarityReg |= ((regValue32 & 0x03U) << 4U);

            regAddr = 0x63U;
            regValue = tempSyncPolarityReg;
            status = sii902x_write(dev, regAddr, regValue);
        }
    }
    return status;
}

static int sii902x_attach(struct udevice *dev)
{
    printf("\n Bridge attached \n");
	return 0;
}

static int sii902x_set_bridge_mode(struct udevice *dev){
    sii902x_configure_sync_mode(dev);
    sii902x_prgm_emb_sync_timing_info(dev);
    sii902x_prgm_AvInfo_Frame(dev);
    sii902x_prgm_mode_reset_regs(dev);
    return 0;
}

static int sii902x_start_device_output(struct udevice *dev){
     /* Enable TMDS output */
    uint8_t regAddr = 0x1A;
    uint8_t regValue = 0;
    sii902x_read(dev, regAddr, &regValue);
     /* Enable HDMI output */
    regValue |= 0x01U;
    /* Enable Output TMDS */
    regValue &= 0xEFU;
    sii902x_write(dev, regAddr, regValue);

    return sii902x_configure_input_bus(dev);
}

// static int sii902x_attach(struct udevice *dev){
//     // printf("sj: %s: found bridge: %s\n", __func__, dev->name);
//     return 0;
// }

// static bool display_mode_valid(void *priv, const struct display_timing *timing)
// {
// 	struct udevice *dev = priv;
// 	struct dm_display_ops *ops = display_get_ops(dev);

// 	if (ops && ops->mode_valid)
// 		return ops->mode_valid(dev, timing);

// 	return true;
// }

// int display_read_timing(struct udevice *dev, struct display_timing *timing)
// {
// 	struct dm_display_ops *ops = display_get_ops(dev);
// 	int panel_bits_per_colour;
// 	u8 buf[EDID_EXT_SIZE];
// 	int ret;

// 	if (ops && ops->read_timing)
// 		return ops->read_timing(dev, timing);

// 	if (!ops || !ops->read_edid)
// 		return -ENOSYS;
// 	ret = ops->read_edid(dev, buf, sizeof(buf));
// 	if (ret < 0)
// 		return ret;

// 	return edid_get_timing_validate(buf, ret, timing,
// 					&panel_bits_per_colour,
// 					display_mode_valid, dev);
// }

static int sii902x_probe(struct udevice *dev)
{
    const uint8_t buffer[11] = {0x00, 0x11, 0x00, 0x1D, 0x11, 0x00, 0x50, 0x03, 0x00, 0x40, 0x01};
    int len = 1;
    int temp_cnt = 0;
	int ret = 0;
    struct display_timing timing;
    // printf("sj: %s\n: found bridge: %s", __func__, dev->name);
	if (device_get_uclass_id(dev->parent) != UCLASS_I2C)
		return -EPROTONOSUPPORT;
	printf("\n\n\n[sj] sii902x probed\n\n\n");
    
    // ret = display_read_timing(dev, &timing);
    // if(!ret) printf("Display Timings:\n");
    // if(!ret)
    //     printf("Pixel clock : min->%d typ->%d max->%d\n",timing.pixelclock.min,timing.pixelclock.typ,timing.pixelclock.max);
    //     printf("Pixel clock : min->%d typ->%d max->%d\n",timing.hactive.min,timing.hactive.typ,timing.hactive.max);
    // Power-on and enable the bridge
    // sii902x_reset(dev);
    // printf("Bridge: reset\n");
    
    // i2c calls

    // dm_i2c_write(struct udevice *dev, uint offset, const uint8_t *buffer,int len);
    dm_i2c_write(dev, 0xC7, &buffer[temp_cnt++], 1);
    dm_i2c_write(dev, 0x1A, &buffer[temp_cnt++], 1);
    dm_i2c_write(dev, 0x00, &buffer[temp_cnt++], 1);
    dm_i2c_write(dev, 0x01, &buffer[temp_cnt++], 1);
    dm_i2c_write(dev, 0x1A, &buffer[temp_cnt++], 1);
    dm_i2c_write(dev, 0x1E, &buffer[temp_cnt++], 1);
    dm_i2c_write(dev, 0x26, &buffer[temp_cnt++], 1);
    dm_i2c_write(dev, 0x25, &buffer[temp_cnt++], 1);
    dm_i2c_write(dev, 0x27, &buffer[temp_cnt++], 1);
    dm_i2c_write(dev, 0x26, &buffer[temp_cnt++], 1);
    dm_i2c_write(dev, 0x1A, &buffer[temp_cnt++], 1);


    // init device
    // sii902x_power_up_transmitter(dev);
    // printf("Bridge: power-up-transmitter\n");

    // sii902x_enable_device(dev);
    // printf("Bridge: enable_device\n");

    // sii902x_configure_input_bus(dev);
    // printf("Bridge: configure_input_bus\n");

    // sii902x_configure_YCMux_mode(dev);
    // printf("Bridge: configure-YCMux\n");

    // sii902x_configure_sync_mode(dev);
    // printf("Bridge: configure_sync_mode\n");
    
    // // // setting mode, timing parameters,etc
    // sii902x_set_bridge_mode(dev);
    // printf("Bridge: set_bridge_mode\n");

	return 0;
}

static int sii902x_reset(struct udevice *dev)
{
    return 0;
}

static int sii902x_init(struct udevice *dev)
{
    unsigned int status = 0;
	u8 chipid[4];
	int ret;

    sii902x_reset(dev);

    ret = sii902x_write(dev, SII902X_REG_TPI_RQB, 0x0);
	if (ret)
		return ret;

	ret = sii902x_bulk_read(dev, SII902X_REG_CHIPID(0), &chipid, 4);
	if (ret) {
		return ret;
	}

	if (chipid[0] != 0xb0) {
		return -EINVAL;
	}

	return ret;
}

static int sii902x_update_bits(struct udevice *dev, uint reg,uint mask, uint val)
{    
    int ret;
	unsigned int tmp, orig;

    ret = sii902x_read(dev, reg, &orig);

    if(ret)
        return ret;

    tmp = orig & ~mask;
    tmp |= val & mask;

    ret = sii902x_write(dev, reg, tmp);

	return ret;
}

static void sii902x_bridge_enable(struct udevice *dev)
{
	sii902x_update_bits(dev, SII902X_PWR_STATE_CTRL,
			   SII902X_AVI_POWER_STATE_MSK,
			   SII902X_AVI_POWER_STATE_D(0));
	sii902x_update_bits(dev, SII902X_SYS_CTRL_DATA,
			   SII902X_SYS_CTRL_PWR_DWN, 0);
}

static void sii902x_bridge_mode_set(struct udevice *dev, struct display_timing *display_timing)
{
	// struct sii902x *sii902x = bridge_to_sii902x(bridge);
	u8 output_mode = SII902X_SYS_CTRL_OUTPUT_DVI;
	// struct regmap *regmap = sii902x->regmap;
	u8 buf[10];
	// struct hdmi_avi_infoframe frame;
	// u16 pixel_clock_10kHz = display_timing->pixelclock.max / 10;
	int ret;

	// if (sii902x->sink_is_hdmi)
	output_mode = SII902X_SYS_CTRL_OUTPUT_HDMI;

	buf[0] = 14850 & 0xff;
	buf[1] = 14850 >> 8;
	buf[2] = 60;
	buf[3] = 0x00;
	buf[4] = 1920 & 0xff;
	buf[5] = 1920 >> 8;
	buf[6] = 1080 & 0xff;
	buf[7] = 1080 >> 8;
	buf[8] = SII902X_TPI_CLK_RATIO_1X | SII902X_TPI_AVI_PIXEL_REP_NONE |
		 SII902X_TPI_AVI_PIXEL_REP_BUS_24BIT;
	buf[9] = SII902X_TPI_AVI_INPUT_RANGE_AUTO |
		 SII902X_TPI_AVI_INPUT_COLORSPACE_RGB;

	ret = sii902x_update_bits(dev, SII902X_SYS_CTRL_DATA,
				 SII902X_SYS_CTRL_OUTPUT_MODE, output_mode);
	if (ret)
		return;

	ret = sii902x_bulk_write(dev, SII902X_TPI_VIDEO_DATA, buf, 10);
}

static int sii902x_probe_new(struct udevice *dev)
{
    printf("\n\n\nsii902x probed\n\n\n");
    sii902x_init(dev);
    sii902x_bridge_enable(dev);
    sii902x_bridge_mode_set(dev,NULL);
    return 0;

}

struct video_bridge_ops sii902x_ops = {
	.attach = sii902x_attach,
};

static const struct udevice_id sii902x_ids[] = {
	{ .compatible = "sil,sii9022", },
	{ }
};

U_BOOT_DRIVER(sil_sii902x) = {
	.name	= "sil_sii902x",
	.id	= UCLASS_VIDEO_BRIDGE,
	.of_match = sii902x_ids,
	.probe	= sii902x_probe,
	.ops	= &sii902x_ops,
};
