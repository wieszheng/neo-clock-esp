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
  Serial.println("[Periphery] I2S 麦克风初始化完成");
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

bool PeripheryManager_::getSpectrumData(uint8_t *outputBands, int numBands) {
  if (!vReal || !vImag)
    return false;

  // 读取 I2S 数据
  size_t bytesRead = 0;
  int16_t i2sBuffer[FFT_SAMPLES];

  // 尝试读取，如果超时或失败则返回
  // 使用 portMAX_DELAY 阻塞直到填满缓冲区 (约 25ms @ 40kHz)
  esp_err_t err = i2s_read(I2S_PORT, (void *)i2sBuffer, sizeof(i2sBuffer),
                           &bytesRead, 100 / portTICK_PERIOD_MS);

  if (err != ESP_OK || bytesRead != sizeof(i2sBuffer)) {
    return false;
  }

  // 填充 FFT 输入
  for (int i = 0; i < FFT_SAMPLES; i++) {
    vReal[i] = (double)i2sBuffer[i];
    vImag[i] = 0;
  }

  // FFT 计算
  // arduinoFFT v1.x API
  FFT.Windowing(vReal, FFT_SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(vReal, vImag, FFT_SAMPLES, FFT_FORWARD);
  FFT.ComplexToMagnitude(vReal, vImag, FFT_SAMPLES);

  // 分频段处理 (简单的线性或对数分箱)
  // 忽略直流分量 (i=0) 和极低频
  int step = (FFT_SAMPLES / 2) / numBands;

  for (int i = 0; i < numBands; i++) {
    double sum = 0;
    int count = 0;

    // 简单线性平均（为了更好的效果，通常建议对数频率映射）
    // 这里为了性能先用线性
    for (int j = 0; j < step; j++) {
      int bin = 2 + i * step + j; // 从第2个bin开始 (忽略 DC 和 <40Hz)
      if (bin < FFT_SAMPLES / 2) {
        sum += vReal[bin];
        count++;
      }
    }

    if (count > 0)
      sum /= count;

    // 映射幅度到 0-255 (或矩阵高度)
    // 这里的 divisor 这里的 divisor 需要根据实际麦克风灵敏度调整
    // 假设 noise gate = FFT_NOISE
    if (sum < FFT_NOISE)
      sum = 0;
    else
      sum -= FFT_NOISE;

    double val = (sum / FFT_AMPLITUDE) * 255.0; // 归一化
    if (val > 255.0)
      val = 255.0;

    outputBands[i] = (uint8_t)val;
  }

  return true;
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
