/**
 * @file MatrixDisplayUi.cpp
 * @brief LED 矩阵 UI 引擎 — 管理应用切换、过渡动画和覆盖层绘制
 *
 * 核心概念:
 *   - App:     独立的显示页面（时间、日期、天气等），通过 AppCallback 绘制
 *   - State:   FIXED (固定显示) / IN_TRANSITION (过渡动画中)
 *   - Overlay: 叠加在 App 之上的覆盖层（通知、闹钟等），始终绘制
 *
 * 更新循环:
 *   update() → [帧率控制] → tick() → [状态机推进 + 绘制App + 绘制覆盖层]
 *
 * 时间单位:
 *   内部使用 "tick" 作为时间单位。1 tick = 1 帧 (由 updateInterval 决定)
 *   例如: 30FPS 时, ticksPerApp=150 ≈ 5秒
 */

#include "MatrixDisplayUi.h"
#include "AwtrixFont.h"
#include "Globals.h"

/// 双缓冲播放器:  player1 用于 App 渲染, player2 用于覆盖层
FastFramePlayer player1;
FastFramePlayer player2;

// ==================================================================
// 构造与初始化
// ==================================================================

MatrixDisplayUi::MatrixDisplayUi(FastLED_NeoMatrix *matrix) {
  this->matrix = matrix;
  this->updateInterval = 33.33;  // 1000ms / 30 FPS
  this->ticksPerApp = 150;       // 默认每页 5 秒 (150 ticks × 33ms)
  this->ticksPerTransition = 15; // 默认过渡 0.5 秒 (15 ticks × 33ms)
  this->appAnimationDirection = SLIDE_DOWN;
  this->nextAppNumber = -1;
  this->lastTransitionDirection = 1;
  this->setAutoTransition = true;
  this->overlayFunctions = nullptr;
  this->overlayCount = 0;
}

/**
 * @brief 初始化矩阵硬件和字体
 *
 * 必须在 setup() 中调用。初始化内容:
 *   - 矩阵硬件 begin()
 *   - 关闭文字换行
 *   - 设置初始亮度
 *   - 加载 AwtrixFont 像素字体
 *   - 绑定 FastFramePlayer 到矩阵
 */
void MatrixDisplayUi::init() {
  matrix->begin();
  matrix->setTextWrap(false); // LED 矩阵不需要自动换行
  matrix->setBrightness(BRIGHTNESS);
  this->matrix->setFont(&AwtrixFont);
  player1.setMatrix(this->matrix);
  player2.setMatrix(this->matrix);
}

// ==================================================================
// 参数配置
// ==================================================================

/**
 * @brief 设置目标帧率 (FPS)
 * @param fps 帧率 (推荐 20-60)
 *
 * @note 修改帧率时会同步缩放 ticksPerApp 和 ticksPerTransition，
 *       保持实际时间不变。例如: 30→60 FPS 时, ticks 自动翻倍。
 */
void MatrixDisplayUi::setTargetFPS(uint8_t fps) {
  float oldInterval = this->updateInterval;
  this->updateInterval = ((float)1.0 / (float)fps) * 1000;

  // 按帧率变化比例缩放 tick 计数
  float changeRatio = oldInterval / (float)this->updateInterval;
  this->ticksPerApp *= changeRatio;
  this->ticksPerTransition *= changeRatio;
}

void MatrixDisplayUi::enablesetAutoTransition() {
  this->setAutoTransition = true;
}
void MatrixDisplayUi::disablesetAutoTransition() {
  this->setAutoTransition = false;
}

/**
 * @brief 设置每个应用的显示时长
 * @param time 毫秒数 (如 5000 = 5 秒)
 */
void MatrixDisplayUi::setTimePerApp(uint16_t time) {
  this->ticksPerApp = (int)((float)time / (float)updateInterval);
}

/**
 * @brief 设置过渡动画时长
 * @param time 毫秒数 (如 500 = 0.5 秒)
 */
void MatrixDisplayUi::setTimePerTransition(uint16_t time) {
  this->ticksPerTransition = (int)((float)time / (float)updateInterval);
}

/**
 * @brief 设置应用切换的动画方向
 * @param dir SLIDE_UP 或 SLIDE_DOWN
 */
void MatrixDisplayUi::setAppAnimation(AnimationDirection dir) {
  this->appAnimationDirection = dir;
}

/**
 * @brief 设置应用列表
 * @param appList 包含所有 App 的 vector（name, callback, enabled, position,
 * duration）
 */
void MatrixDisplayUi::setApps(const std::vector<AppData> &appList) {
  this->apps = appList;
  this->AppCount = appList.size();
}

/**
 * @brief 设置覆盖层回调数组
 * @param overlayFunctions 回调函数数组
 * @param overlayCount     数组大小
 */
void MatrixDisplayUi::setOverlays(OverlayCallback *overlayFunctions,
                                  uint8_t overlayCount) {
  this->overlayFunctions = overlayFunctions;
  this->overlayCount = overlayCount;
}

// ==================================================================
// 应用导航
// ==================================================================

