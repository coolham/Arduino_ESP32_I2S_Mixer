#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include "SD.h"
#include "adjust_mixer.h"
#include "wav_def.h"
#include "common.h"

/*
  引入了一个调节参数f模型。具体算法描述如下：

  假如人声和背景音同时输入M个语音块，一个语音块的N样本。
  1.初始化f  = 1.0;
  2.将第一个语音块进行线性叠加。得到 S(1, 2, 3, ...N)波形序列,从S序列中找出绝对值最大的样本数S(max).
  3.计算本语音块的调节参数 如果S(max) >32767则： f = S(max) / 32767,如果 S(max) <=32767； f用上次语音块的f;
  4.将S(1, 2, 3, ...., N)全部乘 f得到新的S样本集合。
  5.将f趋近于1.0操作 f = (1 - f) / 32 + f;
  6.获取下一个语音块，重复第2步。

*/



extern int8_t log_level;
extern int slice_times;


// 混音函数
int adjust_mixer(audio_source_t* data_list, int32_t sample_num, int32_t src_size, int16_t* dst, int32_t* dst_size)
{
  if (log_level >= S_LOG_DEBUG) {
    printf("adjust_mixer, n=%d\n", slice_times);
  }
  int16_t* dst_pos = dst;
  int16_t* src_pos = NULL;

  int32_t	mix_value = 0;
  int32_t	max_sample = 0;
  register int32_t i = 0;
  float f_ = 1.0;

  audio_source_t* audio_sources = (audio_source_t*)data_list;

  int32_t* values_buf = (int32_t*)malloc(src_size * sizeof(int32_t));
  if (values_buf == NULL) {
    printf("E: can not malloc values: %d\n", (src_size * sizeof(int32_t)));
    return -1;
  }

  //如果只有一个样本，无需混合，只需要赋值就行了
  if (sample_num == 1) {
    src_pos = (int16_t*)audio_sources[0].buf;
    for (i = 0; i < src_size; ++ i) {
      dst_pos[i] = src_pos[i];
    }
    f_ = 1.0;
  }
  else if (sample_num > 0) {
    for (i = 0; i < src_size; ++ i) { //统计样本
      mix_value = 0;
      for (register int32_t v_index = 0; v_index < sample_num; ++ v_index) {
        src_pos = (int16_t*)audio_sources[v_index].buf;
        if (src_pos != NULL)
          mix_value += src_pos[i];
      }

      values_buf[i] = mix_value;

      if (max_sample < abs(mix_value)) //求出一个最大的样本
        max_sample = abs(mix_value);
    }

    if (max_sample * f_ > VOLUMEMAX)
      f_ = VOLUMEMAX * 1.0 / max_sample;

    for (i = 0; i < src_size; ++ i)
      dst_pos[i] = (int16_t)(values_buf[i] * f_);

    //让f_接近1.0
    if (f_ < 1.0)
      f_ = (1.0  - f_) / 32 + f_;
    else if (f_ > 1.0)
      f_ = 1.0;

    *dst_size = src_size;
  }
  free(values_buf);
  return 0;
}

// 读取数据片段，每次读取后关闭文件
int read_slice_one(audio_source_t* audio_source, int sample_num)
{
  char* fname = audio_source->wav_file;
  if (log_level >= S_LOG_VERBOSE) {
    printf("slice file: %s\n", fname);
  }
  File f;
  Wav wav;
  f = SD.open(fname, FILE_READ);
  if (!f) {
    printf("Error read slice file: %s\n", fname);
    return -1;
  }
  f.read((uint8_t*)&wav, sizeof(wav));
  audio_source->ch_num = wav.fmt.NumChannels;

  if (audio_source->buf != NULL) {
    // 释放之前分配的内存
    free(audio_source->buf);
    audio_source->buf = NULL;
  }

  // 采样点数，转成字节数
  int read_size =  sample_num * 2;
  int buf_size = sample_num * 2;

  int offset =  audio_source->offset;
  if (offset != 0) {
    bool sret = f.seek(offset, SeekSet);
    if (!sret) {
      //到达文件尾部
      printf("fseek to file end: %s\n", fname);
      audio_source->end_flag = 1;
      f.close();
      return 1;
    }
    offset += read_size;
  } else {
    offset = sizeof(wav) + read_size;
  }

  // 分配缓存
  char* buf = (char*)malloc(buf_size);
  memset(buf, 0, buf_size);
  if (buf == NULL) {
    printf("malloc slice data failed.\n");
    f.close();
    return -2;
  }

  int n = f.read((uint8_t*)buf, read_size);
  if (n == 0) {
    printf("read data end: %s\n", fname);
    free(buf);
    audio_source->buf = NULL;
    audio_source->end_flag = 1;
  } else {
    audio_source->buf = buf;
    audio_source->offset = offset;
  }

  f.close();
  return 0;
}

// 读取数据文件，直到播放结束才关闭文件
// read keep open slice
int read_ko_slice_one(audio_source_t* audio_source, int sample_num)
{
  char* fname = audio_source->wav_file;
  if (log_level >= S_LOG_VERBOSE) {
    printf("slice file: %s\n", fname);
  }
  Wav wav;
  if (audio_source->fp_enabled == 0) {
    // open file
    audio_source->fp = SD.open(fname, FILE_READ);
    if (! audio_source->fp) {
      printf("Error read slice file: %s\n", fname);
      return -1;
    }
    printf("open slice file: %s\n", fname);
    audio_source->fp_enabled = 1;
    int n = audio_source->fp.read((uint8_t*)&wav, sizeof(wav));
    if (n == 0) {
      printf("read wav header error\n");
      return -1;
    }
  }

  if (audio_source->buf != NULL) {
    // 释放之前分配的内存
    free(audio_source->buf);
    audio_source->buf = NULL;
  }

  // 采样点数，转成字节数
  int read_size =  sample_num * 2;
  int buf_size = sample_num * 2;

  int offset =  audio_source->offset;
  if (offset != 0) {
    bool sret = audio_source->fp.seek(offset, SeekSet);
    if (!sret) {
      //到达文件尾部
      printf("fseek to file end: %s\n", fname);
      audio_source->end_flag = 1;
      audio_source->fp.close();
      audio_source->fp_enabled = 0;
      return 1;
    }
    offset += read_size;
  } else {
    offset = sizeof(wav) + read_size;
  }

  // 分配缓存
  char* buf = (char*)malloc(buf_size);
  memset(buf, 0, buf_size);
  if (buf == NULL) {
    printf("malloc slice data failed.\n");
    audio_source->end_flag = 1;
    audio_source->fp_enabled = 0;
    audio_source->fp.close();
    return -2;
  }

  int n = audio_source->fp.read((uint8_t*)buf, read_size);
  if (n == 0) {
    printf("read data end: %s\n", fname);
    free(buf);
    audio_source->buf = NULL;
    audio_source->end_flag = 1;
  } else {
    audio_source->buf = buf;
    audio_source->offset = offset;
  }

  if (audio_source->end_flag == 1) {
    audio_source->fp.close();
    audio_source->fp_enabled = 0;
  }
  return 0;
}
