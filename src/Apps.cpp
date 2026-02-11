/**
 * @file Apps.cpp
 * @brief NeoClock 应用页面 — 各显示应用的渲染逻辑
 *
 * ESP32 性能优化:
 *   - 星期指示条颜色在循环外预计算，每帧仅转换 2 次 (非 14 次)
 *   - CURRENT_APP 使用 const char* 避免 String 堆分配
 *   - 温度/湿度文字使用 snprintf + char[] 替代 String 拼接
 *   - WeatherApp 天气字符串比较使用缓存的小写副本
 */

#include "Apps.h"
#include "DisplayManager.h"
#include "Globals.h"
#include "PeripheryManager.h"
#include "Tools.h"
#include <time.h>

/// 全局应用列表
std::vector<AppData> Apps;

/// 覆盖层回调数组（按优先级从高到低）
OverlayCallback overlays[] = {AlarmOverlay, TimerOverlay, NotifyOverlay,
                              SpectrumOverlay};

// ==================================================================
// 辅助函数 (静态内联, 零开销)
// ==================================================================

/// 设置应用文字颜色
static inline void applyAppColor(const String &colorHex) {
  if (colorHex.length() > 0) {
    DisplayManager.setTextColor(HEXtoColor(colorHex.c_str()));
  } else {
    DisplayManager.defaultTextColor();
  }
}

/**
 * @brief 绘制底部星期指示条
 *
 * [性能优化] 颜色在循环外预转换 (2次 HEXtoColor)，
 * 而非旧版在循环内每次都转换 (14次 HEXtoColor)
 */
static void drawWeekdayBar(FastLED_NeoMatrix *matrix, int16_t x, int16_t y,
                           int16_t barStartX, int8_t barWidth,
                           const struct tm *timeInfo,
                           const String &activeColorHex,
                           const String &inactiveColorHex) {
  if (!SHOW_WEEKDAY)
    return;

  int dayOffset = START_ON_MONDAY ? 0 : 1;
  int today = (timeInfo->tm_wday + 6 + dayOffset) % 7;

  // 预计算颜色 (循环外, 仅 2 次转换)
  uint16_t activeColor = HEXtoColor(activeColorHex.c_str());
  uint16_t inactiveColor = HEXtoColor(inactiveColorHex.c_str());

  for (int i = 0; i <= 6; i++) {
    uint16_t color = (i == today) ? activeColor : inactiveColor;
    int xPos = barStartX + (i * (barWidth + 1));
    matrix->drawLine(xPos + x, y + 7, xPos + barWidth - 1 + x, y + 7, color);
  }
}

