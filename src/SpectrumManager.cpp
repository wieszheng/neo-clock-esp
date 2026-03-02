/**
 * @file SpectrumManager.cpp
 * @brief 频谱分析管理器实现 — FFT 分析、频段处理、幅度平滑
 *
 * 频谱分析原理:
 *   - 使用 FFT 将时域音频转换为频域
 *   - 将 FFT 结果映射到 32 个频段
 *   - 应用平滑和衰减算法减少闪烁
 *   - 计算峰值显示效果
 *
 * 使用 ArduinoFFT 库: kosme/arduinoFFT@^2.0.4
 */

#include "SpectrumManager.h"
#include "Logger.h"
#include "arduinoFFT.h"

// ==================================================================
// 频段映射表
// ==================================================================

// 使用对数刻度将 FFT 结果映射到 16 个频段
// 低频区域使用更多 FFT 点，高频区域合并
const uint8_t SpectrumManager_::_bandStart[16] = {
    1, 2, 3, 4, 5, 6, 7, 9,
    11, 13, 15, 17, 20, 23, 26, 29};

const uint8_t SpectrumManager_::_bandEnd[16] = {
    2, 3, 4, 5, 6, 7, 9, 11,
    13, 15, 17, 20, 23, 26, 29, 32};

// ==================================================================
// 单例
// ==================================================================

SpectrumManager_ &SpectrumManager_::getInstance()
{
  static SpectrumManager_ instance;
  return instance;
}

SpectrumManager_ &SpectrumManager = SpectrumManager.getInstance();

// ==================================================================
// 初始化
// ==================================================================

void SpectrumManager_::setup()
{
  LOG_INFO("[Spectrum] 初始化频谱分析管理器...");

  memset(_fftReal, 0, sizeof(_fftReal));
  memset(_fftImag, 0, sizeof(_fftImag));
  memset(_fftMagnitudes, 0, sizeof(_fftMagnitudes));
  memset(_barHeights, 0, sizeof(_barHeights));
  memset(_peaks, 0, sizeof(_peaks));
  memset(_previousHeights, 0, sizeof(_previousHeights));

  LOG_INFO("[Spectrum] 频谱分析管理器初始化完成");
}

// ==================================================================
// 主循环
// ==================================================================

void SpectrumManager_::update(const int16_t *audioBuffer, size_t bufferSize)
{
  if (!audioBuffer || bufferSize < FFT_SIZE)
    return;

  computeFFT(audioBuffer);
  mapFrequencies();
  smoothAndDecay();
}

// ==================================================================
// FFT 分析
// ==================================================================

void SpectrumManager_::computeFFT(const int16_t *audioBuffer)
{
  // 准备 FFT 数据 (将 int16_t 转换为 double)
  for (int i = 0; i < FFT_SIZE; i++)
  {
    // 应用汉宁窗减少频谱泄漏
    float window = hanningWindow(i, FFT_SIZE);
    _fftReal[i] = (double)audioBuffer[i] * window;
    _fftImag[i] = 0.0;
  }

  // 创建 FFT 实例并计算
  arduinoFFT FFT = arduinoFFT(_fftReal, _fftImag, FFT_SIZE, AUDIO_SAMPLE_RATE);

  FFT.DCRemoval();
  FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(FFT_FORWARD);
  FFT.ComplexToMagnitude();

  // 提取幅度 (只取前一半，对称的)
  for (int i = 0; i < FFT_SIZE / 2; i++)
  {
    _fftMagnitudes[i] = _fftReal[i];
  }
}

// ==================================================================
// 频段映射
// ==================================================================

void SpectrumManager_::mapFrequencies()
{
  // 将 FFT 结果映射到 32 个频段
  // 使用镜像：前 16 个频段从左到右，后 16 个频段从右到左
  for (int i = 0; i < 16; i++)
  {
    // 计算该频段的平均幅度
    double sum = 0;
    int count = 0;

    for (int j = _bandStart[i]; j < _bandEnd[i] && j < FFT_SIZE / 2; j++)
    {
      sum += _fftMagnitudes[j];
      count++;
    }

    if (count > 0)
    {
      double avg = sum / count;

      // 应用灵敏度调整
      float sensitivityFactor = 0.5f + (_sensitivity * 0.1f);
      avg *= sensitivityFactor;

      // 映射到 0-8 高度
      uint8_t height = (uint8_t)(avg / SPECTRUM_THRESHOLD);
      if (height > SPECTRUM_HEIGHT)
        height = SPECTRUM_HEIGHT;

      // 左侧频段
      _barHeights[i] = height;
      // 右侧镜像频段
      _barHeights[31 - i] = height;
    }
  }
}

// ==================================================================
// 平滑与衰减
// ==================================================================

void SpectrumManager_::smoothAndDecay()
{
  for (int i = 0; i < SPECTRUM_BARS; i++)
  {
    // 平滑处理
    _barHeights[i] = (uint8_t)(_previousHeights[i] * _smoothing +
                                _barHeights[i] * (1.0f - _smoothing));

    // 峰值处理
    if (_barHeights[i] > _peaks[i])
    {
      _peaks[i] = _barHeights[i];
    }
    else
    {
      // 峰值衰减
      _peaks[i] = (uint8_t)(_peaks[i] * _decay);
      if (_peaks[i] < _barHeights[i])
        _peaks[i] = _barHeights[i];
    }

    // 保存当前值用于下次平滑
    _previousHeights[i] = _barHeights[i];
  }
}

// ==================================================================
// 汉宁窗函数
// ==================================================================

float SpectrumManager_::hanningWindow(int n, int N)
{
  return 0.5f * (1.0f - cosf(2.0f * PI * n / (N - 1)));
}

// ==================================================================
// 参数设置
// ==================================================================

void SpectrumManager_::setSensitivity(uint8_t sensitivity)
{
  // 限制在 1-10 范围
  if (sensitivity < 1)
    sensitivity = 1;
  if (sensitivity > 10)
    sensitivity = 10;
  _sensitivity = sensitivity;
  LOG_DEBUG("[Spectrum] 灵敏度设置为: %d", _sensitivity);
}

void SpectrumManager_::setSmoothing(float smoothing)
{
  // 限制在 0.0-1.0 范围
  if (smoothing < 0.0f)
    smoothing = 0.0f;
  if (smoothing > 1.0f)
    smoothing = 1.0f;
  _smoothing = smoothing;
  LOG_DEBUG("[Spectrum] 平滑系数设置为: %.2f", _smoothing);
}

void SpectrumManager_::setDecay(float decay)
{
  // 限制在 0.0-1.0 范围
  if (decay < 0.0f)
    decay = 0.0f;
  if (decay > 1.0f)
    decay = 1.0f;
  _decay = decay;
  LOG_DEBUG("[Spectrum] 衰减系数设置为: %.2f", _decay);
}
