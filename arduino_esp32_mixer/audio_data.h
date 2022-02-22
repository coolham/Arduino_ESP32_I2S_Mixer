#ifndef __AUDIO_DATA_H__
#define __AUDIO_DATA_H__

#include "adjust_mixer.h"


// I2S 控制命令
#define I2S_STREAM_START      1
#define I2S_STREAM_STOP       2

// Wav数据控制命令
#define WAV_PLAY_START        1
#define WAV_PLAY_STOP         2


// Task
void task_i2s_play(void* p);
void task_data_proc(void* p);


// wav control
int start_wav_play(char* wav_files[], int num);
void stop_wav_play();
bool is_wav_playing();
int play_insert_wav(const char* filename);

void init_audio_source(char* wav_files[], int num);
void release_audio_resource();

// double buffer 
void init_double_buf();
int8_t get_read_buf();
int8_t get_write_buf();
void set_buf_ready(uint8_t index);
void swap_buf();
void print_buf_status();

// mesage queue 
int recv_i2s_ctrl_cmd();
void send_i2s_ctrl_cmd(int command);

int recv_data_ctrl_cmd();
void send_data_ctrl_cmd(int command);

// prepare data and mixer
int next_pcm_slice(audio_source_t* audio_list, int audio_src_num, int sample_num, char* dst_ptr);
int prepare_buf_data(audio_source_t* audio_list_ptr, int audio_src_num, int8_t buf_id, int sample_num, char* dst_ptr);

#endif  // __AUDIO_DATA_H__