/**
 * @brief 切换到下一个应用（带过渡动画）
 * @note 仅在 FIXED 状态有效，IN_TRANSITION 中的调用被忽略
 */
void MatrixDisplayUi::nextApp() {
  if (this->state.appState != IN_TRANSITION) {
    this->state.manuelControll = true;
    this->state.appState = IN_TRANSITION;
    this->state.ticksSinceLastStateSwitch = 0;
    this->lastTransitionDirection = this->state.appTransitionDirection;
    this->state.appTransitionDirection = 1; // 正向
  }
}

/**
 * @brief 切换到上一个应用（带过渡动画）
 */
void MatrixDisplayUi::previousApp() {
  if (this->state.appState != IN_TRANSITION) {
    this->state.manuelControll = true;
    this->state.appState = IN_TRANSITION;
    this->state.ticksSinceLastStateSwitch = 0;
    this->lastTransitionDirection = this->state.appTransitionDirection;
    this->state.appTransitionDirection = -1; // 反向
  }
}

/**
 * @brief 过渡到指定索引的应用
 * @param app 目标应用索引
 * @note 自动判断过渡方向（正向/反向）
 */
void MatrixDisplayUi::transitionToApp(uint8_t app) {
  if (app >= this->AppCount)
    return;

  this->state.ticksSinceLastStateSwitch = 0;
  if (app == this->state.currentApp)
    return;

  this->nextAppNumber = app;
  this->lastTransitionDirection = this->state.appTransitionDirection;
  this->state.manuelControll = true;
  this->state.appState = IN_TRANSITION;
  // 按位置关系决定动画方向
  this->state.appTransitionDirection = app < this->state.currentApp ? -1 : 1;
}

/**
 * @brief 立即切换到指定应用（无过渡动画）
 * @param app 目标应用索引
 */
void MatrixDisplayUi::switchToApp(uint8_t app) {
  if (app >= this->AppCount)
    return;
  if (app == this->state.currentApp)
    return;

  this->state.currentApp = app;
  this->state.ticksSinceLastStateSwitch = 0;
  this->state.appState = FIXED;

  // 如果目标应用有自定义时长，使用它
  if (apps[app].duration > 0) {
    this->ticksPerApp =
        (int)((float)apps[app].duration / (float)updateInterval);
  }
}

// ==================================================================
// 核心：下一应用选择算法
// ==================================================================

/**
 * @brief 计算下一个要显示的应用索引
 *
 * 优先级:
 *   1. 如果有手动指定 (nextAppNumber)，直接使用
 *   2. 否则，在 enabled 应用列表中按方向循环跳转
 *
 * @return 下一个应用的索引
 * @note 仅遍历 enabled 状态的应用，被禁用的应用会被跳过
 */
int MatrixDisplayUi::getNextAppNumber() {
  // 1. 手动指定优先
  if (this->nextAppNumber != -1) {
    int temp = this->nextAppNumber;
    this->nextAppNumber = -1;
    return temp;
  }

  // 2. 构建已启用应用的索引列表
  // [性能优化] 使用栈数组替代 std::vector (避免堆分配)
  int enabledIndices[16]; // 最多支持 16 个应用 (足够)
  int enabledCount = 0;
  for (int i = 0; i < (int)apps.size() && enabledCount < 16; i++) {
    if (apps[i].enabled) {
      enabledIndices[enabledCount++] = i;
    }
  }

  if (enabledCount == 0)
    return 0;

  // 3. 找到当前应用在启用列表中的位置
  int currentPos = 0;
  for (int i = 0; i < enabledCount; i++) {
    if (enabledIndices[i] == state.currentApp) {
      currentPos = i;
      break;
    }
  }

  // 4. 按方向计算下一个位置（支持正向/反向循环）
  int nextPos =
      (currentPos + enabledCount + state.appTransitionDirection) % enabledCount;
  return enabledIndices[nextPos];
}

// ==================================================================
// 覆盖层绘制
// ==================================================================

/**
 * @brief 绘制所有覆盖层
 * @note 使用 player2 作为覆盖层专用播放器，避免与 App 的 player1 冲突
 */
void MatrixDisplayUi::drawOverlays() {
  for (uint8_t i = 0; i < this->overlayCount; i++) {
    (this->overlayFunctions[i])(this->matrix, &this->state, &player2);
  }
}

// ==================================================================
// 应用渲染 + 过渡动画
// ==================================================================

/**
 * @brief 渲染当前应用和过渡动画
 *
 * IN_TRANSITION 状态下同时渲染两个 App:
 *   - 当前 App: 以 (x, y) 偏移绘制（逐渐移出）
 *   - 下一 App: 以 (x1, y1) 偏移绘制（逐渐移入）
 *
 * progress = ticksSinceLastStateSwitch / ticksPerTransition (0.0 ~ 1.0)
 */
