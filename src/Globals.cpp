#include "Globals.h"
#include <Preferences.h>

bool AP_MODE = false;

uint8_t BRIGHTNESS = 70;
uint8_t MATRIX_FPS = 30;
bool MATRIX_OFF = false;
bool AUTO_TRANSITION = true;
int MATRIX_LAYOUT = 5;

uint16_t TIME_PER_APP = 5000;
uint16_t TIME_PER_TRANSITION = 500;
uint16_t TEXTCOLOR_565 = 0xFFFF;

// 应用显示开关
bool SHOW_TIME = true;
bool SHOW_DATE = true;
bool SHOW_TEMP = false;
bool SHOW_HUM = false;
bool SHOW_WEATHER = true;
bool SHOW_WIND = false;

String CURRENT_APP = "";

// 应用配置
String TIME_FORMAT = "%H %M";
String DATE_FORMAT = "%m/%d";
bool SHOW_WEEKDAY = true;
bool START_ON_MONDAY = true;

// 应用颜色配置
String TIME_COLOR = "#FFFFFF";
String TIME_WEEKDAY_ACTIVE_COLOR = "#f273e1";
String TIME_WEEKDAY_INACTIVE_COLOR = "#00bfff";
String DATE_COLOR = "#FFFFFF";
String DATE_WEEKDAY_ACTIVE_COLOR = "#f273e1";
String DATE_WEEKDAY_INACTIVE_COLOR = "#00bfff";
String TEMP_COLOR = "#FF6400";
String HUM_COLOR = "#0096FF";
String WIND_COLOR = "#FFFF00";

// 应用图标配置（默认值为空，使用硬编码的默认图标）
String TIME_ICON = "";
String DATE_ICON = "";
String APP_WEATHER_ICON = "";
String WIND_ICON = "";

// 应用单独显示时长(ms)，0表示使用全局TIME_PER_APP
uint16_t TIME_DURATION = 0;
uint16_t DATE_DURATION = 0;
uint16_t TEMP_DURATION = 0;
uint16_t HUM_DURATION = 0;
uint16_t WEATHER_DURATION = 0;
uint16_t WIND_DURATION = 0;

// 应用显示位置(用于排序)
int TIME_POSITION = 0;
int DATE_POSITION = 1;
int TEMP_POSITION = 2;
int HUM_POSITION = 3;
int WEATHER_POSITION = 4;
int WIND_POSITION = 5;
