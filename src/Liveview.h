#ifndef LIVEVIEW_H
#define LIVEVIEW_H

#include "Globals.h"
#include <Arduino.h>
#include <FastLED.h>

/**
 * Liveview — 将 LED 矩阵像素数据实时推送给回调（如 WebSocket）。
 *
 * 两阶段设计，避免 TCP 阻塞拖累渲染帧率：
 *   1. tick()  — 紧随 DisplayManager.tick() 调用，仅做内存采样+CRC，极快
 *   2. flush() — 放在 ServerManager.tick(ws->loop()) 之后调用，执行实际发送
 *
 * 使用方式（main.cpp loop）：
 *   DisplayManager.tick();   // 渲染
 *   Liveview.tick();         // 采样（≈几十μs，不阻塞）
 *   ServerManager.tick();    // ws->loop() 先处理收包
 *   Liveview.flush();        // 此时 TCP 发送缓冲区通常有空间，阻塞概率最低
 */
class Liveview_ {
public:
  static Liveview_ &getInstance();

  /** 绑定要读取的 LED 缓冲区（通常就是 FastLED 的 leds 数组） */
  void setLeds(CRGB *leds);

  /** 设置采样间隔（毫秒，0 = 禁用） */
  void setInterval(uint16_t ms);

  /** 设置数据推送回调函数（应调用 ws->broadcastBIN 等） */
  void setCallback(void (*func)(const char *, size_t));

  /**
   * 采样阶段：仅读取 leds[] 到内部缓冲区并计算 CRC，不发送。
   * 应紧随 DisplayManager.tick() 调用，保证读到最新帧数据。
   */
  void tick();

  /**
   * 发送阶段：若有新帧（CRC 变化），调用回调执行实际网络发送。
   * 应放在 ServerManager.tick() 之后，此时 TCP 缓冲区最宽裕。
   */
  void flush();

private:
  Liveview_() {}

  CRGB *_leds = nullptr;
  uint16_t _interval = 250;
  unsigned long _lastSample = 0;

  void (*_callback)(const char *, size_t) = nullptr;

  uint32_t _pendingChecksum = 0; // 本轮采样结果
  uint32_t _sentChecksum = 0;    // 上次已发送的 CRC
  bool _dirty = false;           // tick() 采样到新帧，等待 flush()

  // 帧缓冲区：前缀(3) + 像素数据(MATRIX_WIDTH * MATRIX_HEIGHT * 3)
  static const size_t _PREFIX_LEN = 3;
  static const size_t _BUF_LEN = _PREFIX_LEN + MATRIX_WIDTH * MATRIX_HEIGHT * 3;
  static const char _PREFIX[]; // "LV:"

  char _buf[_BUF_LEN + 1];
  size_t _bufLen = 0;

  uint32_t _crc32(const byte *data, size_t length);
};

extern Liveview_ &Liveview;

#endif // LIVEVIEW_H
