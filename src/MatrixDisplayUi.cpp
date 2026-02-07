#include "MatrixDisplayUi.h"
// #include <Fonts/TomThumb.h>
#include "Globals.h"
#include "AwtrixFont.h"

FastFramePlayer player1;
FastFramePlayer player2;

MatrixDisplayUi::MatrixDisplayUi(FastLED_NeoMatrix *matrix)
{
    this->matrix = matrix;
    this->updateInterval = 33.33; // 30 FPS
    this->ticksPerApp = 150;
    this->ticksPerTransition = 15;
    this->appAnimationDirection = SLIDE_DOWN;
    this->nextAppNumber = -1;
    this->lastTransitionDirection = 1;
    this->setAutoTransition = true;
    this->overlayFunctions = nullptr;
    this->overlayCount = 0;
}

void MatrixDisplayUi::init()
{
    matrix->begin();
    matrix->setTextWrap(false);
    matrix->setBrightness(BRIGHTNESS);
    this->matrix->setFont(&AwtrixFont);
    player1.setMatrix(this->matrix);
    player2.setMatrix(this->matrix);
}

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

void MatrixDisplayUi::setApps(const std::vector<AppData> &appList)
{
    this->apps = appList;
    this->AppCount = appList.size();
}

void MatrixDisplayUi::setOverlays(OverlayCallback* overlayFunctions, uint8_t overlayCount) {
    this->overlayFunctions = overlayFunctions;
    this->overlayCount = overlayCount;
}

void MatrixDisplayUi::nextApp()
{
    if (this->state.appState != IN_TRANSITION)
    {
        this->state.manuelControll = true;
        this->state.appState = IN_TRANSITION;
        this->state.ticksSinceLastStateSwitch = 0;
        this->lastTransitionDirection = this->state.appTransitionDirection;
        this->state.appTransitionDirection = 1;
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

    // 设置当前应用的显示时长
    if (apps[app].duration > 0)
    {
        this->ticksPerApp = (int)((float)apps[app].duration / (float)updateInterval);
    }
}

int MatrixDisplayUi::getNextAppNumber()
{
    if (this->nextAppNumber != -1)
    {
        int temp = this->nextAppNumber;
        this->nextAppNumber = -1;
        return temp;
    }

    std::vector<int> enabledIndices;
    for (int i = 0; i < apps.size(); i++)
    {
        if (apps[i].enabled)
        {
            enabledIndices.push_back(i);
        }
    }

    if (enabledIndices.empty())
        return 0;

    int currentPos = 0;
    for (int i = 0; i < enabledIndices.size(); i++)
    {
        if (enabledIndices[i] == state.currentApp)
        {
            currentPos = i;
            break;
        }
    }

    int nextPos = (currentPos + enabledIndices.size() + state.appTransitionDirection) % enabledIndices.size();
    return enabledIndices[nextPos];
}
void MatrixDisplayUi::drawOverlays() {
    for (uint8_t i = 0; i < this->overlayCount; i++) {
        (this->overlayFunctions[i])(this->matrix, &this->state);
    }
}

void MatrixDisplayUi::drawApp()
{
    if (apps.empty())
        return;

    switch (state.appState)
    {
    case IN_TRANSITION:
    {
        float progress = (float)state.ticksSinceLastStateSwitch / (float)ticksPerTransition;
        int16_t x = 0, y = 0, x1 = 0, y1 = 0;

        switch (appAnimationDirection)
        {
        case SLIDE_UP:
            x = 0;
            y = -8 * progress;
            x1 = 0;
            y1 = y + 8;
            break;
        case SLIDE_DOWN:
            x = 0;
            y = 8 * progress;
            x1 = 0;
            y1 = y - 8;
            break;
        }

        int8_t dir = state.appTransitionDirection >= 0 ? 1 : -1;
        x *= dir;
        y *= dir;
        x1 *= dir;
        y1 *= dir;

        bool firstFrame = progress < 0.2;
        bool lastFrame = progress > 0.8;

        int nextApp = getNextAppNumber();
        if (state.currentApp < apps.size())
        {
            apps[state.currentApp].callback(matrix, &state, x, y, &player1);
        }
        if (nextApp < apps.size())
        {
            apps[nextApp].callback(matrix, &state, x1, y1, &player1);
        }
        break;
    }
    case FIXED:
        if (state.currentApp < apps.size())
        {
            apps[state.currentApp].callback(matrix, &state, 0, 0, &player1);
        }
        break;
    }
}

void MatrixDisplayUi::tick()
{
    state.ticksSinceLastStateSwitch++;

    if (AppCount > 0)
    {
        switch (state.appState)
        {
        case IN_TRANSITION:
            if (state.ticksSinceLastStateSwitch >= ticksPerTransition)
            {
                state.appState = FIXED;
                state.currentApp = getNextAppNumber();
                state.ticksSinceLastStateSwitch = 0;
                // 切换到新应用时，设置该应用的显示时长
                if (state.currentApp < apps.size() && apps[state.currentApp].duration > 0)
                {
                    ticksPerApp = (int)((float)apps[state.currentApp].duration / (float)updateInterval);
                }
                else
                {
                    // 使用全局时长
                    extern uint16_t TIME_PER_APP;
                    ticksPerApp = (int)((float)TIME_PER_APP / (float)updateInterval);
                }
            }
            break;

        case FIXED:
            if (state.manuelControll)
            {
                state.appTransitionDirection = lastTransitionDirection;
                state.manuelControll = false;
            }
            if (state.ticksSinceLastStateSwitch >= ticksPerApp)
            {
                if (setAutoTransition)
                {
                    int enabledCount = 0;
                    for (auto &app : apps)
                    {
                        if (app.enabled)
                            enabledCount++;
                    }
                    if (enabledCount > 1)
                    {
                        state.appState = IN_TRANSITION;
                    }
                }
                state.ticksSinceLastStateSwitch = 0;
            }
            break;
        }
    }

    matrix->clear();
    if (AppCount > 0)
        drawApp();
    drawOverlays();  // 绘制覆盖层
    matrix->show();
}

int8_t MatrixDisplayUi::update()
{
    long appStart = millis();
    int8_t timeBudget = updateInterval - (appStart - state.lastUpdate);

    if (timeBudget <= 0)
    {
        if (setAutoTransition && state.lastUpdate != 0)
        {
            state.ticksSinceLastStateSwitch += ceil(-timeBudget / updateInterval);
        }
        state.lastUpdate = appStart;
        tick();
    }
    return updateInterval - (millis() - appStart);
}

MatrixDisplayUiState *MatrixDisplayUi::getUiState()
{
    return &state;
}