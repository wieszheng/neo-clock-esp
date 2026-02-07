#ifndef APPS_H
#define APPS_H


#include <FastLED_NeoMatrix.h>
#include "MatrixDisplayUi.h"
#include "FastFramePlayer.h"
#include <vector>

extern std::vector<AppData> Apps;



// 覆盖层函数声明
void NotifyOverlay(FastLED_NeoMatrix* matrix, MatrixDisplayUiState* state);
void SpectrumOverlay(FastLED_NeoMatrix* matrix, MatrixDisplayUiState* state);
void AlarmOverlay(FastLED_NeoMatrix* matrix, MatrixDisplayUiState* state);
void TimerOverlay(FastLED_NeoMatrix* matrix, MatrixDisplayUiState* state);

void TimeApp(FastLED_NeoMatrix* matrix, MatrixDisplayUiState* state, int16_t x, int16_t y, FastFramePlayer *player);
void DateApp(FastLED_NeoMatrix* matrix, MatrixDisplayUiState* state, int16_t x, int16_t y, FastFramePlayer *player);
// void TempApp(FastLED_NeoMatrix* matrix, MatrixDisplayUiState* state, int16_t x, int16_t y, bool firstFrame, bool lastFrame);
// void HumApp(FastLED_NeoMatrix* matrix, MatrixDisplayUiState* state, int16_t x, int16_t y, bool firstFrame, bool lastFrame);
// void WeatherApp(FastLED_NeoMatrix* matrix, MatrixDisplayUiState* state, int16_t x, int16_t y, bool firstFrame, bool lastFrame);
// void WindApp(FastLED_NeoMatrix* matrix, MatrixDisplayUiState* state, int16_t x, int16_t y, bool firstFrame, bool lastFrame);


// 覆盖层回调数组(声明为全局以避免局部变量生命周期问题)
extern OverlayCallback overlays[];

#endif