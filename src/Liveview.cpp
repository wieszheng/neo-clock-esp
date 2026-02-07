#include "Liveview.h"

const char Liveview::_LIVEVIEW_PREFIX[] = "LV:";

Liveview::Liveview() {
    _matrix = nullptr;
    _leds = nullptr;
    _interval = 0;
    _lastUpdate = 0;
    callbackFunction = nullptr;
    _lastChecksum = 0;
    memset(_liveviewBuffer, 0, sizeof(_liveviewBuffer));
}

void Liveview::begin(FastLED_NeoMatrix* matrix, CRGB* leds, uint16_t interval) {
    this->_matrix = matrix;
    _leds = leds;
    _interval = interval;
    _lastUpdate = millis();
    callbackFunction = nullptr;
    _lastChecksum = 0;
}

void Liveview::setCallback(void (*func)(const char*, size_t)) {
    callbackFunction = func;
}

void Liveview::loop() {
    if (_interval > 0 && (millis() - _lastUpdate) >= _interval) {
        _lastUpdate = millis();
        if (callbackFunction != nullptr) {
            fillBuffer();
        }
    }
}

void Liveview::fillBuffer() {
    // 1. 设置头部 
    uint8_t* ptr = (uint8_t*)_liveviewBuffer;
    
    // 如果必须保留前缀（建议前缀也改短）
    memcpy(ptr, _LIVEVIEW_PREFIX, _LIVEVIEW_PREFIX_LENGHT); 
    ptr += _LIVEVIEW_PREFIX_LENGHT;

    // 2. 填充 LED 数值 (直接写入字节，不转字符串)
    for (int y = 0; y < MATRIX_HEIGHT; y++)
    {
        for (int x = 0; x < MATRIX_WIDTH; x++)
        {
            // 获取物理索引 (处理 ZigZag 排列)
            int index = this->_matrix->XY(x, y);
            
            // 直接赋值，极快
            *ptr++ = _leds[index].r;
            *ptr++ = _leds[index].g;
            *ptr++ = _leds[index].b;
        }
    }

    // 3. 设置后缀
    // memcpy(ptr, _LIVEVIEW_SUFFIX, _LIVEVIEW_SUFFIX_LENGHT);
    // ptr += _LIVEVIEW_SUFFIX_LENGHT;

    // 计算实际长度
    size_t totalLen = ptr - (uint8_t*)_liveviewBuffer;

    // 4. 计算 Checksum (现在是对二进制数据计算，速度更快)
    uint32_t newChecksum = CRC32::calculate((byte *)_liveviewBuffer, totalLen);
    
    if (_lastChecksum != newChecksum)
    {
        _lastChecksum = newChecksum;
        // 发送二进制数据
        callbackFunction((const char*)_liveviewBuffer, totalLen);
    }
}

