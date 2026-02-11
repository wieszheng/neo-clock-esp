/**
 * @file Tools.cpp
 * @brief 工具函数 — 颜色转换、文字宽度计算、UTF-8 处理
 *
 * ESP32 性能优化:
 *   - CharMap: std::map → 静态数组 (O(1) 查找, 零堆分配)
 *   - HEXtoColor: 去除 String 操作，使用纯 C 字符串解析
 *   - getTextWidth: 直接数组索引，消除 map::count + map::operator[]
 *   - utf8ascii: 增加 C 字符串重载，避免不必要的 String 构造
 */

#include "Tools.h"

// ==================================================================
// 字符宽度查找表 (替代 std::map, O(1) 查找)
// ==================================================================
static const uint8_t CHAR_WIDTH_TABLE[256] = {
    // 0-31: 控制字符
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0,
    // 32-63: 空格、标点、数字
    2, 2, 4, 4, 4, 4, 4, 2, 3, 3, 4, 4, 3, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 2, 3, 4, 4, 4, 4,
    // 64-95: 大写字母
    4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 4, 4, 4, 6, 5, 4, 4, 5, 4, 4, 4, 4, 4, 6, 4,
    4, 4, 4, 4, 4, 4, 4,
    // 96-127: 小写字母
    3, 4, 4, 4, 4, 4, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 2, 4, 4, 0,
    // 128-255: 扩展字符 (默认大多为 4)
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 2, 4, 4, 4, 4, 3, 4, 3, 4, 4,
    4, 4, 4, 4, 3, 4, 4, 4, 4, 2, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4};

// ==================================================================
// 颜色转换
// ==================================================================
static inline uint8_t hexCharToVal(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return 0;
}

static inline uint8_t hexPairToU8(const char *p) {
  return (hexCharToVal(p[0]) << 4) | hexCharToVal(p[1]);
}

String RGBtoHEX(int r, int g, int b) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%02X%02X%02X", r, g, b);
  return String(buf);
}

uint16_t HEXtoColor(const char *hex) {
  if (!hex || *hex == '\0')
    return 0;
  if (*hex == '#')
    hex++;

  size_t len = 0;
  const char *p = hex;
  while (*p && len < 7) {
    len++;
    p++;
  }
  if (len < 6)
    return 0;

  uint8_t r = hexPairToU8(hex);
  uint8_t g = hexPairToU8(hex + 2);
  uint8_t b = hexPairToU8(hex + 4);

  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

uint32_t hexTo888(const char *hex) {
  if (!hex)
    return 0;
  if (*hex == '#')
    hex++;

  uint8_t r = hexPairToU8(hex);
  uint8_t g = hexPairToU8(hex + 2);
  uint8_t b = hexPairToU8(hex + 4);
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

uint32_t hsvToRgb(uint8_t h, uint8_t s, uint8_t v) {
  CHSV hsv(h, s, v);
  CRGB rgb;
  hsv2rgb_spectrum(hsv, rgb);
  return ((uint16_t)(rgb.r & 0xF8) << 8) | ((uint16_t)(rgb.g & 0xFC) << 3) |
         (rgb.b >> 3);
}

// ==================================================================
// 文字宽度计算
// ==================================================================
uint16_t getTextWidth(const char *text, bool ignoreUpperCase) {
  uint16_t width = 0;
  for (const char *c = text; *c != '\0'; ++c) {
    uint8_t ch = ignoreUpperCase ? (uint8_t)*c : (uint8_t)toupper(*c);
    uint8_t w = CHAR_WIDTH_TABLE[ch];
    width += (w > 0) ? w : 4;
  }
  return width;
}

// ==================================================================
// UTF-8 → ASCII 转换
// ==================================================================
static byte c1;

byte utf8ascii(byte ascii) {
  if (ascii < 128) {
    c1 = 0;
    return ascii;
  }

  byte last = c1;
  c1 = ascii;

  switch (last) {
  case 0xC2:
    return ascii - 34;
  case 0xC3:
    return (ascii | 0xC0) - 34;
  case 0x82:
    if (ascii == 0xAC)
      return 0xEA;
    break;
  }
  return 0;
}

String utf8ascii(String s) {
  // 复用 char* 版本
  return utf8ascii(s.c_str());
}

String utf8ascii(const char *s) {
  size_t len = strlen(s);
  char buf[len + 1];
  size_t j = 0;

  for (size_t i = 0; i < len; i++) {
    char c = utf8ascii((byte)s[i]);
    if (c != 0)
      buf[j++] = c;
  }
  buf[j] = '\0';
  return String(buf);
}