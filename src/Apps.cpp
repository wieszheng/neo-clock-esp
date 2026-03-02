/**
 * @file Apps.cpp
 * @brief 应用函数实现 — 时间、日期、天气等显示应用
 *
 * 本文件实现：
 *   - 时间/日期应用 (支持自定义格式和颜色)
 *   - 温度/湿度应用 (显示 DHT22 传感器数据)
 *   - 天气应用 (显示室外天气)
 *   - 风速应用 (显示风速数据)
 *   - 频谱应用 (音乐律动可视化)
 *   - 星期指示条绘制
 *   - 覆盖层函数 (预留扩展)
 */

#include "Apps.h"
#include "DisplayManager.h"
#include "Globals.h"
#include "Tools.h"
#include <time.h>

// 频谱管理器前向声明
class SpectrumManager_;
extern SpectrumManager_ &SpectrumManager;

// ==================================================================
// 全局应用列表
// ==================================================================

std::vector<AppData> Apps;

// 覆盖层回调数组（按优先级从高到低）
OverlayCallback overlays[] = {AlarmOverlay, TimerOverlay, NotifyOverlay,
                              SpectrumOverlay};

// 设置应用文字颜色
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

// 颜色在循环外预转换
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

  // 预计算颜色 (循环外, 仅 2 次转换)
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

  // 格式化时间
  char fmt[32];
  strncpy(fmt, TIME_FORMAT.c_str(), sizeof(fmt));
  fmt[sizeof(fmt) - 1] = '\0';

  // 冒号闪烁 (短格式, 每秒切换)
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

  // 计算文字宽度 (用常量字符宽度估算, 足够精确)
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

  // [性能优化] snprintf + char[] 替代 String 拼接 (避免堆分配)
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
// 频谱应用
// ==================================================================

/**
 * @brief RGB565 颜色线性插值
 * @param color1 起始颜色 (RGB565)
 * @param color2 结束颜色 (RGB565)
 * @param ratio 插值比例 (0.0-1.0)
 * @return 插值后的颜色 (RGB565)
 */
static uint16_t lerpColor(uint16_t color1, uint16_t color2, float ratio)
{
  // RGB565 转换为 RGB888
  uint8_t r1 = ((color1 >> 11) & 0x1F) << 3;
  uint8_t g1 = ((color1 >> 5) & 0x3F) << 2;
  uint8_t b1 = (color1 & 0x1F) << 3;

  uint8_t r2 = ((color2 >> 11) & 0x1F) << 3;
  uint8_t g2 = ((color2 >> 5) & 0x3F) << 2;
  uint8_t b2 = (color2 & 0x1F) << 3;

  // 线性插值
  uint8_t r = (uint8_t)(r1 + (r2 - r1) * ratio);
  uint8_t g = (uint8_t)(g1 + (g2 - g1) * ratio);
  uint8_t b = (uint8_t)(b1 + (b2 - b1) * ratio);

  // RGB888 转换为 RGB565
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void SpectrumApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                 int16_t x, int16_t y, FastFramePlayer *player)
{
  CURRENT_APP = "Spectrum";

  // 获取频谱数据
  const uint8_t *barHeights = SpectrumManager.getBarHeights();
  const uint8_t *peaks = SpectrumManager.getPeaks();

  // 获取颜色配置
  uint16_t colorStart = HEXtoColor(SPECTRUM_COLOR_START.c_str());
  uint16_t colorEnd = HEXtoColor(SPECTRUM_COLOR_END.c_str());

  // 渲染频谱
  for (int i = 0; i < MATRIX_WIDTH; i++)
  {
    // 计算颜色渐变
    float ratio = (float)i / (float)(MATRIX_WIDTH - 1);
    uint16_t color = lerpColor(colorStart, colorEnd, ratio);

    // 绘制频谱条
    uint8_t height = barHeights[i];
    for (int h = 0; h < height; h++)
    {
      matrix->drawPixel(i + x, (MATRIX_HEIGHT - 1 - h) + y, color);
    }

    // 绘制峰值点
    if (SPECTRUM_SHOW_PEAKS && peaks[i] > 0 && peaks[i] > height)
    {
      matrix->drawPixel(i + x, (MATRIX_HEIGHT - peaks[i]) + y, 0xFFFF); // 白色峰值
    }
  }
}

// ==================================================================
// 覆盖层实现
// ==================================================================

void SpectrumOverlay(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                     FastFramePlayer *player)
{
  // 频谱覆盖层（不需要实现）
}

void AlarmOverlay(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                  FastFramePlayer *player)
{
  // 桩代码
}
void TimerOverlay(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                  FastFramePlayer *player)
{
  // 桩代码
}
void NotifyOverlay(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                   FastFramePlayer *player)
{
  // 桩代码
}
