#ifndef PERIPHERY_MANAGER_H
#define PERIPHERY_MANAGER_H

#include <Arduino.h>
#include <DHT.h>

class PeripheryManager_ {
public:
  static PeripheryManager_ &getInstance();

  void setup();
  void tick();

  bool isSensorAvailable();

  /** 开启/关闭 LDR 自动亮度（默认关闭，由设置页或 WebSocket 指令控制） */
  void setAutoBrightness(bool enable);
  bool isAutoBrightnessEnabled() const { return _autoBrightness; }

  /** 获取最近一次 LDR 计算出的亮度值（0-255） */
  uint8_t getLdrBrightness() const { return _ldrBrightness; }

private:
  // DHT22 读取
  void readDHT22();

  // LDR 采样与自动亮度
  void updateLDR();

  // DHT22 实例
  DHT *dht = nullptr;

  // 传感器数据
  float temperature = 0.0f;
  float humidity = 0.0f;
  bool sensorAvailable = false;
  unsigned long lastUpdate = 0;
  uint8_t _retryCount = 0;

  // LDR 自动亮度
  bool _autoBrightness = false;
  uint8_t _ldrBrightness = 128;
  unsigned long _ldrLastUpdate = 0;

  // 滑动平均：环形缓冲 8 次采样
  static const uint8_t LDR_AVG_SIZE = 8;
  uint16_t _ldrSamples[LDR_AVG_SIZE] = {};
  uint8_t _ldrSampleIdx = 0;
  bool _ldrSamplesFull = false;

  // 传感器配置常量
  static const int DHT_PIN = 4;
  static const int DHT_TYPE = DHT22;
  static const unsigned long READ_INTERVAL = 2000;
  static const unsigned long RETRY_INTERVAL = 5000;
  static const uint8_t MAX_RETRIES = 3;

  // LDR 配置常量
  // GPIO34 是 ESP32 ADC1_CH6，输入专用引脚，不影响其他外设
  static const int LDR_PIN = 34;
  static const unsigned long LDR_INTERVAL = 200; // ms，采样间隔

  // 亮度映射范围
  static const uint8_t LDR_BRIGHT_MIN = 10; // 最暗环境下的最低亮度（防止全灭）
  static const uint8_t LDR_BRIGHT_MAX = 255; // 最亮环境下的最高亮度
};

extern PeripheryManager_ &PeripheryManager;

#endif
