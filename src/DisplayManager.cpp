#include "DisplayManager.h"
#include "Apps.h"
#include "Liveview.h"
#include "Tools.h"
#include <ArduinoJson.h>

// ==================================================================
// 硬件实例
// ==================================================================
CRGB leds[NUM_LEDS];

FastLED_NeoMatrix *matrix =
    new FastLED_NeoMatrix(leds, 8, 8, 4, 1,
                          NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS +
                              NEO_MATRIX_PROGRESSIVE);

MatrixDisplayUi *ui = new MatrixDisplayUi(matrix);

// 用于将 Web 逻辑坐标映射为物理 FastLED 索引（处理 Zigzag, 蛇形布局等）
static uint16_t liveviewPixelMap(int16_t x, int16_t y) {
  if (matrix)
    return matrix->XY(x, y);
  return y * MATRIX_WIDTH + x;
}

// ==================================================================
// 单例
// ==================================================================
DisplayManager_ &DisplayManager_::getInstance() {
  static DisplayManager_ instance;
  return instance;
}

DisplayManager_ &DisplayManager = DisplayManager.getInstance();

// ==================================================================
// 初始化
// ==================================================================
void DisplayManager_::setup() {
  FastLED.addLeds<NEOPIXEL, MATRIX_PIN>(leds, MATRIX_WIDTH * MATRIX_HEIGHT);
  setMatrixLayout(MATRIX_LAYOUT);

  ui->setAppAnimation(SLIDE_DOWN);
  ui->setTargetFPS(MATRIX_FPS);
  ui->setTimePerApp(TIME_PER_APP);
  ui->setTimePerTransition(TIME_PER_TRANSITION);
  ui->init();

  // 初始化 Liveview 模块
  Liveview.setLeds(leds, liveviewPixelMap);
  Liveview.setInterval(250);

  _state = DisplayState();
  _state.scrollX = MATRIX_WIDTH;
}

// ==================================================================
// 亮度与颜色
// ==================================================================
void DisplayManager_::setBrightness(uint8_t bri) {
  matrix->setBrightness(MATRIX_OFF ? 0 : bri);
}
void DisplayManager_::setTextColor(uint16_t color) {
  matrix->setTextColor(color);
}
void DisplayManager_::setMatrixState(bool on) {
  MATRIX_OFF = !on;
  setBrightness(BRIGHTNESS);
}
void DisplayManager_::defaultTextColor() { setTextColor(TEXTCOLOR_565); }

// ==================================================================
// 矩阵布局
// ==================================================================
void DisplayManager_::setMatrixLayout(int layout) {
  delete matrix;

  // 简化布局枚举切换分支
  uint8_t type = NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS +
                 NEO_MATRIX_PROGRESSIVE;
  int w = 8, h = 8, tilesX = 4, tilesY = 1;

  switch (layout) {
  case 0: // 32x8 单块 Col+Zigzag
    w = 32;
    h = 8;
    tilesX = 1;
    tilesY = 1;
    type = NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS +
           NEO_MATRIX_ZIGZAG;
    break;
  case 2: // 32x8 单块 Row+Zigzag
    w = 32;
    h = 8;
    tilesX = 1;
    tilesY = 1;
    type =
        NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG;
    break;
  case 3: // 32x8 单块 Bottom+Col+Prog
    w = 32;
    h = 8;
    tilesX = 1;
    tilesY = 1;
    type = NEO_MATRIX_BOTTOM + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS +
           NEO_MATRIX_PROGRESSIVE;
    break;
  case 4: // 4x1 拼 Row+Zigzag
    type =
        NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG;
    break;
  case 5: // Default
  default:
    break;
  }

  if (tilesX > 1) {
    matrix = new FastLED_NeoMatrix(leds, w, h, tilesX, tilesY, type);
  } else {
    matrix = new FastLED_NeoMatrix(leds, w, h, type);
  }

  delete ui;
  ui = new MatrixDisplayUi(matrix);
}

