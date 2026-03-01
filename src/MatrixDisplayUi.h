/**
 * @file MatrixDisplayUi.h
 * @brief LED 矩阵 UI 引擎头文件 — 定义应用框架、状态机和回调接口
 *
 * 本文件定义了整个 UI 系统的核心数据结构:
 *   - AppState / AnimationDirection   枚举类型
 *   - MatrixDisplayUiState            UI 状态机数据
 *   - AppData                         应用描述结构体
 *   - AppCallback / OverlayCallback   回调函数类型
 *   - MatrixDisplayUi                 UI 引擎类
 */

#ifndef MATRIX_DISPLAY_UI_H
#define MATRIX_DISPLAY_UI_H

#include "FastFramePlayer.h"
#include "DisplayManager.h"
#include <FastLED_NeoMatrix.h>
#include <vector>

// ==================================================================
// 枚举定义
// ==================================================================

/// 应用显示状态
enum AppState
{
  FIXED,        ///< 固定显示（当前应用正在展示）
  IN_TRANSITION ///< 过渡动画中（正在切换到下一应用）
};

/// 切换动画方向
enum AnimationDirection
{
  SLIDE_UP,   ///< 当前页上滑移出，新页从底部滑入
  SLIDE_DOWN, ///< 当前页下滑移出，新页从顶部滑入
  SLIDE_LEFT, ///< (预留) 左滑
  SLIDE_RIGHT ///< (预留) 右滑
};

// ==================================================================
// 状态数据
// ==================================================================

/// UI 引擎的运行时状态
struct MatrixDisplayUiState
{
  AppState appState = FIXED;         ///< 当前状态 (FIXED / IN_TRANSITION)
  int currentApp = 0;                ///< 当前显示的应用索引
  int ticksSinceLastStateSwitch = 0; ///< 从上次状态切换以来经过的 tick 数
  int8_t appTransitionDirection = 1; ///< 切换方向 (+1=正向, -1=反向)
  bool manuelControll = false;       ///< 是否处于手动控制模式
  unsigned long lastUpdate =
      0;                  ///< 上次 update() 调用的时间戳（unsigned 防溢出）
  int cachedNextApp = -1; ///< 过渡开始时锁定的目标 App 索引，-1=未锁定
};

// ==================================================================
// 回调类型
// ==================================================================

/**
 * @brief 应用绘制回调
 * @param matrix  矩阵驱动指针
 * @param state   UI 状态指针
 * @param x, y    绘制偏移量（过渡动画时非零）
 * @param player  FastFramePlayer 用于播放图标动画
 */
typedef void (*AppCallback)(FastLED_NeoMatrix *, MatrixDisplayUiState *,
                            int16_t, int16_t, FastFramePlayer *);

/**
 * @brief 覆盖层绘制回调
 * @param matrix  矩阵驱动指针
 * @param state   UI 状态指针
 * @param player  FastFramePlayer (覆盖层专用播放器)
 */
typedef void (*OverlayCallback)(FastLED_NeoMatrix *, MatrixDisplayUiState *,
                                FastFramePlayer *);

// ==================================================================
// 应用数据
// ==================================================================

/// 描述一个显示应用的元数据
struct AppData
{
  String name;          ///< 应用名称 (如 "time", "date")
  AppCallback callback; ///< 绘制回调函数
  bool enabled;         ///< 是否启用
  int position;         ///< 排序位置（数值小的排前面）
  uint16_t duration;    ///< 自定义显示时长 (ms)，0 = 使用全局时长
};

// ==================================================================
// UI 引擎
// ==================================================================

/**
 * @class MatrixDisplayUi
 * @brief LED 矩阵 UI 引擎
 *
 * 核心功能:
 *   - 管理多个 App 的轮播显示
 *   - 提供上下滑动过渡动画
 *   - 支持手动和自动切换
 *   - 在 App 之上绘制覆盖层 (通知/闹钟等)
 *   - 帧率控制和丢帧补偿
 */
class MatrixDisplayUi
{
private:
  FastLED_NeoMatrix *matrix;  ///< 矩阵硬件驱动
  MatrixDisplayUiState state; ///< 运行时状态
  std::vector<AppData> apps;  ///< 已注册的应用列表

  int ticksPerApp;                          ///< 每个应用的显示 tick 数
  int ticksPerTransition;                   ///< 过渡动画的 tick 数
  float updateInterval;                     ///< 帧间隔 (ms)
  AnimationDirection appAnimationDirection; ///< 动画方向
  int nextAppNumber;                        ///< 手动指定的下一应用 (-1=未指定)
  int8_t lastTransitionDirection;           ///< 手动控制前的方向（用于恢复）
  bool setAutoTransition;                   ///< 是否启用自动轮播
  int _enabledAppCount = 0;                 ///< 缓存的已启用 App 数量，避免每帧遍历

  // 覆盖层
  OverlayCallback *overlayFunctions; ///< 覆盖层回调数组
  uint8_t overlayCount;              ///< 覆盖层数量

  // --- 内部方法 ---
  int getNextAppNumber();      ///< 计算下一应用索引（有副作用，仅在过渡开始时调用一次）
  void drawApp();              ///< 渲染当前/过渡中的应用
  void drawOverlays();         ///< 渲染覆盖层
  void tick();                 ///< 状态机推进 + 绘制一帧
  void _rebuildEnabledCount(); ///< 预计算并缓存 enabledCount [Fix3]

public:
  MatrixDisplayUi(FastLED_NeoMatrix *matrix);

  // 初始化
  void init();

  // 参数配置
  void setTargetFPS(uint8_t fps);
  void setTimePerApp(uint16_t time);
  void setTimePerTransition(uint16_t time);
  void setAppAnimation(AnimationDirection dir);
  void setApps(const std::vector<AppData> &appList);
  void setOverlays(OverlayCallback *overlayFunctions, uint8_t overlayCount);

  // 自动切换控制
  void enablesetAutoTransition();
  void disablesetAutoTransition();

  // 导航
  void nextApp();
  void previousApp();
  void transitionToApp(uint8_t app);
  void switchToApp(uint8_t app);

  // 帧率控制入口
  int8_t update();
  MatrixDisplayUiState *getUiState();

  int AppCount = 0; ///< 当前已注册的应用数量
};

#endif // MATRIX_DISPLAY_UI_H