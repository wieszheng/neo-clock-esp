/**
 * @file PeripheryManager.cpp
 * @brief 外设管理器 — DHT22 温湿度传感器 + LDR 5228 自动亮度 + 按钮
 *
 * LDR 自动亮度原理:
 *   - LDR 5228 一端接 3.3V，另一端经 10kΩ 下拉电阻接 GND，中点接 GPIO34
 *   - 光线增强 → LDR 阻值下降 → GPIO34 电压升高 → ADC 读数增大 → 亮度提高
 *   - 采用对数映射将 ADC 值(0-4095)映射到亮度(LDR_BRIGHT_MIN~LDR_BRIGHT_MAX)
 *   - 8 次滑动平均消除抖动，每 200ms 采样一次，完全非阻塞
 */

#include "PeripheryManager.h"
#include "DisplayManager.h"
#include "Globals.h"
#include "Logger.h"
#include <math.h>

// ==================================================================
// 全局按钮实例
// ==================================================================

// TTP223 触摸按钮：输出 HIGH 表示按下，需要启用内部下拉电阻
EasyButton button_left(BUTTON_LEFT_PIN, true, true);
EasyButton button_right(BUTTON_RIGHT_PIN, true, true);
EasyButton button_back(BUTTON_BACK_PIN, true, true);
EasyButton button_ok(BUTTON_OK_PIN, true, true);

// ==================================================================
// 按钮回调函数
// ==================================================================

void button_left_pressed()
{
  LOG_DEBUG("[Button] 左按钮按下");
  DisplayManager.leftButton();
}

void button_right_pressed()
{
  LOG_DEBUG("[Button] 右按钮按下");
  DisplayManager.rightButton();
}

void button_back_pressed()
{
  LOG_DEBUG("[Button] 返回按钮按下");
  // 可自定义功能
}

void button_ok_pressed()
{
  LOG_DEBUG("[Button] 确认按钮按下");
  DisplayManager.selectButton();
}

// ==================================================================
// 单例
// ==================================================================

PeripheryManager_ &PeripheryManager_::getInstance()
{
  static PeripheryManager_ instance;
  return instance;
}

PeripheryManager_ &PeripheryManager = PeripheryManager_::getInstance();

// ==================================================================
// 初始化
// ==================================================================

void PeripheryManager_::setup()
{
  LOG_INFO("[Periphery] 初始化外设管理器...");

  // --- DHT22 ---
  dht = new DHT(DHT_PIN, DHT_TYPE);
  dht->begin();

  temperature = 0.0f;
  humidity = 0.0f;
  sensorAvailable = false;
  lastUpdate = 0;
  _retryCount = 0;

  // --- LDR ---
  memset(_ldrSamples, 0, sizeof(_ldrSamples));
  _ldrSampleIdx = 0;
  _ldrSamplesFull = false;
  _ldrLastUpdate = 0;
  _ldrBrightness = BRIGHTNESS;

  // --- 按钮 ---
  button_left.begin();
  button_right.begin();
  button_back.begin();
  button_ok.begin();

  button_left.onPressed(button_left_pressed);
  button_right.onPressed(button_right_pressed);
  button_back.onPressed(button_back_pressed);
  button_ok.onPressed(button_ok_pressed);

  LOG_INFO("[Periphery] 按钮初始化完成");

  // 首次读取 DHT22
  LOG_INFO("[Periphery] 测试读取DHT22传感器...");
  readDHT22();

  if (sensorAvailable)
  {
    LOG_INFO("[Periphery] DHT22 初始化成功: %.1f°C, %.1f%%", temperature, humidity);
    INDOOR_TEMP = temperature;
    INDOOR_HUM = humidity;
  }
  else
  {
    LOG_WARN("[Periphery] DHT22 传感器读取失败");
  }

  LOG_INFO("[Periphery] 外设管理器初始化完成");
}

// ==================================================================
// 自动亮度开关
// ==================================================================

void PeripheryManager_::setAutoBrightness(bool enable)
{
  if (!enable)
  {
    DisplayManager.setBrightness(BRIGHTNESS);
    LOG_INFO("[Periphery] LDR 自动亮度已关闭，恢复手动亮度: %d", BRIGHTNESS);
  }
  else
  {
    LOG_INFO("[Periphery] LDR 自动亮度已开启");
  }
}

