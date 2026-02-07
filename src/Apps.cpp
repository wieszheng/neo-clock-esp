#include "Apps.h"
#include "DisplayManager.h"
#include "Tools.h"
#include "Globals.h"
#include <time.h>


std::vector<AppData> Apps;

// 覆盖层回调数组定义
OverlayCallback overlays[] = {
    AlarmOverlay,      // 最高优先级
    TimerOverlay,
    NotifyOverlay,
    SpectrumOverlay
};


void TimeApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state, int16_t x, int16_t y, FastFramePlayer *player)
{

    CURRENT_APP = "Time";

    // 设置文字颜色
    if (TIME_COLOR.length() > 0)
    {
        uint16_t color = HEXtoColor(TIME_COLOR.c_str());
        DisplayManager.setTextColor(color);
    }
    else
    {
        DisplayManager.defaultTextColor();
    }

    time_t now = time(nullptr);
    struct tm *timeInfo = localtime(&now);

    // 获取时间格式
    char fmt[32];
    strncpy(fmt, TIME_FORMAT.c_str(), sizeof(fmt));

    // 智能闪烁: 寻找第一个冒号或空格作为分隔符
    // 仅在秒数是奇数时隐藏它
    if (now % 2)
    {
        char *sep = strchr(fmt, ':');
        if (!sep)
            sep = strchr(fmt, ' ');
        // 只有当分隔符存在，且不是长格式(秒在跳动时冒号不闪烁视觉更好)时才闪烁
        // 这里做一个简单判断：如果字符串很长(包含秒)，通常不闪烁冒号以免太乱
        if (sep && strlen(TIME_FORMAT.c_str()) < 8)
        {
            *sep = ' '; // 替换为空格以"隐藏"
        }
    }

    char text[32];
    strftime(text, sizeof(text), fmt, timeInfo);
    int charWidth = 4;
    int textPixelWidth = strlen(text) * charWidth - 1;

    bool showIcon = (textPixelWidth <= 22);

    int16_t textX = 0;
    int16_t barStartX = 0;
    int8_t barWidth = 0;

    if (showIcon)
    {
        // === 模式 A: 图标 + 文字 ===

        // (1) 绘制图标
        // const uint16_t *iconData = getIconDataPtr(TIME_ICON);
        // if (iconData == nullptr)
        // {
        //     iconData = ANIM_6215_DATA; // 默认图标
        // }
        player->loadUser("14825.anim");
        player->play(x, y);

        // (2) 文字居中计算
        // 右侧可用区域: x=10 到 x=31 (宽度 22px)
        // 居中公式: 区域左边 + (区域宽 - 文字宽) / 2
        textX = 10 + (22 - textPixelWidth) / 2;

        // (3) 星期条参数
        barStartX = 10; // 虽然文字居中，但星期条最好铺满整个右侧区域(10-31)看起来更整齐
        barWidth = 2;   // (2+1)*7-1 = 20px (接近22px)
        DisplayManager.printText(textX + x, 6 + y, text, false, false);
    }
    else
    {
        // === 模式 B: 全屏文字 ===

        // (1) 文字居中计算
        // 区域 [0, 31], 宽 32px
        textX = (32 - textPixelWidth) / 2;
        // (2) 星期条参数
        barStartX = 2; // (32 - 27) / 2
        barWidth = 3;  // (3+1)*7-1 = 27px
        DisplayManager.printText(textX + x, 6 + y, text, true, false);
    }

    // 显示星期指示器
    if (!SHOW_WEEKDAY)
        return;
    int dayOffset = START_ON_MONDAY ? 0 : 1;
    for (int i = 0; i <= 6; i++)
    {
        uint16_t color = (i == (timeInfo->tm_wday + 6 + dayOffset) % 7) ? HEXtoColor(TIME_WEEKDAY_ACTIVE_COLOR.c_str()) : HEXtoColor(TIME_WEEKDAY_INACTIVE_COLOR.c_str());

        int xPos = barStartX + (i * (barWidth + 1)); // 间隔固定为1
        matrix->drawLine(xPos + x, y + 7, xPos + barWidth - 1 + x, y + 7, color);
    }
}

void DateApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state, int16_t x, int16_t y, FastFramePlayer *player)
{
    CURRENT_APP = "Date";

    // 设置文字颜色
    if (DATE_COLOR.length() > 0)
    {
        uint16_t color = HEXtoColor(DATE_COLOR.c_str());
        DisplayManager.setTextColor(color);
    }
    else
    {
        DisplayManager.defaultTextColor();
    }

    time_t now = time(nullptr);
    struct tm *timeInfo = localtime(&now);

    char text[32];
    strftime(text, sizeof(text), DATE_FORMAT.c_str(), timeInfo);
    int charWidth = 4;
    int textPixelWidth = strlen(text) * charWidth - 1;

    bool showIcon = (textPixelWidth <= 24);

    int16_t textX = 0;
    int16_t barStartX = 0;
    int8_t barWidth = 0;
    if (showIcon)
    {
        // === 模式 A: 图标 + 文字 ===
        // const uint16_t *iconData = getIconDataPtr(DATE_ICON);
        // if (iconData == nullptr)
        // {
        //     iconData = ANIM_332_DATA; // 默认图标
        // }

        player->loadUser("21987.anim");
        player->play(x, y);

        textX = 10 + (22 - textPixelWidth) / 2;
        barStartX = 10; 
        barWidth = 2; 
        if (DATE_FORMAT.lastIndexOf(".") != -1)
        {
            // 日期格式为 "DD.MM." 时，调整文字位置以避免与图标重叠
            textX += 1;
        }
        DisplayManager.printText(textX + x, 6 + y, text, false, false);

    }
    else
    {
        // === 模式 B: 全屏文字 ===
        textX = (32 - textPixelWidth) / 2;
        barStartX = 2; 
        barWidth = 3; 
        DisplayManager.printText(textX + x, 6 + y, text, true, false);
    }

    // 显示星期指示器
    if (!SHOW_WEEKDAY)
        return;

    int dayOffset = START_ON_MONDAY ? 0 : 1;
    for (int i = 0; i <= 6; i++)
    {
        uint16_t color = (i == (timeInfo->tm_wday + 6 + dayOffset) % 7) ? HEXtoColor(DATE_WEEKDAY_ACTIVE_COLOR.c_str()) : HEXtoColor(DATE_WEEKDAY_INACTIVE_COLOR.c_str());

        int xPos = barStartX + (i * (barWidth + 1)); // 间隔固定为1
        matrix->drawLine(xPos + x, y + 7, xPos + barWidth - 1 + x, y + 7, color);
    }
}
