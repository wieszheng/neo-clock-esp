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

  // 麦克风与频谱分析
  void initMic();
  bool getSpectrumData(uint8_t *outputBands, int numBands);

private:
  // DHT22传感器配置
  void readDHT22();

  // I2S 配置
  void initI2S();
  void sampleAudio();
  void computeFFT();

  // DHT22实例
  DHT *dht;

  // 传感器数据缓存
  float temperature;
  float humidity;
  bool sensorAvailable;
  unsigned long lastUpdate;
  uint8_t _retryCount;
  ;

  // 音频数据缓存
  double *vReal;
  double *vImag;

  // 传感器配置常量
  static const int DHT_PIN = 4;
  static const int DHT_TYPE = DHT22;
  static const unsigned long READ_INTERVAL = 2000;
  static const unsigned long RETRY_INTERVAL = 5000;
  static const uint8_t MAX_RETRIES = 3;
};

extern PeripheryManager_ &PeripheryManager;

#endif
