#ifndef __AUDIO_I2S_H__
#define __AUDIO_I2S_H__

int I2S_Init();
int I2S_Write(char* data, int num_bytes);
void I2S_Clear();

#endif //__AUDIO_I2S_H__