// ==================================================================
// 主循环
// ==================================================================
void DisplayManager_::tick() {
  if (_state.status != DISPLAY_NORMAL) {
    matrix->clear();
    switch (_state.status) {
    case DISPLAY_AP_MODE:
      _renderAPMode();
      break;
    case DISPLAY_CONNECTING:
      _renderConnecting();
      break;
    case DISPLAY_CONNECTED:
      _renderConnected();
      break;
    case DISPLAY_CONNECT_FAILED:
      _renderConnectFailed();
      break;
    default:
      break;
    }
    matrix->show();
  } else {
    ui->update();
  }
}

// ==================================================================
// 基础绘制
// ==================================================================
void DisplayManager_::clear() { matrix->clear(); }
void DisplayManager_::show() { matrix->show(); }

void DisplayManager_::printText(int16_t x, int16_t y, const char *text,
                                bool centered, bool ignoreUppercase) {
  if (centered) {
    uint16_t textWidth = getTextWidth(text, ignoreUppercase);
    int16_t textX = (MATRIX_WIDTH - textWidth) / 2;
    matrix->setCursor(textX, y);
  } else {
    matrix->setCursor(x, y);
  }

  if (!ignoreUppercase) {
    char upperText[64];
    size_t len = 0;
    const char *p = text;
    while (*p && len < 63) {
      upperText[len++] = toupper(*p);
      p++;
    }
    upperText[len] = '\0';
    matrix->print(upperText);
  } else {
    matrix->print(text);
  }
}

// ==================================================================
// 设置应用
// ==================================================================
void DisplayManager_::applyAllSettings() {
  ui->setTargetFPS(MATRIX_FPS);
  ui->setTimePerApp(TIME_PER_APP);
  ui->setTimePerTransition(TIME_PER_TRANSITION);
  setBrightness(BRIGHTNESS);
  setTextColor(TEXTCOLOR_565);

  if (AUTO_TRANSITION)
    ui->enablesetAutoTransition();
  else
    ui->disablesetAutoTransition();
}

void DisplayManager_::loadNativeApps() {
  Apps.clear();

  Apps.push_back({"time", TimeApp, SHOW_TIME, TIME_POSITION, TIME_DURATION});
  Apps.push_back({"date", DateApp, SHOW_DATE, DATE_POSITION, DATE_DURATION});
  Apps.push_back({"temp", TempApp, SHOW_TEMP, TEMP_POSITION, TEMP_DURATION});
  Apps.push_back({"hum", HumApp, SHOW_HUM, HUM_POSITION, HUM_DURATION});
  Apps.push_back({"weather", WeatherApp, SHOW_WEATHER, WEATHER_POSITION,
                  WEATHER_DURATION});
  Apps.push_back({"wind", WindApp, SHOW_WIND, WIND_POSITION, WIND_DURATION});

  std::sort(Apps.begin(), Apps.end(), [](const AppData &a, const AppData &b) {
    return a.position < b.position;
  });

  ui->setApps(Apps);
}

void DisplayManager_::updateAppVector(const char *json) {
  DynamicJsonDocument doc(1024);
  auto error = deserializeJson(doc, json);
  if (error)
    return;

  for (const auto &app : doc.as<JsonArray>()) {
    String name = app["name"].as<String>();
    bool show = app.containsKey("show") ? app["show"].as<bool>() : true;

    if (name == "time")
      SHOW_TIME = show;
    else if (name == "date")
      SHOW_DATE = show;
    else if (name == "temp")
      SHOW_TEMP = show;
    else if (name == "hum")
      SHOW_HUM = show;
    else if (name == "music")
      SHOW_SPECTRUM = show;
  }

  loadNativeApps();
}

