/**
 * @file Apps.cpp
 * @brief 应用函数实现 — 时间、日期、天气等显示应用
 *
 * 本文件实现：
 *   - 时间/日期应用 (支持自定义格式和颜色)
 *   - 温度/湿度应用 (显示 DHT22 传感器数据)
 *   - 天气应用 (显示室外天气)
 *   - 风速应用 (显示风速数据)
 *   - 星期指示条绘制
 *   - 覆盖层函数 (预留扩展)
 */

#include "Apps.h"
#include "DisplayManager.h"
#include "Globals.h"
#include "PeripheryManager.h"
#include "Tools.h"
#include <time.h>

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
// 覆盖层实现
// ==================================================================

void SpectrumOverlay(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                     FastFramePlayer *player)
{
  // 暂时为空，如果需要在所有界面显示频谱条可在此实现
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