// ==================================================================
// 主循环
// ==================================================================

void PeripheryManager_::tick()
{
  unsigned long now = millis();

  // --- DHT22 读取（2s / 5s 重试间隔）---
  unsigned long interval = (_retryCount > 0) ? RETRY_INTERVAL : READ_INTERVAL;
  if (now - lastUpdate >= interval)
  {
    lastUpdate = now;
    readDHT22();

    if (sensorAvailable)
    {
      INDOOR_TEMP = temperature;
      INDOOR_HUM = humidity;
      _retryCount = 0;
    }
    else
    {
      _retryCount++;
      if (_retryCount >= MAX_RETRIES)
      {
        _retryCount = 0;
        LOG_WARN("[Periphery] DHT22 多次重试失败，等待下一周期");
      }
    }
  }

  // --- LDR 自动亮度 ---
  if (now - _ldrLastUpdate >= LDR_INTERVAL)
  {
    _ldrLastUpdate = now;
    updateLDR();
  }

  // --- 按钮读取 ---
  button_left.read();
  button_right.read();
  button_back.read();
  button_ok.read();
}

// ==================================================================
// LDR 采样与亮度映射
// ==================================================================

void PeripheryManager_::updateLDR()
{
  uint16_t raw = (uint16_t)analogRead(LDR_PIN);

  _ldrSamples[_ldrSampleIdx] = raw;
  _ldrSampleIdx = (_ldrSampleIdx + 1) % LDR_AVG_SIZE;
  if (_ldrSampleIdx == 0)
    _ldrSamplesFull = true;

  uint8_t validCount = _ldrSamplesFull ? LDR_AVG_SIZE : _ldrSampleIdx;
  if (validCount == 0)
    return;

  uint32_t sum = 0;
  for (uint8_t i = 0; i < validCount; i++)
    sum += _ldrSamples[i];
  uint16_t avg = (uint16_t)(sum / validCount);

  float logMax = log1pf(4095.0f);
  float logVal = log1pf((float)avg);
  float logRatio = logVal / logMax;

  if (avg < _ldrObservedMin)
    _ldrObservedMin = avg;
  if (avg > _ldrObservedMax)
    _ldrObservedMax = avg;

  const uint16_t LDR_CAL_MIN_RANGE = 200;
  float ratio = logRatio;
  if (_ldrObservedMax > _ldrObservedMin && (_ldrObservedMax - _ldrObservedMin) >= LDR_CAL_MIN_RANGE)
  {
    float lin = (float)(avg - _ldrObservedMin) / (float)(_ldrObservedMax - _ldrObservedMin);
    if (lin < 0.0f)
      lin = 0.0f;
    if (lin > 1.0f)
      lin = 1.0f;
    ratio = lin;
  }

  if (_ldrInvert)
  {
    ratio = 1.0f - ratio;
  }

  const float LDR_GAMMA = 0.7f;
  float adjRatio = powf(ratio, LDR_GAMMA);

  uint8_t brightness =
      (uint8_t)(LDR_BRIGHT_MIN + adjRatio * (LDR_BRIGHT_MAX - LDR_BRIGHT_MIN));

  int diff = (int)brightness - (int)_ldrBrightness;
  if (abs(diff) >= 2)
  {
    _ldrBrightness = brightness;

    if (AUTO_BRIGHTNESS && !MATRIX_OFF)
    {
      DisplayManager.setBrightness(_ldrBrightness);
    }
  }
}

// ==================================================================
// DHT22 传感器读取
// ==================================================================

void PeripheryManager_::readDHT22()
{
  float temp = dht->readTemperature();
  float hum = dht->readHumidity();

  if (!isnan(temp) && !isnan(hum) && temp >= -40 && temp <= 80 && hum >= 0 && hum <= 100)
  {
    temperature = temp;
    humidity = hum;
    sensorAvailable = true;
  }
  else
  {
    sensorAvailable = false;
  }
}

bool PeripheryManager_::isSensorAvailable()
{
  return sensorAvailable;
}
