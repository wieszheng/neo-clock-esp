#include "ServerManager.h"
#include "DisplayManager.h"
#include "Globals.h"
#include "Apps.h"
#include <ArduinoJson.h>

ServerManager_& ServerManager_::getInstance() {
    static ServerManager_ instance;
    return instance;
}

ServerManager_& ServerManager = ServerManager.getInstance();


// 注意：HTTP 服务器已移除，配网功能由 WebConfig 提供
// WebSocket 服务器保留在 81 端口用于控制面板

void ServerManager_::handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[WS] Client #%u disconnected\n", num);
            break;
            
        case WStype_CONNECTED: {
            IPAddress ip = ws->remoteIP(num);
            Serial.printf("[WS] Client #%u connected from %s\n", num, ip.toString().c_str());
            // 连接时立即发送当前状态
            broadcastStats();
            break;
        }
        
        case WStype_TEXT: {
            Serial.printf("[WS] Received: %s\n", payload);
            
            DynamicJsonDocument doc(2048);
            DeserializationError error = deserializeJson(doc, payload);
            
            if (error) {
                Serial.println("[WS] Parse error");
                return;
            }
            
            String type = doc["type"].as<String>();
            String requestId = doc.containsKey("requestId") ? doc["requestId"].as<String>() : String("");
            
            if (type == "getConfig") {
                broadcastConfig();
            }
            else if (type == "getStats") {
                broadcastStats();
            }
            else if (type == "getLiveview") {
                // 启用实时预览
                Serial.println("[WS] Enabling liveview");
                EnableLiveview = true;
            }
            else if (type == "stopLiveview") {
                // 停止实时预览
                EnableLiveview = false;
            }
            else if (type == "setBrightness") {
                int value = doc["value"].as<int>();
                // 支持 0-100 范围（小程序使用）和 0-255 范围（ESP32 内部）
                if (value <= 100) {
                    // 小程序发送的是 0-100，转换为 0-255
                    BRIGHTNESS = (uint8_t)((value * 255) / 100);
                } else {
                    // 直接使用 0-255 值
                    BRIGHTNESS = (uint8_t)value;
                }
                DisplayManager.setBrightness(BRIGHTNESS);
                saveSettings();
                sendAck(num, requestId, "setBrightness", true);
                broadcastStats();
            }
            else if (type == "setPower") {
                // 设置电源状态：true=开启，false=关闭
                bool powered = doc["powered"].as<bool>();
                MATRIX_OFF = !powered;
                DisplayManager.setBrightness(BRIGHTNESS);
                saveSettings();
                sendAck(num, requestId, "setPower", true);
                // 立即广播状态更新
                broadcastStats();
            }
            else if (type == "setAutoPlay") {
                // 设置自动轮播：true=开启，false=关闭
                bool autoPlay = doc["autoPlay"].as<bool>();
                AUTO_TRANSITION = autoPlay;
                DisplayManager.applyAllSettings();
                saveSettings();
                sendAck(num, requestId, "setAutoPlay", true);
                // 立即广播状态更新
                broadcastStats();
            }
            else if (type == "settingsUpdate") {
                if (doc.containsKey("settings")) {
                    auto s = doc["settings"];
                    if (s.containsKey("appTime")) TIME_PER_APP = s["appTime"].as<int>();
                    if (s.containsKey("brightness")) {
                        int value = s["brightness"].as<int>();
                        BRIGHTNESS = value <= 100 ? (uint8_t)((value * 255) / 100) : (uint8_t)value;
                    }
                    if (s.containsKey("autoTransition")) AUTO_TRANSITION = s["autoTransition"].as<bool>();
                    if (s.containsKey("showWeekday")) SHOW_WEEKDAY = s["showWeekday"].as<bool>();
                    if (s.containsKey("timeFormat")) TIME_FORMAT = s["timeFormat"].as<String>();
                    if (s.containsKey("dateFormat")) DATE_FORMAT = s["dateFormat"].as<String>();

                    DisplayManager.applyAllSettings();
                    saveSettings();

                    sendAck(num, requestId, "settingsUpdate", true);
                    broadcastConfig();
                    broadcastStats();
                } else {
                    sendAck(num, requestId, "settingsUpdate", false, "missing settings");
                }
            }
            else if (type == "appNext") {
                DisplayManager.nextApp();
                sendAck(num, requestId, "appNext", true);
                broadcastStats();
            }
            else if (type == "appPrev") {
                DisplayManager.previousApp();
                sendAck(num, requestId, "appPrev", true);
                broadcastStats();
            }
            else if (type == "cmd") {
                String action = doc["action"].as<String>();
                if (action == "next") {
                    DisplayManager.nextApp();
                    sendAck(num, requestId, "cmd.next", true);
                    // 切换应用后立即广播状态
                    broadcastStats();
                } else if (action == "prev") {
                    DisplayManager.previousApp();
                    sendAck(num, requestId, "cmd.prev", true);
                    // 切换应用后立即广播状态
                    broadcastStats();
                } else if (action == "toggle") {
                    // 保留 toggle 命令以保持向后兼容
                    MATRIX_OFF = !MATRIX_OFF;
                    DisplayManager.setBrightness(BRIGHTNESS);
                    saveSettings();
                    broadcastStats();
                } else if (action == "restart") {
                    ESP.restart();
                } else if (action == "leftClick") {
                    DisplayManager.leftButton();
                } else if (action == "rightClick") {
                    DisplayManager.rightButton();
                } 
            }
            break;
        }
        
        default:
            break;
    }
}

