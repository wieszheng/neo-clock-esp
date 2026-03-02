/**
 * @file AudioManager.cpp
 * @brief 音频采集管理器实现 — INMP441 麦克风 I2S 音频数据采集
 *
 * I2S 音频采集原理:
 *   - 使用 ESP32 内置 I2S 驱动读取 INMP441 麦克风数据
 *   - 16kHz 采样率，16位单声道
 *   - DMA 方式传输，非阻塞读取
 *   - 128 点 FFT 分析缓冲区
 */

#include "AudioManager.h"
#include "Logger.h"

// ==================================================================
// 单例
// ==================================================================

AudioManager_ &AudioManager_::getInstance()
{
  static AudioManager_ instance;
  return instance;
}

AudioManager_ &AudioManager = AudioManager_::getInstance();

// ==================================================================
// 初始化
// ==================================================================

void AudioManager_::setup()
{
  LOG_INFO("[Audio] 初始化音频采集管理器...");
  LOG_INFO("[Audio] I2S 引脚: BCLK=%d, WS=%d, SD=%d", I2S_BCLK_PIN, I2S_WS_PIN, I2S_SD_PIN);

  configureI2S();

  // 测试读取一次验证硬件
  size_t bytesRead;
  int16_t testBuffer[64];
  i2s_read(_i2sPort, testBuffer, sizeof(testBuffer), &bytesRead, 100);

  if (bytesRead > 0)
  {
    _audioAvailable = true;
    LOG_INFO("[Audio] I2S 音频采集初始化成功");
  }
  else
  {
    _audioAvailable = false;
    LOG_WARN("[Audio] I2S 音频采集初始化失败，检查硬件连接");
  }

  memset(_audioBuffer, 0, sizeof(_audioBuffer));
  memset(_readBuffer, 0, sizeof(_readBuffer));
  _hasNewData = false;
  _lastReadTime = millis();
}

// ==================================================================
// I2S 配置
// ==================================================================

void AudioManager_::configureI2S()
{
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = AUDIO_SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0};

  i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_BCLK_PIN,
      .ws_io_num = I2S_WS_PIN,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = I2S_SD_PIN};

  esp_err_t result = i2s_driver_install(_i2sPort, &i2s_config, 0, NULL);
  if (result != ESP_OK)
  {
    LOG_ERROR("[Audio] I2S 驱动安装失败: %d", result);
    return;
  }

  result = i2s_set_pin(_i2sPort, &pin_config);
  if (result != ESP_OK)
  {
    LOG_ERROR("[Audio] I2S 引脚配置失败: %d", result);
    return;
  }

  LOG_INFO("[Audio] I2S 配置完成");
}

// ==================================================================
// 主循环
// ==================================================================

void AudioManager_::tick()
{
  if (!_audioAvailable)
    return;

  // 每 10ms 读取一次音频数据 (约 160 个采样点)
  unsigned long now = millis();
  if (now - _lastReadTime < 10)
    return;
  _lastReadTime = now;

  size_t bytesRead;
  i2s_read(_i2sPort, _readBuffer, sizeof(int16_t) * AUDIO_BUFFER_SIZE, &bytesRead, 0);

  if (bytesRead >= sizeof(int16_t) * FFT_SIZE)
  {
    // 复制数据到输出缓冲区
    memcpy(_audioBuffer, _readBuffer, sizeof(int16_t) * FFT_SIZE);
    _hasNewData = true;
  }
}
