/**
 * @file WebConfigManager.cpp
 * @brief WiFi 配网管理器实现 — AP 模式、DNS、HTTP 服务器
 *
 * 本文件实现：
 *   - WiFi 连接管理与自动重连
 *   - AP 模式启动与 Captive Portal
 *   - HTTP 路由处理
 *   - WiFi 凭据持久化存储
 *   - 配网 HTML 页面生成
 */

#include "WebConfigManager.h"
#include "DisplayManager.h"
#include "Globals.h"
#include <ArduinoJson.h>
#include <esp_wifi.h>

WebConfigManager_ &WebConfigManager_::getInstance()
{
    static WebConfigManager_ instance;
    return instance;
}

WebConfigManager_ &WebConfigManager = WebConfigManager_::getInstance();

// =====================================================
// 初始化与主循环
// =====================================================

/**
 * @brief 初始化配网管理器
 *
 * 检查已保存凭据 → 尝试连接 → 失败则启动 AP 配网
 */
void WebConfigManager_::setup()
{
    connState = WIFI_STATE_IDLE;
    portalActive = false;
    connectStartTime = 0;
    lastReconnectAttempt = 0;
    lastConnectMessage = "";
    httpServer = nullptr;
    dnsServer = nullptr;

    generateAPName();

    Serial.println("\n[WebConfig] 初始化配网管理器...");
    Serial.printf("[WebConfig] AP 名称: %s\n", apSSID.c_str());

    // 尝试加载已保存的 WiFi 凭据
    if (loadCredentials())
    {
        Serial.printf("[WebConfig] 已保存的 WiFi: %s\n", savedSSID.c_str());

        // 尝试连接已保存的网络
        Serial.println("[WebConfig] 尝试连接已保存的 WiFi...");
        if (tryConnect(savedSSID, savedPassword))
        {
            Serial.println("[WebConfig] ✅ WiFi 连接成功！");
            connState = WIFI_STATE_CONNECTED;
            AP_MODE = false;

            // NTP 时间同步
            configTime(8 * 3600, 0, "ntp.aliyun.com", "pool.ntp.org",
                       "time.nist.gov");
            Serial.println("[WebConfig] NTP 时间同步已配置 (UTC+8)");

            // 显示连接成功画面
            DisplayManager.setDisplayStatus(DISPLAY_CONNECTED, savedSSID,
                                            WiFi.localIP().toString());
            return;
        }
        else
        {
            Serial.println("[WebConfig] ❌ 连接已保存的 WiFi 失败");
        }
    }
    else
    {
        Serial.println("[WebConfig] 未找到已保存的 WiFi 凭据");
    }

    // 连接失败或无凭据，启动 AP 配网模式
    startAPMode();

    // 通知 DisplayManager 进入 AP 模式显示
    DisplayManager.setDisplayStatus(DISPLAY_AP_MODE, apSSID, "192.168.4.1");
}

void WebConfigManager_::tick()
{
    // AP 配网模式下的处理
    if (portalActive)
    {
        if (dnsServer)
            dnsServer->processNextRequest();
        if (httpServer)
            httpServer->handleClient();
    }

    // 处理连接中状态
    if (connState == WIFI_STATE_CONNECTING)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            connState = WIFI_STATE_CONNECTED;
            AP_MODE = false;
            lastConnectMessage = "连接成功！IP: " + WiFi.localIP().toString();
            Serial.printf("[WebConfig] ✅ WiFi 连接成功！IP: %s\n",
                          WiFi.localIP().toString().c_str());

            // 保存凭据
            saveCredentials(connectingSSID, connectingPassword);

            // NTP 时间同步
            configTime(8 * 3600, 0, "ntp.aliyun.com", "pool.ntp.org",
                       "time.nist.gov");

            // 显示连接成功画面 (5秒后自动切换到正常模式)
            DisplayManager.setDisplayStatus(DISPLAY_CONNECTED, connectingSSID,
                                            WiFi.localIP().toString());
        }
        else if (millis() - connectStartTime > WIFI_CONNECT_TIMEOUT)
        {
            connState = WIFI_STATE_CONNECT_FAILED;
            lastConnectMessage = "连接超时，请检查密码是否正确";
            Serial.println("[WebConfig] ❌ WiFi 连接超时");

            // 确保 AP 模式仍然可用
            if (!portalActive)
            {
                startAPMode();
            }

            // 显示连接失败画面
            DisplayManager.setDisplayStatus(DISPLAY_CONNECT_FAILED, connectingSSID,
                                            "");
        }
    }

    // STA 模式下的断线检测与重连
    if (connState == WIFI_STATE_CONNECTED && WiFi.status() != WL_CONNECTED)
    {
        connState = WIFI_STATE_DISCONNECTED;
        lastReconnectAttempt = millis();
        Serial.println("[WebConfig] ⚠️ WiFi 断开连接");

        // 显示连接中动画（等待重连）
        DisplayManager.setDisplayStatus(DISPLAY_CONNECTING, "", savedSSID);
    }

    // 断线重连
    if (connState == WIFI_STATE_DISCONNECTED)
    {
        if (millis() - lastReconnectAttempt > WIFI_RECONNECT_INTERVAL)
        {
            Serial.println("[WebConfig] 尝试重新连接...");
            lastReconnectAttempt = millis();

            if (savedSSID.length() > 0)
            {
                WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
                connectStartTime = millis();
                connState = WIFI_STATE_CONNECTING;
                connectingSSID = savedSSID;
                connectingPassword = savedPassword;

                // 更新显示为连接中动画
                DisplayManager.setDisplayStatus(DISPLAY_CONNECTING, "", savedSSID);
            }
        }
    }
}

