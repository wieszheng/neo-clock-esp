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
class PeripheryManager_ {
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
   * @brief 检查自动亮度是否开启
   * @return true=已开启, false=已关闭
   */
  bool isAutoBrightnessEnabled() const { return _autoBrightness; }

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
  // 传感器配置常量
  // ==================================================================

  /// DHT22 传感器引脚
  static const int DHT_PIN = 4;

  /// DHT22 传感器类型
  static const int DHT_TYPE = DHT22;

  /// DHT22 正常读取间隔 (毫秒)
  static const unsigned long READ_INTERVAL = 2000;

  /// DHT22 读取失败重试间隔 (毫秒)
  static const unsigned long RETRY_INTERVAL = 5000;

  /// DHT22 最大重试次数
  static const uint8_t MAX_RETRIES = 3;

  // ==================================================================
  // LDR 配置常量
  // ==================================================================

  /// LDR 光敏电阻引脚 (ESP32 ADC1_CH6，输入专用引脚)
  static constexpr int LDR_PIN = 34;

  /// LDR 采样间隔 (毫秒)
  static constexpr uint32_t LDR_INTERVAL = 200;

  /// 滑动平均缓冲区大小
  static const uint8_t LDR_AVG_SIZE = 8;

  /// LDR 极值衰减间隔 (每分钟衰减一次，适应环境变化)
  static constexpr uint32_t LDR_DECAY_INTERVAL = 60000;

  /// 全暗环境最低亮度 (防止完全熄灭)
  static constexpr uint8_t LDR_BRIGHT_MIN = 10;

  /// 全亮环境最高亮度
  static constexpr uint8_t LDR_BRIGHT_MAX = 255;

  // ==================================================================
  // 成员变量
  // ==================================================================

  /// DHT22 实例
  DHT *dht = nullptr;

  // DHT22 传感器数据
  float temperature = 0.0f;           ///< 温度 (°C)
  float humidity = 0.0f;             ///< 湿度 (%)
  bool sensorAvailable = false;        ///< 传感器是否可用
  unsigned long lastUpdate = 0;       ///< 上次更新时间
  uint8_t _retryCount = 0;           ///< 当前重试次数

  // LDR 自动亮度
  bool _autoBrightness = false;        ///< 是否开启自动亮度
  uint8_t _ldrBrightness = 128;      ///< LDR 计算出的亮度值
  unsigned long _ldrLastUpdate = 0;   ///< 上次 LDR 采样时间

  // LDR 滑动平均缓冲区 (环形缓冲)
  uint16_t _ldrSamples[LDR_AVG_SIZE] = {};
  uint8_t _ldrSampleIdx = 0;
  bool _ldrSamplesFull = false;

  // LDR 动态 ADC 极值 (自动学习环境光线范围)
  uint16_t _ldrAdcMin = 4095;       ///< 初始为最大值，方便向下学习
  uint16_t _ldrAdcMax = 0;          ///< 初始为最小值，方便向上学习
  unsigned long _ldrLastDecay = 0;   ///< 上次极值衰减时间
};

extern PeripheryManager_ &PeripheryManager;

#endif
