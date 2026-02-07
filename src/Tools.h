#ifndef TOOLS_H
#define TOOLS_H

#include <FastLED_NeoMatrix.h>
#include <map>

// RGBtoHEX
String RGBtoHEX(int r, int g, int b);

uint16_t HEXtoColor(String hex);

uint32_t hexTo888(const char* hex);

uint32_t hsvToRgb(uint8_t h, uint8_t s, uint8_t v);

uint16_t getTextWidth(const char *text, bool ignoreUpperCase);

byte utf8ascii(byte ascii);

String utf8ascii(String s);


#endif