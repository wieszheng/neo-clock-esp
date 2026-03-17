/**
 * @file Apps.cpp
 * @brief 应用函数实现 — 时间、日期、天气等显示应用
 */

#include "Apps.h"
#include "DisplayManager.h"
#include "Globals.h"
#include "Tools.h"
#include <arduinoFFT.h>
#include <time.h>

// ==================================================================
// 全局应用列表
// ==================================================================

std::vector<AppData> Apps;

// 覆盖层回调数组
OverlayCallback overlays[] = {SpectrumOverlay};

// ==================================================================
// FFT 变量 (频谱覆盖层专用)
// ==================================================================

#define FFT_SAMPLES 512
#define SAMPLING_FREQ 10000
#define SAMPLING_PERIOD_US (1000000UL / SAMPLING_FREQ)
#define AMPLITUDE 300 // 灵敏度控制 (越小越灵敏)
#define NUM_BANDS 32  // 32条频谱，对应32列
#define NOISE 220     // 噪声阈值 (越小越灵敏)
#define MIC_PIN 33
#define SPECTRUM_MODES 5 // 模式数量 (与参考代码一致)

static double vReal[FFT_SAMPLES];
static double vImag[FFT_SAMPLES];
static byte peak[NUM_BANDS] = {0};
static byte peakSpeed[NUM_BANDS] = {0}; // 峰值下落速度 (用于平滑下落)
static int oldBarHeights[NUM_BANDS] = {0};
static int bandValues[NUM_BANDS] = {0};
static unsigned long peekDecayTime = 0;
static int spectrumMode = 0; // 当前模式
static int colorTime = 0;    // 颜色时间

ArduinoFFT<double> FFT(vReal, vImag, FFT_SAMPLES, (double)SAMPLING_FREQ);

// 频段映射表 (32 bands, 低频到高频分布)
// 每个频段覆盖的FFT索引范围

static const int bandRanges[][2] = {
    {6, 9}, {9, 11}, {11, 13}, {13, 15}, {15, 17}, {17, 19}, {19, 21}, {21, 23}, {23, 25}, {25, 27}, {27, 29}, {29, 31}, {31, 33}, {33, 35}, {35, 38}, {38, 41}, {41, 44}, {44, 47}, {47, 50}, {50, 53}, {53, 56}, {56, 59}, {59, 62}, {62, 65}, {65, 68}, {68, 71}, {71, 74}, {74, 77}, {77, 80}, {80, 83}, {83, 87}, {87, 91}};

// 切换频谱模式
void spectrumNextMode()
{
  spectrumMode = (spectrumMode + 1) % SPECTRUM_MODES;
}

// 获取当前模式名称 (与参考代码一致)
String spectrumGetModeName()
{
  const char *names[] = {"彩虹条", "镜像峰", "紫色条", "中心条", "变色条", "瀑布流"};
  return names[spectrumMode];
}

// ==================================================================
// 辅助函数
// ==================================================================

static inline void applyAppColor(const String &colorHex)
{
  if (colorHex.length() > 0)
  {
    DisplayManager.setTextColor(HEXtoColor(colorHex.c_str()));
  }
  else
  {
    DisplayManager.defaultTextColor();
  }
}

static void drawWeekdayBar(FastLED_NeoMatrix *matrix, int16_t x, int16_t y,
                           int16_t barStartX, int8_t barWidth,
                           const struct tm *timeInfo,
                           const String &activeColorHex,
                           const String &inactiveColorHex)
{
  if (!SHOW_WEEKDAY)
    return;

  int dayOffset = START_ON_MONDAY ? 0 : 1;
  int today = (timeInfo->tm_wday + 6 + dayOffset) % 7;

  uint16_t activeColor = HEXtoColor(activeColorHex.c_str());
  uint16_t inactiveColor = HEXtoColor(inactiveColorHex.c_str());

  for (int i = 0; i <= 6; i++)
  {
    uint16_t color = (i == today) ? activeColor : inactiveColor;
    int xPos = barStartX + (i * (barWidth + 1));
    matrix->drawLine(xPos + x, y + 7, xPos + barWidth - 1 + x, y + 7, color);
  }
}

// ==================================================================
// 时间应用
// ==================================================================

void TimeApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state, int16_t x,
             int16_t y, FastFramePlayer *player)
{
  CURRENT_APP = "Time";
  applyAppColor(TIME_COLOR);

  time_t now = time(nullptr);
  struct tm *timeInfo = localtime(&now);

  char fmt[32];
  strncpy(fmt, TIME_FORMAT.c_str(), sizeof(fmt));
  fmt[sizeof(fmt) - 1] = '\0';

  if (now % 2)
  {
    char *sep = strchr(fmt, ':');
    if (!sep)
      sep = strchr(fmt, ' ');
    if (sep && strlen(TIME_FORMAT.c_str()) < 8)
    {
      *sep = ' ';
    }
  }

  char text[32];
  strftime(text, sizeof(text), fmt, timeInfo);

  int textPixelWidth = strlen(text) * 4 - 1;
  bool showIcon = (textPixelWidth <= 22);

  int16_t textX, barStartX;
  int8_t barWidth;

  if (showIcon)
  {
    String icon = (TIME_ICON.endsWith(".anim")) ? TIME_ICON : "38863.anim";
    player->loadUser(icon);
    player->play(x, y);
    textX = 10 + (22 - textPixelWidth) / 2;
    barStartX = 10;
    barWidth = 2;
    DisplayManager.printText(textX + x, 6 + y, text, false, false);
  }
  else
  {
    textX = (32 - textPixelWidth) / 2;
    barStartX = 2;
    barWidth = 3;
    DisplayManager.printText(textX + x, 6 + y, text, true, false);
  }

  drawWeekdayBar(matrix, x, y, barStartX, barWidth, timeInfo,
                 TIME_WEEKDAY_ACTIVE_COLOR, TIME_WEEKDAY_INACTIVE_COLOR);
}

// ==================================================================
// 日期应用
// ==================================================================

void DateApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state, int16_t x,
             int16_t y, FastFramePlayer *player)
{
  CURRENT_APP = "Date";
  applyAppColor(DATE_COLOR);

  time_t now = time(nullptr);
  struct tm *timeInfo = localtime(&now);

  char text[32];
  strftime(text, sizeof(text), DATE_FORMAT.c_str(), timeInfo);

  int textPixelWidth = strlen(text) * 4 - 1;
  bool showIcon = (textPixelWidth <= 24);

  int16_t textX, barStartX;
  int8_t barWidth;

  if (showIcon)
  {
    String icon = (DATE_ICON.endsWith(".anim")) ? DATE_ICON : "21987.anim";
    player->loadUser(icon);
    player->play(x, y);
    textX = 10 + (22 - textPixelWidth) / 2;
    barStartX = 10;
    barWidth = 2;
    if (DATE_FORMAT.lastIndexOf(".") != -1)
      textX += 1;
    DisplayManager.printText(textX + x, 6 + y, text, false, false);
  }
  else
  {
    textX = (32 - textPixelWidth) / 2;
    barStartX = 2;
    barWidth = 3;
    DisplayManager.printText(textX + x, 6 + y, text, true, false);
  }

  drawWeekdayBar(matrix, x, y, barStartX, barWidth, timeInfo,
                 DATE_WEEKDAY_ACTIVE_COLOR, DATE_WEEKDAY_INACTIVE_COLOR);
}

// ==================================================================
// 温度应用
// ==================================================================

void TempApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state, int16_t x,
             int16_t y, FastFramePlayer *player)
{
  CURRENT_APP = "Temperature";
  DisplayManager.defaultTextColor();

  player->loadUser("fire_ball_29266.anim");
  player->play(x, y);

  char text[16];
  snprintf(text, sizeof(text), "%d°C", (int)std::round(INDOOR_TEMP));

  uint16_t textWidth = getTextWidth(text, true);
  int16_t textX = (23 - textWidth) / 2;
  DisplayManager.printText(textX + 12, 6 + y, utf8ascii(String(text)).c_str(),
                           false, false);
}

// ==================================================================
// 湿度应用
// ==================================================================

void HumApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state, int16_t x,
            int16_t y, FastFramePlayer *player)
{
  CURRENT_APP = "Humidity";
  DisplayManager.defaultTextColor();

  player->loadUser("Blue_Fireball_38863.anim");
  player->play(x, y);

  matrix->setCursor(14 + x, 6 + y);
  matrix->print((int)INDOOR_HUM);
  matrix->print("%");
}

