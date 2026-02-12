/**
 * @file PeripheryManager.cpp
 * @brief 外设管理器 — DHT22 温湿度传感器
 *
 * ESP32 性能优化:
 *   - 消除 readDHT22() 中的 delay() 阻塞
 *   - 失败时在下一个 tick 周期重试，而非循环等待
 *   - 非阻塞: 主循环延迟从最坏 ~6 秒 降为 0
 */

#include "PeripheryManager.h"
#include "Globals.h"
#include <math.h>

PeripheryManager_ &PeripheryManager_::getInstance() {
  static PeripheryManager_ instance;
  return instance;
}

PeripheryManager_ &PeripheryManager = PeripheryManager_::getInstance();

#include <arduinoFFT.h>
#include <driver/i2s.h>

arduinoFFT FFT = arduinoFFT();

void PeripheryManager_::setup() {
  Serial.println("[Periphery] 初始化外设管理器...");

  dht = new DHT(DHT_PIN, DHT_TYPE);
  dht->begin();

  temperature = 0.0;
  humidity = 0.0;
  sensorAvailable = false;
  lastUpdate = 0;
  _retryCount = 0;

  // 初始化麦克风
  initMic();

  // 首次读取
  Serial.println("[Periphery] 测试读取DHT22传感器...");
  readDHT22();

  if (sensorAvailable) {
    Serial.printf("[Periphery] DHT22 初始化成功: %.1f°C, %.1f%%\n", temperature,
                  humidity);
    INDOOR_TEMP = temperature;
    INDOOR_HUM = humidity;
  } else {
    Serial.println("[Periphery] 警告: DHT22 传感器读取失败");
  }

  Serial.println("[Periphery] 外设管理器初始化完成");
}

void PeripheryManager_::initMic() {
  vReal = (double *)malloc(FFT_SAMPLES * sizeof(double));
  vImag = (double *)malloc(FFT_SAMPLES * sizeof(double));

  if (!vReal || !vImag) {
    Serial.println("[Periphery] FFT 内存分配失败!");
    return;
  }

  initI2S();

  // 创建互斥锁
  _audioMutex = xSemaphoreCreateMutex();

  // 启动音频处理任务 (Core 0, 优先级 1)
  // 8192Stack深度以确保足够空间
  xTaskCreatePinnedToCore(audioTask, "AudioTask", 8192, this, 1,
                          &_audioTaskHandle, 0);

  Serial.println("[Periphery] I2S 麦克风初始化完成 (异步任务已启动)");
}

void PeripheryManager_::initI2S() {
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = I2S_SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // INMP441 L/R 接地为左声道
      .communication_format = static_cast<i2s_comm_format_t>(
          I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = FFT_SAMPLES / 2, // 较小的缓冲块以减少延迟
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0};

  i2s_pin_config_t pin_config = {.bck_io_num = I2S_SCK,
                                 .ws_io_num = I2S_WS,
                                 .data_out_num = I2S_PIN_NO_CHANGE,
                                 .data_in_num = I2S_SD};

  if (i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL) != ESP_OK) {
    Serial.println("[Periphery] I2S 驱动安装失败");
    return;
  }
  if (i2s_set_pin(I2S_PORT, &pin_config) != ESP_OK) {
    Serial.println("[Periphery] I2S 引脚配置失败");
    return;
  }
  i2s_set_clk(I2S_PORT, I2S_SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT,
              I2S_CHANNEL_MONO);
}

// 静态任务入口
void PeripheryManager_::audioTask(void *pvParameters) {
  PeripheryManager_ *instance = (PeripheryManager_ *)pvParameters;
  while (true) {
    instance->processAudio();
    // 简单让渡，防止看门狗 (通常 i2s_read 会阻塞足够时间)
    vTaskDelay(1);
  }
}

