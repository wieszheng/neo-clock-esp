/**
 * @file AudioManager.h
 * @brief 音频采集管理器 — INMP441 麦克风 I2S 音频数据采集
 *
 * 本文件定义：
 *   - AudioManager_ 单例类: 管理 I2S 音频采集
 *   - 音频缓冲区管理
 *   - 非阻塞音频数据读取
 *
 * 硬件连接 (INMP441):
 *   - SCK (BCLK) -> GPIO15
 *   - WS (LRCK)  -> GPIO13
 *   - SD (DIN)   -> GPIO14
 *   - VDD        -> 3.3V
 *   - GND        -> GND
 *   - L/R        -> GND (左声道)
 */

#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>
#include <driver/i2s.h>

// ==================================================================
// I2S 引脚配置
// ==================================================================

/// I2S 位时钟引脚 (BCLK)
#define I2S_BCLK_PIN 15

/// I2S 左右声道选择引脚 (LRCK/WS)
#define I2S_WS_PIN 13

/// I2S 数据输入引脚 (SD/DIN)
#define I2S_SD_PIN 14

// ==================================================================
// 音频配置常量
// ==================================================================

/// I2S 采样率 (Hz) - 16kHz 足够音乐频谱分析，降低 CPU 负载
#define AUDIO_SAMPLE_RATE 16000

/// FFT 样本大小 (必须是 2 的幂)
#define FFT_SIZE 128

/// 音频缓冲区大小 (FFT_SIZE * 2 以确保有足够数据)
#define AUDIO_BUFFER_SIZE (FFT_SIZE * 2)

// ==================================================================
// 音频管理器类
// ==================================================================

/**
 * @class AudioManager_
 * @brief 音频采集管理器
 *
 * 功能：
 *   - I2S 音频数据采集
 *   - 非阻塞数据读取
 *   - 提供原始音频数据缓冲区
 *   - 音频初始化与错误处理
 */
class AudioManager_
{
public:
  /**
   * @brief 获取单例实例
   * @return AudioManager_ 单例引用
   */
  static AudioManager_ &getInstance();

  /**
   * @brief 初始化音频采集
   *
   * 配置 I2S 驱动，初始化音频采集
   */
  void setup();

  /**
   * @brief 主循环 - 采集音频数据
   *
   * 非阻塞式采集，填充音频缓冲区
   */
  void tick();

  /**
   * @brief 检查是否有新的音频数据可用
   * @return true=有新数据, false=无新数据
   */
  bool hasNewData() const { return _hasNewData; }

  /**
   * @brief 获取音频数据指针
   * @return 音频缓冲区指针 (int16_t[FFT_SIZE])
   */
  const int16_t *getAudioBuffer() const { return _audioBuffer; }

  /**
   * @brief 重置新数据标志
   */
  void resetNewDataFlag() { _hasNewData = false; }

  /**
   * @brief 检查音频采集是否正常工作
   * @return true=正常, false=故障
   */
  bool isAudioAvailable() const { return _audioAvailable; }

private:
  AudioManager_() {} ///< 私有构造函数

  /// I2S 配置
  void configureI2S();

  /// 成员变量
  i2s_port_t _i2sPort = I2S_NUM_0;

  /// 音频缓冲区
  int16_t _audioBuffer[FFT_SIZE] = {0};
  int16_t _readBuffer[AUDIO_BUFFER_SIZE] = {0};

  /// 状态标志
  bool _audioAvailable = false;
  bool _hasNewData = false;
  unsigned long _lastReadTime = 0;
};

extern AudioManager_ &AudioManager;

#endif