// =====================================================
// WiFi 连接管理
// =====================================================

void WebConfigManager_::generateAPName()
{
    uint32_t chipId = 0;
    for (int i = 0; i < 17; i += 8)
    {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    apSSID = String(AP_SSID_PREFIX) + String(chipId & 0xFFFF, HEX);
    apSSID.toUpperCase();
}

bool WebConfigManager_::loadCredentials()
{
    prefs.begin("wifi-cred", true); // 只读
    savedSSID = prefs.getString("ssid", "");
    savedPassword = prefs.getString("password", "");
    prefs.end();

    return savedSSID.length() > 0;
}

void WebConfigManager_::saveCredentials(const String &ssid,
                                        const String &password)
{
    prefs.begin("wifi-cred", false); // 读写
    prefs.putString("ssid", ssid);
    prefs.putString("password", password);
    prefs.end();

    savedSSID = ssid;
    savedPassword = password;
    Serial.printf("[WebConfig] WiFi 凭据已保存: %s\n", ssid.c_str());
}

void WebConfigManager_::clearCredentials()
{
    prefs.begin("wifi-cred", false);
    prefs.clear();
    prefs.end();

    savedSSID = "";
    savedPassword = "";
    Serial.println("[WebConfig] WiFi 凭据已清除");
}

bool WebConfigManager_::tryConnect(const String &ssid, const String &password,
                                   unsigned long timeout)
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    Serial.printf("[WebConfig] 正在连接 %s", ssid.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf("[WebConfig] 已连接！IP: %s\n",
                      WiFi.localIP().toString().c_str());
        return true;
    }

    Serial.printf("[WebConfig] 连接 %s 失败 (status=%d)\n", ssid.c_str(),
                  WiFi.status());
    WiFi.disconnect();
    return false;
}

// =====================================================
// AP 模式管理
// =====================================================

void WebConfigManager_::startAPMode()
{
    Serial.println("[WebConfig] 🌐 启动 AP 配网模式...");

    // 设置 AP + STA 模式 (保留 STA 以便后续连接)
    WiFi.mode(WIFI_AP_STA);

    // 配置 AP
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);

    if (strlen(AP_PASSWORD) > 0)
    {
        WiFi.softAP(apSSID.c_str(), AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CONN);
    }
    else
    {
        WiFi.softAP(apSSID.c_str());
    }

    delay(100);

    Serial.printf("[WebConfig] AP 热点: %s\n", apSSID.c_str());
    Serial.printf("[WebConfig] AP IP: %s\n", WiFi.softAPIP().toString().c_str());

    connState = WIFI_STATE_AP_MODE;
    AP_MODE = true;

    // 启动 DNS (Captive Portal)
    startDNS();

    // 启动 HTTP 服务器
    startHTTPServer();

    portalActive = true;
    Serial.println("[WebConfig] ✅ 配网门户已启动");
}

void WebConfigManager_::stopAPMode()
{
    stopHTTPServer();
    stopDNS();

    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);

    portalActive = false;
    AP_MODE = false;
    Serial.println("[WebConfig] AP 模式已关闭");
}

void WebConfigManager_::startDNS()
{
    if (!dnsServer)
    {
        dnsServer = new DNSServer();
    }
    // 将所有域名解析到 AP IP（Captive Portal）
    dnsServer->start(DNS_PORT, "*", AP_IP);
    Serial.println("[WebConfig] DNS 服务已启动 (Captive Portal)");
}

void WebConfigManager_::stopDNS()
{
    if (dnsServer)
    {
        dnsServer->stop();
        delete dnsServer;
        dnsServer = nullptr;
    }
}

// =====================================================
// HTTP 服务器
// =====================================================

