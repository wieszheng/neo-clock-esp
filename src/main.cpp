#include <Arduino.h>
#include "DisplayManager.h"
#include "Apps.h"
#include <WiFi.h>

void setup()
{
    Serial.begin(115200);
    Serial.println("\n\n=== NeoClock 矩阵时钟启动 ===");

    // 初始化 LittleFS 文件系统
    Serial.println("初始化文件系统...");
    if (!LittleFS.begin(true, "/littlefs"))
    {
        Serial.println("LittleFS 挂载失败！正在格式化...");
        // if (!LittleFS.format()) {
        //     Serial.println("LittleFS 格式化失败！");
        // } else {
        //     Serial.println("LittleFS 格式化成功，已挂载");
        // }
    }
    else
    {
        Serial.println("LittleFS 挂载成功");
    }

    DisplayManager.setup();
    delay(500);
    // 加载应用
    Serial.println("加载应用...");
    DisplayManager.loadNativeApps();
    DisplayManager.applyAllSettings();
    
    Serial.println("========================================");
    Serial.println("   系统初始化完成");
    Serial.println("========================================");
    Serial.printf("应用数量: %d\n", Apps.size());
    Serial.printf("Web访问地址: http://%s\n",
                  AP_MODE ? WiFi.softAPIP().toString().c_str() : WiFi.localIP().toString().c_str());
    Serial.println("========================================\n");
}

void loop()
{
    DisplayManager.tick();
}
