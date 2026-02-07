#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include "DisplayManager.h"
#include "PeripheryManager.h"
#include "WeatherManager.h"
#include "ServerManager.h"

#include "Apps.h"

WebSocketsServer webSocket(81);

void setup()
{
    Serial.begin(115200);
    Serial.println("\n\n=== NeoClock 矩阵时钟启动 ===");
    WiFi.begin();
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
    PeripheryManager.setup();
    DisplayManager.setup();
    // 启动天气管理器
    WeatherManager.setup();

    delay(500);
    // 加载应用
    Serial.println("加载应用...");
    DisplayManager.loadNativeApps();
    DisplayManager.applyAllSettings();
    
    // 启动WebSocket服务器（仅用于控制面板，端口81）
    Serial.println("启动WebSocket服务器...");
    ServerManager.setup(&webSocket);

    // 初始化 Liveview（100ms 更新间隔）
    Serial.println("初始化 Liveview...");

    DisplayManager.setLiveviewCallback([](const char* data, size_t length) {
        ServerManager.sendLiveviewData(data, length);
    });

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
    PeripheryManager.tick();
    ServerManager.tick();
}
