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
 *
 * 修复记录:
 *   [Fix1] getNextAppNumber() 有消费副作用，过渡期间只调用一次并缓存到
 *          state.cachedNextApp，drawApp() 和 tick() 统一读缓存。
 *   [Fix2] 过渡动画中当前/下一 App 各用独立播放器 (player1/player2)，
 *          避免载入动画互相覆盖导致闪烁。固定显示时恢复 player1 给当前 App。
 *   [Fix3] enabledCount 在 setApps()/setOverlays() 时预计算并缓存到
 *          _enabledAppCount，tick() 直接读缓存，消除每帧 O(n) 遍历。
 *   [Fix4] update() 的 timeBudget 改为 long 类型，消除 int8_t 溢出导致的
 *          丢帧补偿错误；补偿逻辑不再受 setAutoTransition 限制。
 */

#include "MatrixDisplayUi.h"
#include "AwtrixFont.h"
#include "Globals.h"

/// player1: 固定显示 / 过渡中的「当前页」播放器
/// player2: 过渡中的「下一页」播放器 / 覆盖层播放器
FastFramePlayer player1;
FastFramePlayer player2;

// ==================================================================
// 构造与初始化
// ==================================================================

MatrixDisplayUi::MatrixDisplayUi(FastLED_NeoMatrix *matrix)
{
  this->matrix = matrix;
}

/**
 * @brief 初始化矩阵硬件和字体
 */
void MatrixDisplayUi::init()
{
  this->matrix->begin();
  this->matrix->setTextWrap(false);
  this->matrix->setBrightness(BRIGHTNESS);
  this->matrix->setFont(&AwtrixFont);

  player1.setMatrix(this->matrix);
  player2.setMatrix(this->matrix);
}

// ==================================================================
// 参数配置
// ==================================================================

void MatrixDisplayUi::setTargetFPS(uint8_t fps)
{
  float oldInterval = this->updateInterval;
  this->updateInterval = ((float)1.0 / (float)fps) * 1000;

  float changeRatio = oldInterval / (float)this->updateInterval;
  this->ticksPerApp *= changeRatio;
  this->ticksPerTransition *= changeRatio;
}

void MatrixDisplayUi::enablesetAutoTransition()
{
  this->setAutoTransition = true;
}
void MatrixDisplayUi::disablesetAutoTransition()
{
  this->setAutoTransition = false;
}

void MatrixDisplayUi::setTimePerApp(uint16_t time)
{
  this->ticksPerApp = (int)((float)time / (float)updateInterval);
}

void MatrixDisplayUi::setTimePerTransition(uint16_t time)
{
  this->ticksPerTransition = (int)((float)time / (float)updateInterval);
}

void MatrixDisplayUi::setAppAnimation(AnimationDirection dir)
{
  this->appAnimationDirection = dir;
}

/**
 * @brief 设置应用列表，同时预计算 enabledCount 缓存 [Fix3]
 */
void MatrixDisplayUi::setApps(const std::vector<AppData> &appList)
{
  this->apps = appList;
  this->AppCount = appList.size();
  _rebuildEnabledCount();
}

void MatrixDisplayUi::setOverlays(OverlayCallback *overlayFunctions,
                                  uint8_t overlayCount)
{
  this->overlayFunctions = overlayFunctions;
  this->overlayCount = overlayCount;
}

// ==================================================================
// 内部：重建 enabledCount 缓存 [Fix3]
// ==================================================================
void MatrixDisplayUi::_rebuildEnabledCount()
{
  int count = 0;
  for (auto &app : apps)
  {
    if (app.enabled)
      count++;
  }
  _enabledAppCount = count;
}

// ==================================================================
// 应用导航
// ==================================================================

void MatrixDisplayUi::nextApp()
{
  if (this->state.appState != IN_TRANSITION)
  {
    this->state.manuelControll = true;
    this->state.appState = IN_TRANSITION;
    this->state.ticksSinceLastStateSwitch = 0;
    this->lastTransitionDirection = this->state.appTransitionDirection;
    this->state.appTransitionDirection = 1;
    // [Fix1] 过渡开始时立即锁定目标 App
    this->state.cachedNextApp = getNextAppNumber();
  }
}