void DisplayManager_::setNewSettings(const char *json) {
  DynamicJsonDocument doc(512);
  auto error = deserializeJson(doc, json);
  if (error)
    return;

  if (doc.containsKey("appTime"))
    TIME_PER_APP = doc["appTime"];
  if (doc.containsKey("transition"))
    TIME_PER_TRANSITION = doc["transition"];
  if (doc.containsKey("brightness"))
    BRIGHTNESS = doc["brightness"];
  if (doc.containsKey("fps"))
    MATRIX_FPS = doc["fps"];
  if (doc.containsKey("autoTransition"))
    AUTO_TRANSITION = doc["autoTransition"];

  applyAllSettings();
}

// ==================================================================
// 导航与按钮
// ==================================================================
void DisplayManager_::nextApp() { ui->nextApp(); }
void DisplayManager_::previousApp() { ui->previousApp(); }
void DisplayManager_::leftButton() { ui->previousApp(); }
void DisplayManager_::rightButton() { ui->nextApp(); }
void DisplayManager_::selectButton() {}

// ==================================================================
// 状态显示系统
// ==================================================================
void DisplayManager_::setDisplayStatus(DisplayStatus status,
                                       const String &line1,
                                       const String &line2) {
  _state.status = status;
  _state.line1 = line1;
  _state.line2 = line2;
  _state.scrollX = MATRIX_WIDTH + 4;
  _state.scrollTextWidth = getTextWidth(line2.c_str(), false);
  _state.startTime = millis();
  _state.animFrame = 0;
  _state.lastAnimTime = millis();
  _state.lastScrollTime = millis();

  Serial.printf("[Display] 状态切换: %d, L1='%s', L2='%s'\n", status,
                line1.c_str(), line2.c_str());
}

// 辅助: RGB565 转换宏
#define RGB565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

void DisplayManager_::_drawWiFiIcon(int16_t x, int16_t y, uint16_t color) {
  for (int i = 1; i <= 5; i++)
    matrix->drawPixel(x + i, y + 0, color);
  matrix->drawPixel(x + 0, y + 1, color);
  matrix->drawPixel(x + 6, y + 1, color);
  for (int i = 2; i <= 4; i++)
    matrix->drawPixel(x + i, y + 2, color);
  matrix->drawPixel(x + 1, y + 3, color);
  matrix->drawPixel(x + 5, y + 3, color);
  matrix->drawPixel(x + 3, y + 4, color);
  matrix->drawPixel(x + 2, y + 5, color);
  matrix->drawPixel(x + 4, y + 5, color);
  matrix->drawPixel(x + 3, y + 6, color);
}

void DisplayManager_::_drawScrollText(int16_t y, const String &text,
                                      uint16_t color) {
  if (millis() - _state.lastScrollTime >= 50) {
    _state.lastScrollTime = millis();
    _state.scrollX--;
    if (_state.scrollX < -_state.scrollTextWidth) {
      _state.scrollX = MATRIX_WIDTH + 2;
    }
  }
  matrix->setTextColor(color);
  matrix->setCursor(_state.scrollX, y);
  matrix->print(text);
}

void DisplayManager_::_renderAPMode() {
  unsigned long now = millis();
  float breath = (sin((now - _state.startTime) * 0.003) + 1.0) * 0.5;
  uint8_t intensity = 80 + (uint8_t)(breath * 175);
  uint16_t iconColor = RGB565((uint8_t)(0x63 * intensity / 255),
                              (uint8_t)(0x66 * intensity / 255),
                              (uint8_t)(0xF1 * intensity / 255));

  // 1. 绘制滚动文字 (背景层)
  if (millis() - _state.lastScrollTime >= 50) {
    _state.lastScrollTime = millis();
    _state.scrollX--;
    if (_state.scrollX < 9 - _state.scrollTextWidth)
      _state.scrollX = MATRIX_WIDTH + 2;
  }
  matrix->setTextColor(0xFFFF);
  matrix->setCursor(_state.scrollX, 6);
  matrix->print(_state.line2);

  // 2. 清除左侧图标区域 (遮罩)
  // 填充黑色矩形覆盖文字，防止文字穿透图标
  matrix->fillRect(0, 0, 9, 8, 0);

  // 3. 绘制图标和分隔线 (前景层)
  _drawWiFiIcon(0, 0, iconColor);

  uint16_t dimColor = RGB565(0x30, 0x30, 0x50);
  for (int i = 0; i < 8; i++)
    matrix->drawPixel(8, i, dimColor);
}

