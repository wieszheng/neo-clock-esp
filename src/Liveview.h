#ifndef LIVEVIEW_H
#define LIVEVIEW_H

#include <Arduino.h>
#include <FastLED_NeoMatrix.h>
#include <FastLED.h>
#include "Globals.h"

class Liveview {
private:
    FastLED_NeoMatrix* _matrix;
    CRGB* _leds;
    uint16_t _interval;
    unsigned long _lastUpdate;
    void (*callbackFunction)(const char*, size_t);
    
    // 缓冲区大小：前缀(3) + 像素数据(256*3) = 771
    static const size_t _LIVEVIEW_PREFIX_LENGHT = 3;
    static const size_t _LIVEVIEW_SUFFIX_LENGHT = 0;
    static const size_t _LIVEVIEW_BUFFER_LENGHT = _LIVEVIEW_PREFIX_LENGHT + MATRIX_HEIGHT * MATRIX_WIDTH * 3 + _LIVEVIEW_SUFFIX_LENGHT;
    
    char _liveviewBuffer[_LIVEVIEW_BUFFER_LENGHT + 1]; // +1 for null terminator
    uint32_t _lastChecksum;
    
    static const char _LIVEVIEW_PREFIX[];
    
    void fillBuffer();
    
    // 简单的 CRC32 计算
    class CRC32 {
    public:
        static uint32_t calculate(byte* data, size_t length) {
            uint32_t crc = 0xFFFFFFFF;
            for (size_t i = 0; i < length; i++) {
                crc ^= data[i];
                for (int j = 0; j < 8; j++) {
                    if (crc & 1) {
                        crc = (crc >> 1) ^ 0xEDB88320;
                    } else {
                        crc >>= 1;
                    }
                }
            }
            return ~crc;
        }
    };

public:
    Liveview();
    void begin(FastLED_NeoMatrix* matrix, CRGB* leds, uint16_t interval);
    void setCallback(void (*func)(const char*, size_t));
    void loop();
};

#endif