void MatrixDisplayUi::previousApp()
{
  if (this->state.appState != IN_TRANSITION)
  {
    this->state.manuelControll = true;
    this->state.appState = IN_TRANSITION;
    this->state.ticksSinceLastStateSwitch = 0;
    this->lastTransitionDirection = this->state.appTransitionDirection;
    this->state.appTransitionDirection = -1;
    // [Fix1] 过渡开始时立即锁定目标 App
    this->state.cachedNextApp = getNextAppNumber();
  }
}

void MatrixDisplayUi::transitionToApp(uint8_t app)
{
  if (app >= this->AppCount)
    return;

  this->state.ticksSinceLastStateSwitch = 0;
  if (app == this->state.currentApp)
    return;

  this->nextAppNumber = app;
  this->lastTransitionDirection = this->state.appTransitionDirection;
  this->state.manuelControll = true;
  this->state.appState = IN_TRANSITION;
  this->state.appTransitionDirection = app < this->state.currentApp ? -1 : 1;
  // [Fix1] 过渡开始时立即锁定目标 App
  this->state.cachedNextApp = getNextAppNumber();
}

void MatrixDisplayUi::switchToApp(uint8_t app)
{
  if (app >= this->AppCount)
    return;
  if (app == this->state.currentApp)
    return;

  this->state.currentApp = app;
  this->state.ticksSinceLastStateSwitch = 0;
  this->state.appState = FIXED;
  this->state.cachedNextApp = -1;

  if (apps[app].duration > 0)
  {
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
 * ⚠️ 注意副作用: 若 nextAppNumber != -1 会消费并清空它。
 *    只在过渡开始时（nextApp/previousApp/transitionToApp）调用一次，
 *    结果缓存到 state.cachedNextApp，之后统一读缓存。[Fix1]
 */
int MatrixDisplayUi::getNextAppNumber()
{
  if (this->nextAppNumber != -1)
  {
    int temp = this->nextAppNumber;
    this->nextAppNumber = -1;
    return temp;
  }

  int enabledIndices[16];
  int enabledCount = 0;
  for (int i = 0; i < (int)apps.size() && enabledCount < 16; i++)
  {
    if (apps[i].enabled)
    {
      enabledIndices[enabledCount++] = i;
    }
  }

  if (enabledCount == 0)
    return 0;

  int currentPos = 0;
  for (int i = 0; i < enabledCount; i++)
  {
    if (enabledIndices[i] == state.currentApp)
    {
      currentPos = i;
      break;
    }
  }

  int nextPos =
      (currentPos + enabledCount + state.appTransitionDirection) % enabledCount;
  return enabledIndices[nextPos];
}

// ==================================================================
// 覆盖层绘制
// ==================================================================

void MatrixDisplayUi::drawOverlays()
{
  for (uint8_t i = 0; i < this->overlayCount; i++)
  {
    (this->overlayFunctions[i])(this->matrix, &this->state, &player2);
  }
}

// ==================================================================
// 应用渲染 + 过渡动画
// ==================================================================

/**
 * @brief 渲染当前应用和过渡动画
 *
 * [Fix1] 直接读 state.cachedNextApp，不再调用 getNextAppNumber()
 * [Fix2] 过渡中当前页用 player1，下一页用 player2，互不干扰
 */
void MatrixDisplayUi::drawApp()
{
  if (apps.empty())
    return;

  switch (this->state.appState)
  {
  case IN_TRANSITION:
  {
    int nextApp = this->state.cachedNextApp;
    if (nextApp < 0 || nextApp >= (int)apps.size())
      nextApp = this->state.currentApp; // 安全回退

    float progress =
        (float)this->state.ticksSinceLastStateSwitch / (float)this->ticksPerTransition;

    int16_t x = 0, y = 0;
    int16_t x1 = 0, y1 = 0;

    switch (this->appAnimationDirection)
    {
    case SLIDE_UP:
      y = -8 * progress;
      y1 = y + 8;
      break;
    case SLIDE_DOWN:
      y = 8 * progress;
      y1 = y - 8;
      break;
    default:
      break;
    }

    int8_t dir = this->state.appTransitionDirection >= 0 ? 1 : -1;
    x *= dir;
    y *= dir;
    x1 *= dir;
    y1 *= dir;
    this->matrix->drawRect(x, y, x1, y1, this->matrix->Color(0, 0, 0));
    // [Fix2] 当前页用 player1，下一页用 player2
    if (this->state.currentApp < (int)this->apps.size())
    {
      this->apps[this->state.currentApp].callback(this->matrix, &this->state, x, y, &player1);
    }
    if (nextApp < (int)this->apps.size())
    {
      this->apps[nextApp].callback(this->matrix, &this->state, x1, y1, &player2);
    }
    break;
  }

  case FIXED:
    // [Fix2] 固定显示只用 player1
    if (this->state.currentApp < (int)this->apps.size())
    {
      this->apps[this->state.currentApp].callback(this->matrix, &this->state, 0, 0, &player1);
    }
    break;
  }
}

// ==================================================================
// 状态机 + 主渲染循环
// ==================================================================

void MatrixDisplayUi::tick()
{
  this->state.ticksSinceLastStateSwitch++;

  if (this->AppCount > 0)
  {
    switch (this->state.appState)
    {

    case IN_TRANSITION:
      if (this->state.ticksSinceLastStateSwitch >= this->ticksPerTransition)
      {
        // [Fix1] 直接读缓存，不再二次调用 getNextAppNumber()
        int nextApp = this->state.cachedNextApp;
        if (nextApp < 0 || nextApp >= (int)this->apps.size())
          nextApp = 0;

        this->state.appState = FIXED;
        this->state.currentApp = nextApp;
        this->state.cachedNextApp = -1;
        this->state.ticksSinceLastStateSwitch = 0;

        if (this->state.currentApp < (int)this->apps.size() &&
            this->apps[this->state.currentApp].duration > 0)
        {
          ticksPerApp = (int)((float)this->apps[this->state.currentApp].duration /
                              (float)this->updateInterval);
        }
        else
        {
          extern uint16_t TIME_PER_APP;
          ticksPerApp = (int)((float)TIME_PER_APP / (float)this->updateInterval);
        }
      }
      break;

    case FIXED:
      if (this->state.manuelControll)
      {
        this->state.appTransitionDirection = lastTransitionDirection;
        this->state.manuelControll = false;
      }

      if (this->state.ticksSinceLastStateSwitch >= this->ticksPerApp)
      {
        // [Fix3] 直接读缓存的 enabledCount，不再每帧遍历
        if (this->setAutoTransition && _enabledAppCount > 1)
        {
          this->state.appState = IN_TRANSITION;
          // [Fix1] 切换前锁定目标 App
          this->state.cachedNextApp = getNextAppNumber();
        }
        this->state.ticksSinceLastStateSwitch = 0;
      }
      break;
    }
  }

  this->matrix->clear();
  if (this->AppCount > 0)
    this->drawApp();
  this->drawOverlays();
  DisplayManager.gammaCorrection();
  this->matrix->show();
}

// ==================================================================
// 帧率控制入口
// ==================================================================

/**
 * @brief 帧率控制器 — UI 更新入口点
 *
 * [Fix4] timeBudget 改为 long 消除 int8_t 溢出；
 *        丢帧补偿不再受 setAutoTransition 限制，手动模式下过渡也正常追帧。
 */
int8_t MatrixDisplayUi::update()
{
  unsigned long appStart = millis();
  // [Fix4] 用 long 计算，避免 int8_t 溢出（超过 128ms 时会错误补偿）
  long timeBudget = (long)this->updateInterval - (long)(appStart - this->state.lastUpdate);

  if (timeBudget <= 0)
  {
    // [Fix4] 补偿不再受 setAutoTransition 限制
    if (this->state.lastUpdate != 0)
    {
      this->state.ticksSinceLastStateSwitch +=
          (int)(-timeBudget / (long)this->updateInterval);
    }
    this->state.lastUpdate = appStart;
    this->tick();
  }

  // 返回值保持 int8_t 兼容（调用方用于可选 delay）
  long remaining = (long)this->updateInterval - (long)(millis() - appStart);
  return (int8_t)(remaining > 127 ? 127
                                  : (remaining < -128 ? -128 : remaining));
}

MatrixDisplayUiState *MatrixDisplayUi::getUiState() { return &state; }