// ==================================================================
// 天气应用
// ==================================================================

void WeatherApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                int16_t x, int16_t y, FastFramePlayer *player)
{
  CURRENT_APP = "Weather";
  DisplayManager.defaultTextColor();

  char wBuf[32];
  strncpy(wBuf, CURRENT_WEATHER.c_str(), sizeof(wBuf));
  wBuf[sizeof(wBuf) - 1] = '\0';
  for (char *p = wBuf; *p; p++)
    *p = tolower(*p);

  if (strstr(wBuf, "rain"))
  {
    player->loadSystem(3);
  }
  else if (strstr(wBuf, "cloud") || strstr(wBuf, "overcast"))
  {
    player->loadSystem(6);
  }
  else if (strstr(wBuf, "snow"))
  {
    player->loadSystem(1);
  }
  else
  {
    player->loadSystem(8);
  }

  player->play(x, y);

  char text[16];
  snprintf(text, sizeof(text), "%d°C", (int)std::round(OUTDOOR_TEMP));
  uint16_t textWidth = getTextWidth(text, true);
  int16_t textX = (23 - textWidth) / 2;
  DisplayManager.printText(textX + 11, 6 + y, utf8ascii(String(text)).c_str(),
                           false, false);
}

// ==================================================================
// 风速应用
// ==================================================================

void WindApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state, int16_t x,
             int16_t y, FastFramePlayer *player)
{
  CURRENT_APP = "Wind";
  DisplayManager.defaultTextColor();

  String icon = (WIND_ICON.endsWith(".anim")) ? WIND_ICON : "16642.anim";
  player->loadUser(icon);
  player->play(x, y);

  char text[16];
  snprintf(text, sizeof(text), "%dm/s", (int)std::round(WEATHER_WIND_SPEED));
  uint16_t textWidth = getTextWidth(text, true);
  int16_t textX = (23 - textWidth) / 2;

  DisplayManager.printText(textX + 8, 6 + y, utf8ascii(String(text)).c_str(),
                           false, true);
}

// ==================================================================
// 频谱覆盖层 - 绘制模式 (参考代码风格)
// ==================================================================

// 模式0: 彩虹条 (rainbowBars)
static void rainbowBars(FastLED_NeoMatrix *matrix, int band, int barHeight)
{
  for (int y = 7; y >= 7 - barHeight; y--)
  {
    matrix->drawPixel(band, y, hsvToRgb(band * 8, 255, 255));
  }
}

// 模式1: 镜像峰 (outrunPeak) - 只显示峰值
static void outrunPeak(FastLED_NeoMatrix *matrix, int band)
{
  int peakHeight = 7 - peak[band];
  matrix->drawPixel(band, peakHeight, hsvToRgb(band * 8 + colorTime * 2, 255, 255));
}

// 模式2: 紫色条 (purpleBars)
static void purpleBars(FastLED_NeoMatrix *matrix, int band, int barHeight)
{
  for (int y = 7; y >= 7 - barHeight; y--)
  {
    // 紫色调: 根据y位置渐变
    uint16_t color = hsvToRgb(180 + (7 - y) * 10, 255, 200);
    matrix->drawPixel(band, y, color);
  }
  // 峰值
  matrix->drawPixel(band, 7 - peak[band], matrix->Color(255, 255, 255));
}

// 模式3: 中心条 (centerBars) - 从中间向两边
static void centerBars(FastLED_NeoMatrix *matrix, int band, int barHeight)
{
  if (barHeight % 2 == 0)
    barHeight--;
  int yStart = (8 - barHeight) / 2;

  for (int y = yStart; y <= yStart + barHeight; y++)
  {
    // 热力图色调: 黄->红
    int colorIdx = constrain((y - yStart) * 255 / (barHeight + 1), 0, 255);
    uint16_t color = hsvToRgb(colorIdx / 2, 255, 200);
    matrix->drawPixel(band, y, color);
  }
}

// 模式4: 变色条 (changingBars)
static void changingBars(FastLED_NeoMatrix *matrix, int band, int barHeight)
{
  for (int y = 7; y >= 7 - barHeight; y--)
  {
    // 颜色随y和时间变化
    matrix->drawPixel(band, y, hsvToRgb((7 - y) * 32 + colorTime * 3, 255, 255));
  }
}

