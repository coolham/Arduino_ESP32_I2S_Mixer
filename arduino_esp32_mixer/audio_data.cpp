#include <arduino.h>

#include "wav_def.h"
#include "adjust_mixer.h"
#include "audio_data.h"
#include "audio_i2s.h"
#include "common.h"

static const char *TAG = "audio_data";

//#define PCM_BUF_SIZE            (16560/2)    // 按4字节对齐（16bits, 双声道）
#define PCM_BUF_SIZE            (1024*4)   // 按4字节对齐（16bits, 双声道）

int audio_list_num = 0;
audio_source_t audio_list[MAX_SOURCE_NUM];
char pcm_buf_0[PCM_BUF_SIZE];
char pcm_buf_1[PCM_BUF_SIZE];

int8_t cur_index = 0;    // 0: buf_0; 1:buf_1
bool buf_ready[2] = { false, false };
bool buf_consumed[2] = { true, true };

bool playing_flag = false;  // 是否有播放任务进行中

extern int8_t log_level;

extern QueueHandle_t data_cmd_queue;
extern QueueHandle_t i2s_cmd_queue;
extern SemaphoreHandle_t mutex;

int slice_times = 0;


// I2S 播放Task
void task_i2s_play(void* p)
{
  printf("task_i2s_play start, core=%d\n", xPortGetCoreID());

  int total_bytes = 0;
  bool task_play_run = false;
  uint32_t count = 0;
  while (1)
  {
    int command = recv_i2s_ctrl_cmd();
    switch (command) {
      case I2S_STREAM_START:
        // start play
        printf("start I2S stream...\n");
        task_play_run = true;
        delay(100);
        count = 0;
        break;
      case I2S_STREAM_STOP:
        // stop command
        task_play_run = false;
        I2S_Clear();
        break;
    }

    if (task_play_run) {
      int8_t id = get_read_buf();
      switch (id) {
        case 0:
          I2S_Write(pcm_buf_0, PCM_BUF_SIZE);
          swap_buf();
          break;
        case 1:
          I2S_Write(pcm_buf_1, PCM_BUF_SIZE);
          swap_buf();
          break;
        default:
          //printf("i2s task hunger...\n");
          break;
      }
    }

    delay(1);
    count++;
  }
}

// 初始化缓存区
void init_double_buf()
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  cur_index = 0;
  buf_ready[0] = false;
  buf_ready[1] = false;
  buf_consumed[0] = true;
  buf_consumed[1] = true;
  memset(pcm_buf_0, 0, PCM_BUF_SIZE);
  memset(pcm_buf_1, 0, PCM_BUF_SIZE);
  xSemaphoreGive(mutex);
}

void print_buf_status()
{
  printf("cur=%d  %d:%d    %d:%d\n", cur_index, buf_ready[0], buf_consumed[0], buf_ready[1], buf_consumed[1]);
}

// 获取可播放的Buf ID
int8_t get_read_buf()
{
  int8_t ret = -1;
  xSemaphoreTake(mutex, portMAX_DELAY);
  if (buf_ready[cur_index] && !buf_consumed[cur_index]) {
    ret = cur_index;
  }
  xSemaphoreGive(mutex);
  return ret;
}

// 获取可写入数据的Buf ID
int8_t get_write_buf()
{
  int8_t ret = -1;
  xSemaphoreTake(mutex, portMAX_DELAY);
  if (!buf_ready[cur_index] && buf_consumed[cur_index]) {
    ret = cur_index;
  } else if (!buf_ready[1 - cur_index] && buf_consumed[1 - cur_index]) {
    ret = 1 - cur_index;
  }
  xSemaphoreGive(mutex);
  return ret;
}

// 设置Buf数据准备好
void set_buf_ready(uint8_t index)
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  buf_ready[index] = true;
  buf_consumed[index] = false;
  xSemaphoreGive(mutex);
}

// I2S完成当前Buf播放，切换到下一个Buf
void swap_buf()
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  buf_ready[cur_index] = false;
  buf_consumed[cur_index] = true;
  cur_index = 1 - cur_index;
  xSemaphoreGive(mutex);
}

// 播放Task接收控制命令
int recv_i2s_ctrl_cmd()
{
  int command = 0;
  xQueueReceive(i2s_cmd_queue, &command, 0);
  if (command != 0) {
    if (log_level >= S_LOG_DEBUG) printf("RECV: I2s cmd=%d\n", command);
  }
  return command;
}

