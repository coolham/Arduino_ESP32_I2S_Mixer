/*

*/
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "audio_i2s.h"
#include "wav_def.h"
#include "audio_data.h"
#include "common.h"



#define CONFIG_HSPI
//#define CONFGI_VSPI

// HSPI
#ifdef CONFIG_HSPI
#define HSPI_MISO   12
#define HSPI_MOSI   13
#define HSPI_SCLK   14
#define HSPI_SS     15
#endif

// VSPI
#ifdef CONFIG_VSPI
#define VSPI_MISO   19
#define VSPI_MOSI   23
#define VSPI_SCLK   18
#define VSPI_SS     5
#endif


//#define AUDIO_DIR  "/mono"   // 单声道音频
#define AUDIO_DIR  "/audio1"   // 单声道音频

#define MAX_QUEUE_LENGTH     5   // 队列长度
#define MAX_MESSAGE_BYTES    4   // 消息长度， 4字节

int8_t log_level = S_LOG_INFO;
//int8_t log_level = S_LOG_VERBOSE;

#ifdef CONFIG_HSPI
static SPIClass hspi = SPIClass(HSPI);
#endif

#ifdef CONFGI_VSPI
static SPIClass vspi = SPIClass(VSPI);
#endif 

QueueHandle_t data_cmd_queue = NULL;
QueueHandle_t i2s_cmd_queue = NULL;

SemaphoreHandle_t mutex;


void setup() {
  Serial.begin(115200);

#ifdef CONFIG_HSPI
    hspi.begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_SS);
#endif
#ifdef CONFIG_VSPI
    vspi.begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI, VSPI_SS);
#endif

  uint32_t frequency = 40 * 1000000;
  const char * mountpoint = "/sd";
  uint8_t max_files = 10;

#ifdef CONFIG_HSPI
    if(!SD.begin(HSPI_SS, hspi, frequency, mountpoint, max_files, false)){
        Serial.println("Card Mount Failed");
        return;
    }
#endif

#ifdef CONFIG_VSPI
    if(!SD.begin(VSPI_SS, vspi, frequency, mountpoint, max_files, false)){
        Serial.println("Card Mount Failed");
        return;
    }
#endif

  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  int ret = I2S_Init();
  if (ret != 0) {
    Serial.println("I2S_audio_init failed!");
  }

  check_sdcard();
  delay(1000);

  mutex = xSemaphoreCreateMutex();

  // Audio I2S init
  I2S_Init();

  // init multi task
  task_init();

  //read_wav(SD, "/audio/angel.wav");
  testFileIO(SD, "/test.txt");

  delay(3000);
}

void task_init()
{
  data_cmd_queue = xQueueCreate( MAX_QUEUE_LENGTH, MAX_MESSAGE_BYTES);
  if (data_cmd_queue == 0) {
    Serial.println("Error creating the data cmd msg queue.");
  }

  i2s_cmd_queue = xQueueCreate( MAX_QUEUE_LENGTH, MAX_MESSAGE_BYTES);
  if (i2s_cmd_queue == 0) {
    Serial.println("Error creating the i2s cmd msg queue.");
  }

  xTaskCreate(
    task_data_proc,
    "task_data_proc",   // 任务名
    4096,  // This stack size can be checked & adjusted by reading the Stack Highwater
    NULL,
    2,  // 任务优先级, with 3 (configMAX_PRIORITIES - 1) 是最高的，0是最低的.
    NULL);

  xTaskCreate(
    task_i2s_play,
    "task_i2s_play",   // 任务名
    2048,  // This stack size can be checked & adjusted by reading the Stack Highwater
    NULL,
    2,  // 任务优先级, with 3 (configMAX_PRIORITIES - 1) 是最高的，0是最低的.
    NULL);  // Run task at Core 1, xPortGetCoreID()
}

void check_sdcard()
{
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
}

void test_case_1()
{
  printf("--------Test Case 1: Play one wav--------------\n");
  char* wav_files[] = {
    AUDIO_DIR"/hum (1).wav"
  };
  int num = sizeof(wav_files) / sizeof(wav_files[0]);
  printf("wav num=%d\n", num);

  int ret = start_wav_play(wav_files, num);
  if (ret != 0) {
    printf("Error: start play failed, ret=%d\n");
  }
  delay(2000);
  play_insert_wav(AUDIO_DIR"/drag (1).wav");
  while (is_wav_playing())
    delay(10);
  printf("--------Test Case 1: End----------------------\n\n");
}


void test_case_2()
{
  printf("--------Test Case 2: play 2 wav --------------\n");
  char* wav_files[] = {
    AUDIO_DIR"/track (1).wav",
    AUDIO_DIR"/font (1).wav"
  };
  int num = sizeof(wav_files) / sizeof(wav_files[0]);
  int ret = start_wav_play(wav_files, num);
  if (ret != 0) {
    printf("Error: start play failed, ret=%d\n");
  }
  delay(6000);
  play_insert_wav(AUDIO_DIR"/clash (1).wav");
  play_insert_wav(AUDIO_DIR"/blaster (1).wav");
  while (is_wav_playing())
    delay(10);
  printf("--------Test Case 2: End----------------------\n\n");
}

void test_case_3()
{
  printf("--------Test Case 3: play 3 wav --------------\n");
  char* wav_files[] = {
    AUDIO_DIR"/angel_10.wav",
    AUDIO_DIR"/preon (2).wav",
    AUDIO_DIR"/low battery.wav"
  };
  int num = sizeof(wav_files) / sizeof(wav_files[0]);
  int ret = start_wav_play(wav_files, num);
  if (ret != 0) {
    printf("Error: start play failed, ret=%d\n");
  }
  while (is_wav_playing())
    delay(10);
  printf("--------Test Case 3: End----------------------\n\n");
}

