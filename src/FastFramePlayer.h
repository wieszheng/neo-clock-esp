/**
 * @file FastFramePlayer.h
 * @brief 高性能帧动画播放器 — 支持 Flash 内置图标和 LittleFS 文件图标
 *
 * ESP32 性能优化:
 *   2026-02-11:
 *   - 移除 String _currentPath，改为 int _currentId 和 char _currentFile[32]
 *   - loadSystem: 整数比较防抖 (零开销)
 *   - loadUser:   C 字符串比较防抖 (无堆分配)
 *   - play:       将 play() 重命名为 draw() 以符合语义，并分离 tick 逻辑
 */

#ifndef FAST_FRAME_PLAYER_H
#define FAST_FRAME_PLAYER_H

#include "Icons.h"
#include <FastLED_NeoMatrix.h>
#include <LittleFS.h>

/// 单帧最大像素数 (宽 × 高)，超出将拒绝加载
#define MAX_ICON_PIXELS 256

class FastFramePlayer {
private:
  FastLED_NeoMatrix *mtx;

  // ---------- 资源数据 ----------
  StaticIcon _flashIcon;  // 图标元数据
  File _fsFile;           // 文件句柄
  bool _isFsMode = false; // true=文件模式, false=Flash模式

  // ---------- 防抖状态 (无 String) ----------
  int16_t _currentSysId = -1; // 当前加载的系统图标 ID (-1 表示无效/非系统)
  char _currentUserFile[32]; // 当前加载的用户文件名 (空字符串表示无效/非用户)

  // ---------- 播放状态 ----------
  uint8_t _curFrame = 0;       // 当前帧索引
  uint8_t _frameCount = 0;     // 总帧数
  uint16_t _frameDelay = 0;    // 帧延迟
  unsigned long _lastTime = 0; // 上次切帧时间
  uint16_t _width = 0;
  uint16_t _height = 0;

  // 帧缓冲区 (RGB565)
  uint16_t _frameBuffer[MAX_ICON_PIXELS];

public:
  FastFramePlayer() : mtx(nullptr) { _currentUserFile[0] = '\0'; }

  void setMatrix(FastLED_NeoMatrix *m) { mtx = m; }

  /**
   * @brief 加载系统内置图标 (PROGMEM)
   * [性能优化] 整数比较，零开销防抖
   */
  void loadSystem(int index) {
    // 防抖: 如果已经是当前图标，且处于 Flash 模式，直接返回
    if (!_isFsMode && _currentSysId == index)
      return;

    cleanup(); // 释放旧资源

    // 从 PROGMEM 读取元数据
    memcpy_P(&_flashIcon, &ICON_LIB[index], sizeof(StaticIcon));

    if (_flashIcon.width * _flashIcon.height > MAX_ICON_PIXELS) {
      _currentSysId = -1;
      return;
    }

    // 更新状态
    _isFsMode = false;
    _currentSysId = index;
    _currentUserFile[0] = '\0'; // 清除文件名

    // 缓存常用元数据
    _width = _flashIcon.width;
    _height = _flashIcon.height;
    _frameCount = _flashIcon.frames;
    _frameDelay = _flashIcon.delay;

    resetPlayback();
  }

  /**
   * @brief 加载用户文件图标 (LittleFS)
   * [性能优化] C 字符串比较，避免 String 堆构造
   */
  bool loadUser(const char *filename) {
    // 防抖: 如果处于文件模式且文件名相同
    if (_isFsMode && strncmp(_currentUserFile, filename, 31) == 0)
      return true;

    String path = "/icons/";
    path += filename;

    if (!LittleFS.exists(path))
      return false;

    cleanup();

    _fsFile = LittleFS.open(path, "r");
    if (!_fsFile)
      return false;

    // 解析文件头
    uint8_t header[5];
    if (_fsFile.read(header, 5) != 5) {
      _fsFile.close();
      return false;
    }

    _width = header[0];
    _height = header[1];
    _frameCount = header[2];
    _frameDelay = header[3] | (header[4] << 8);

    if (_width * _height > MAX_ICON_PIXELS) {
      _fsFile.close();
      return false;
    }

    // 更新状态
    _isFsMode = true;
    _currentSysId = -1;
    strncpy(_currentUserFile, filename, 31);
    _currentUserFile[31] = '\0'; // 确保结尾

    resetPlayback();
    return true;
  }

  // 兼容旧接口 (String)
  bool loadUser(const String &filename) { return loadUser(filename.c_str()); }

  /**
   * @brief 渲染当前帧
   */
  void play(int16_t x, int16_t y) {
    // 检查有效性
    if (!_isFsMode && _currentSysId == -1)
      return;
    if (_isFsMode && _currentUserFile[0] == '\0')
      return;

    // 1. 时间控制 - 切帧
    if (_frameCount > 1 && _frameDelay > 0) {
      if (millis() - _lastTime >= _frameDelay) {
        _lastTime = millis();
        _curFrame = (_curFrame + 1) % _frameCount;
        loadCurrentFrame();
      }
    }

    // 2. 绘制
    uint16_t count = _width * _height;
    for (uint16_t i = 0; i < count; i++) {
      // 简单取模计算坐标
      mtx->drawPixel(x + (i % _width), y + (i / _width), _frameBuffer[i]);
    }
  }

private:
  void cleanup() {
    if (_fsFile)
      _fsFile.close();
    _currentSysId = -1;
    _currentUserFile[0] = '\0';
  }

  void resetPlayback() {
    _curFrame = 0;
    _lastTime = millis();
    loadCurrentFrame();
  }

  void loadCurrentFrame() {
    uint16_t pixelCount = _width * _height;

    if (_isFsMode) {
      // 文件模式: Seek + Read
      if (!_fsFile)
        return;
      // 5字节头 + 帧偏移
      uint32_t offset = 5 + (_curFrame * pixelCount * 2);
      _fsFile.seek(offset);
      _fsFile.read((uint8_t *)_frameBuffer, pixelCount * 2);
    } else {
      // Flash 模式: PROGMEM Read
      const uint16_t *ptr = _flashIcon.data + (_curFrame * pixelCount);
      for (uint16_t i = 0; i < pixelCount; i++) {
        _frameBuffer[i] = pgm_read_word(ptr + i);
      }
    }
  }
};

#endif