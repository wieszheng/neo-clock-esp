/**
 * @file WeatherManager.cpp
 * @brief 天气管理器 — 定时从 OpenWeatherMap 获取天气数据
 *
 * ESP32 性能优化:
 *   - 使用 Stream 流式解析 JSON，避免 http.getString() 创建巨大的 String
 * 临时对象 这就消除了 2KB+ 的瞬时 RAM 分配，大幅减少内存碎片风险。
 *   - 增加 HTTP 响应流的超时和错误处理。
 */

#include "WeatherManager.h"
#include "Globals.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>

WeatherManager_ &WeatherManager_::getInstance() {
  static WeatherManager_ instance;
  return instance;
}

WeatherManager_ &WeatherManager = WeatherManager_::getInstance();

void WeatherManager_::setup() {
  lastUpdate = 0;
  startBackgroundTask();
}

void WeatherManager_::tick() {
  // 已由后台任务负责
}

static void weatherTask(void *param) {
  (void)param;
  for (;;) {
    if (WEATHER_API_KEY.length() > 0 && WiFi.status() == WL_CONNECTED) {
      // Serial.println("[WeatherTask] Starting fetch");
      WeatherManager.fetchOnce();
    }
    vTaskDelay(WEATHER_UPDATE_INTERVAL / portTICK_PERIOD_MS);
  }
}

void WeatherManager_::startBackgroundTask() {
  if (taskHandle == NULL) {
    // 降低栈大小从 4096 到 3072 (如果流式解析省内存)
    // 但为了安全起见保持 4096
    xTaskCreatePinnedToCore(weatherTask, "WeatherTask", 4096, NULL, 1,
                            &taskHandle, 1);
    Serial.println("[Weather] Background task started");
  }
}

void WeatherManager_::stopBackgroundTask() {
  if (taskHandle != NULL) {
    vTaskDelete(taskHandle);
    taskHandle = NULL;
    Serial.println("[Weather] Background task stopped");
  }
}

void WeatherManager_::fetchOnce() {
  fetchWeather();
  lastUpdate = millis();
}

void WeatherManager_::fetchWeather() {
  String url =
      "http://api.openweathermap.org/data/2.5/weather?q=" + WEATHER_CITY +
      "&appid=" + WEATHER_API_KEY + "&units=metric&lang=en";

  WiFiClient client;
  HTTPClient http;

  Serial.print("[Weather] Fetching...");

  // [网络优化] 设置超时，防止网络卡顿时阻塞任务过久
  http.setTimeout(5000);

  if (http.begin(client, url)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      // [内存优化] 使用 Stream 流式解析，不从堆分配巨大的 String 缓冲区
      // http.getString() 会分配整个 Payload 大小的内存
      // deserializeJson(doc, http.getStream()) 则是边读边解析

      WiFiClient *stream = http.getStreamPtr();

      // 计算 capacity:
      // 天气 JSON 通常 < 1KB，但留足余量。
      // 静态分配在栈上或使用小得多的 buffer
      DynamicJsonDocument doc(2048);

      // 直接从流反序列化
      DeserializationError err = deserializeJson(doc, *stream);

      if (!err) {
        // 解析温度与湿度
        if (doc.containsKey("main")) {
          JsonObject main = doc["main"];
          if (main.containsKey("temp"))
            OUTDOOR_TEMP = main["temp"];
          if (main.containsKey("humidity"))
            OUTDOOR_HUM = main["humidity"];
          if (main.containsKey("pressure"))
            WEATHER_PRESSURE = main["pressure"];

          Serial.printf(" OK (%.1fC, %.0f%%)\n", OUTDOOR_TEMP, OUTDOOR_HUM);
        }

        // 解析天气描述与图标
        if (doc.containsKey("weather")) {
          JsonArray arr = doc["weather"];
          if (arr.size() > 0) {
            JsonObject w0 = arr[0];
            const char *mainGroup = w0["main"];
            const char *desc = w0["description"];
            const char *icon = w0["icon"];

            if (mainGroup)
              CURRENT_WEATHER = String(mainGroup);
            else if (desc)
              CURRENT_WEATHER = String(desc);

            if (icon)
              WEATHER_ICON = String(icon);
          }
        }

        // 解析风
        if (doc.containsKey("wind")) {
          JsonObject wind = doc["wind"];
          if (wind.containsKey("speed"))
            WEATHER_WIND_SPEED = wind["speed"];
          if (wind.containsKey("deg"))
            WEATHER_WIND_DIR = wind["deg"];
        }

        // 日出/日落
        if (doc.containsKey("sys")) {
          JsonObject sys = doc["sys"];
          if (sys.containsKey("sunrise"))
            WEATHER_SUNRISE = sys["sunrise"];
          if (sys.containsKey("sunset"))
            WEATHER_SUNSET = sys["sunset"];
        }

        if (doc.containsKey("cod"))
          WEATHER_COD = doc["cod"];

      } else {
        Serial.print("[Weather] JSON Parse Failed: ");
        Serial.println(err.c_str());
      }
    } else {
      Serial.printf("[Weather] HTTP Error: %d\n", httpCode);
    }
    http.end();
  } else {
    Serial.println("[Weather] Connection failed");
  }
}
