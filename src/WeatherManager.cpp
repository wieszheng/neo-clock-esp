#include "WeatherManager.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include "Globals.h"

WeatherManager_ &WeatherManager_::getInstance()
{
    static WeatherManager_ instance;
    return instance;
}

WeatherManager_ &WeatherManager = WeatherManager_::getInstance();

void WeatherManager_::setup()
{
    lastUpdate = 0;
    // 创建后台任务负责周期性获取天气，避免阻塞主循环
    startBackgroundTask();
}

void WeatherManager_::tick()
{
    // 已由后台任务负责，主循环无需调用
}

static void weatherTask(void *param) {
    (void)param;
    for (;;) {
        if (WEATHER_API_KEY.length() > 0 && WiFi.status() == WL_CONNECTED) {
            Serial.println("[WeatherTask] Starting fetch");
            WeatherManager.fetchOnce();
        } else {
            Serial.println("[WeatherTask] Skipping fetch (no API key or WiFi not connected)");
        }
        // 等待指定间隔
        vTaskDelay(WEATHER_UPDATE_INTERVAL / portTICK_PERIOD_MS);
    }
}

void WeatherManager_::startBackgroundTask()
{
    if (taskHandle == NULL) {
        xTaskCreatePinnedToCore(weatherTask, "WeatherTask", 4096, NULL, 1, &taskHandle, 1);
        Serial.println("[Weather] Background task started");
    }
}

void WeatherManager_::stopBackgroundTask()
{
    if (taskHandle != NULL) {
        vTaskDelete(taskHandle);
        taskHandle = NULL;
        Serial.println("[Weather] Background task stopped");
    }
}

void WeatherManager_::fetchOnce()
{
    fetchWeather();
    lastUpdate = millis();
}

void WeatherManager_::fetchWeather()
{
    // 使用 OpenWeatherMap 当前天气接口（HTTP），单位：摄氏度
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + WEATHER_CITY + "&appid=" + WEATHER_API_KEY + "&units=metric&lang=en";

    WiFiClient client;
    HTTPClient http;
    Serial.print("[Weather] Fetching: "); Serial.println(url);
    if (http.begin(client, url)) {
        int httpCode = http.GET();
        Serial.printf("[Weather] HTTP code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            Serial.printf("[Weather] Payload len=%d\n", payload.length());
            const size_t capacity = 2*1024;
            DynamicJsonDocument doc(capacity);
            auto err = deserializeJson(doc, payload);
            if (!err) {
                Serial.println("[Weather] JSON parsed");
                // 解析温度与湿度
                if (doc.containsKey("main")) {
                    JsonObject main = doc["main"].as<JsonObject>();
                    if (main.containsKey("temp")) OUTDOOR_TEMP = main["temp"].as<float>();
                    Serial.printf("[Weather] 室外温度=%.1f°C\n", OUTDOOR_TEMP);
                    if (main.containsKey("humidity")) OUTDOOR_HUM = main["humidity"].as<float>();
                    Serial.printf("[Weather] 室外湿度=%.1f%%\n", OUTDOOR_HUM);
                    if (main.containsKey("pressure")) {
                        WEATHER_PRESSURE = main["pressure"].as<int>();
                        Serial.printf("[Weather] pressure=%d hPa\n", WEATHER_PRESSURE);
                    }
                }

                // 解析天气描述与图标
                if (doc.containsKey("weather") && doc["weather"].is<JsonArray>()) {
                    JsonArray arr = doc["weather"].as<JsonArray>();
                    if (arr.size() > 0) {
                        JsonObject w0 = arr[0].as<JsonObject>();
                        if (w0.containsKey("main")) {
                            CURRENT_WEATHER = String((const char*)w0["main"].as<const char*>());
                        } else if (w0.containsKey("description")) {
                            CURRENT_WEATHER = String((const char*)w0["description"].as<const char*>());
                        }
                        if (w0.containsKey("icon")) {
                            WEATHER_ICON = String((const char*)w0["icon"].as<const char*>());
                            Serial.print("[Weather] icon: "); Serial.println(WEATHER_ICON);
                        }
                        Serial.print("[Weather] desc: "); Serial.println(CURRENT_WEATHER);
                    }
                }

                // 解析风
                if (doc.containsKey("wind")) {
                    JsonObject wind = doc["wind"].as<JsonObject>();
                    if (wind.containsKey("speed")) {
                        WEATHER_WIND_SPEED = wind["speed"].as<float>();
                        Serial.printf("[Weather] wind speed=%.2f m/s\n", WEATHER_WIND_SPEED);
                    }
                    if (wind.containsKey("deg")) {
                        WEATHER_WIND_DIR = wind["deg"].as<int>();
                        Serial.printf("[Weather] wind dir=%d deg\n", WEATHER_WIND_DIR);
                    }
                }

                // 城市名、日出/日落、时区、响应码
                // if (doc.containsKey("name")) {
                //     WEATHER_CITY = String((const char*)doc["name"].as<const char*>());
                //     Serial.print("[Weather] city: "); Serial.println(WEATHER_CITY);
                // }
                if (doc.containsKey("sys")) {
                    JsonObject sys = doc["sys"].as<JsonObject>();
                    if (sys.containsKey("sunrise")) {
                        WEATHER_SUNRISE = sys["sunrise"].as<unsigned long>();
                        Serial.printf("[Weather] sunrise=%lu\n", WEATHER_SUNRISE);
                    }
                    if (sys.containsKey("sunset")) {
                        WEATHER_SUNSET = sys["sunset"].as<unsigned long>();
                        Serial.printf("[Weather] sunset=%lu\n", WEATHER_SUNSET);
                    }
                }
                if (doc.containsKey("cod")) {
                    WEATHER_COD = doc["cod"].as<int>();
                    Serial.printf("[Weather] cod=%d\n", WEATHER_COD);
                }
            }
        }
        http.end();
    }
}
