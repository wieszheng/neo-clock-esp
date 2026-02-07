#include "Globals.h"
#include <Preferences.h>

// Preferences 对象用于保存和加载设置
static Preferences preferences;

bool EnableLiveview = false;
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
bool SHOW_TEMP = true;
bool SHOW_HUM = true;
bool SHOW_WEATHER = false;
bool SHOW_WIND = true;

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



// 室内温湿度(来自DHT22传感器)
float INDOOR_TEMP = 0.0;
float INDOOR_HUM = 0.0;

// 室外温湿度(来自Weather API)
float OUTDOOR_TEMP = 0.0;
float OUTDOOR_HUM = 0.0;

// Weather API 配置
String WEATHER_API_KEY = "56a3001073cd43567e2bd7c2dd5f0573"; // 在此填入 OpenWeatherMap API key（可通过 Web 配置）
String WEATHER_CITY = "Changping";
unsigned long WEATHER_UPDATE_INTERVAL = 3 * 60 * 1000; // 每3分钟更新一次

// 额外天气数据默认值
String CURRENT_WEATHER = "Clear";
String WEATHER_ICON = "";
int WEATHER_PRESSURE = 0;
float WEATHER_WIND_SPEED = 0.0;
int WEATHER_WIND_DIR = 0;
unsigned long WEATHER_SUNRISE = 0;
unsigned long WEATHER_SUNSET = 0;
int WEATHER_COD = 0;


void loadSettings() {
    preferences.begin("neo-clock", true);  // 只读模式加载
    
    TIME_PER_APP = preferences.getUInt("appTime", 5000);
    BRIGHTNESS = preferences.getUChar("brightness", 70);
    AUTO_TRANSITION = preferences.getBool("autoTrans", true);
    SHOW_WEEKDAY = preferences.getBool("showWeek", true);
    SHOW_TIME = preferences.getBool("showTime", true);
    SHOW_DATE = preferences.getBool("showDate", true);
    SHOW_TEMP = preferences.getBool("showTemp", false);
    SHOW_HUM = preferences.getBool("showHum", false);
    SHOW_WIND = preferences.getBool("showWind", false);

    // 加载时间日期格式
    TIME_FORMAT = preferences.getString("timeFmt", "%H %M");
    DATE_FORMAT = preferences.getString("dateFmt", "%m/%d");
    
    // 加载颜色配置
    TIME_COLOR = preferences.getString("timeColor", "#FFFFFF");
    TIME_WEEKDAY_ACTIVE_COLOR = preferences.getString("timeWkAct", "#f273e1");
    TIME_WEEKDAY_INACTIVE_COLOR = preferences.getString("timeWkInact", "#00bfff");
    DATE_COLOR = preferences.getString("dateColor", "#FFFFFF");
    DATE_WEEKDAY_ACTIVE_COLOR = preferences.getString("dateWkAct", "#f273e1");
    DATE_WEEKDAY_INACTIVE_COLOR = preferences.getString("dateWkInact", "#00bfff");
    TEMP_COLOR = preferences.getString("tempColor", "#FF6400");
    HUM_COLOR = preferences.getString("humColor", "#0096FF");
    
    // 加载图标配置
    TIME_ICON = preferences.getString("timeIcon", "");
    DATE_ICON = preferences.getString("dateIcon", "");
    APP_WEATHER_ICON = preferences.getString("weatherIcon", "");
    WIND_ICON = preferences.getString("windIcon", "");

    // 加载应用显示时长
    TIME_DURATION = preferences.getUShort("timeDur", 0);
    DATE_DURATION = preferences.getUShort("dateDur", 0);
    TEMP_DURATION = preferences.getUShort("tempDur", 0);
    HUM_DURATION = preferences.getUShort("humDur", 0);
    WEATHER_DURATION = preferences.getUShort("weatherDur", 0);
    WIND_DURATION = preferences.getUShort("windDur", 0);

    // 加载应用显示位置
    TIME_POSITION = preferences.getInt("timePos", 0);
    DATE_POSITION = preferences.getInt("datePos", 1);
    TEMP_POSITION = preferences.getInt("tempPos", 2);
    HUM_POSITION = preferences.getInt("humPos", 3);
    WEATHER_POSITION = preferences.getInt("weatherPos", 4);
    WIND_POSITION = preferences.getInt("windPos", 5);

    preferences.end();  // 关闭 Preferences
    
    Serial.println("设置加载完成");
    Serial.printf("应用切换时间: %d ms\n", TIME_PER_APP);
    Serial.printf("亮度: %d\n", BRIGHTNESS);
    Serial.printf("时间格式: %s\n", TIME_FORMAT.c_str());
    Serial.printf("日期格式: %s\n", DATE_FORMAT.c_str());
}

void saveSettings() {
    preferences.begin("neo-clock", false);  // 读写模式保存
    
    preferences.putUInt("appTime", TIME_PER_APP);
    preferences.putUChar("brightness", BRIGHTNESS);
    preferences.putBool("autoTrans", AUTO_TRANSITION);
    preferences.putBool("showWeek", SHOW_WEEKDAY);
    preferences.putBool("showTime", SHOW_TIME);
    preferences.putBool("showDate", SHOW_DATE);
    preferences.putBool("showTemp", SHOW_TEMP);
    preferences.putBool("showHum", SHOW_HUM);
    preferences.putBool("showWind", SHOW_WIND);

    // 保存时间日期格式
    preferences.putString("timeFmt", TIME_FORMAT);
    preferences.putString("dateFmt", DATE_FORMAT);
    
    // 保存颜色配置
    preferences.putString("timeColor", TIME_COLOR);
    preferences.putString("timeWkAct", TIME_WEEKDAY_ACTIVE_COLOR);
    preferences.putString("timeWkInact", TIME_WEEKDAY_INACTIVE_COLOR);
    preferences.putString("dateColor", DATE_COLOR);
    preferences.putString("dateWkAct", DATE_WEEKDAY_ACTIVE_COLOR);
    preferences.putString("dateWkInact", DATE_WEEKDAY_INACTIVE_COLOR);
    preferences.putString("tempColor", TEMP_COLOR);
    preferences.putString("humColor", HUM_COLOR);
    
    // 保存图标配置
    preferences.putString("timeIcon", TIME_ICON);
    preferences.putString("dateIcon", DATE_ICON);
    preferences.putString("weatherIcon", APP_WEATHER_ICON);
    preferences.putString("windIcon", WIND_ICON);

    // 保存应用显示时长
    preferences.putUShort("timeDur", TIME_DURATION);
    preferences.putUShort("dateDur", DATE_DURATION);
    preferences.putUShort("tempDur", TEMP_DURATION);
    preferences.putUShort("humDur", HUM_DURATION);
    preferences.putUShort("weatherDur", WEATHER_DURATION);
    preferences.putUShort("windDur", WIND_DURATION);

    // 保存应用显示位置
    preferences.putInt("timePos", TIME_POSITION);
    preferences.putInt("datePos", DATE_POSITION);
    preferences.putInt("tempPos", TEMP_POSITION);
    preferences.putInt("humPos", HUM_POSITION);
    preferences.putInt("weatherPos", WEATHER_POSITION);
    preferences.putInt("windPos", WIND_POSITION);

    preferences.end();  // 关闭 Preferences

    Serial.println("设置已保存");
}