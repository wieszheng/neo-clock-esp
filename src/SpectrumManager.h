/**
 * @file SpectrumManager.h
 * @brief 频谱分析管理器 — FFT 分析、频段处理、幅度平滑
 *
 * 本文件定义：
 *   - SpectrumManager_ 单例类: 管理 FFT 频谱分析
 *   - 频段划分与映射
 *   - 幅度平滑与衰减
 *   - 频谱可视化数据输出
 */

#ifndef SPECTRUM_MANAGER_H
#define SPECTRUM_MANAGER_H

#include <Arduino.h>

// ==================================================================
// 频谱配置常量
// ==================================================================

/// 频谱条数量 (32×8 矩阵，每列一个频段)
#define SPECTRUM_BARS 32

/// 频谱高度 (8 行 LED)
#define SPECTRUM_HEIGHT 8

/// FFT 样本大小
#define FFT_SIZE 128

/// 频谱平滑系数 (0.0-1.0，越大越平滑)
#define SPECTRUM_SMOOTHING 0.6f

/// 频谱衰减系数 (0.0-1.0，越大衰减越快)
#define SPECTRUM_DECAY 0.85f

/// 最小灵敏度阈值 (低于此值视为静音)
#define SPECTRUM_THRESHOLD 100

// ==================================================================
// 频谱渲染模式
// ==================================================================

/// 频谱渲染模式枚举
enum SpectrumMode
{
  SPECTRUM_MODE_BAR = 0,   ///< 条形图模式 (经典频谱)
  SPECTRUM_MODE_DOT = 1,    ///< 点阵图模式 (LED 矩阵风格)
  SPECTRUM_MODE_MIRROR = 2, ///< 镜像模式 (中心对称，默认)
  SPECTRUM_MODE_WAVE = 3    ///< 波形模式 (平滑曲线)
};

// ==================================================================
// 频谱管理器类
// ==================================================================

/**
 * @class SpectrumManager_
 * @brief 频谱分析管理器
 *
 * 功能：
 *   - FFT 频谱分析
 *   - 频段划分与映射
 *   - 幅度平滑与衰减
 *   - 频谱数据输出
 *   - 渲染模式切换
 */
class SpectrumManager_
{
public:
  /**
   * @brief 获取单例实例
   * @return SpectrumManager_ 单例引用
   */
  static SpectrumManager_ &getInstance();

  /**
   * @brief 初始化频谱分析
   *
   * 初始化 FFT 库，配置频段映射
   */
  void setup();

  /**
   * @brief 主循环 - 更新频谱数据
   *
   * 执行 FFT 分析，更新频谱数据
   * @param audioBuffer 音频数据指针
   * @param bufferSize 音频数据大小
   */
  void update(const int16_t *audioBuffer, size_t bufferSize);

  /**
   * @brief 获取频谱条高度数组
   * @return 频谱条高度数组 (uint8_t[SPECTRUM_BARS])
   */
  const uint8_t *getBarHeights() const { return _barHeights; }

  /**
   * @brief 获取峰值数组
   * @return 峰值数组 (uint8_t[SPECTRUM_BARS])
   */
  const uint8_t *getPeaks() const { return _peaks; }

  /**
   * @brief 设置渲染模式
   * @param mode 渲染模式
   */
  void setMode(SpectrumMode mode) { _mode = mode; }

  /**
   * @brief 获取当前渲染模式
   * @return 当前渲染模式
   */
  SpectrumMode getMode() const { return _mode; }

  /**
   * @brief 设置灵敏度
   * @param sensitivity 灵敏度值 (1-10)
   */
  void setSensitivity(uint8_t sensitivity);

  /**
   * @brief 设置平滑系数
   * @param smoothing 平滑系数 (0.0-1.0)
   */
  void setSmoothing(float smoothing);

  /**
   * @brief 设置衰减系数
   * @param decay 衰减系数 (0.0-1.0)
   */
  void setDecay(float decay);

private:
  SpectrumManager_() {} ///< 私有构造函数

  /// FFT 分析
  void computeFFT(const int16_t *audioBuffer);

  /// 频段映射
  void mapFrequencies();

  /// 平滑与衰减
  void smoothAndDecay();

  /// 汉宁窗函数
  float hanningWindow(int n, int N);

  /// 成员变量
  double _fftReal[FFT_SIZE];
  double _fftImag[FFT_SIZE];
  double _fftMagnitudes[FFT_SIZE / 2];

  uint8_t _barHeights[SPECTRUM_BARS] = {0};
  uint8_t _peaks[SPECTRUM_BARS] = {0};
  uint8_t _previousHeights[SPECTRUM_BARS] = {0};

  SpectrumMode _mode = SPECTRUM_MODE_MIRROR;
  float _smoothing = SPECTRUM_SMOOTHING;
  float _decay = SPECTRUM_DECAY;
  uint8_t _sensitivity = 5;

  // 频段映射索引 - 使用对数刻度将 FFT 结果映射到 16 个频段
  static const uint8_t _bandStart[16];
  static const uint8_t _bandEnd[16];
};

extern SpectrumManager_ &SpectrumManager;

#endif