void WebConfigManager_::startHTTPServer()
{
    if (!httpServer)
    {
        httpServer = new WebServer(80);
    }

    // 注册路由
    httpServer->on("/", HTTP_GET, [this]()
                   { handleRoot(); });
    httpServer->on("/scan", HTTP_GET, [this]()
                   { handleScan(); });
    httpServer->on("/connect", HTTP_POST, [this]()
                   { handleConnect(); });
    httpServer->on("/status", HTTP_GET, [this]()
                   { handleStatus(); });
    httpServer->on("/restart", HTTP_POST, [this]()
                   { handleRestart(); });

    // Captive Portal 检测端点
    httpServer->on("/generate_204", HTTP_GET,
                   [this]()
                   { handleRoot(); }); // Android
    httpServer->on("/fwlink", HTTP_GET, [this]()
                   { handleRoot(); }); // Windows
    httpServer->on("/hotspot-detect.html", HTTP_GET,
                   [this]()
                   { handleRoot(); }); // iOS
    httpServer->on("/connecttest.txt", HTTP_GET,
                   [this]()
                   { handleRoot(); }); // Windows 11
    httpServer->on("/redirect", HTTP_GET, [this]()
                   { handleRoot(); });

    httpServer->onNotFound([this]()
                           { handleNotFound(); });

    httpServer->begin();
    Serial.println("[WebConfig] HTTP 服务器已启动 (端口 80)");
}

void WebConfigManager_::stopHTTPServer()
{
    if (httpServer)
    {
        httpServer->stop();
        delete httpServer;
        httpServer = nullptr;
    }
}

// =====================================================
// HTTP 路由处理
// =====================================================

void WebConfigManager_::handleRoot()
{
    httpServer->send_P(200, "text/html; charset=utf-8", getConfigPageHtml());
}

