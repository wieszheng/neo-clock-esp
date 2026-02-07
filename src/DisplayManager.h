#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <FastLED_NeoMatrix.h>
#include "MatrixDisplayUi.h"
#include "Globals.h"
#include <FastLED.h>


class DisplayManager_ {
private:
    // Liveview 相关成员变量
    CRGB* _liveviewLeds;
    uint16_t _liveviewInterval;
    unsigned long _liveviewLastUpdate;
    void (*_liveviewCallback)(const char*, size_t);

    // Liveview 缓冲区大小：前缀(3) + 像素数据(256*3) = 771
    static const size_t _LIVEVIEW_PREFIX_LENGTH = 3;
    static const size_t _LIVEVIEW_BUFFER_LENGTH = _LIVEVIEW_PREFIX_LENGTH + MATRIX_HEIGHT * MATRIX_WIDTH * 3;

    char _liveviewBuffer[_LIVEVIEW_BUFFER_LENGTH + 1];
    uint32_t _lastChecksum;

    static const char _LIVEVIEW_PREFIX[];

    // Liveview 内部方法
    void _fillLiveviewBuffer();
    uint32_t _calculateCRC32(byte* data, size_t length);

public:
    static DisplayManager_& getInstance();

    void setup();
    void tick();
    void clear();
    void show();

    void setMatrixLayout(int layout);
    void setBrightness(uint8_t bri);
    void setTextColor(uint16_t color);
    void setMatrixState(bool on);

    void printText(int16_t x, int16_t y, const char* text, bool centered, bool ignoreUppercase);

    void defaultTextColor();
    void applyAllSettings();

    void loadNativeApps();
    void updateAppVector(const char* json);
    void setNewSettings(const char* json);

    void nextApp();
    void previousApp();

    void leftButton();
    void selectButton();
    void rightButton();

    // Liveview 公开方法
    void setLiveviewCallback(void (*func)(const char*, size_t));
};
extern DisplayManager_& DisplayManager;

#endif