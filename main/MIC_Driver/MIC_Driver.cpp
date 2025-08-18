#include "MIC_Driver.h"

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "esp_err.h"
#include "esp_log.h"

#define I2S_CHANNEL_NUM 1

#define I2S_CONFIG_DEFAULT(sample_rate, channel_fmt, bits_per_chan) { \
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate), \
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(bits_per_chan, channel_fmt), \
    .gpio_cfg = { \
        .mclk = GPIO_NUM_NC, \
        .bclk = GPIO_NUM_15, \
        .ws   = GPIO_NUM_2, \
        .dout = GPIO_NUM_NC, \
        .din  = GPIO_NUM_39, \
        .invert_flags = { \
            .mclk_inv = false, \
            .bclk_inv = false, \
            .ws_inv   = false, \
        }, \
    }, \
}

static i2s_chan_handle_t rx_handle = NULL;   // I2S rx channel handler

static esp_err_t i2s_init(i2s_port_t i2s_num, uint32_t sample_rate, int channel_format, int bits_per_chan)
{
    esp_err_t ret_val = ESP_OK;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(i2s_num, I2S_ROLE_MASTER);

    ret_val |= i2s_new_channel(&chan_cfg, NULL, &rx_handle);
    i2s_std_config_t std_cfg = I2S_CONFIG_DEFAULT(16000, I2S_SLOT_MODE_MONO, I2S_DATA_BIT_WIDTH_32BIT);
    // std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_RIGHT;
    // std_cfg.clk_cfg.mclk_multiple = EXAMPLE_MCLK_MULTIPLE;   //The default is I2S_MCLK_MULTIPLE_256. If not using 24-bit data width, 256 should be enough
    ret_val |= i2s_channel_init_std_mode(rx_handle, &std_cfg);
    ret_val |= i2s_channel_enable(rx_handle);

    return ret_val;
}

void MIC_Init() 
{
    i2s_init(I2S_NUM_1, 16000, 2, 32);
}

bool
MIC_GetSample(int& sample)
{
    static int32_t rxBuf;
    static size_t  rxBytes;

    i2s_channel_read(rx_handle, &rxBuf, sizeof(rxBuf), &rxBytes, 1000);
    if (rxBytes < 4) return false;

    sample = rxBuf >> 14;

    return true;
}
