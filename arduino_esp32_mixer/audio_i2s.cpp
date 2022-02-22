#include <FreeRTOS.h>
#include <arduino.h>
#include <driver/i2s.h>
#include "esp_system.h"
#include "audio_i2s.h"
#include "common.h"



#define WROVER_E_BOARD
//#define WROOM_32E_BOARD

#ifdef WROVER_E_BOARD
#define PIN_I2S_BCLK      4    
#define PIN_I2S_LRC       5    
#define PIN_I2S_DOUT      18   
#endif

#ifdef WROOM_32E_BOARD
#define PIN_I2S_BCLK      4    //26
#define PIN_I2S_LRC       17    //17
#define PIN_I2S_DOUT      26   // 5
#endif


// 44100Hz, 16bit, 
// 单声道 or 双声道
#define I2S_CHANNEL       1   // 1: Mono; 2: Stero
//#define I2S_CHANNEL       2   // 1: Mono; 2: Stero

#define I2S_NUM           I2S_NUM_0
#define SAMPLE_RATE       (44100)

extern int8_t log_level;

int I2S_Init() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,                           //2-channels
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 1024
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = PIN_I2S_BCLK,
    .ws_io_num = PIN_I2S_LRC,
    .data_out_num = PIN_I2S_DOUT,
    .data_in_num = -1   //Not used
  };
  int ret = i2s_driver_install((i2s_port_t)I2S_NUM, &i2s_config, 0, NULL);
  if (ret != 0) {
    Serial.println("i2s_driver_install failed.");
    return -1;
  }

  ret = i2s_set_pin((i2s_port_t)I2S_NUM, &pin_config);
  if (ret != 0) {
    Serial.println("i2s_driver_install failed.");
    return -1;
  }

  ret = i2s_set_clk((i2s_port_t)I2S_NUM, SAMPLE_RATE, (i2s_bits_per_sample_t)16, (i2s_channel_t)I2S_CHANNEL);
  if (ret != 0) {
    Serial.println("i2s_driver_install failed.");
    return -1;
  }
  Serial.println("I2S_Init success.");
  return 0;
}

int I2S_Write(char* data, int num_bytes) {
  if (log_level >= S_LOG_VERBOSE) {
    Serial.print("I2S_write: date=0x");
    Serial.print((int)data, HEX);
    Serial.print(", num_bytes=");
    Serial.println(num_bytes);
  }
  int ret = i2s_write_bytes((i2s_port_t)0, (const char *)data, num_bytes, portMAX_DELAY);
  return ret;
}

void I2S_Clear()
{
  i2s_zero_dma_buffer(I2S_NUM);  
}