// ==================================================================
// 时间应用
// ==================================================================
void TimeApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state, int16_t x,
             int16_t y, FastFramePlayer *player) {
  CURRENT_APP = "Time";
  applyAppColor(TIME_COLOR);

  time_t now = time(nullptr);
  struct tm *timeInfo = localtime(&now);

  // 格式化时间
  char fmt[32];
  strncpy(fmt, TIME_FORMAT.c_str(), sizeof(fmt));
  fmt[sizeof(fmt) - 1] = '\0';

  // 冒号闪烁 (短格式, 每秒切换)
  if (now % 2) {
    char *sep = strchr(fmt, ':');
    if (!sep)
      sep = strchr(fmt, ' ');
    if (sep && strlen(TIME_FORMAT.c_str()) < 8) {
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

  if (showIcon) {
    player->loadUser("14825.anim");
    player->play(x, y);
    textX = 10 + (22 - textPixelWidth) / 2;
    barStartX = 10;
    barWidth = 2;
    DisplayManager.printText(textX + x, 6 + y, text, false, false);
  } else {
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
             int16_t y, FastFramePlayer *player) {
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

  if (showIcon) {
    player->loadUser("21987.anim");
    player->play(x, y);
    textX = 10 + (22 - textPixelWidth) / 2;
    barStartX = 10;
    barWidth = 2;
    if (DATE_FORMAT.lastIndexOf(".") != -1)
      textX += 1;
    DisplayManager.printText(textX + x, 6 + y, text, false, false);
  } else {
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
             int16_t y, FastFramePlayer *player) {
  CURRENT_APP = "Temperature";
  DisplayManager.defaultTextColor();

  player->loadUser("38863.anim");
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
            int16_t y, FastFramePlayer *player) {
  CURRENT_APP = "Humidity";
  DisplayManager.defaultTextColor();

  player->loadUser("38865.anim");
  player->play(x, y);

  matrix->setCursor(14 + x, 6 + y);
  matrix->print((int)INDOOR_HUM);
  matrix->print("%");
}

// ==================================================================
// 天气应用
// ==================================================================
void WeatherApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                int16_t x, int16_t y, FastFramePlayer *player) {
  CURRENT_APP = "Weather";
  DisplayManager.defaultTextColor();

  // [性能优化] 使用 C 字符串操作避免 String::toLowerCase() 堆分配
  // CURRENT_WEATHER 通常很短 (<20字符), 栈上处理即可
  char wBuf[32];
  strncpy(wBuf, CURRENT_WEATHER.c_str(), sizeof(wBuf));
  wBuf[sizeof(wBuf) - 1] = '\0';
  for (char *p = wBuf; *p; p++)
    *p = tolower(*p);

  if (strstr(wBuf, "rain")) {
    player->loadSystem(3);
  } else if (strstr(wBuf, "cloud") || strstr(wBuf, "overcast")) {
    player->loadSystem(6);
  } else if (strstr(wBuf, "snow")) {
    player->loadSystem(1);
  } else {
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
             int16_t y, FastFramePlayer *player) {
  CURRENT_APP = "Wind";
  DisplayManager.defaultTextColor();

  player->loadUser("29266.anim");
  player->play(x, y);

  char text[16];
  snprintf(text, sizeof(text), "%dm/s", (int)std::round(WEATHER_WIND_SPEED));
  uint16_t textWidth = getTextWidth(text, true);
  int16_t textX = (23 - textWidth) / 2;

  DisplayManager.printText(textX + 8, 6 + y, utf8ascii(String(text)).c_str(),
                           false, true);
}

// ==================================================================
// 频谱应用 (New)
// ==================================================================
void SpectrumApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                 int16_t x, int16_t y, FastFramePlayer *player) {
  // 如果处于过渡状态，为了流畅度可以选择不计算 FFT
  if (state->appState == IN_TRANSITION)
    return;

  CURRENT_APP = "Music";
  DisplayManager.defaultTextColor();

  uint8_t bands[32];           // 假设宽度 32
  int width = matrix->width(); // 可能是 32
  if (width > 32)
    width = 32;

  // 从 PeripheryManager 获取频段数据
  // 注意: getSpectrumData 可能耗时 (25ms)
  if (PeripheryManager.getSpectrumData(bands, width)) {
    for (int i = 0; i < width; i++) {
      uint8_t val = bands[i];
      if (val == 0)
        continue;

      int h = map(val, 0, 255, 0, 8); // 映射到高度 8
      if (h > 8)
        h = 8;

      // 绘制柱状图
      // hue 根据 x 位置变化
      uint8_t hue = i * 255 / width;

      // 自底向上绘制
      for (int j = 0; j < h; j++) {
        // y + 7 是底部 (假设高度8)
        // ColorHSV(hue, sat, val)
        matrix->drawPixel(x + i, y + 7 - j, matrix->ColorHSV(hue, 255, 255));
      }
    }
  }
}

// ==================================================================
// 覆盖层实现
// ==================================================================

void SpectrumOverlay(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                     FastFramePlayer *player) {
  // 暂时为空，如果需要在所有界面显示频谱条可在此实现
}

void AlarmOverlay(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                  FastFramePlayer *player) {
  // 桩代码
}
void TimerOverlay(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                  FastFramePlayer *player) {
  // 桩代码
}
void NotifyOverlay(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                   FastFramePlayer *player) {
  // 桩代码
}