// 发送播放任务控制命令
void send_i2s_ctrl_cmd(int command)
{
  if (log_level >= S_LOG_DEBUG)
    printf("SEND: I2S cmd=%d\n", command);
  xQueueSend(i2s_cmd_queue, &command,  (TickType_t)100);
}

// 结束数据任务控制命令
int recv_data_ctrl_cmd()
{
  int command = 0;
  xQueueReceive(data_cmd_queue, &command, 0);
  if (command != 0) {
    if (log_level >= S_LOG_DEBUG) printf("RECV: data cmd=%d\n", command);
  }
  return command;
}

// 发送数据任务控制命令
void send_data_ctrl_cmd(int command)
{
  if (log_level >= S_LOG_DEBUG)
    printf("SEND: data cmd=%d\n", command);
  xQueueSend(data_cmd_queue, &command,  (TickType_t)100);
}

//--------------------------------------------------------------------
// 开始播放Wav文件
int start_wav_play(char* wav_files[], int num)
{
  if (playing_flag) {
    printf("play task still runing, can not start new play!\n");
    return -1;
  }
  printf("start_wav_play, num=%d\n", num);
  playing_flag = true;
  init_audio_source(wav_files, num);
  audio_list_num = num;
  send_data_ctrl_cmd(WAV_PLAY_START);
  return 0;
}

// 停止播放Wav文件
void stop_wav_play()
{
  printf("stop_wav_play\n");
  send_i2s_ctrl_cmd(I2S_STREAM_STOP);
  send_data_ctrl_cmd(WAV_PLAY_STOP);
  //release_audio_resource();
  playing_flag = false;
}

// 检查当前是否有播放进行中
bool is_wav_playing()
{
  return playing_flag;
}

