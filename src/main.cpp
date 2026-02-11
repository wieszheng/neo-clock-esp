#include "DisplayManager.h"
#include "PeripheryManager.h"
#include "ServerManager.h"
#include "WeatherManager.h"
#include "WebConfigManager.h"
#include <Arduino.h>
#include <WebSocketsServer.h>
#include <WiFi.h>

#include "Apps.h"

WebSocketsServer webSocket(81);

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== NeoClock 矩阵时钟启动 ===");

  // 初始化 LittleFS 文件系统
  Serial.println("初始化文件系统...");
  if (!LittleFS.begin(true, "/littlefs")) {
    Serial.println("LittleFS 挂载失败！正在格式化...");
  } else {
    Serial.println("LittleFS 挂载成功");
  }

  // 初始化外设（传感器等）
  PeripheryManager.setup();

  // 初始化显示
  DisplayManager.setup();

  // 加载全局设置
  Serial.println("加载设置...");
  loadSettings();

  // ========================================
  // Web 配网管理器 (核心！)
  // 自动处理：已保存WiFi凭据→尝试连接→失败则启动AP配网
  // ========================================
  Serial.println("启动配网管理器...");
  WebConfigManager.setup();

  // 启动天气管理器 (后台任务会自动等到WiFi连接)
  WeatherManager.setup();

  delay(500); // 给一点时间让后台任务启动

  // 加载应用
  Serial.println("加载应用...");
  DisplayManager.loadNativeApps();
  DisplayManager.applyAllSettings();

  // 启动WebSocket服务器
  Serial.println("启动WebSocket服务器...");
  ServerManager.setup(&webSocket);

  // 初始化 Liveview
  Serial.println("初始化 Liveview...");
  DisplayManager.setLiveviewCallback([](const char *data, size_t length) {
    if (WebConfigManager.isConnected()) {
      ServerManager.sendLiveviewData(data, length);
    }
  });

  if (WebConfigManager.isAPMode()) {
    // AP 配网模式提示
    Serial.println("⚠️ WiFi 未连接，进入配网模式");
    Serial.printf("请连接热点: %s\n", WebConfigManager.getAPName().c_str());
    Serial.printf("并访问 http://192.168.4.1 进行配网\n");
  }

  Serial.println("========================================");
  Serial.println("   系统初始化完成");
  Serial.println("========================================");

  if (WebConfigManager.isConnected()) {
    Serial.printf("WiFi: 已连接 (%s)\n", WiFi.SSID().c_str());
    Serial.printf("IP 地址: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.printf("模式: AP 配网 (IP: %s)\n",
                  WiFi.softAPIP().toString().c_str());
  }

  Serial.printf("应用数量: %d\n", Apps.size());
  Serial.println("Web控制面板: http://[Device IP]");
  Serial.println("WebSocket: ws://[Device IP]:81");
  Serial.println("========================================\n");
}

void loop() {
  // 配网管理器始终需要 tick（处理HTTP请求、DNS、断线重连等）
  WebConfigManager.tick();

  // 显示管理器始终 tick
  DisplayManager.tick();

  // 外设管理器始终 tick
  PeripheryManager.tick();

  // WebSocket 仅在连接 WiFi 后才处理
  if (WebConfigManager.isConnected()) {
    ServerManager.tick();
  }
}