// 模式5: 瀑布流 (waterfall) - 使用旧的oldBarHeights绘制
static void waterfall(FastLED_NeoMatrix *matrix, int band)
{
  // 底部显示当前值
  int intensity = constrain(bandValues[band] / 2000, 0, 160);
  matrix->drawPixel(band, 7, hsvToRgb(160 - intensity, 255, 255));

  // 上方显示历史值 (使用oldBarHeights作为历史)
  int historyHeight = oldBarHeights[band] / 100;
  if (historyHeight > 0)
  {
    for (int y = 6; y >= 7 - historyHeight && y >= 0; y--)
    {
      matrix->drawPixel(band, y, hsvToRgb(160 - (6 - y) * 10, 255, 200));
    }
  }
}

// 峰值绘制 (白色峰值)
static void whitePeak(FastLED_NeoMatrix *matrix, int band)
{
  matrix->drawPixel(band, 7 - peak[band], matrix->Color(255, 255, 255));
}

// ==================================================================
// 频谱覆盖层
// ==================================================================

void SpectrumOverlay(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                     FastFramePlayer *player)
{
  if (!SPECTRUM_ACTIVE)
    return;

  // 瀑布流模式不需要清屏
  if (spectrumMode != 5)
    matrix->fillScreen(0);

  // 重置频段值
  memset(bandValues, 0, sizeof(bandValues));

  // 采样
  unsigned long startTime;
  for (int i = 0; i < FFT_SAMPLES; i++)
  {
    startTime = micros();
    vReal[i] = (double)analogRead(MIC_PIN);
    vImag[i] = 0.0;
    while ((micros() - startTime) < SAMPLING_PERIOD_US)
    {
    }
  }

  // FFT 计算
  FFT.dcRemoval();
  FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD, false);
  FFT.compute(FFT_FORWARD);
  FFT.complexToMagnitude();

  // 解析频段 (参考代码的逻辑)
  for (int i = 2; i < FFT_SAMPLES / 2; i++)
  {
    if (vReal[i] > NOISE)
    {
      for (int band = 0; band < NUM_BANDS; band++)
      {
        if (i >= bandRanges[band][0] && i < bandRanges[band][1])
        {
          bandValues[band] += (int)vReal[i];
          break;
        }
      }
    }
  }

  // 处理并绘制频谱条
  for (int band = 0; band < NUM_BANDS; band++)
  {
    // 缩放条形高度
    int barHeight = bandValues[band] / AMPLITUDE;
    if (barHeight > 8)
      barHeight = 8;

    // 帧间平滑
    barHeight = (oldBarHeights[band] + barHeight) / 2;

    // 峰值上升
    if (barHeight > peak[band])
    {
      peak[band] = min(8, barHeight);
      peakSpeed[band] = 0; // 重置下落速度
    }
    // 峰值平滑下落 (使用加速度)
    else if (peak[band] > 0)
    {
      peakSpeed[band]++;           // 速度递增 (加速度效果)
      if (peakSpeed[band] >= 3)    // 速度累积到阈值时下落
      {
        peak[band]--;
        peakSpeed[band] = 2;       // 保留部分速度，形成连续下落效果
      }
    }

    // 根据模式绘制条形
    switch (spectrumMode)
    {
    case 0:
      rainbowBars(matrix, band, barHeight);
      whitePeak(matrix, band);
      break;
    case 1:
      // 无条形，只绘制峰值
      outrunPeak(matrix, band);
      break;
    case 2:
      purpleBars(matrix, band, barHeight);
      break;
    case 3:
      centerBars(matrix, band, barHeight);
      break;
    case 4:
      changingBars(matrix, band, barHeight);
      whitePeak(matrix, band);
      break;
    case 5:
      waterfall(matrix, band);
      break;
    }

    // 保存旧高度用于下一帧
    oldBarHeights[band] = barHeight;
  }

  // 颜色时间更新 (每60ms)
  if ((millis() - peekDecayTime) >= 70)
  {
    colorTime++;
    peekDecayTime = millis();
  }
}

// ==================================================================
// 其他覆盖层 (预留)
// ==================================================================

void AlarmOverlay(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                  FastFramePlayer *player)
{
}

void TimerOverlay(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                  FastFramePlayer *player)
{
}

void NotifyOverlay(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                   FastFramePlayer *player)
{
}
