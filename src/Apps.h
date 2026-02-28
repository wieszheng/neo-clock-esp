/**
 * @file Apps.h
 * @brief 应用函数声明 — 时间、日期、天气等显示应用
 *
 * 本文件定义：
 *   - Apps 全局应用列表
 *   - 各应用绘制函数 (TimeApp, DateApp 等)
 *   - 覆盖层函数 (通知、闹钟、定时器等)
 *   - OverlayCallback 全局数组
 */

#ifndef APPS_H
#define APPS_H

#include "FastFramePlayer.h"
#include "MatrixDisplayUi.h"
#include <FastLED_NeoMatrix.h>
#include <vector>

// ==================================================================
// 全局应用列表
// ==================================================================

/// 全局应用列表 (由 DisplayManager 加载和排序)
extern std::vector<AppData> Apps;

// ==================================================================
// 覆盖层函数声明
// ==================================================================

/**
 * @brief 通知覆盖层
 * @param matrix 矩阵驱动指针
 * @param state UI 状态指针
 * @param player 动画播放器
 */
void NotifyOverlay(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                   FastFramePlayer *player);

/**
 * @brief 频谱覆盖层
 * @param matrix 矩阵驱动指针
 * @param state UI 状态指针
 * @param player 动画播放器
 */
void SpectrumOverlay(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                     FastFramePlayer *player);

/**
 * @brief 闹钟覆盖层
 * @param matrix 矩阵驱动指针
 * @param state UI 状态指针
 * @param player 动画播放器
 */
void AlarmOverlay(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                  FastFramePlayer *player);

/**
 * @brief 定时器覆盖层
 * @param matrix 矩阵驱动指针
 * @param state UI 状态指针
 * @param player 动画播放器
 */
void TimerOverlay(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                  FastFramePlayer *player);

// ==================================================================
// 应用绘制函数声明
// ==================================================================

/**
 * @brief 时间应用绘制函数
 * @param matrix 矩阵驱动指针
 * @param state UI 状态指针
 * @param x X 偏移量
 * @param y Y 偏移量
 * @param player 动画播放器
 */
void TimeApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state, int16_t x,
             int16_t y, FastFramePlayer *player);

/**
 * @brief 日期应用绘制函数
 * @param matrix 矩阵驱动指针
 * @param state UI 状态指针
 * @param x X 偏移量
 * @param y Y 偏移量
 * @param player 动画播放器
 */
void DateApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state, int16_t x,
             int16_t y, FastFramePlayer *player);

/**
 * @brief 温度应用绘制函数
 * @param matrix 矩阵驱动指针
 * @param state UI 状态指针
 * @param x X 偏移量
 * @param y Y 偏移量
 * @param player 动画播放器
 */
void TempApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state, int16_t x,
             int16_t y, FastFramePlayer *player);

/**
 * @brief 湿度应用绘制函数
 * @param matrix 矩阵驱动指针
 * @param state UI 状态指针
 * @param x X 偏移量
 * @param y Y 偏移量
 * @param player 动画播放器
 */
void HumApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state, int16_t x,
            int16_t y, FastFramePlayer *player);

/**
 * @brief 天气应用绘制函数
 * @param matrix 矩阵驱动指针
 * @param state UI 状态指针
 * @param x X 偏移量
 * @param y Y 偏移量
 * @param player 动画播放器
 */
void WeatherApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                int16_t x, int16_t y, FastFramePlayer *player);

/**
 * @brief 风速应用绘制函数
 * @param matrix 矩阵驱动指针
 * @param state UI 状态指针
 * @param x X 偏移量
 * @param y Y 偏移量
 * @param player 动画播放器
 */
void WindApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state, int16_t x,
             int16_t y, FastFramePlayer *player);

/**
 * @brief 频谱应用绘制函数
 * @param matrix 矩阵驱动指针
 * @param state UI 状态指针
 * @param x X 偏移量
 * @param y Y 偏移量
 * @param player 动画播放器
 */
void SpectrumApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                 int16_t x, int16_t y, FastFramePlayer *player);

// ==================================================================
// 覆盖层回调数组
// ==================================================================

/// 覆盖层回调数组 (声明为全局以避免局部变量生命周期问题)
extern OverlayCallback overlays[];

#endif