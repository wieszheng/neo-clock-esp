#ifndef MATRIX_DISPLAY_UI_H
#define MATRIX_DISPLAY_UI_H

#include <FastLED_NeoMatrix.h>
#include <vector>
#include "FastFramePlayer.h"

enum AppState
{
    FIXED,
    IN_TRANSITION
};

enum AnimationDirection
{
    SLIDE_UP,
    SLIDE_DOWN,
    SLIDE_LEFT,
    SLIDE_RIGHT
};

struct MatrixDisplayUiState
{
    AppState appState = FIXED;
    int currentApp = 0;
    int ticksSinceLastStateSwitch = 0;
    int8_t appTransitionDirection = 1;
    bool manuelControll = false;
    unsigned long lastUpdate = 0;
};

typedef void (*AppCallback)(FastLED_NeoMatrix *, MatrixDisplayUiState *, int16_t, int16_t, FastFramePlayer *);
typedef void (*OverlayCallback)(FastLED_NeoMatrix *, MatrixDisplayUiState *, FastFramePlayer *);

struct AppData
{
    String name;
    AppCallback callback;
    bool enabled;
    int position;
    uint16_t duration; // 应用显示时长(ms)，0表示使用全局时长
};

class MatrixDisplayUi
{
private:
    FastLED_NeoMatrix *matrix;
    MatrixDisplayUiState state;
    std::vector<AppData> apps;

    int ticksPerApp;
    int ticksPerTransition;
    float updateInterval;
    AnimationDirection appAnimationDirection;
    int nextAppNumber;
    int8_t lastTransitionDirection;
    bool setAutoTransition;

    // Overlay支持

    OverlayCallback *overlayFunctions;
    uint8_t overlayCount;

    int getNextAppNumber();
    void drawApp();
    void drawOverlays();
    void tick();

public:
    MatrixDisplayUi(FastLED_NeoMatrix *matrix);

    void init();
    void setTargetFPS(uint8_t fps);
    void setTimePerApp(uint16_t time);
    void setTimePerTransition(uint16_t time);
    void setAppAnimation(AnimationDirection dir);
    void setApps(const std::vector<AppData> &appList);
    void setOverlays(OverlayCallback *overlayFunctions, uint8_t overlayCount);

    void enablesetAutoTransition();
    void disablesetAutoTransition();

    void nextApp();
    void previousApp();
    void transitionToApp(uint8_t app);
    void switchToApp(uint8_t app);

    int8_t update();
    MatrixDisplayUiState *getUiState();

    int AppCount = 0;
};

#endif