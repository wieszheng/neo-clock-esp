#ifndef APPS_H
#define APPS_H

#include "FastFramePlayer.h"
#include "MatrixDisplayUi.h"
#include <FastLED_NeoMatrix.h>
#include <vector>

extern std::vector<AppData> Apps;

// 覆盖层函数声明
void NotifyOverlay(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                   FastFramePlayer *player);
void SpectrumOverlay(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                     FastFramePlayer *player);
void AlarmOverlay(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                  FastFramePlayer *player);
void TimerOverlay(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                  FastFramePlayer *player);

void TimeApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state, int16_t x,
             int16_t y, FastFramePlayer *player);
void DateApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state, int16_t x,
             int16_t y, FastFramePlayer *player);
void TempApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state, int16_t x,
             int16_t y, FastFramePlayer *player);
void HumApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state, int16_t x,
            int16_t y, FastFramePlayer *player);
void WeatherApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                int16_t x, int16_t y, FastFramePlayer *player);
void WindApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state, int16_t x,
             int16_t y, FastFramePlayer *player);
void SpectrumApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                 int16_t x, int16_t y, FastFramePlayer *player);

// 覆盖层回调数组(声明为全局以避免局部变量生命周期问题)
extern OverlayCallback overlays[];

#endif