void DisplayManager_::_renderConnecting() {
  unsigned long now = millis();
  if (now - _state.lastAnimTime >= 150) {
    _state.lastAnimTime = now;
    _state.animFrame = (_state.animFrame + 1) % MATRIX_WIDTH;
  }

  uint16_t dotColor = RGB565(0x63, 0x66, 0xF1);
  uint16_t trailColor = RGB565(0x30, 0x30, 0x78);
  uint16_t dimTrail = RGB565(0x18, 0x18, 0x3C);

  for (int i = 0; i < 3; i++) {
    int pos = (_state.animFrame + i * 4) % MATRIX_WIDTH;
    matrix->drawPixel(pos, 0, (i == 0) ? dotColor : trailColor);
    if (i == 0) {
      matrix->drawPixel((pos - 1 + MATRIX_WIDTH) % MATRIX_WIDTH, 0, trailColor);
      matrix->drawPixel((pos - 2 + MATRIX_WIDTH) % MATRIX_WIDTH, 0, dimTrail);
    }
  }

  if (millis() - _state.lastScrollTime >= 55) {
    _state.lastScrollTime = millis();
    _state.scrollX--;
    if (_state.scrollX < -_state.scrollTextWidth)
      _state.scrollX = MATRIX_WIDTH + 2;
  }
  matrix->setTextColor(0xFFFF);
  matrix->setCursor(_state.scrollX, 7);
  matrix->print(_state.line2);
}

void DisplayManager_::_renderConnected() {
  unsigned long elapsed = millis() - _state.startTime;
  if (elapsed > 5000) {
    _state.status = DISPLAY_NORMAL;
    AP_MODE = false;
    return;
  }

  float breath = (sin(elapsed * 0.005) + 1.0) * 0.5;
  uint8_t gBright = 100 + (uint8_t)(breath * 80);
  uint16_t checkColor = RGB565(0x10, gBright, 0x30);

  // 1. 绘制滚动文字 (背景层)
  if (millis() - _state.lastScrollTime >= 50) {
    _state.lastScrollTime = millis();
    _state.scrollX--;
    if (_state.scrollX < 9 - _state.scrollTextWidth)
      _state.scrollX = MATRIX_WIDTH + 2;
  }
  matrix->setTextColor(0xFFFF);
  matrix->setCursor(_state.scrollX, 6);
  matrix->print(_state.line2);

  // 2. 清除 Check 图标区域 (遮罩)
  matrix->fillRect(0, 0, 9, 8, 0);

  // 3. 绘制 Check 图标 (前景层)
  const int8_t checkX[] = {1, 2, 3, 4, 5, 6, 7};
  const int8_t checkY[] = {5, 6, 7, 6, 5, 4, 3};
  for (int i = 0; i < 7; i++)
    matrix->drawPixel(checkX[i], checkY[i], checkColor);
}

void DisplayManager_::_renderConnectFailed() {
  unsigned long elapsed = millis() - _state.startTime;
  if (elapsed > 3000) {
    setDisplayStatus(DISPLAY_AP_MODE, _state.line1, "192.168.4.1");
    return;
  }

  if ((elapsed / 300) % 2 != 0)
    return;

  uint16_t xColor = RGB565(0xEF, 0x44, 0x44);
  for (int i = 0; i < 6; i++) {
    matrix->drawPixel(1 + i, 1 + i, xColor);
    matrix->drawPixel(6 - i, 1 + i, xColor);
  }
  matrix->setTextColor(xColor);
  matrix->setCursor(10, 6);
  matrix->print("FAIL");
}