void ServerManager_::sendAck(uint8_t num, const String& requestId, const String& action, bool ok, const String& message) {
    StaticJsonDocument<256> doc;
    doc["type"] = ok ? "ack" : "error";
    if (requestId.length() > 0) doc["requestId"] = requestId;
    doc["action"] = action;
    doc["ok"] = ok;
    if (!ok && message.length() > 0) doc["message"] = message;
    String output;
    serializeJson(doc, output);
    ws->sendTXT(num, output);
}

void ServerManager_::broadcastConfig() {
    StaticJsonDocument<2048> doc;
    doc["type"] = "config";
    
    JsonObject data = doc.createNestedObject("data");
    JsonArray appsArr = data.createNestedArray("apps");
    
    for (auto& app : Apps) {
        JsonObject appObj = appsArr.createNestedObject();
        appObj["id"] = app.name;
        appObj["name"] = app.name;
        // 前端显示用名称（避免小程序维护映射表）
        if (app.name == "time") appObj["displayName"] = "时间";
        else if (app.name == "date") appObj["displayName"] = "日期";
        else if (app.name == "temp") appObj["displayName"] = "温度";
        else if (app.name == "hum") appObj["displayName"] = "湿度";
        else if (app.name == "weather") appObj["displayName"] = "天气";
        else if (app.name == "wind") appObj["displayName"] = "风速";
        else appObj["displayName"] = app.name;
        appObj["enabled"] = app.enabled;
        appObj["position"] = app.position;
        appObj["duration"] = app.duration;
        
        if (app.name == "time") {
            appObj["color"] = TIME_COLOR;
            appObj["weekdayActive"] = TIME_WEEKDAY_ACTIVE_COLOR;
            appObj["weekdayInactive"] = TIME_WEEKDAY_INACTIVE_COLOR;
            appObj["timeFormat"] = TIME_FORMAT; // 添加时间格式
            if (TIME_ICON.length() > 0) {
                appObj["iconName"] = TIME_ICON;
            }
        } else if (app.name == "date") {
            appObj["color"] = DATE_COLOR;
            appObj["weekdayActive"] = DATE_WEEKDAY_ACTIVE_COLOR;
            appObj["weekdayInactive"] = DATE_WEEKDAY_INACTIVE_COLOR;
            appObj["dateFormat"] = DATE_FORMAT; // 添加日期格式
            if (DATE_ICON.length() > 0) {
                appObj["iconName"] = DATE_ICON;
            }
        } else if (app.name == "temp") {
            appObj["color"] = TEMP_COLOR;
        } else if (app.name == "hum") {
            appObj["color"] = HUM_COLOR;
        } else if (app.name == "wind") {
            appObj["color"] = WIND_COLOR;
            if (WIND_ICON.length() > 0) {
                appObj["iconName"] = WIND_ICON;
            }
        } else if (app.name == "weather") {
            if (APP_WEATHER_ICON.length() > 0) {
                appObj["iconName"] = APP_WEATHER_ICON;
            }
        }
    }
    
    JsonObject settings = data.createNestedObject("settings");
    settings["appTime"] = TIME_PER_APP;
    // 亮度：返回 0-100 范围（小程序使用）
    settings["brightness"] = (int)((BRIGHTNESS * 100) / 255);
    settings["autoTransition"] = AUTO_TRANSITION;
    settings["showWeekday"] = SHOW_WEEKDAY;
    settings["timeFormat"] = TIME_FORMAT;
    settings["dateFormat"] = DATE_FORMAT;
    
    String output;
    serializeJson(doc, output);
    ws->broadcastTXT(output);
}

void ServerManager_::broadcastStats() {
    StaticJsonDocument<512> doc;
    doc["type"] = "stats";

    JsonObject data = doc.createNestedObject("data");
    data["temp"] = INDOOR_TEMP;
    data["hum"] = INDOOR_HUM;
    // 亮度：返回 0-100 范围（小程序使用）
    data["brightness"] = (int)((BRIGHTNESS * 100) / 255);
    data["currentApp"] = CURRENT_APP;
    // 电源状态：true=开启，false=关闭
    data["isPowered"] = !MATRIX_OFF;
    // 自动轮播状态
    data["autoPlay"] = AUTO_TRANSITION;
    // 连接状态（始终为 true，因为已连接）
    data["isOnline"] = true;

    String output;
    serializeJson(doc, output);
    ws->broadcastTXT(output);
}


// 发送 liveview 数据（由 Liveview 类回调调用）
void ServerManager_::sendLiveviewData(const char* data, size_t length) {
    if (ws->connectedClients() > 0 && EnableLiveview) {
        ws->broadcastBIN((uint8_t*)data, length);
    }
}

void ServerManager_::notifyClients(const String& event, const String& data) {
    StaticJsonDocument<256> doc;
    doc["type"] = event;
    doc["data"]["app"] = data;
    
    String output;
    serializeJson(doc, output);
    ws->broadcastTXT(output);
}

// RGB565转十六进制颜色字符串
String ServerManager_::rgb565ToHex(uint16_t rgb565) {
    // RGB565格式: RRRRR GGGGGG BBBBB
    uint8_t r = ((rgb565 >> 11) & 0x1F) << 3;  // 5位红色，扩展到8位
    uint8_t g = ((rgb565 >> 5) & 0x3F) << 2;   // 6位绿色，扩展到8位
    uint8_t b = (rgb565 & 0x1F) << 3;          // 5位蓝色，扩展到8位
    
    char hex[8];
    sprintf(hex, "#%02X%02X%02X", r, g, b);
    return String(hex);
}


void ServerManager_::setup(WebSocketsServer* websocket) {
    this->ws = websocket;
    
    ws->begin();
    ws->onEvent([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
        this->handleWebSocketEvent(num, type, payload, length);
    });
    
    Serial.println("WebSocket服务器已启动 (端口81)");
}

void ServerManager_::tick() {
    ws->loop();
}