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
#define AMPLITUDE 30
#define NUM_BANDS 32
#define MIC_PIN 33
#define SPECTRUM_MODES 5 // 模式数量

static double vReal[FFT_SAMPLES];
static double vImag[FFT_SAMPLES];
static byte peak[NUM_BANDS] = {0};
static int oldBarHeights[NUM_BANDS] = {0};
static int bandValues[NUM_BANDS] = {0};
static unsigned long peekDecayTime = 0;
static int spectrumMode = 0; // 当前模式
static int colorTime = 0;    // 颜色时间

ArduinoFFT<double> FFT(vReal, vImag, FFT_SAMPLES, (double)SAMPLING_FREQ);

// 频段映射表 (FFT索引范围)
static const int bandRanges[][2] = {
    {6, 9}, {9, 11}, {11, 13}, {13, 15}, {15, 17}, {17, 19}, {19, 21}, {21, 23}, {23, 25}, {25, 27}, {27, 29}, {29, 31}, {31, 33}, {33, 35}, {35, 38}, {38, 41}, {41, 44}, {44, 47}, {47, 50}, {50, 53}, {53, 56}, {56, 59}, {59, 62}, {62, 65}, {65, 68}, {68, 71}, {71, 74}, {74, 77}, {77, 80}, {80, 83}, {83, 87}, {87, 91}};

// 切换频谱模式
void spectrumNextMode()
{
  spectrumMode = (spectrumMode + 1) % SPECTRUM_MODES;
}

// 获取当前模式名称
String spectrumGetModeName()
{
  const char *names[] = {"彩虹", "镜像", "渐变", "波浪", "中心"};
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
// 频谱覆盖层 - 绘制模式
// ==================================================================

// 模式0: 彩虹条形图
static void drawSpectrumRainbow(FastLED_NeoMatrix *matrix, int barHeight, int band, int color)
{
  matrix->drawFastVLine((MATRIX_WIDTH - 1 - band), (8 - barHeight), barHeight, hsvToRgb(color, 255, 255));
  matrix->drawPixel((MATRIX_WIDTH - 1 - band), 8 - peak[band] - 1, matrix->Color(255, 255, 255));
}

// 模式1:
static void drawSpectrumMirror(FastLED_NeoMatrix *matrix, int barHeight, int band)
{
  for (int y = 8; y >= 8 - barHeight; y--)
  {
    matrix->drawPixel((MATRIX_WIDTH - 1 - band), y, hsvToRgb(y * (255 / MATRIX_WIDTH / 5) + colorTime, 255, 255));
  }
}

// 模式2: 渐变模式 (根据高度变色)
static void drawSpectrumGradient(FastLED_NeoMatrix *matrix, int barHeight, int band)
{
  // 根据高度计算颜色: 蓝->绿->黄->红
  uint16_t color;
  if (barHeight <= 2)
    color = 0x001F; // 蓝
  else if (barHeight <= 4)
    color = 0x07E0; // 绿
  else if (barHeight <= 6)
    color = 0xFFE0; // 黄
  else
    color = 0xF800; // 红

  matrix->drawFastVLine((MATRIX_WIDTH - 1 - band), (8 - barHeight), barHeight, color);
  matrix->drawPixel((MATRIX_WIDTH - 1 - band), 8 - peak[band] - 1, matrix->Color(255, 255, 255));
}

// 模式3: 波浪模式 (圆点)
static void drawSpectrumWave(FastLED_NeoMatrix *matrix, int barHeight, int band, int color)
{
  for (int h = 0; h < barHeight; h++)
  {
    int pixelY = 7 - h;
    uint16_t dotColor = hsvToRgb(color + h * 20, 255, 255);
    matrix->drawPixel((MATRIX_WIDTH - 1 - band), pixelY, dotColor);
  }
  matrix->drawPixel((MATRIX_WIDTH - 1 - band), 8 - peak[band] - 1, matrix->Color(255, 255, 255));
}

// 模式4: 中心爆发模式
static void drawSpectrumCenter(FastLED_NeoMatrix *matrix, int barHeight, int band)
{
  int centerX = MATRIX_WIDTH / 2;
  int x = (band < 16) ? (centerX - 1 - (15 - band)) : (centerX + (band - 16));
  uint16_t color = hsvToRgb((colorTime * 5 + band * 10) % 360, 255, 255);

  for (int h = 0; h < barHeight; h++)
  {
    int pixelY = 7 - h;
    matrix->drawPixel(x, pixelY, color);
  }
  matrix->drawPixel(x, 8 - peak[band] - 1, matrix->Color(255, 255, 255));
}

// ==================================================================
// 频谱覆盖层
// ==================================================================

void SpectrumOverlay(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                     FastFramePlayer *player)
{
  if (!SPECTRUM_ACTIVE)
    return;

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

  // 解析频段
  for (int i = 2; i < FFT_SAMPLES / 2; i++)
  {
    if (vReal[i] < 300)
      continue;

    for (int band = 0; band < NUM_BANDS; band++)
    {
      if (i > bandRanges[band][0] && i <= bandRanges[band][1])
      {
        bandValues[band] += (int)vReal[i];
        break;
      }
    }
  }

  // 绘制频谱条
  int color = 0;
  for (int band = 0; band < NUM_BANDS; band++)
  {
    int barHeight = bandValues[band] / AMPLITUDE;
    if (barHeight > 8)
      barHeight = 8;

    barHeight = (oldBarHeights[band] + barHeight) / 2;

    if (barHeight > peak[band])
      peak[band] = min(8, barHeight);

    // 根据模式绘制
    switch (spectrumMode)
    {
    case 0:
      drawSpectrumRainbow(matrix, barHeight, band, color);
      break;
    case 1:
      drawSpectrumMirror(matrix, barHeight, band);
      break;
    case 2:
      drawSpectrumGradient(matrix, barHeight, band);
      break;
    case 3:
      drawSpectrumWave(matrix, barHeight, band, color);
      break;
    case 4:
      drawSpectrumCenter(matrix, barHeight, band);
      break;
    }

    color += 360 / (NUM_BANDS + 4);
    oldBarHeights[band] = barHeight;
  }

  // 顶点衰减
  if ((millis() - peekDecayTime) >= 70)
  {
    for (byte band = 0; band < NUM_BANDS; band++)
    {
      if (peak[band] > 0)
        peak[band] -= 1;
    }
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
