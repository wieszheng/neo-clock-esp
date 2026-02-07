#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <FastLED_NeoMatrix.h>
#include "MatrixDisplayUi.h"
#include "Globals.h"




class DisplayManager_ {
private:
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
    
};
extern DisplayManager_& DisplayManager;

#endif