// 实际音频处理循环 (运行在 AudioTask 中)
void PeripheryManager_::processAudio() {
  if (!vReal || !vImag) {
    vTaskDelay(100);
    return;
  }

  size_t bytesRead = 0;
  int16_t i2sBuffer[FFT_SAMPLES];

  // 1. 读取 I2S (阻塞等待)
  // 使用 portMAX_DELAY 确保读满缓冲区
  esp_err_t err = i2s_read(I2S_PORT, (void *)i2sBuffer, sizeof(i2sBuffer),
                           &bytesRead, portMAX_DELAY);

  if (err != ESP_OK || bytesRead != sizeof(i2sBuffer)) {
    return; // 读取失败
  }

  // 2. 填充并加窗
  for (int i = 0; i < FFT_SAMPLES; i++) {
    vReal[i] = (double)i2sBuffer[i];
    vImag[i] = 0;
  }

  // 加窗减少频谱泄漏
  FFT.Windowing(vReal, FFT_SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);

  // 3. FFT 计算
  FFT.Compute(vReal, vImag, FFT_SAMPLES, FFT_FORWARD);
  FFT.ComplexToMagnitude(vReal, vImag, FFT_SAMPLES);

  // 4. 分频段处理 (对数映射优化)
  // 频率范围: 0 ~ 20kHz
  // 关注范围: ~60Hz 到 ~14kHz
  // 分箱策略: Logarithmic

  const double samplingFreq = I2S_SAMPLE_RATE;

  uint8_t tempBands[FFT_NUM_BANDS];

  for (int i = 0; i < FFT_NUM_BANDS; i++) {
    // 简单的对数分布计算
    // Low bound of band i
    // range 2..FFT_SAMPLES/2

    // 算法: 使用 pow 映射 i (0..31) 到 bin index
    // 比如 32个频段，我们希望涵盖 bin 2 到 bin 256 (因高频通常能量低且bin多)

    int startBin =
        (int)(2.0 * pow((FFT_SAMPLES / 4.0) / 2.0, (double)i / FFT_NUM_BANDS));
    int endBin = (int)(2.0 * pow((FFT_SAMPLES / 4.0) / 2.0,
                                 (double)(i + 1) / FFT_NUM_BANDS));

    if (endBin <= startBin)
      endBin = startBin + 1;
    if (endBin > FFT_SAMPLES / 2)
      endBin = FFT_SAMPLES / 2;

    double sum = 0;
    int count = 0;

    for (int j = startBin; j < endBin; j++) {
      sum += vReal[j];
      count++;
    }

    if (count > 0)
      sum /= count;

    // 噪声门限
    if (sum < FFT_NOISE)
      sum = 0;
    else
      sum -= FFT_NOISE;

    // 幅度映射
    double val = (sum / FFT_AMPLITUDE) * 255.0;

    // 简单的非线性增益 (让小声音更明显)
    // val = pow(val / 255.0, 0.8) * 255.0;

    if (val > 255.0)
      val = 255.0;
    tempBands[i] = (uint8_t)val;
  }

  // 5. 更新共享数据 (互斥锁保护)
  if (xSemaphoreTake(_audioMutex, 10 / portTICK_PERIOD_MS) == pdTRUE) {
    memcpy(_sharedBands, tempBands, FFT_NUM_BANDS);
    xSemaphoreGive(_audioMutex);
  }
}

// 主线程获取数据 (非阻塞)
bool PeripheryManager_::getSpectrumData(uint8_t *outputBands, int numBands) {
  if (!_audioMutex)
    return false;

  bool success = false;
  if (xSemaphoreTake(_audioMutex, 0) == pdTRUE) { // 立即尝试获取，不等待
    // 如果请求数量不同，进行安全拷贝
    int count = (numBands < FFT_NUM_BANDS) ? numBands : FFT_NUM_BANDS;
    memcpy(outputBands, _sharedBands, count);

    // 如果请求更多，填充0
    if (numBands > count) {
      memset(outputBands + count, 0, numBands - count);
    }

    xSemaphoreGive(_audioMutex);
    success = true;
  }

  // 如果获取锁失败，outputBands 保持不变（上一帧数据），防止闪烁
  // 或者可以在这里返回 false，由上层决定

  return success;
}

void PeripheryManager_::tick() {
  unsigned long currentTime = millis();

  // 正常读取间隔 或 重试间隔 (失败后 5 秒重试, 而非立即循环阻塞)
  unsigned long interval = (_retryCount > 0) ? RETRY_INTERVAL : READ_INTERVAL;

  if (currentTime - lastUpdate >= interval) {
    lastUpdate = currentTime;
    readDHT22();

    if (sensorAvailable) {
      INDOOR_TEMP = temperature;
      INDOOR_HUM = humidity;
      _retryCount = 0; // 重置重试计数
    } else {
      _retryCount++;
      if (_retryCount >= MAX_RETRIES) {
        _retryCount = 0; // 放弃重试，等待下一个正常周期
        Serial.println("[Periphery] DHT22 多次重试失败，等待下一周期");
      }
    }
  }
}

bool PeripheryManager_::isSensorAvailable() { return sensorAvailable; }

/**
 * @brief 非阻塞式 DHT22 读取 (单次)
 *
 * [性能优化] 移除了旧版的 for 循环 + delay() 重试机制
 *   旧实现: 失败 3 次 → 阻塞 3 × READ_INTERVAL = 6 秒
 *   新实现: 失败后返回，tick() 在下一个周期自动重试
 */
void PeripheryManager_::readDHT22() {
  float temp = dht->readTemperature();
  float hum = dht->readHumidity();

  if (!isnan(temp) && !isnan(hum) && temp >= -40 && temp <= 80 && hum >= 0 &&
      hum <= 100) {
    temperature = temp;
    humidity = hum;
    sensorAvailable = true;
  } else {
    sensorAvailable = false;
  }
}
