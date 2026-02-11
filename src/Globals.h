#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <vector>

// 硬件配置
#define MATRIX_PIN 32
#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

// INMP441 I2S 麦克风配置
#define I2S_WS 14  // LRCLK (Word Select)
#define I2S_SD 15  // SDIN (Serial Data)
#define I2S_SCK 13 // BCLK (Bit Clock)
#define I2S_PORT I2S_NUM_0
#define I2S_SAMPLE_RATE 40000
#define I2S_SAMPLE_BITS 16
#define I2S_READ_LEN (2 * 1024)
#define I2S_CHANNEL_NUM 1
#define I2S_FORMAT I2S_CHANNEL_FMT_ONLY_LEFT

// FFT 配置
#define FFT_SAMPLES 1024
#define FFT_SAMPLING_FREQ 40000
#define FFT_AMPLITUDE 1000
#define FFT_NOISE 500
#define FFT_NUM_BANDS 32

extern bool AP_MODE;

// 系统配置
extern bool EnableLiveview;
extern int MATRIX_LAYOUT;
extern uint8_t BRIGHTNESS;
extern uint8_t MATRIX_FPS;
extern bool AUTO_TRANSITION;
extern bool MATRIX_OFF;

extern uint16_t TIME_PER_APP;
extern uint16_t TIME_PER_TRANSITION;
extern uint16_t TEXTCOLOR_565;

// 应用显示开关
extern bool SHOW_TIME;
extern bool SHOW_DATE;
extern bool SHOW_TEMP;
extern bool SHOW_HUM;
extern bool SHOW_WEATHER;
extern bool SHOW_WIND;
extern bool SHOW_SPECTRUM;

// 当前应用
extern String CURRENT_APP;

// 应用配置
extern String TIME_FORMAT;
extern String DATE_FORMAT;
extern bool SHOW_WEEKDAY;
extern bool START_ON_MONDAY;

// 应用颜色配置
extern String TIME_COLOR;
extern String TIME_WEEKDAY_ACTIVE_COLOR;
extern String TIME_WEEKDAY_INACTIVE_COLOR;
extern String DATE_COLOR;
extern String DATE_WEEKDAY_ACTIVE_COLOR;
extern String DATE_WEEKDAY_INACTIVE_COLOR;
extern String TEMP_COLOR;
extern String HUM_COLOR;
extern String WIND_COLOR;

// 应用图标配置
extern String TIME_ICON;
extern String DATE_ICON;
extern String APP_WEATHER_ICON;
extern String WIND_ICON;

// 应用单独显示时长(ms)
extern uint16_t TIME_DURATION;
extern uint16_t DATE_DURATION;
extern uint16_t TEMP_DURATION;
extern uint16_t HUM_DURATION;
extern uint16_t WEATHER_DURATION;
extern uint16_t WIND_DURATION;

// 应用显示位置(用于排序)
extern int TIME_POSITION;
extern int DATE_POSITION;
extern int TEMP_POSITION;
extern int HUM_POSITION;
extern int WEATHER_POSITION;
extern int WIND_POSITION;

// 传感器数据
// 室内温湿度(来自DHT22传感器)
extern float INDOOR_TEMP;
extern float INDOOR_HUM;

// 室外温湿度(来自Weather API)
extern float OUTDOOR_TEMP;
extern float OUTDOOR_HUM;

// Weather API 配置
extern String WEATHER_API_KEY;
extern String WEATHER_CITY;
extern unsigned long WEATHER_UPDATE_INTERVAL; // 毫秒

// 额外天气数据
extern String CURRENT_WEATHER;
extern String WEATHER_ICON;
extern int WEATHER_PRESSURE;
extern float WEATHER_WIND_SPEED;
extern int WEATHER_WIND_DIR;
extern unsigned long WEATHER_SUNRISE;
extern unsigned long WEATHER_SUNSET;
extern int WEATHER_COD;

void saveSettings();
void loadSettings();
#endif