// 在播放中插入新的Wav文件
int play_insert_wav(const char* filename)
{
  printf("play_insert_wav: %s\n", filename);
  if (!playing_flag) {
    printf("not wav in playing, insert failed.");
    return -1;
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  audio_source_t* ptr = &audio_list[audio_list_num];
  strcpy(ptr->wav_file, filename);
  ptr->ch_num = 2;
  ptr->end_flag = 0;
  ptr->offset = 0;
  ptr->buf = NULL;
  audio_list_num++;
  xSemaphoreGive(mutex);
  printf("cur audio_list_num=%d\n", audio_list_num);
  return 0;
}

// 初始化播放资源
void init_audio_source(char* wav_files[], int num)
{
  xSemaphoreTake(mutex, portMAX_DELAY);
  for (int i = 0; i < num; i++) {
    printf("add wav: %s\n", wav_files[i]);
    strcpy(audio_list[i].wav_file, wav_files[i]);
    audio_list[i].offset = 0;
    audio_list[i].buf = NULL;
    audio_list[i].end_flag = 0;
  }
  xSemaphoreGive(mutex);
}

// 释放播放资源
void release_audio_resource()
{
  printf("release audio resource\n");
  for (int i = 0; i < MAX_SOURCE_NUM; i++) {
    if (audio_list[i].buf != NULL) {
      free(audio_list[i].buf);
      audio_list[i].buf = NULL;
    }
    audio_list[i].offset = 0;
    audio_list[i].end_flag = 0;
#ifdef KEEP_FILE_OPEN
    // 关闭文件
    if (audio_list[i].fp_enabled == 1) {
      audio_list[i].fp.close();
      audio_list[i].fp_enabled = 0;
    }
#endif
  }
  playing_flag = false;
  printf("finish release resource\n");
}

//-------------------------------------------------------------
// 播放数据处理Task
void task_data_proc(void* p)
{
  printf("task_data_proc start, core=%d\n", xPortGetCoreID());

  audio_source_t* audio_list_ptr = audio_list;
  int sample_point_num = PCM_BUF_SIZE / 2;

  int ret = 0;
  bool task_data_run_flag = false;
  slice_times = 0;
  while (1)
  {
    int cmd = recv_data_ctrl_cmd();
    switch (cmd) {
      case WAV_PLAY_START:
        printf("start new play...\n");
        init_double_buf();
        task_data_run_flag = true;
        slice_times = 0;
        send_i2s_ctrl_cmd(I2S_STREAM_START);
        break;
      case WAV_PLAY_STOP:
        printf("recv stop play wave cmd\n");
        release_audio_resource();
        task_data_run_flag = false;
        send_i2s_ctrl_cmd(I2S_STREAM_STOP);
        break;
    }
    if (task_data_run_flag) {
      int8_t buf_id = get_write_buf();
      if (buf_id == 1 || buf_id == 0) {
        // 准备buf数据
        if (log_level >= S_LOG_DEBUG) printf("prepare buf %d\n", buf_id);

        char* dst_ptr = (buf_id == 0) ? pcm_buf_0 : pcm_buf_1;
        ret = prepare_buf_data(audio_list_ptr, audio_list_num, buf_id, sample_point_num, dst_ptr);
        if (ret == 0) {
          if (log_level >= S_LOG_DEBUG) printf("buf_%d ready\n", buf_id);
          set_buf_ready(buf_id);
          slice_times++;
        } else if (ret == 1) {
          printf("task data end\n");
          task_data_run_flag = false;
        } else {
          printf("prepare data error: %d\n", ret);
          printf("Abnormal stop playing!\n");
          stop_wav_play();
        }
      }
    }
    delay(1);
  }
  printf("task_data_proc should not exit.\n");
}


// 准备buf数据
int prepare_buf_data(audio_source_t* audio_list_ptr, int audio_src_num, int8_t buf_id, int sample_num, char* dst_ptr)
{
  if (log_level >= S_LOG_VERBOSE)
    printf("-->prepare buf %d data\n", buf_id);

  int ret = next_pcm_slice(audio_list_ptr, audio_src_num, sample_num, dst_ptr);

  if (ret == 1) {
    // 无更多数据，结束播放
    printf("no valid audio source, should stop play.\n");
    send_data_ctrl_cmd(I2S_STREAM_STOP);
    //delay(100);
    return 1;   // 数据完成
  } else if ( ret < 0) {
    // 其它错误
    printf("next_pcm_slice return err:%d, stop mixer task\n", ret);
    return ret;
  }
  if (log_level >= S_LOG_VERBOSE)
    printf("-->finish buf %d data\n", buf_id);
  return 0;
}

// 获取音频数据片段
int next_pcm_slice(audio_source_t* audio_list, int audio_src_num, int sample_point_num, char* dst_ptr)
{
  int src_size = sample_point_num;  // 采样数
  int dst_size = sample_point_num;

  //  int ret = read_slice_data(audio_list, audio_src_num, sample_point_num);
  //  if (ret != 0) {
  //    Serial.println("read_slice_data error!");
  //    return -1;
  //  }

  audio_source_t valid_audio_list[MAX_SOURCE_NUM];
  int valid_audio_num = 0;
  bool has_audio = false;
  for (int i = 0; i < audio_src_num; i++) {
    audio_source_t* audio_source =  &audio_list[i];
    if (audio_source->end_flag != 1) {

#ifdef KEEP_FILE_OPEN
      int ret = read_ko_slice_one(audio_source, sample_point_num);
#else
      int ret = read_slice_one(audio_source, sample_point_num);
#endif

      if (ret != 0) {
        printf("read_slice_data error!");
        return -1;
      }
    }
    if (audio_list[i].end_flag != 1 && audio_list[i].buf != NULL) {
      has_audio = true;
      strcpy(valid_audio_list[valid_audio_num].wav_file, audio_list[i].wav_file);
      valid_audio_list[valid_audio_num].fp =  audio_list[i].fp;
      valid_audio_list[valid_audio_num].buf =  audio_list[i].buf;
      valid_audio_list[valid_audio_num].offset =  audio_list[i].offset;
      valid_audio_list[valid_audio_num].end_flag =  audio_list[i].end_flag;
      valid_audio_list[valid_audio_num].ch_num =  audio_list[i].ch_num;
      valid_audio_num++;
    }
  }
  if (log_level >= S_LOG_DEBUG) printf("valid_audio_num=%d\n", valid_audio_num);

  if (has_audio) {
    adjust_mixer((audio_source_t*)valid_audio_list, valid_audio_num, src_size, (int16_t*)dst_ptr, &dst_size);
    return 0;
  } else {
    return 1;
  }
}
