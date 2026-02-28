/**
 * @file Globals.h
 * @brief 全局变量与系统配置 — 硬件引脚、系统参数、应用配置
 *
 * 本文件定义了整个 NeoClock 系统的全局变量和配置常量：
 *   - 硬件引脚定义 (GPIO32)
 *   - 矩阵显示参数 (32x8)
 *   - 系统配置 (亮度、帧率、自动切换等)
 *   - 应用显示配置 (时间、日期、天气等)
 *   - 传感器数据 (温湿度)
 *   - Weather API 配置
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <vector>

// ==================================================================
// 硬件引脚定义
// ==================================================================

/// LED 矩阵数据引脚 (WS2812B / SK6812 数据输入)
#define MATRIX_PIN 32

/// LED 矩阵宽度 (像素数)
#define MATRIX_WIDTH 32

/// LED 矩阵高度 (像素数)
#define MATRIX_HEIGHT 8

/// LED 总数量 (宽 × 高)
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

/// 是否处于 AP 配网模式
extern bool AP_MODE;

// ==================================================================
// 系统配置
// ==================================================================

/// 是否启用 Liveview (实时预览功能)
extern bool EnableLiveview;

/// 矩阵布局模式 (0-5, 不同硬件拼接方式)
extern int MATRIX_LAYOUT;

/// 矩阵亮度 (0-255)
extern uint8_t BRIGHTNESS;

/// LDR 自动亮度开关
extern bool AUTO_BRIGHTNESS;

/// 矩阵帧率 (FPS)
extern uint8_t MATRIX_FPS;

/// 是否自动切换应用
extern bool AUTO_TRANSITION;

/// 矩阵电源开关 (true=关闭, false=开启)
extern bool MATRIX_OFF;

/// 单个应用显示时长 (毫秒)
extern uint16_t TIME_PER_APP;

/// 应用切换动画时长 (毫秒)
extern uint16_t TIME_PER_TRANSITION;

/// 默认文字颜色 (RGB565 格式)
extern uint16_t TEXTCOLOR_565;

// ==================================================================
// 应用显示开关
// ==================================================================

/// 是否显示时间应用
extern bool SHOW_TIME;
/// 是否显示日期应用
extern bool SHOW_DATE;
/// 是否显示温度应用
extern bool SHOW_TEMP;
/// 是否显示湿度应用
extern bool SHOW_HUM;
/// 是否显示天气应用
extern bool SHOW_WEATHER;
/// 是否显示风速应用
extern bool SHOW_WIND;
/// 是否显示频谱应用
extern bool SHOW_SPECTRUM;

// ==================================================================
// 当前应用状态
// ==================================================================

/// 当前正在显示的应用名称
extern String CURRENT_APP;

// ==================================================================
// 应用配置
// ==================================================================

/// 时间显示格式 (如 "%H %M")
extern String TIME_FORMAT;

/// 日期显示格式 (如 "%m/%d")
extern String DATE_FORMAT;

/// 是否显示星期指示条
extern bool SHOW_WEEKDAY;

/// 星期一是否作为一周的开始
extern bool START_ON_MONDAY;

// ==================================================================
// 应用颜色配置
// ==================================================================

/// 时间应用文字颜色 (HEX 格式, 如 "#FFFFFF")
extern String TIME_COLOR;

/// 时间应用星期激活颜色
extern String TIME_WEEKDAY_ACTIVE_COLOR;

/// 时间应用星期未激活颜色
extern String TIME_WEEKDAY_INACTIVE_COLOR;

/// 日期应用文字颜色
extern String DATE_COLOR;

/// 日期应用星期激活颜色
extern String DATE_WEEKDAY_ACTIVE_COLOR;

/// 日期应用星期未激活颜色
extern String DATE_WEEKDAY_INACTIVE_COLOR;

/// 温度应用文字颜色
extern String TEMP_COLOR;

/// 湿度应用文字颜色
extern String HUM_COLOR;

/// 风速应用文字颜色
extern String WIND_COLOR;

// ==================================================================
// 应用图标配置
// ==================================================================

/// 时间应用自定义图标文件名 (空=使用默认)
extern String TIME_ICON;

/// 日期应用自定义图标文件名
extern String DATE_ICON;

/// 天气应用自定义图标文件名
extern String APP_WEATHER_ICON;

/// 风速应用自定义图标文件名
extern String WIND_ICON;

// ==================================================================
// 应用单独显示时长 (毫秒)
// ==================================================================

/// 时间应用显示时长 (0=使用全局时长)
extern uint16_t TIME_DURATION;

/// 日期应用显示时长
extern uint16_t DATE_DURATION;

/// 温度应用显示时长
extern uint16_t TEMP_DURATION;

/// 湿度应用显示时长
extern uint16_t HUM_DURATION;

/// 天气应用显示时长
extern uint16_t WEATHER_DURATION;

/// 风速应用显示时长
extern uint16_t WIND_DURATION;

// ==================================================================
// 应用显示位置 (用于排序)
// ==================================================================

/// 时间应用排序位置
extern int TIME_POSITION;

/// 日期应用排序位置
extern int DATE_POSITION;

/// 温度应用排序位置
extern int TEMP_POSITION;

/// 湿度应用排序位置
extern int HUM_POSITION;

/// 天气应用排序位置
extern int WEATHER_POSITION;

/// 风速应用排序位置
extern int WIND_POSITION;

// ==================================================================
// 传感器数据
// ==================================================================

/// 室内温度 (来自 DHT22 传感器, 单位: °C)
extern float INDOOR_TEMP;

/// 室内湿度 (来自 DHT22 传感器, 单位: %)
extern float INDOOR_HUM;

/// 室外温度 (来自 Weather API, 单位: °C)
extern float OUTDOOR_TEMP;

/// 室外湿度 (来自 Weather API, 单位: %)
extern float OUTDOOR_HUM;

// ==================================================================
// Weather API 配置
// ==================================================================

/// OpenWeatherMap API 密钥
extern String WEATHER_API_KEY;

/// 查询天气的城市名称
extern String WEATHER_CITY;

/// 天气更新间隔 (毫秒)
extern unsigned long WEATHER_UPDATE_INTERVAL;

// ==================================================================
// 额外天气数据
// ==================================================================

/// 当前天气描述 (如 "Clear", "Rain")
extern String CURRENT_WEATHER;

/// 天气图标代码
extern String WEATHER_ICON;

/// 气压 (单位: hPa)
extern int WEATHER_PRESSURE;

/// 风速 (单位: m/s)
extern float WEATHER_WIND_SPEED;

/// 风向 (单位: 度)
extern int WEATHER_WIND_DIR;

/// 日出时间戳
extern unsigned long WEATHER_SUNRISE;

/// 日落时间戳
extern unsigned long WEATHER_SUNSET;

/// 天气响应代码
extern int WEATHER_COD;

// ==================================================================
// 设置存储函数
// ==================================================================

/**
 * @brief 保存设置到 Flash
 *
 * 使用 Preferences 库将当前配置保存到 Flash 存储器
 */
void saveSettings();

/**
 * @brief 从 Flash 加载设置
 *
 * 使用 Preferences 库从 Flash 存储器读取配置
 */
void loadSettings();
#endif