void WebConfigManager_::handleScan()
{
    Serial.println("[WebConfig] 扫描 WiFi 网络...");

    int n = WiFi.scanNetworks();

    DynamicJsonDocument doc(2048);
    JsonArray networks = doc.createNestedArray("networks");

    for (int i = 0; i < n && i < 20; i++)
    {
        JsonObject net = networks.createNestedObject();
        net["ssid"] = WiFi.SSID(i);
        net["rssi"] = WiFi.RSSI(i);
        net["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        net["channel"] = WiFi.channel(i);
    }

    doc["count"] = n;

    String output;
    serializeJson(doc, output);

    httpServer->sendHeader("Access-Control-Allow-Origin", "*");
    httpServer->send(200, "application/json", output);

    WiFi.scanDelete();
    Serial.printf("[WebConfig] 扫描完成，发现 %d 个网络\n", n);
}

void WebConfigManager_::handleConnect()
{
    if (!httpServer->hasArg("plain"))
    {
        httpServer->send(400, "application/json",
                         "{\"ok\":false,\"msg\":\"缺少请求体\"}");
        return;
    }

    String body = httpServer->arg("plain");
    DynamicJsonDocument doc(512);
    auto err = deserializeJson(doc, body);

    if (err)
    {
        httpServer->send(400, "application/json",
                         "{\"ok\":false,\"msg\":\"JSON 解析失败\"}");
        return;
    }

    String ssid = doc["ssid"].as<String>();
    String password = doc["password"].as<String>();

    if (ssid.length() == 0)
    {
        httpServer->send(400, "application/json",
                         "{\"ok\":false,\"msg\":\"SSID 不能为空\"}");
        return;
    }

    Serial.printf("[WebConfig] 收到连接请求: SSID=%s\n", ssid.c_str());

    // 保存连接信息，设置为连接中状态
    connectingSSID = ssid;
    connectingPassword = password;
    connectStartTime = millis();
    connState = WIFI_STATE_CONNECTING;
    lastConnectMessage = "正在连接...";

    // 先回复客户端
    httpServer->sendHeader("Access-Control-Allow-Origin", "*");
    httpServer->send(200, "application/json",
                     "{\"ok\":true,\"msg\":\"正在连接，请稍候...\"}");

    // 显示连接中画面
    DisplayManager.setDisplayStatus(DISPLAY_CONNECTING, "", ssid);

    // 异步连接（在 tick 中检测结果）
    WiFi.begin(ssid.c_str(), password.c_str());
}

void WebConfigManager_::handleStatus()
{
    DynamicJsonDocument doc(512);

    doc["state"] = (int)connState;

    switch (connState)
    {
    case WIFI_STATE_CONNECTED:
        doc["stateText"] = "已连接";
        doc["ip"] = WiFi.localIP().toString();
        doc["ssid"] = WiFi.SSID();
        doc["rssi"] = WiFi.RSSI();
        doc["mac"] = WiFi.macAddress();
        break;
    case WIFI_STATE_CONNECTING:
        doc["stateText"] = "正在连接...";
        doc["ssid"] = connectingSSID;
        break;
    case WIFI_STATE_AP_MODE:
        doc["stateText"] = "配网模式";
        doc["apSSID"] = apSSID;
        doc["apIP"] = WiFi.softAPIP().toString();
        break;
    case WIFI_STATE_CONNECT_FAILED:
        doc["stateText"] = "连接失败";
        break;
    case WIFI_STATE_DISCONNECTED:
        doc["stateText"] = "已断开";
        break;
    default:
        doc["stateText"] = "空闲";
        break;
    }

    doc["message"] = lastConnectMessage;
    doc["savedSSID"] = savedSSID;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["uptime"] = millis() / 1000;

    String output;
    serializeJson(doc, output);

    httpServer->sendHeader("Access-Control-Allow-Origin", "*");
    httpServer->send(200, "application/json", output);
}

void WebConfigManager_::handleRestart()
{
    httpServer->send(200, "application/json",
                     "{\"ok\":true,\"msg\":\"设备即将重启...\"}");
    delay(1000);
    ESP.restart();
}

void WebConfigManager_::handleNotFound()
{
    // Captive Portal: 将所有未知请求重定向到配网页面
    httpServer->sendHeader("Location", "http://192.168.4.1", true);
    httpServer->send(302, "text/plain", "");
}

// =====================================================
// 公共接口
// =====================================================

String WebConfigManager_::getIP() const
{
    if (connState == WIFI_STATE_CONNECTED)
    {
        return WiFi.localIP().toString();
    }
    else if (connState == WIFI_STATE_AP_MODE)
    {
        return WiFi.softAPIP().toString();
    }
    return "0.0.0.0";
}

void WebConfigManager_::forceAPMode()
{
    Serial.println("[WebConfig] 手动触发 AP 配网模式");
    WiFi.disconnect();
    startAPMode();
}

// =====================================================
// 配网 HTML 页面生成
// =====================================================

const char *WebConfigManager_::getConfigPageHtml()
{
    static const char html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>NeoClock 配网</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        
        :root {
            --bg: #f0f2f5;
            --card: #ffffff;
            --primary: #6366f1;
            --primary-light: #818cf8;
            --primary-dark: #4f46e5;
            --success: #10b981;
            --warning: #f59e0b;
            --danger: #ef4444;
            --text: #1e293b;
            --text-secondary: #64748b;
            --border: #e2e8f0;
            --shadow: 0 4px 24px rgba(0,0,0,0.08);
            --shadow-lg: 0 8px 40px rgba(0,0,0,0.12);
            --radius: 16px;
            --radius-sm: 10px;
        }
        
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
            background: var(--bg);
            color: var(--text);
            min-height: 100vh;
            line-height: 1.6;
        }
        
        .container {
            max-width: 420px;
            margin: 0 auto;
            padding: 20px 16px;
            min-height: 100vh;
        }
        
        /* ===== Header ===== */
        .header {
            text-align: center;
            padding: 32px 0 24px;
        }
        
        .header .logo {
            width: 64px; height: 64px;
            background: linear-gradient(135deg, var(--primary), var(--primary-light));
            border-radius: 20px;
            display: inline-flex;
            align-items: center;
            justify-content: center;
            margin-bottom: 16px;
            box-shadow: 0 8px 24px rgba(99,102,241,0.3);
            animation: float 3s ease-in-out infinite;
        }
        
        @keyframes float {
            0%,100% { transform: translateY(0); }
            50% { transform: translateY(-6px); }
        }
        
        .header .logo svg { width: 36px; height: 36px; fill: white; }
        
        .header h1 {
            font-size: 22px;
            font-weight: 700;
            color: var(--text);
            letter-spacing: -0.02em;
        }
        
        .header p {
            font-size: 14px;
            color: var(--text-secondary);
            margin-top: 4px;
        }
        
        /* ===== Status Badge ===== */
        .status-bar {
            background: var(--card);
            border-radius: var(--radius-sm);
            padding: 12px 16px;
            display: flex;
            align-items: center;
            gap: 10px;
            margin-bottom: 16px;
            box-shadow: var(--shadow);
            transition: all 0.3s ease;
        }
        
        .status-dot {
            width: 10px; height: 10px;
            border-radius: 50%;
            background: var(--warning);
            flex-shrink: 0;
            animation: pulse-dot 2s infinite;
        }
        
        .status-dot.connected { background: var(--success); animation: none; }
        .status-dot.connecting { background: var(--primary); }
        .status-dot.failed { background: var(--danger); animation: none; }
        
        @keyframes pulse-dot {
            0%,100% { opacity: 1; transform: scale(1); }
            50% { opacity: 0.5; transform: scale(0.85); }
        }
        
        .status-text {
            font-size: 13px;
            color: var(--text-secondary);
            flex: 1;
        }
        
        .status-text strong {
            color: var(--text);
            font-weight: 600;
        }
        
        /* ===== Cards ===== */
        .card {
            background: var(--card);
            border-radius: var(--radius);
            padding: 20px;
            margin-bottom: 16px;
            box-shadow: var(--shadow);
        }
        
        .card-title {
            font-size: 15px;
            font-weight: 600;
            color: var(--text);
            margin-bottom: 16px;
            display: flex;
            align-items: center;
            gap: 8px;
        }
        
        .card-title .icon {
            width: 32px; height: 32px;
            background: linear-gradient(135deg, var(--primary), var(--primary-light));
            border-radius: 8px;
            display: flex;
            align-items: center;
            justify-content: center;
        }
        
        .card-title .icon svg { width: 18px; height: 18px; fill: white; }
        
        /* ===== WiFi List ===== */
        .wifi-list {
            list-style: none;
            max-height: 280px;
            overflow-y: auto;
            scrollbar-width: thin;
        }
        
        .wifi-list::-webkit-scrollbar { width: 4px; }
        .wifi-list::-webkit-scrollbar-thumb { background: #cbd5e1; border-radius: 4px; }
        
        .wifi-item {
            display: flex;
            align-items: center;
            padding: 12px;
            border-radius: var(--radius-sm);
            cursor: pointer;
            transition: all 0.2s ease;
            gap: 12px;
            border: 1px solid transparent;
        }
        
        .wifi-item:hover {
            background: #f8fafc;
            border-color: var(--border);
        }
        
        .wifi-item.selected {
            background: #eef2ff;
            border-color: var(--primary-light);
        }
        
        .wifi-icon {
            width: 36px; height: 36px;
            background: #f1f5f9;
            border-radius: 10px;
            display: flex;
            align-items: center;
            justify-content: center;
            flex-shrink: 0;
        }
        
        .wifi-icon svg { width: 20px; height: 20px; fill: var(--text-secondary); }
        .wifi-item.selected .wifi-icon { background: var(--primary); }
        .wifi-item.selected .wifi-icon svg { fill: white; }
        
        .wifi-info { flex: 1; min-width: 0; }
        .wifi-name {
            font-size: 14px;
            font-weight: 500;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        }
        .wifi-detail {
            font-size: 12px;
            color: var(--text-secondary);
            margin-top: 2px;
        }
        
        .wifi-signal {
            display: flex;
            align-items: flex-end;
            gap: 2px;
            height: 16px;
            flex-shrink: 0;
        }
        
        .wifi-signal .bar {
            width: 3px;
            background: #cbd5e1;
            border-radius: 1px;
            transition: all 0.3s ease;
        }
        
        .wifi-signal .bar.active { background: var(--success); }
        .wifi-signal .bar:nth-child(1) { height: 4px; }
        .wifi-signal .bar:nth-child(2) { height: 7px; }
        .wifi-signal .bar:nth-child(3) { height: 11px; }
        .wifi-signal .bar:nth-child(4) { height: 16px; }
        
        /* ===== Form ===== */
        .form-group {
            margin-bottom: 14px;
        }
        
        .form-label {
            font-size: 13px;
            font-weight: 500;
            color: var(--text-secondary);
            margin-bottom: 6px;
            display: block;
        }
        
        .form-input {
            width: 100%;
            padding: 12px 14px;
            border: 1.5px solid var(--border);
            border-radius: var(--radius-sm);
            font-size: 15px;
            color: var(--text);
            background: #fafbfc;
            outline: none;
            transition: all 0.2s ease;
        }
        
        .form-input:focus {
            border-color: var(--primary);
            box-shadow: 0 0 0 3px rgba(99,102,241,0.12);
            background: white;
        }
        
        .form-input::placeholder { color: #94a3b8; }
        
        .password-wrap {
            position: relative;
        }
        
        .password-toggle {
            position: absolute;
            right: 12px;
            top: 50%;
            transform: translateY(-50%);
            background: none;
            border: none;
            color: var(--text-secondary);
            cursor: pointer;
            padding: 4px;
        }
        
        /* ===== Buttons ===== */
        .btn {
            width: 100%;
            padding: 8px;
            border: none;
            border-radius: var(--radius-sm);
            font-size: 15px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s ease;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 8px;
        }
        
        .btn-primary {
            background: linear-gradient(135deg, var(--primary), var(--primary-dark));
            color: white;
            box-shadow: 0 2px 14px rgba(99,102,241,0.35);
        }
        
        .btn-primary:hover {
            transform: translateY(-1px);
            box-shadow: 0 6px 20px rgba(99,102,241,0.4);
        }
        
        .btn-primary:active {
            transform: translateY(0);
        }
        
        .btn-primary:disabled {
            opacity: 0.6;
            cursor: not-allowed;
            transform: none;
        }
        
        .btn-outline {
            background: transparent;
            color: var(--primary);
            border: 1.5px solid var(--border);
        }
        
        .btn-outline:hover {
            background: #f8fafc;
            border-color: var(--primary-light);
        }
        
        .btn-sm {
            padding: 4px 16px;
            font-size: 13px;
            width: auto;
        }
        
        /* ===== Loading Spinner ===== */
        .spinner {
            width: 18px; height: 18px;
            border: 2.5px solid rgba(255,255,255,0.3);
            border-top-color: white;
            border-radius: 50%;
            animation: spin 0.8s linear infinite;
            display: none;
        }
        
        .btn.loading .spinner { display: block; }
        .btn.loading .btn-text { display: none; }
        
        @keyframes spin { to { transform: rotate(360deg); } }
        
        /* ===== Toast ===== */
        .toast {
            position: fixed;
            bottom: 24px;
            left: 50%;
            transform: translateX(-50%) translateY(100px);
            background: var(--text);
            color: white;
            padding: 12px 24px;
            border-radius: var(--radius-sm);
            font-size: 14px;
            box-shadow: var(--shadow-lg);
            transition: transform 0.3s ease;
            z-index: 1000;
            max-width: 90%;
            text-align: center;
        }
        
        .toast.show { transform: translateX(-50%) translateY(0); }
        .toast.success { background: var(--success); }
        .toast.error { background: var(--danger); }
        
        /* ===== Scan placeholder ===== */
        .scan-placeholder {
            text-align: center;
            padding: 32px 16px;
            color: var(--text-secondary);
        }
        
        .scan-placeholder svg { width: 48px; height: 48px; fill: #cbd5e1; margin-bottom: 12px; }
        .scan-placeholder p { font-size: 14px; }
        
        /* ===== Info Section ===== */
        .info-row {
            display: flex;
            justify-content: space-between;
            padding: 8px 0;
            border-bottom: 1px solid #f1f5f9;
            font-size: 13px;
        }
        .info-row:last-child { border-bottom: none; }
        .info-label { color: var(--text-secondary); }
        .info-value { color: var(--text); font-weight: 500; font-family: monospace; }
        
        /* ===== Footer ===== */
        .footer {
            text-align: center;
            padding: 24px 0;
            font-size: 12px;
            color: #94a3b8;
        }
        
        .btn-group {
            display: flex;
            gap: 10px;
            margin-top: 14px;
        }
        .btn-group .btn { flex: 1; }
    </style>
</head>
<body>
    <div class="container">
        <!-- Header -->
        <div class="header">
            <div class="logo">
                <svg viewBox="0 0 24 24"><path d="M12 2C6.5 2 2 6.5 2 12s4.5 10 10 10 10-4.5 10-10S17.5 2 12 2zm0 18c-4.41 0-8-3.59-8-8s3.59-8 8-8 8 3.59 8 8-3.59 8-8 8zm.5-13H11v6l5.2 3.2.8-1.3-4.5-2.7V7z"/></svg>
            </div>
            <h1>NeoClock 网络配置</h1>
            <p>连接 WiFi 以启用完整功能</p>
        </div>
        
        <!-- Status Bar -->
        <div class="status-bar" id="statusBar">
            <div class="status-dot" id="statusDot"></div>
            <div class="status-text" id="statusText">
                <strong>配网模式</strong> · 等待配置
            </div>
        </div>
        
        <!-- WiFi List Card -->
        <div class="card">
            <div class="card-title">
                <div class="icon">
                    <svg viewBox="0 0 24 24"><path d="M1 9l2 2c4.97-4.97 13.03-4.97 18 0l2-2C16.93 2.93 7.08 2.93 1 9zm8 8l3 3 3-3c-1.65-1.66-4.34-1.66-6 0zm-4-4l2 2c2.76-2.76 7.24-2.76 10 0l2-2C15.14 9.14 8.87 9.14 5 13z"/></svg>
                </div>
                可用网络
                <button class="btn btn-outline btn-sm" onclick="scanWiFi()" id="scanBtn" style="margin-left:auto">
                    <span class="btn-text">扫描</span>
                    <div class="spinner"></div>
                </button>
            </div>
            
            <div id="wifiListArea">
                <div class="scan-placeholder" id="scanPlaceholder">
                    <svg viewBox="0 0 24 24"><path d="M1 9l2 2c4.97-4.97 13.03-4.97 18 0l2-2C16.93 2.93 7.08 2.93 1 9zm8 8l3 3 3-3c-1.65-1.66-4.34-1.66-6 0zm-4-4l2 2c2.76-2.76 7.24-2.76 10 0l2-2C15.14 9.14 8.87 9.14 5 13z"/></svg>
                    <p>点击"扫描"搜索附近的 WiFi 网络</p>
                </div>
                <ul class="wifi-list" id="wifiList" style="display:none"></ul>
            </div>
        </div>
        
        <!-- Connect Form Card -->
        <div class="card" id="connectCard">
            <div class="card-title">
                <div class="icon">
                    <svg viewBox="0 0 24 24"><path d="M18 8h-1V6c0-2.76-2.24-5-5-5S7 3.24 7 6v2H6c-1.1 0-2 .9-2 2v10c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V10c0-1.1-.9-2-2-2zM12 17c-1.1 0-2-.9-2-2s.9-2 2-2 2 .9 2 2-.9 2-2 2zm3.1-9H8.9V6c0-1.71 1.39-3.1 3.1-3.1s3.1 1.39 3.1 3.1v2z"/></svg>
                </div>
                连接网络
            </div>
            
            <div class="form-group">
                <label class="form-label">WiFi 名称 (SSID)</label>
                <input type="text" class="form-input" id="ssidInput" placeholder="选择或输入 WiFi 名称" autocomplete="off">
            </div>
            
            <div class="form-group">
                <label class="form-label">密码</label>
                <div class="password-wrap">
                    <input type="password" class="form-input" id="passInput" placeholder="输入 WiFi 密码" autocomplete="off">
                    <button class="password-toggle" onclick="togglePassword()" type="button">
                        <svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor" id="eyeIcon">
                            <path d="M12 4.5C7 4.5 2.73 7.61 1 12c1.73 4.39 6 7.5 11 7.5s9.27-3.11 11-7.5c-1.73-4.39-6-7.5-11-7.5zM12 17c-2.76 0-5-2.24-5-5s2.24-5 5-5 5 2.24 5 5-2.24 5-5 5zm0-8c-1.66 0-3 1.34-3 3s1.34 3 3 3 3-1.34 3-3-1.34-3-3-3z"/>
                        </svg>
                    </button>
                </div>
            </div>
            
            <button class="btn btn-primary" id="connectBtn" onclick="connectWiFi()">
                <span class="btn-text">连接</span>
                <div class="spinner"></div>
            </button>
        </div>
        
        <!-- Device Info Card -->
        <div class="card">
            <div class="card-title">
                <div class="icon" style="background: linear-gradient(135deg, #10b981, #34d399);">
                    <svg viewBox="0 0 24 24"><path d="M13 9h-2v2H9v2h2v2h2v-2h2v-2h-2V9zm1-7.06c1.09.53 2 1.84 2 3.06 0 1.68-1.36 3.15-2.99 3.15-1.99 0-3.51-1.82-2.99-3.89.33-1.28 1.51-2.18 2.82-2.3l1.16-.02zM12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm0 18c-4.41 0-8-3.59-8-8s3.59-8 8-8 8 3.59 8 8-3.59 8-8 8z"/></svg>
                </div>
                设备信息
            </div>

            <div class="info-row"><span class="info-label">配网地址</span><span class="info-value">192.168.4.1</span></div>
            <div class="info-row"><span class="info-label">固件版本</span><span class="info-value">v1.0.0</span></div>
            <div class="info-row"><span class="info-label">芯片型号</span><span class="info-value">ESP32</span></div>
            <div class="info-row"><span class="info-label">可用内存</span><span class="info-value" id="infoHeap">-</span></div>
            
            <div class="btn-group">
                <button class="btn btn-outline btn-sm" onclick="restartDevice()">重启设备</button>
            </div>
        </div>
        
        <div class="footer">
            NeoClock · 像素时钟 · ESP32
        </div>
    </div>
    
    <!-- Toast -->
    <div class="toast" id="toast"></div>
    
    <script>
        let selectedSSID = '';
        let statusTimer = null;
        
        // ===== WiFi 扫描 =====
        async function scanWiFi() {
            const btn = document.getElementById('scanBtn');
            btn.classList.add('loading');
            btn.disabled = true;
            
            try {
                const r = await fetch('/scan');
                const data = await r.json();
                renderWiFiList(data.networks || []);
                showToast('发现 ' + (data.count || 0) + ' 个网络');
            } catch (e) {
                showToast('扫描失败', 'error');
            } finally {
                btn.classList.remove('loading');
                btn.disabled = false;
            }
        }
        
        function renderWiFiList(networks) {
            const list = document.getElementById('wifiList');
            const placeholder = document.getElementById('scanPlaceholder');
            
            if (!networks.length) {
                placeholder.style.display = 'block';
                list.style.display = 'none';
                return;
            }
            
            placeholder.style.display = 'none';
            list.style.display = 'block';
            
            // 按信号强度排序，去重
            const seen = new Set();
            const unique = networks.filter(n => {
                if (!n.ssid || seen.has(n.ssid)) return false;
                seen.add(n.ssid);
                return true;
            }).sort((a,b) => b.rssi - a.rssi);
            
            list.innerHTML = unique.map(n => {
                const bars = getSignalBars(n.rssi);
                return `<li class="wifi-item${n.ssid === selectedSSID ? ' selected' : ''}" onclick="selectWiFi('${escapeHtml(n.ssid)}', ${n.secure})">
                    <div class="wifi-icon">
                        <svg viewBox="0 0 24 24"><path d="M1 9l2 2c4.97-4.97 13.03-4.97 18 0l2-2C16.93 2.93 7.08 2.93 1 9zm8 8l3 3 3-3c-1.65-1.66-4.34-1.66-6 0zm-4-4l2 2c2.76-2.76 7.24-2.76 10 0l2-2C15.14 9.14 8.87 9.14 5 13z"/></svg>
                    </div>
                    <div class="wifi-info">
                        <div class="wifi-name">${escapeHtml(n.ssid)}</div>
                        <div class="wifi-detail">${n.secure ? '🔒 加密' : '🔓 开放'} · CH ${n.channel || '-'}</div>
                    </div>
                    <div class="wifi-signal">
                        <div class="bar ${bars >= 1 ? 'active' : ''}"></div>
                        <div class="bar ${bars >= 2 ? 'active' : ''}"></div>
                        <div class="bar ${bars >= 3 ? 'active' : ''}"></div>
                        <div class="bar ${bars >= 4 ? 'active' : ''}"></div>
                    </div>
                </li>`;
            }).join('');
        }
        
        function getSignalBars(rssi) {
            if (rssi >= -50) return 4;
            if (rssi >= -60) return 3;
            if (rssi >= -70) return 2;
            return 1;
        }
        
        function selectWiFi(ssid, secure) {
            selectedSSID = ssid;
            document.getElementById('ssidInput').value = ssid;
            document.getElementById('passInput').focus();
            
            // 更新选中状态
            document.querySelectorAll('.wifi-item').forEach(el => el.classList.remove('selected'));
            event.currentTarget.classList.add('selected');
        }
        
        // ===== WiFi 连接 =====
        async function connectWiFi() {
            const ssid = document.getElementById('ssidInput').value.trim();
            const pass = document.getElementById('passInput').value;
            
            if (!ssid) { showToast('请输入 WiFi 名称', 'error'); return; }
            
            const btn = document.getElementById('connectBtn');
            btn.classList.add('loading');
            btn.disabled = true;
            
            try {
                const r = await fetch('/connect', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ ssid, password: pass })
                });
                const data = await r.json();
                
                if (data.ok) {
                    showToast('正在连接 ' + ssid + '...', 'success');
                    startStatusPolling();
                } else {
                    showToast(data.msg || '连接请求失败', 'error');
                    btn.classList.remove('loading');
                    btn.disabled = false;
                }
            } catch (e) {
                showToast('请求失败，请重试', 'error');
                btn.classList.remove('loading');
                btn.disabled = false;
            }
        }
        
        // ===== 状态轮询 =====
        function startStatusPolling() {
            if (statusTimer) clearInterval(statusTimer);
            
            statusTimer = setInterval(async () => {
                try {
                    const r = await fetch('/status');
                    const data = await r.json();
                    updateStatusUI(data);
                    
                    // 连接成功或失败后停止轮询
                    if (data.state === 2) { // CONNECTED
                        clearInterval(statusTimer);
                        statusTimer = null;
                        const btn = document.getElementById('connectBtn');
                        btn.classList.remove('loading');
                        btn.disabled = false;
                        showToast('✅ 连接成功！IP: ' + data.ip, 'success');
                    } else if (data.state === 5) { // CONNECT_FAILED
                        clearInterval(statusTimer);
                        statusTimer = null;
                        const btn = document.getElementById('connectBtn');
                        btn.classList.remove('loading');
                        btn.disabled = false;
                        showToast('❌ ' + (data.message || '连接失败'), 'error');
                    }
                } catch (e) { /* ignore fetch errors during connecting */ }
            }, 2000);
        }
        
        function updateStatusUI(data) {
            const dot = document.getElementById('statusDot');
            const text = document.getElementById('statusText');
            
            dot.className = 'status-dot';
            
            switch (data.state) {
                case 2: // CONNECTED
                    dot.classList.add('connected');
                    text.innerHTML = '<strong>已连接</strong> · ' + (data.ssid || '') + ' · ' + (data.ip || '');
                    break;
                case 1: // CONNECTING
                    dot.classList.add('connecting');
                    text.innerHTML = '<strong>正在连接</strong> · ' + (data.ssid || '');
                    break;
                case 5: // FAILED
                    dot.classList.add('failed');
                    text.innerHTML = '<strong>连接失败</strong> · ' + (data.message || '');
                    break;
                default:
                    text.innerHTML = '<strong>配网模式</strong> · 等待配置';
            }
            
            // 更新设备信息
            if (data.freeHeap) {
                document.getElementById('infoHeap').textContent = Math.round(data.freeHeap / 1024) + ' KB';
            }
        }
        
        // ===== 工具函数 =====
        function togglePassword() {
            const p = document.getElementById('passInput');
            p.type = p.type === 'password' ? 'text' : 'password';
        }
        
        function escapeHtml(s) {
            return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;').replace(/'/g,'&#39;');
        }
        
        function showToast(msg, type) {
            const t = document.getElementById('toast');
            t.textContent = msg;
            t.className = 'toast' + (type ? ' ' + type : '');
            t.classList.add('show');
            setTimeout(() => t.classList.remove('show'), 3000);
        }
        
        async function restartDevice() {
            if (!confirm('确定要重启设备吗？')) return;
            try {
                await fetch('/restart', { method: 'POST' });
                showToast('设备正在重启...', 'success');
            } catch (e) {
                showToast('重启指令已发送');
            }
        }
        
        // ===== 启动时自动扫描 & 获取状态 =====
        window.addEventListener('load', () => {
            scanWiFi();
            fetch('/status').then(r => r.json()).then(updateStatusUI).catch(() => {});
        });
    </script>
</body>
</html>
)rawliteral";

    return html;
}
