/**
 * @file main.cpp
 * @brief NeoClock 矩阵时钟主程序
 *
 * 本文件实现：
 *   - 系统初始化 (LittleFS、外设、显示、网络等)
 *   - 主循环调度
 *   - 各模块协调 (显示、传感器、通信)
 *
 * 硬件平台: ESP32
 * 矩阵规格: 32×8 LED (WS2812B/SK6812)
 */

#include "DisplayManager.h"
#include "Liveview.h"
#include "PeripheryManager.h"
#include "ServerManager.h"
#include "WeatherManager.h"
#include "WebConfigManager.h"
#include <Arduino.h>
#include <WebSocketsServer.h>
#include <WiFi.h>

#include "Apps.h"

// ==================================================================
// WebSocket 服务器 (端口 81)
// ==================================================================
WebSocketsServer webSocket(81);

// ==================================================================
// 系统初始化
// ==================================================================

/**
 * @brief 系统初始化函数
 *
 * 初始化顺序：
 *   1. 串口通信
 *   2. LittleFS 文件系统
 *   3. 外设 (DHT22、LDR)
 *   4. LED 矩阵显示
 *   5. 加载配置
 *   6. WiFi 配网管理器 (核心)
 *   7. 天气管理器
 *   8. 应用加载
 *   9. WebSocket 服务器
 *   10. Liveview 实时预览
 */
void setup()
{
  Serial.begin(115200);
  Serial.println("\n\n=== NeoClock 矩阵时钟启动 ===");

  // 初始化 LittleFS 文件系统
  Serial.println("初始化文件系统...");
  if (!LittleFS.begin(true, "/littlefs"))
  {
    Serial.println("LittleFS 挂载失败！正在格式化...");
  }
  else
  {
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
  Liveview.setCallback([](const char *data, size_t length)
                       {
    if (WebConfigManager.isConnected()) {
      ServerManager.sendLiveviewData(data, length);
    } });

  if (WebConfigManager.isAPMode())
  {
    // AP 配网模式提示
    Serial.println("⚠️ WiFi 未连接，进入配网模式");
    Serial.printf("请连接热点: %s\n", WebConfigManager.getAPName().c_str());
    Serial.printf("并访问 http://192.168.4.1 进行配网\n");
  }

  Serial.println("========================================");
  Serial.println("   系统初始化完成");
  Serial.println("========================================");

  if (WebConfigManager.isConnected())
  {
    Serial.printf("WiFi: 已连接 (%s)\n", WiFi.SSID().c_str());
    Serial.printf("IP 地址: %s\n", WiFi.localIP().toString().c_str());
  }
  else
  {
    Serial.printf("模式: AP 配网 (IP: %s)\n",
                  WiFi.softAPIP().toString().c_str());
  }

  Serial.printf("应用数量: %d\n", Apps.size());
  Serial.println("Web控制面板: http://[Device IP]");
  Serial.println("WebSocket: ws://[Device IP]:81");
  Serial.println("========================================\n");
}

// ==================================================================
// 主循环
// ==================================================================

/**
 * @brief 主循环函数
 *
 * 循环顺序 (保证性能和正确性)：
 *   1. WebConfigManager.tick() - 始终需要 (处理配网、DNS、重连等)
 *   2. DisplayManager.tick() - 渲染当前帧到 leds[]
 *   3. Liveview.tick() - 采样 leds[] (纯内存操作，极快)
 *   4. PeripheryManager.tick() - 传感器读取、LDR 更新
 *   5. ServerManager.tick() - WebSocket 收包处理 (ws->loop())
 *   6. Liveview.flush() - 发送采样帧 (TCP 缓冲区最宽裕时)
 *
 * 性能优化：
 *   - Liveview 采样在 ws->loop() 之前，避免网络延迟影响渲染
 *   - Liveview.flush() 在 ws->loop() 之后，此时 TCP 缓冲区腾空，阻塞概率最低
 */
void loop()
{
  // 配网管理器始终需要 tick（处理HTTP请求、DNS、断线重连等）
  WebConfigManager.tick();

  // 1. 渲染当前帧到 leds[]
  DisplayManager.tick();

  // 2. 立即采样 leds[]（纯内存操作，≈几十μs，不阻塞渲染）
  Liveview.tick();

  // 外设管理器始终 tick
  PeripheryManager.tick();

  // 3. WebSocket 收包处理（ws->loop），让 TCP 内核缓冲区尽量腾空
  if (WebConfigManager.isConnected())
  {
    ServerManager.tick();
  }

  // 4. 发送采样帧（在 ws->loop() 之后，TCP 缓冲区最宽裕，阻塞概率最低）
  if (WebConfigManager.isConnected())
  {
    Liveview.flush();
  }
}
