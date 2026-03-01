/**
 * @file PeripheryManager.cpp
 * @brief 外设管理器 — DHT22 温湿度传感器 + LDR 5228 自动亮度
 *
 * LDR 自动亮度原理:
 *   - LDR 5228 一端接 3.3V，另一端经 10kΩ 下拉电阻接 GND，中点接 GPIO34
 *   - 光线增强 → LDR 阻值下降 → GPIO34 电压升高 → ADC 读数增大 → 亮度提高
 *   - 采用对数映射将 ADC 值(0-4095)映射到亮度(LDR_BRIGHT_MIN~LDR_BRIGHT_MAX)
 *   - 8 次滑动平均消除抖动，每 200ms 采样一次，完全非阻塞
 *   - 仅在 AUTO_BRIGHTNESS 开启时生效，不修改全局 BRIGHTNESS 变量，
 *     避免干扰用户手动亮度设置的保存/加载逻辑
 */

#include "PeripheryManager.h"
#include "DisplayManager.h"
#include "Globals.h"
#include <math.h>

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
  Serial.println("[Periphery] 初始化外设管理器...");

  // --- DHT22 ---
  dht = new DHT(DHT_PIN, DHT_TYPE);
  dht->begin();

  temperature = 0.0f;
  humidity = 0.0f;
  sensorAvailable = false;
  lastUpdate = 0;
  _retryCount = 0;

  // --- LDR ---
  // GPIO34 为 ADC 专用输入引脚，无需 pinMode 设置
  memset(_ldrSamples, 0, sizeof(_ldrSamples));
  _ldrSampleIdx = 0;
  _ldrSamplesFull = false;
  _ldrLastUpdate = 0;
  _ldrBrightness = BRIGHTNESS; // 初始值跟随全局设置

  // 首次读取 DHT22
  Serial.println("[Periphery] 测试读取DHT22传感器...");
  readDHT22();

  if (sensorAvailable)
  {
    Serial.printf("[Periphery] DHT22 初始化成功: %.1f°C, %.1f%%\n", temperature,
                  humidity);
    INDOOR_TEMP = temperature;
    INDOOR_HUM = humidity;
  }
  else
  {
    Serial.println("[Periphery] 警告: DHT22 传感器读取失败");
  }

  Serial.println("[Periphery] 外设管理器初始化完成");
}

// ==================================================================
// 自动亮度开关
// ==================================================================
void PeripheryManager_::setAutoBrightness(bool enable)
{
  if (!enable)
  {
    // 关闭时恢复全局亮度设置
    DisplayManager.setBrightness(BRIGHTNESS);
    Serial.printf("[Periphery] LDR 自动亮度已关闭，恢复手动亮度: %d\n",
                  BRIGHTNESS);
  }
  else
  {
    Serial.println("[Periphery] LDR 自动亮度已开启");
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
        Serial.println("[Periphery] DHT22 多次重试失败，等待下一周期");
      }
    }
  }

  // --- LDR 自动亮度（200ms 采样）---
  if (now - _ldrLastUpdate >= LDR_INTERVAL)
  {
    _ldrLastUpdate = now;
    updateLDR();
  }
}

// ==================================================================
// LDR 5228 采样与亮度映射
// ==================================================================
void PeripheryManager_::updateLDR()
{
  // 读取 ADC（0~4095，12bit）
  uint16_t raw = (uint16_t)analogRead(LDR_PIN);

  // 写入环形缓冲
  _ldrSamples[_ldrSampleIdx] = raw;
  _ldrSampleIdx = (_ldrSampleIdx + 1) % LDR_AVG_SIZE;
  if (_ldrSampleIdx == 0)
    _ldrSamplesFull = true;

  // 计算滑动平均
  uint8_t validCount = _ldrSamplesFull ? LDR_AVG_SIZE : _ldrSampleIdx;
  if (validCount == 0)
    return;

  uint32_t sum = 0;
  for (uint8_t i = 0; i < validCount; i++)
    sum += _ldrSamples[i];
  uint16_t avg = (uint16_t)(sum / validCount);

  // 原始对数映射备用（用于未校准或范围太窄时）
  float logMax = log1pf(4095.0f);
  float logVal = log1pf((float)avg);
  float logRatio = logVal / logMax; // 0.0 ~ 1.0

  // 更新观测到的 min/max（用于自适应校准）
  if (avg < _ldrObservedMin)
    _ldrObservedMin = avg;
  if (avg > _ldrObservedMax)
    _ldrObservedMax = avg;

  // 自适应线性映射：当观测到的范围足够大时使用线性映射提高暗区响应
  const uint16_t LDR_CAL_MIN_RANGE = 200; // 需要的最小 ADC 范围才能激活校准
  float ratio = logRatio;
  bool usedCal = false;
  if (_ldrObservedMax > _ldrObservedMin && (_ldrObservedMax - _ldrObservedMin) >= LDR_CAL_MIN_RANGE)
  {
    float lin = (float)(avg - _ldrObservedMin) / (float)(_ldrObservedMax - _ldrObservedMin);
    if (lin < 0.0f)
      lin = 0.0f;
    if (lin > 1.0f)
      lin = 1.0f;
    ratio = lin;
    usedCal = true;
  }

  // 支持反转映射：若硬件接法导致 ADC 随光变小则开启反转
  if (_ldrInvert)
  {
    ratio = 1.0f - ratio;
  }

  // 对比率做 gamma 校正以提高响应灵敏度，gamma<1 会放大变化
  const float LDR_GAMMA = 0.7f; // 调整此值以调节灵敏度（0.4-1.0 为常用范围）
  float adjRatio = powf(ratio, LDR_GAMMA);

  uint8_t brightness =
      (uint8_t)(LDR_BRIGHT_MIN + adjRatio * (LDR_BRIGHT_MAX - LDR_BRIGHT_MIN));

  // 日志：打印原始 ADC、平均值、原始比率/线性比率、校正后比率、gamma、计算亮度、校准信息及当前状态
  // Serial.printf("[Periphery] LDR raw=%u avg=%u ratio=%.3f adj=%.3f gamma=%.2f calc=%u cur=%u auto=%d MATRIX_OFF=%d calMin=%u calMax=%u usedCal=%d invert=%d\n",
  //               raw, avg, logRatio, adjRatio, LDR_GAMMA, brightness, _ldrBrightness,
  //               AUTO_BRIGHTNESS ? 1 : 0, MATRIX_OFF ? 1 : 0, _ldrObservedMin, _ldrObservedMax, usedCal ? 1 : 0, _ldrInvert ? 1 : 0);

  // 滞回：变化超过 3 个亮度单位才更新，避免频繁调整
  int diff = (int)brightness - (int)_ldrBrightness;
  if (abs(diff) >= 2)
  {
    uint8_t old = _ldrBrightness;
    _ldrBrightness = brightness;

    Serial.printf("[Periphery] LDR brightness change: %u -> %u (diff=%d)\n",
                  old, _ldrBrightness, diff);

    if (AUTO_BRIGHTNESS && !MATRIX_OFF)
    {
      DisplayManager.setBrightness(_ldrBrightness);
    }
  }
}

// ==================================================================
// DHT22 读取（非阻塞单次，失败由 tick() 下一周期重试）
// ==================================================================
void PeripheryManager_::readDHT22()
{
  float temp = dht->readTemperature();
  float hum = dht->readHumidity();

  if (!isnan(temp) && !isnan(hum) && temp >= -40 && temp <= 80 && hum >= 0 &&
      hum <= 100)
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

bool PeripheryManager_::isSensorAvailable() { return sensorAvailable; }
