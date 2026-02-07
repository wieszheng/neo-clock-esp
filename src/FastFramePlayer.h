#ifndef FAST_FRAME_PLAYER_H
#define FAST_FRAME_PLAYER_H

#include <FastLED_NeoMatrix.h>
#include <LittleFS.h>
#include "Icons.h" // 你的内置图标库

#define MAX_ICON_PIXELS 256

class FastFramePlayer {
private:
    FastLED_NeoMatrix *mtx;
    
    // --- 双轨资源 ---
    StaticIcon _flashIcon;      // 用于存储内置图标信息
    File _fsFile;               // 用于存储文件对象
    bool _isFsMode = false;     // 标记当前模式：true=文件, false=Flash
    
    // --- 播放状态 ---
    String _currentPath = "";   // 记录当前加载的路径或ID
    uint8_t _curFrame = 0;
    unsigned long _lastTime = 0;
    uint16_t _frameDelay = 0;

    // 缓冲区（支持最大图标尺寸）
    uint16_t _frameBuffer[MAX_ICON_PIXELS]; 

public:
    FastFramePlayer() : mtx(nullptr) {}
    void setMatrix(FastLED_NeoMatrix *m) { mtx = m; }

    // --- 模式A: 加载内置图标 (传入索引) ---
    void loadSystem(int index) {
        String newId = "SYS:" + String(index);
        if (_currentPath == newId) return; // 防抖

        // 清理旧资源
        cleanup();
        
        _isFsMode = false;
        _currentPath = newId;
        memcpy_P(&_flashIcon, &ICON_LIB[index], sizeof(StaticIcon));
    
        // 验证尺寸
        if (_flashIcon.width * _flashIcon.height > MAX_ICON_PIXELS) {
            _currentPath = "";
            return;
        }

        resetPlayback();
    }

    // --- 模式B: 加载社区/自定义图标 (传入文件名) ---
    bool loadUser(String filename) {
        String newId = "FS:" + filename;
        if (_currentPath == newId) return true;

        String path = "/icons/" + filename;
        // Serial.println("Loading FastFrame from: " + path);
        if (!LittleFS.exists(path)) return false;

        // 清理旧资源
        cleanup();

        _fsFile = LittleFS.open(path, "r");
        if (!_fsFile) return false;

        // 读取文件头 (对应 StaticIcon 结构)
        // 假设文件头存储顺序: width(1), height(1), frames(1), delay(2)
        // 注意：这里需要你生成文件时遵循这个协议
        uint8_t header[5];
        if (_fsFile.read(header, 5) != 5) {
            _fsFile.close();
            return false;
        }
        
        _flashIcon.width  = header[0];
        _flashIcon.height = header[1];
        _flashIcon.frames = header[2];
        _flashIcon.delay  = header[3] | (header[4] << 8); // 小端序

        // 验证尺寸
        if (_flashIcon.width * _flashIcon.height > MAX_ICON_PIXELS) {
            _fsFile.close();
            return false;
        }

        _isFsMode = true;
        _currentPath = newId;
        resetPlayback();
        return true;
    }

    void play(int16_t x, int16_t y) {
        if (_currentPath == "") return;

        uint16_t pixelCount = _flashIcon.width * _flashIcon.height;
        if (pixelCount == 0 || pixelCount > MAX_ICON_PIXELS) return;

        // 1. 时间控制（仅多帧动画需要）
        if (_flashIcon.frames > 1 && _flashIcon.delay > 0) {
            if (millis() - _lastTime >= _flashIcon.delay) {
                _lastTime = millis();
                _curFrame = (_curFrame + 1) % _flashIcon.frames;
                loadCurrentFrame();  // 加载新帧数据
            }
        } else if (_curFrame == 0) {
            // 单帧图标：只加载一次
            loadCurrentFrame();
            _curFrame = 1;  // 防止重复加载
        }

        // 2. 渲染（从Buffer绘制）
        for (uint16_t i = 0; i < pixelCount; i++) {
            uint16_t px = i % _flashIcon.width;
            uint16_t py = i / _flashIcon.width;
            mtx->drawPixel(x + px, y + py, _frameBuffer[i]);
        }
    }

private:
    void cleanup() {
        if (_fsFile) {
            _fsFile.close();
        }
        _currentPath = "";
        memset(_frameBuffer, 0, sizeof(_frameBuffer));
    }

    void resetPlayback() {
        _curFrame = 0;
        _lastTime = millis();
        loadCurrentFrame();  // 预加载第一帧
    }

    void loadCurrentFrame() {
        uint16_t pixelCount = _flashIcon.width * _flashIcon.height;
        uint32_t frameSize = pixelCount * 2;  // 字节数
        
        if (_isFsMode) {
            // 文件模式
            if (!_fsFile) return;
            
            uint32_t offset = 5 + (_curFrame * frameSize);
            if (!_fsFile.seek(offset)) {
                _currentPath = "";
                return;
            }
            
            size_t bytesRead = _fsFile.read((uint8_t*)_frameBuffer, frameSize);
            if (bytesRead != frameSize) {
                _currentPath = "";
            }
        } else {
            // Flash模式
            uint32_t pixelOffset = _curFrame * pixelCount;
            
            for (uint16_t i = 0; i < pixelCount; i++) {
                _frameBuffer[i] = pgm_read_word(_flashIcon.data + pixelOffset + i);
            }
        }
    }
};
#endif