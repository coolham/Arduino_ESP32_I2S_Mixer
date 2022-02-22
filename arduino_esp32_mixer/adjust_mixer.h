#ifndef __ADJUST_MIX_H__
#define __ADJUST_MIX_H__

#include "FS.h"

#define KEEP_FILE_OPEN      // 打开Wav文件后，不关闭，直到播放任务结束

#define MAX_SOURCE_NUM      10
#define VOLUMEMAX           32767


typedef struct audio_source{
    char    wav_file[32];     // wav文件名称，最大32字节
    File    fp;
    char    fp_enabled;       // 1: 文件打开， fp有效
    int     offset;           // 读取文件偏移量
    char*   buf;
    char    ch_num;           // Mono = 1, Stereo = 2,
    char    end_flag;
} audio_source_t;



int read_slice_one(audio_source_t* audio_source, int sample_num);
int read_ko_slice_one(audio_source_t* audio_source, int sample_num);

int adjust_mixer(audio_source_t* data_list, int32_t sample_num, int32_t src_size, int16_t* dst, int32_t* dst_size);



#endif // __ADJUST_MIX_H__
