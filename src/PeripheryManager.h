/**
 * @file PeripheryManager.h
 * @brief 外设管理器 — DHT22 传感器与 LDR 自动亮度
 *
 * 本文件定义：
 *   - PeripheryManager_ 单例类: 管理外设
 *   - DHT22 温湿度传感器读取
 *   - LDR 光敏电阻自动亮度调节
 *   - 传感器配置常量 (引脚定义等)
 */

#ifndef PERIPHERY_MANAGER_H
#define PERIPHERY_MANAGER_H

#include <Arduino.h>
#include <DHT.h>

// ==================================================================
// 外设管理器类
// ==================================================================

/**
 * @class PeripheryManager_
 * @brief 外设管理器
 *
 * 功能：
 *   - 管理 DHT22 温湿度传感器读取
 *   - 管理 LDR 光敏电阻自动亮度调节
 *   - 提供传感器状态查询接口
 */
class PeripheryManager_
{
public:
  /**
   * @brief 获取单例实例
   * @return PeripheryManager_ 单例引用
   */
  static PeripheryManager_ &getInstance();

  /**
   * @brief 初始化外设管理器
   *
   * 初始化 DHT22 传感器、LDR 采样缓冲区
   */
  void setup();

  /**
   * @brief 主循环 - 读取传感器、更新 LDR 亮度
   *
   * 非阻塞式读取，由 tick() 调度执行
   */
  void tick();

  /**
   * @brief 检查 DHT22 传感器是否可用
   * @return true=传感器可用, false=传感器故障或未连接
   */
  bool isSensorAvailable();

  // ==================================================================
  // LDR 自动亮度控制
  // ==================================================================

  /**
   * @brief 开启/关闭 LDR 自动亮度
   * @param enable true=开启, false=关闭
   *
   * 默认关闭，由设置页或 WebSocket 指令控制
   */
  void setAutoBrightness(bool enable);

  /**
   * @brief 获取最近一次 LDR 计算出的亮度值
   * @return 亮度值 (0-255)
   */
  uint8_t getLdrBrightness() const { return _ldrBrightness; }

private:
  // ==================================================================
  // DHT22 传感器相关
  // ==================================================================

  /// DHT22 读取
  void readDHT22();

  // ==================================================================
  // LDR 自动亮度相关
  // ==================================================================

  /// LDR 采样与自动亮度计算
  void updateLDR();

  // ==================================================================
  // 成员变量
  // ==================================================================

  /// DHT22 实例
  DHT *dht = nullptr;

  // 传感器数据
  float temperature = 0.0f;
  float humidity = 0.0f;
  bool sensorAvailable = false;
  unsigned long lastUpdate = 0;
  uint8_t _retryCount = 0;

  // LDR 自动亮度
  uint8_t _ldrBrightness = 128;
  unsigned long _ldrLastUpdate = 0;

  // 滑动平均：环形缓冲 8 次采样
  static const uint8_t LDR_AVG_SIZE = 8;
  uint16_t _ldrSamples[LDR_AVG_SIZE] = {};
  uint8_t _ldrSampleIdx = 0;
  bool _ldrSamplesFull = false;

  // 自适应校准：记录观测到的 ADC 最小/最大值，用于动态映射以提高灵敏度
  uint16_t _ldrObservedMin = 4095;
  uint16_t _ldrObservedMax = 0;

  // 若为 true 则对映射取反（适用于接法导致 ADC 与光照反向的情况）
  bool _ldrInvert = true;

  // 传感器配置常量
  static const int DHT_PIN = 4;
  static const int DHT_TYPE = DHT22;
  static const unsigned long READ_INTERVAL = 2000;
  static const unsigned long RETRY_INTERVAL = 5000;
  static const uint8_t MAX_RETRIES = 3;

  // LDR 配置常量
  // GPIO34 是 ESP32 ADC1_CH6，输入专用引脚，不影响其他外设
  static const int LDR_PIN = 34;
  static const unsigned long LDR_INTERVAL = 1000; // ms，采样间隔

  // 亮度映射范围
  static const uint8_t LDR_BRIGHT_MIN = 6;   // 最暗环境下的最低亮度（防止全灭）
  static const uint8_t LDR_BRIGHT_MAX = 100; // 最亮环境下的最高亮度
};

extern PeripheryManager_ &PeripheryManager;

#endif
