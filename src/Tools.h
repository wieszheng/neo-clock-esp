/**
 * @file Tools.h
 * @brief 工具函数声明 — 颜色转换、文字宽度计算、UTF-8 处理
 */

#ifndef TOOLS_H
#define TOOLS_H

#include <FastLED_NeoMatrix.h>
// 注意: 移除了 <map>, CharMap 已替换为静态数组

/// RGB → HEX 字符串 (如 "FF0000")
String RGBtoHEX(int r, int g, int b);

/// HEX 颜色字符串 → RGB565 (支持 "#RRGGBB" 或 "RRGGBB")
uint16_t HEXtoColor(const char *hex);

/// HEX 颜色字符串 → RGB888 (24位)
uint32_t hexTo888(const char *hex);

/// HSV → RGB565
uint32_t hsvToRgb(uint8_t h, uint8_t s, uint8_t v);

/// 计算文字的像素宽度 (基于 AwtrixFont 字符宽度表)
uint16_t getTextWidth(const char *text, bool ignoreUpperCase);

/// UTF-8 单字节转换
byte utf8ascii(byte ascii);

/// UTF-8 字符串转 ASCII 字符串
String utf8ascii(String s);
String utf8ascii(const char *s);

#endif