void test_case_4()
{
  printf("--------Test Case 4: play 4 wav --------------\n");
  char* wav_files[] = {
    AUDIO_DIR"/angel_10.wav",
    AUDIO_DIR"/preon (2).wav",
    AUDIO_DIR"/low battery.wav",
    AUDIO_DIR"/can_10.wav",
  };
  int num = sizeof(wav_files) / sizeof(wav_files[0]);
  int ret = start_wav_play(wav_files, num);
  if (ret != 0) {
    printf("Error: start play failed, ret=%d\n");
  }
  while (is_wav_playing())
    delay(10);
  printf("--------Test Case 4: End----------------------\n\n");
}

void test_case_5()
{
  printf("--------Test Case 5: play 5 wav --------------\n");
  char* wav_files[] = {
    AUDIO_DIR"/angel_10.wav",
    AUDIO_DIR"/preon (2).wav",
    AUDIO_DIR"/low battery.wav",
    AUDIO_DIR"/can_10.wav",
    AUDIO_DIR"/power on.wav"
  };
  int num = sizeof(wav_files) / sizeof(wav_files[0]);
  int ret = start_wav_play(wav_files, num);
  if (ret != 0) {
    printf("Error: start play failed, ret=%d\n");
  }
  while (is_wav_playing())
    delay(10);
  printf("--------Test Case 5: End----------------------\n\n");
}

void test_case_6()
{
  printf("--------Test Case 6: play 6 wav --------------\n");
  char* wav_files[] = {
    AUDIO_DIR"/angel_10.wav",
    AUDIO_DIR"/preon (2).wav",
    AUDIO_DIR"/low battery.wav",
    AUDIO_DIR"/can_10.wav",
    AUDIO_DIR"/power on.wav",
    AUDIO_DIR"/loud volume.wav"
  };
  int num = sizeof(wav_files) / sizeof(wav_files[0]);
  int ret = start_wav_play(wav_files, num);
  if (ret != 0) {
    printf("Error: start play failed, ret=%d\n");
  }
  while (is_wav_playing())
    delay(10);
  printf("--------Test Case 6: End----------------------\n\n");
}

void test_case_7()
{
  printf("--------Test Case 7: play 7 wav --------------\n");
  char* wav_files[] = {
    AUDIO_DIR"/angel_10.wav",
    AUDIO_DIR"/preon (2).wav",
    AUDIO_DIR"/low battery.wav",
    AUDIO_DIR"/can_10.wav",
    AUDIO_DIR"/power on.wav",
    AUDIO_DIR"/loud volume.wav",
    AUDIO_DIR"/fire blade.wav",
  };
  int num = sizeof(wav_files) / sizeof(wav_files[0]);
  int ret = start_wav_play(wav_files, num);
  if (ret != 0) {
    printf("Error: start play failed, ret=%d\n");
  }
  while (is_wav_playing())
    delay(10);
  printf("--------Test Case 7: End----------------------\n\n");
}

void test_case_10()
{
  printf("--------Test Case 10: play 2, stop------------\n");
  char* wav_files[] = {
    AUDIO_DIR"/angel_10.wav",
    AUDIO_DIR"/preon (2).wav",
  };
  int num = sizeof(wav_files) / sizeof(wav_files[0]);
  int ret = start_wav_play(wav_files, num);
  if (ret != 0) {
    printf("Error: start play failed, ret=%d\n");
  }
  delay(3000);
  stop_wav_play();
  printf("--------Test Case 10: End----------------------\n\n");
}

void test_case_11()
{
  printf("--------Test Case 11: play 2, insert------------\n");
  char* wav_files[] = {
    AUDIO_DIR"/angel_10.wav",
    AUDIO_DIR"/preon (2).wav",
  };
  int num = sizeof(wav_files) / sizeof(wav_files[0]);
  int ret = start_wav_play(wav_files, num);
  if (ret != 0) {
    printf("Error: start play failed, ret=%d\n");
  }
  delay(3000);
  play_insert_wav(AUDIO_DIR"/can_10.wav");
  while (is_wav_playing())
    delay(10);
  printf("--------Test Case 11: End----------------------\n\n");
}

void loop() {

  int fsize = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  Serial.print("free heap size(k)=");
  Serial.println((fsize / 1024));

  test_case_1();
  delay(2000);

  test_case_2();
  delay(2000);

//  test_case_3();
//  delay(2000);
//
//  test_case_4();
//  delay(2000);
//
//  test_case_5();
//  delay(2000);
//
//  test_case_6();
//  delay(2000);
//
//  test_case_7();
//  delay(2000);
//
//  test_case_10();
//  delay(2000);
//
//  test_case_11();
//  delay(2000);

  delay(10000);
}


/*
8G:
1048576 bytes read for 1325 ms
1048576 bytes written for 3264 ms

32G:
1048576 bytes read for 1806 ms
1048576 bytes written for 5692 ms
*/
void testFileIO(fs::FS &fs, const char * path) {
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if (file) {
    len = file.size();
    size_t flen = len;
    start = millis();
    while (len) {
      size_t toRead = len;
      if (toRead > 512) {
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %u ms\n", flen, end);
    file.close();
  } else {
    Serial.println("Failed to open file for reading");
  }


  file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  size_t i;
  start = millis();
  for (i = 0; i < 2048; i++) {
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
  file.close();
}
