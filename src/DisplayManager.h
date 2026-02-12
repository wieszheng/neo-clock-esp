#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "Globals.h"
#include "MatrixDisplayUi.h"
#include <Arduino.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>
#include <vector>

// 显示状态枚举
enum DisplayStatus {
  DISPLAY_NORMAL = 0,    // 正常应用显示
  DISPLAY_AP_MODE,       // AP 配网模式 - 显示热点信息
  DISPLAY_CONNECTING,    // WiFi 连接中动画
  DISPLAY_CONNECTED,     // 连接成功 - 短暂显示 IP
  DISPLAY_CONNECT_FAILED // 连接失败提示
};

class DisplayManager_ {
private:
  // Liveview 相关成员变量
  CRGB *_liveviewLeds;
  uint16_t _liveviewInterval;
  unsigned long _liveviewLastUpdate;
  void (*_liveviewCallback)(const char *, size_t);

  // Liveview 缓冲区大小：前缀(3) + 像素数据(256*3) = 771
  static const size_t _LIVEVIEW_PREFIX_LENGTH = 3;
  static const size_t _LIVEVIEW_BUFFER_LENGTH =
      _LIVEVIEW_PREFIX_LENGTH + MATRIX_HEIGHT * MATRIX_WIDTH * 3;

  char _liveviewBuffer[_LIVEVIEW_BUFFER_LENGTH + 1];
  uint32_t _lastChecksum;

  static const char _LIVEVIEW_PREFIX[];

  // Liveview 内部方法
  void _fillLiveviewBuffer();
  uint32_t _calculateCRC32(byte *data, size_t length);

  // ===== 状态显示相关 =====
  struct DisplayState {
    DisplayStatus status = DISPLAY_NORMAL;
    String line1;                // 主显示文字
    String line2;                // 副显示文字 (滚动)
    int16_t scrollX = 0;         // 滚动位置
    int16_t scrollTextWidth = 0; // 滚动文字像素宽度
    unsigned long lastScrollTime = 0;
    unsigned long startTime = 0;
    uint8_t animFrame = 0;
    unsigned long lastAnimTime = 0;
  } _state;

  // 状态画面渲染方法
  void _renderAPMode();
  void _renderConnecting();
  void _renderConnected();
  void _renderConnectFailed();
  void _drawWiFiIcon(int16_t x, int16_t y, uint16_t color);
  void _drawScrollText(int16_t y, const String &text, uint16_t color);

public:
  static DisplayManager_ &getInstance();

  void setup();
  void tick();
  void clear();
  void show();

  void setMatrixLayout(int layout);
  void setBrightness(uint8_t bri);
  void setTextColor(uint16_t color);
  void setMatrixState(bool on);

  void printText(int16_t x, int16_t y, const char *text, bool centered,
                 bool ignoreUppercase);

  void defaultTextColor();
  void applyAllSettings();

  void loadNativeApps();
  void updateAppVector(const char *json);
  void setNewSettings(const char *json);

  void nextApp();
  void previousApp();

  void leftButton();
  void selectButton();
  void rightButton();

  // Liveview 公开方法
  void setLiveviewCallback(void (*func)(const char *, size_t));

  // ===== 状态显示公开方法 =====
  /**
   * 设置显示状态
   * @param status  显示状态枚举
   * @param line1   主文字 (如 AP 名称)
   * @param line2   副文字 (如 IP 地址，会滚动显示)
   */
  void setDisplayStatus(DisplayStatus status, const String &line1 = "",
                        const String &line2 = "");

  /**
   * 获取当前显示状态
   */
  DisplayStatus getDisplayStatus() const { return _state.status; }
};
extern DisplayManager_ &DisplayManager;

#endif