void MatrixDisplayUi::drawApp() {
  if (apps.empty())
    return;

  switch (state.appState) {
  case IN_TRANSITION: {
    // 计算过渡进度 (0.0 → 1.0)
    float progress =
        (float)state.ticksSinceLastStateSwitch / (float)ticksPerTransition;

    int16_t x = 0, y = 0;   // 当前 App 的偏移
    int16_t x1 = 0, y1 = 0; // 下一 App 的偏移

    // 根据动画方向计算 Y 轴偏移 (目前仅支持上下滑动)
    switch (appAnimationDirection) {
    case SLIDE_UP:
      y = -8 * progress; // 当前页向上移出
      y1 = y + 8;        // 下一页从下方移入
      break;
    case SLIDE_DOWN:
      y = 8 * progress; // 当前页向下移出
      y1 = y - 8;       // 下一页从上方移入
      break;
    default:
      break;
    }

    // 方向修正: 反向切换时翻转偏移
    int8_t dir = state.appTransitionDirection >= 0 ? 1 : -1;
    x *= dir;
    y *= dir;
    x1 *= dir;
    y1 *= dir;

    // 渲染当前 App 和下一个 App
    int nextApp = getNextAppNumber();
    if (state.currentApp < (int)apps.size()) {
      apps[state.currentApp].callback(matrix, &state, x, y, &player1);
    }
    if (nextApp < (int)apps.size()) {
      apps[nextApp].callback(matrix, &state, x1, y1, &player1);
    }
    break;
  }

  case FIXED:
    // 固定显示：无偏移
    if (state.currentApp < (int)apps.size()) {
      apps[state.currentApp].callback(matrix, &state, 0, 0, &player1);
    }
    break;
  }
}

// ==================================================================
// 状态机 + 主渲染循环
// ==================================================================

/**
 * @brief UI 状态机核心 tick
 *
 * 每帧执行:
 *   1. 递增 tick 计数器
 *   2. 状态机推进:
 *      - IN_TRANSITION: 过渡完成后切换到 FIXED
 *      - FIXED: 显示时间到达后切换到 IN_TRANSITION
 *   3. 清屏 → 绘制 App → 绘制覆盖层 → 输出到矩阵
 */
void MatrixDisplayUi::tick() {
  state.ticksSinceLastStateSwitch++;

  if (AppCount > 0) {
    switch (state.appState) {

    // ---- 过渡动画中 ----
    case IN_TRANSITION:
      if (state.ticksSinceLastStateSwitch >= ticksPerTransition) {
        // 过渡完成 → 切换到 FIXED
        state.appState = FIXED;
        state.currentApp = getNextAppNumber();
        state.ticksSinceLastStateSwitch = 0;

        // 加载新应用的自定义时长（如果设置了）
        if (state.currentApp < (int)apps.size() &&
            apps[state.currentApp].duration > 0) {
          ticksPerApp = (int)((float)apps[state.currentApp].duration /
                              (float)updateInterval);
        } else {
          // 回退到全局时长
          extern uint16_t TIME_PER_APP;
          ticksPerApp = (int)((float)TIME_PER_APP / (float)updateInterval);
        }
      }
      break;

    // ---- 固定显示中 ----
    case FIXED:
      // 手动切换后恢复自动方向
      if (state.manuelControll) {
        state.appTransitionDirection = lastTransitionDirection;
        state.manuelControll = false;
      }

      // 显示时间到达，触发自动切换
      if (state.ticksSinceLastStateSwitch >= ticksPerApp) {
        if (setAutoTransition) {
          // 至少 2 个已启用的应用才触发自动切换
          int enabledCount = 0;
          for (auto &app : apps) {
            if (app.enabled)
              enabledCount++;
          }
          if (enabledCount > 1) {
            state.appState = IN_TRANSITION;
          }
        }
        state.ticksSinceLastStateSwitch = 0;
      }
      break;
    }
  }

  // ---- 渲染流水线 ----
  matrix->clear(); // 清屏
  if (AppCount > 0)
    drawApp();    // 绘制 App
  drawOverlays(); // 绘制覆盖层
  matrix->show(); // 输出到硬件
}

// ==================================================================
// 帧率控制入口
// ==================================================================

/**
 * @brief 帧率控制器 — UI 更新入口点
 *
 * 由 DisplayManager::tick() 在主循环中调用。
 * 根据 updateInterval 进行帧率控制:
 *   - 返回值 > 0: 距离下一帧还有多少毫秒（调用方可以 delay）
 *   - 返回值 ≤ 0: 需要立即渲染（可能丢帧）
 *
 * @return 剩余时间预算 (ms)，正值表示还有空闲时间
 *
 * @note 自动补偿丢帧: 如果超时严重，会自动跳过 tick 计数以追赶进度
 */
int8_t MatrixDisplayUi::update() {
  long appStart = millis();
  int8_t timeBudget = updateInterval - (appStart - state.lastUpdate);

  if (timeBudget <= 0) {
    // 如果落后太多，补偿丢失的 tick 数
    if (setAutoTransition && state.lastUpdate != 0) {
      state.ticksSinceLastStateSwitch += ceil(-timeBudget / updateInterval);
    }
    state.lastUpdate = appStart;
    tick();
  }

  return updateInterval - (millis() - appStart);
}

/**
 * @brief 获取 UI 状态对象的指针
 * @return 当前 MatrixDisplayUiState 指针
 */
MatrixDisplayUiState *MatrixDisplayUi::getUiState() { return &state; }