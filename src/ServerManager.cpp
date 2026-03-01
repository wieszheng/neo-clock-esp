/**
 * @file ServerManager.cpp
 * @brief WebSocket 服务器实现 — 远程控制与数据广播
 *
 * 本文件实现：
 *   - WebSocket 事件处理 (连接、断开、消息)
 *   - JSON 命令解析与执行
 *   - 配置和状态广播
 *   - 图标上传处理
 *   - Liveview 实时预览数据推送
 */

#include "ServerManager.h"
#include "Apps.h"
#include "DisplayManager.h"
#include "Globals.h"
#include "PeripheryManager.h"
#include <ArduinoJson.h>

ServerManager_ &ServerManager_::getInstance()
{
  static ServerManager_ instance;
  return instance;
}

ServerManager_ &ServerManager = ServerManager.getInstance();

// 注意：HTTP 服务器已移除，配网功能由 WebConfig 提供
// WebSocket 服务器保留在 81 端口用于控制面板

void ServerManager_::handleWebSocketEvent(uint8_t num, WStype_t type,
                                          uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:
    Serial.printf("[WS] Client #%u disconnected\n", num);
    break;

  case WStype_CONNECTED:
  {
    IPAddress ip = ws->remoteIP(num);
    Serial.printf("[WS] Client #%u connected from %s\n", num,
                  ip.toString().c_str());
    // 连接时立即发送当前状态
    broadcastStats();
    break;
  }
    // ==================== icon upload (.anim) ====================
    // 协议:
    // 1) TEXT: {type:'uploadIcon', requestId, phase:'start', filename,
    // totalBytes, chunkSize} 2) BIN : raw bytes, continuous 3) TEXT:
    // {type:'uploadIcon', requestId, phase:'finish'} 返回: ack/error (带
    // requestId)
    static bool uploading = false;
    static uint8_t uploadClient = 0;
    static String uploadRequestId = "";
    static String uploadPath = "";
    static size_t uploadTotal = 0;
    static size_t uploadReceived = 0;
    static File uploadFile;

  case WStype_BIN:
  {
    if (uploading && num == uploadClient)
    {
      if (uploadFile)
      {
        size_t written = uploadFile.write(payload, length);
        uploadReceived += written;
        // Serial.printf("[WS] Received BIN chunk: %u bytes (total: %u/%u)\n",
        // (unsigned)length, (unsigned)uploadReceived, (unsigned)uploadTotal);
      }
    }
    break;
  }
  case WStype_TEXT:
  {
    Serial.printf("[WS] Received: %s\n", payload);

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);

    if (error)
    {
      Serial.println("[WS] Parse error");
      return;
    }

    String type = doc["type"].as<String>();
    String requestId = doc.containsKey("requestId")
                           ? doc["requestId"].as<String>()
                           : String("");

    if (type == "uploadIcon")
    {
      String phase =
          doc.containsKey("phase") ? doc["phase"].as<String>() : String("");

      if (phase == "start")
      {
        String filename = doc.containsKey("filename")
                              ? doc["filename"].as<String>()
                              : String("");
        uploadTotal = doc.containsKey("totalBytes")
                          ? (size_t)doc["totalBytes"].as<uint32_t>()
                          : 0;

        if (filename.length() == 0 || !filename.endsWith(".anim"))
        {
          sendAck(num, requestId, "uploadIcon", false, "invalid filename");
          return;
        }
        if (uploading)
        {
          sendAck(num, requestId, "uploadIcon", false, "busy");
          return;
        }

        uploadPath = "/icons/" + filename;

        // 确保目录存在
        if (!LittleFS.exists("/icons"))
        {
          LittleFS.mkdir("/icons");
        }

        uploadFile = LittleFS.open(uploadPath, "w");
        if (!uploadFile)
        {
          sendAck(num, requestId, "uploadIcon", false, "open file failed");
          return;
        }

        uploading = true;
        uploadClient = num;
        uploadRequestId = requestId;
        uploadReceived = 0;

        Serial.printf("[WS] uploadIcon start: %s total=%u\n",
                      uploadPath.c_str(), (unsigned)uploadTotal);
        // start 阶段不立即 ack，等待 finish 后统一 ack
        return;
      }

      if (phase == "finish")
      {
        if (!uploading || uploadClient != num || uploadRequestId != requestId)
        {
          sendAck(num, requestId, "uploadIcon", false, "no active upload");
          return;
        }

        if (uploadFile)
        {
          uploadFile.flush();
          uploadFile.close();
        }

        bool ok = (uploadTotal == 0) ? (uploadReceived > 0)
                                     : (uploadReceived == uploadTotal);

        Serial.printf("[WS] uploadIcon finish: received=%u ok=%d\n",
                      (unsigned)uploadReceived, ok ? 1 : 0);

        uploading = false;
        uploadClient = 0;
        uploadRequestId = "";
        uploadPath = "";
        uploadTotal = 0;
        uploadReceived = 0;

        if (ok)
        {
          sendAck(num, requestId, "uploadIcon", true);
          // 可选：推送更新后的图标列表给该客户端
          sendIconList(num);
        }
        else
        {
          // 删除不完整文件
          if (LittleFS.exists(uploadPath))
          {
            LittleFS.remove(uploadPath);
          }
          sendAck(num, requestId, "uploadIcon", false, "size mismatch");
        }
        return;
      }

      sendAck(num, requestId, "uploadIcon", false, "invalid phase");
      return;
    }
    if (type == "getConfig")
    {
      broadcastConfig();
    }
    else if (type == "getStats")
    {
      broadcastStats();
    }
    else if (type == "getIconList")
    {
      sendIconList(num);
    }
    else if (type == "getLiveview")
    {
      // 启用实时预览
      Serial.println("[WS] Enabling liveview");
      EnableLiveview = true;
    }
    else if (type == "stopLiveview")
    {
      // 停止实时预览
      EnableLiveview = false;
    }
    else if (type == "appsUpdate")
    {
      // 处理应用更新
      if (doc.containsKey("apps"))
      {
        for (auto app : doc["apps"].as<JsonArray>())
        {
          String name = app["name"].as<String>();
          bool show = app["show"].as<bool>();
          int position = app.containsKey("pos") ? app["pos"].as<int>() : -1;
          uint16_t duration =
              app.containsKey("duration") ? app["duration"].as<int>() : 0;

          if (name == "time")
          {
            SHOW_TIME = show;
            TIME_DURATION = duration;
            TIME_POSITION = position;
            if (app.containsKey("color"))
              TIME_COLOR = app["color"].as<String>();
            if (app.containsKey("weekdayActive"))
              TIME_WEEKDAY_ACTIVE_COLOR = app["weekdayActive"].as<String>();
            if (app.containsKey("weekdayInactive"))
              TIME_WEEKDAY_INACTIVE_COLOR = app["weekdayInactive"].as<String>();
            if (app.containsKey("iconName"))
              TIME_ICON = app["iconName"].as<String>();
          }
          else if (name == "date")
          {
            SHOW_DATE = show;
            DATE_DURATION = duration;
            DATE_POSITION = position;
            if (app.containsKey("color"))
              DATE_COLOR = app["color"].as<String>();
            if (app.containsKey("weekdayActive"))
              DATE_WEEKDAY_ACTIVE_COLOR = app["weekdayActive"].as<String>();
            if (app.containsKey("weekdayInactive"))
              DATE_WEEKDAY_INACTIVE_COLOR = app["weekdayInactive"].as<String>();
            if (app.containsKey("iconName"))
              DATE_ICON = app["iconName"].as<String>();
          }
          else if (name == "temp")
          {
            SHOW_TEMP = show;
            TEMP_DURATION = duration;
            TEMP_POSITION = position;
            if (app.containsKey("color"))
              TEMP_COLOR = app["color"].as<String>();
          }
          else if (name == "hum")
          {
            SHOW_HUM = show;
            HUM_DURATION = duration;
            HUM_POSITION = position;
            if (app.containsKey("color"))
              HUM_COLOR = app["color"].as<String>();
          }
          else if (name == "wind")
          {
            SHOW_WIND = show;
            WIND_DURATION = duration;
            WIND_POSITION = position;
            if (app.containsKey("color"))
              WIND_COLOR = app["color"].as<String>();
            if (app.containsKey("iconName"))
              WIND_ICON = app["iconName"].as<String>();
          }
          else if (name == "weather")
          {
            SHOW_WEATHER = show;
            WEATHER_DURATION = duration;
            WEATHER_POSITION = position;
            if (app.containsKey("iconName"))
              APP_WEATHER_ICON = app["iconName"].as<String>();
          }
          // 更新Apps中的position和duration
          if (position >= 0)
          {
            for (auto &a : Apps)
            {
              if (a.name == name)
              {
                a.position = position;
                a.duration = duration;
                break;
              }
            }
          }
        }
        DisplayManager.loadNativeApps();
      }

      // appsUpdate 只处理 apps，settings 由 settingsUpdate 单独处理
      // （避免更新应用时覆盖亮度/自动轮播等设置）

      saveSettings();
      Serial.println("[WS] 设置已保存");

      // ACK + 广播权威配置
      sendAck(num, requestId, "appsUpdate", true);
      broadcastConfig();
      broadcastStats();
    }
    else if (type == "setBrightness")
    {
      int value = doc["value"].as<int>();
      // 支持 0-100 范围（小程序使用）和 0-255 范围（ESP32 内部）
      if (value <= 100)
      {
        // 小程序发送的是 0-100，转换为 0-255
        BRIGHTNESS = (uint8_t)((value * 255) / 100);
      }
      else
      {
        // 直接使用 0-255 值
        BRIGHTNESS = (uint8_t)value;
      }
      // 手动设置亮度时自动关闭 LDR 自动亮度，避免手动值立刻被覆盖
      if (AUTO_BRIGHTNESS)
      {
        AUTO_BRIGHTNESS = false;
        PeripheryManager.setAutoBrightness(false);
      }
      DisplayManager.setBrightness(BRIGHTNESS);
      saveSettings();
      sendAck(num, requestId, "setBrightness", true);
      broadcastStats();
    }
    else if (type == "setPower")
    {
      // 设置电源状态：true=开启，false=关闭
      bool powered = doc["powered"].as<bool>();
      MATRIX_OFF = !powered;
      DisplayManager.setBrightness(BRIGHTNESS);
      saveSettings();
      sendAck(num, requestId, "setPower", true);
      // 立即广播状态更新
      broadcastStats();
    }
    else if (type == "setAutoBrightness")
    {
      // LDR 自动亮度开关
      // 协议: {type:"setAutoBrightness", enabled: true/false}
      bool enabled = doc["enabled"].as<bool>();
      AUTO_BRIGHTNESS = enabled;
      PeripheryManager.setAutoBrightness(enabled);
      saveSettings();
      sendAck(num, requestId, "setAutoBrightness", true);
      broadcastStats();
    }
    else if (type == "setAutoPlay")
    {
      // 设置自动轮播：true=开启，false=关闭
      bool autoPlay = doc["autoPlay"].as<bool>();
      AUTO_TRANSITION = autoPlay;
      DisplayManager.applyAllSettings();
      saveSettings();
      sendAck(num, requestId, "setAutoPlay", true);
      // 立即广播状态更新
      broadcastStats();
    }
    else if (type == "setWeatherConfig")
    {
      String city =
          doc.containsKey("city") ? doc["city"].as<String>() : String("");
      String apiKey =
          doc.containsKey("apiKey") ? doc["apiKey"].as<String>() : String("");

      city.trim();
      apiKey.trim();

      if (city.length() == 0)
      {
        sendAck(num, requestId, "setWeatherConfig", false, "missing city");
        return;
      }
      if (apiKey.length() == 0)
      {
        sendAck(num, requestId, "setWeatherConfig", false, "missing apiKey");
        return;
      }

      WEATHER_CITY = city;
      WEATHER_API_KEY = apiKey;
      saveSettings();

      sendAck(num, requestId, "setWeatherConfig", true);
      broadcastConfig();
    }
    else if (type == "setDisplayConfig")
    {
      // 仅做持久化与回填；颜色校准暂不处理具体效果
      String layout =
          doc.containsKey("layout") ? doc["layout"].as<String>() : String("");
      String colorCalibration = doc.containsKey("colorCalibration")
                                    ? doc["colorCalibration"].as<String>()
                                    : String("");

      // 这些字段当前固件未使用，但为了新协议对接，先保存到 settings（需要
      // saveSettings/loadSettings 支持） 这里先做 ACK +
      // broadcastConfig；具体持久化字段将在 settings 存储实现后生效
      // 为避免破坏现有存储结构，暂不强行写入全局变量。

      (void)layout;
      (void)colorCalibration;

      saveSettings();
      sendAck(num, requestId, "setDisplayConfig", true);
      broadcastConfig();
    }
    else if (type == "settingsUpdate")
    {
      // 旧协议保留向后兼容，但不再作为新路径使用
      if (doc.containsKey("settings"))
      {
        auto s = doc["settings"];
        if (s.containsKey("appTime"))
          TIME_PER_APP = s["appTime"].as<int>();
        if (s.containsKey("brightness"))
        {
          int value = s["brightness"].as<int>();
          BRIGHTNESS =
              value <= 100 ? (uint8_t)((value * 255) / 100) : (uint8_t)value;
        }
        if (s.containsKey("autoTransition"))
          AUTO_TRANSITION = s["autoTransition"].as<bool>();
        if (s.containsKey("showWeekday"))
          SHOW_WEEKDAY = s["showWeekday"].as<bool>();
        if (s.containsKey("timeFormat"))
          TIME_FORMAT = s["timeFormat"].as<String>();
        if (s.containsKey("dateFormat"))
          DATE_FORMAT = s["dateFormat"].as<String>();
        DisplayManager.applyAllSettings();
        saveSettings();
        sendAck(num, requestId, "settingsUpdate", true);
        broadcastConfig();
        broadcastStats();
      }
      else
      {
        sendAck(num, requestId, "settingsUpdate", false, "missing settings");
      }
    }
    else if (type == "appNext")
    {
      DisplayManager.nextApp();
      sendAck(num, requestId, "appNext", true);
      broadcastStats();
    }
    else if (type == "appPrev")
    {
      DisplayManager.previousApp();
      sendAck(num, requestId, "appPrev", true);
      broadcastStats();
    }
    else if (type == "cmd")
    {
      String action = doc["action"].as<String>();
      if (action == "next")
      {
        DisplayManager.nextApp();
        sendAck(num, requestId, "cmd.next", true);
        // 切换应用后立即广播状态
        broadcastStats();
      }
      else if (action == "prev")
      {
        DisplayManager.previousApp();
        sendAck(num, requestId, "cmd.prev", true);
        // 切换应用后立即广播状态
        broadcastStats();
      }
      else if (action == "toggle")
      {
        // 保留 toggle 命令以保持向后兼容
        MATRIX_OFF = !MATRIX_OFF;
        DisplayManager.setBrightness(BRIGHTNESS);
        saveSettings();
        broadcastStats();
      }
      else if (action == "restart")
      {
        ESP.restart();
      }
      else if (action == "leftClick")
      {
        DisplayManager.leftButton();
      }
      else if (action == "rightClick")
      {
        DisplayManager.rightButton();
      }
    }
    break;
  }

  default:
    break;
  }
}

/**
 * @brief 发送 ACK/错误响应
 * @param num 客户端 ID
 * @param requestId 请求 ID
 * @param action 动作名称
 * @param ok 是否成功
 * @param message 错误消息 (失败时)
 */
void ServerManager_::sendAck(uint8_t num, const String &requestId,
                             const String &action, bool ok,
                             const String &message)
{
  StaticJsonDocument<256> doc;
  doc["type"] = ok ? "ack" : "error";
  if (requestId.length() > 0)
    doc["requestId"] = requestId;
  doc["action"] = action;
  doc["ok"] = ok;
  if (!ok && message.length() > 0)
    doc["message"] = message;
  String output;
  serializeJson(doc, output);
  ws->sendTXT(num, output);
}

/**
 * @brief 广播当前配置到所有客户端
 *
 * 发送所有应用的配置（启用状态、位置、颜色、图标等）
 */
void ServerManager_::broadcastConfig()
{
  StaticJsonDocument<2048> doc;
  doc["type"] = "config";

  JsonObject data = doc.createNestedObject("data");
  JsonArray appsArr = data.createNestedArray("apps");

  for (auto &app : Apps)
  {
    JsonObject appObj = appsArr.createNestedObject();
    appObj["id"] = app.name;
    appObj["name"] = app.name;
    // 前端显示用名称（避免小程序维护映射表）
    if (app.name == "time")
      appObj["displayName"] = "时间";
    else if (app.name == "date")
      appObj["displayName"] = "日期";
    else if (app.name == "temp")
      appObj["displayName"] = "温度";
    else if (app.name == "hum")
      appObj["displayName"] = "湿度";
    else if (app.name == "weather")
      appObj["displayName"] = "天气";
    else if (app.name == "wind")
      appObj["displayName"] = "风速";
    else
      appObj["displayName"] = app.name;
    appObj["enabled"] = app.enabled;
    appObj["position"] = app.position;
    appObj["duration"] = app.duration;

    if (app.name == "time")
    {
      appObj["color"] = TIME_COLOR;
      appObj["weekdayActive"] = TIME_WEEKDAY_ACTIVE_COLOR;
      appObj["weekdayInactive"] = TIME_WEEKDAY_INACTIVE_COLOR;
      appObj["timeFormat"] = TIME_FORMAT; // 添加时间格式
      if (TIME_ICON.length() > 0)
      {
        appObj["iconName"] = TIME_ICON;
      }
    }
    else if (app.name == "date")
    {
      appObj["color"] = DATE_COLOR;
      appObj["weekdayActive"] = DATE_WEEKDAY_ACTIVE_COLOR;
      appObj["weekdayInactive"] = DATE_WEEKDAY_INACTIVE_COLOR;
      appObj["dateFormat"] = DATE_FORMAT; // 添加日期格式
      if (DATE_ICON.length() > 0)
      {
        appObj["iconName"] = DATE_ICON;
      }
    }
    else if (app.name == "temp")
    {
      appObj["color"] = TEMP_COLOR;
    }
    else if (app.name == "hum")
    {
      appObj["color"] = HUM_COLOR;
    }
    else if (app.name == "wind")
    {
      appObj["color"] = WIND_COLOR;
      if (WIND_ICON.length() > 0)
      {
        appObj["iconName"] = WIND_ICON;
      }
    }
    else if (app.name == "weather")
    {
      if (APP_WEATHER_ICON.length() > 0)
      {
        appObj["iconName"] = APP_WEATHER_ICON;
      }
    }
  }

  JsonObject settings = data.createNestedObject("settings");
  settings["appTime"] = TIME_PER_APP;
  // 亮度：返回 0-100 范围（小程序使用）
  settings["brightness"] = (int)((BRIGHTNESS * 100) / 255);
  settings["autoBrightness"] = AUTO_BRIGHTNESS;
  settings["autoTransition"] = AUTO_TRANSITION;
  settings["showWeekday"] = SHOW_WEEKDAY;
  settings["timeFormat"] = TIME_FORMAT;
  settings["dateFormat"] = DATE_FORMAT;

  // weather config
  settings["weatherCity"] = WEATHER_CITY;
  settings["weatherApiKey"] = WEATHER_API_KEY;

  String output;
  serializeJson(doc, output);
  ws->broadcastTXT(output);
}

/**
 * @brief 广播当前状态到所有客户端
 *
 * 发送温湿度、亮度、自动亮度、当前应用、电源状态、自动播放等
 */
void ServerManager_::broadcastStats()
{
  StaticJsonDocument<512> doc;
  doc["type"] = "stats";

  JsonObject data = doc.createNestedObject("data");
  data["temp"] = INDOOR_TEMP;
  data["hum"] = INDOOR_HUM;
  // 亮度：返回 0-100 范围（小程序使用）
  data["brightness"] = (int)((BRIGHTNESS * 100) / 255);
  data["autoBrightness"] = AUTO_BRIGHTNESS;
  data["ldrBrightness"] =
      (int)((PeripheryManager.getLdrBrightness() * 100) / 255);
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

/**
 * @brief 发送 Liveview 实时预览数据
 * @param data 像素数据
 * @param length 数据长度
 *
 * 由 Liveview 类回调调用，通过 WebSocket 广播二进制数据
 */
void ServerManager_::sendLiveviewData(const char *data, size_t length)
{
  if (ws->connectedClients() > 0 && EnableLiveview)
  {
    ws->broadcastBIN((uint8_t *)data, length);
  }
}

/**
 * @brief 通知所有客户端事件
 * @param event 事件类型
 * @param data 事件数据
 */
void ServerManager_::notifyClients(const String &event, const String &data)
{
  StaticJsonDocument<256> doc;
  doc["type"] = event;
  doc["data"]["app"] = data;

  String output;
  serializeJson(doc, output);
  ws->broadcastTXT(output);
}

/**
 * @brief RGB565 转换为十六进制颜色字符串
 * @param rgb565 RGB565 颜色值
 * @return HEX 格式颜色字符串 (如 "#FFFFFF")
 */
String ServerManager_::rgb565ToHex(uint16_t rgb565)
{
  // RGB565格式: RRRRR GGGGGG BBBBB
  uint8_t r = ((rgb565 >> 11) & 0x1F) << 3; // 5位红色，扩展到8位
  uint8_t g = ((rgb565 >> 5) & 0x3F) << 2;  // 6位绿色，扩展到8位
  uint8_t b = (rgb565 & 0x1F) << 3;         // 5位蓝色，扩展到8位

  char hex[8];
  sprintf(hex, "#%02X%02X%02X", r, g, b);
  return String(hex);
}

/**
 * @brief 发送用户图标列表
 * @param num 客户端 ID
 *
 * 从 LittleFS /icons 目录读取所有 .anim 文件并发送
 */
void ServerManager_::sendIconList(uint8_t num)
{
  Serial.println("[WS] Scanning LittleFS /icons directory...");

  DynamicJsonDocument doc(4096); // 增加容量以处理更多文件名
  doc["type"] = "iconList";
  JsonArray icons = doc.createNestedArray("data");

  File root = LittleFS.open("/icons");
  if (!root || !root.isDirectory())
  {
    Serial.println("[WS] Error: /icons directory not found");
    // 发送空列表
    String output;
    serializeJson(doc, output);
    ws->sendTXT(num, output);
    return;
  }

  File file = root.openNextFile();
  int count = 0;
  while (file)
  {
    String fileName = file.name();
    // 兼容处理：有些版本的 LittleFS file.name() 可能返回全路径，我们只取文件名
    if (fileName.lastIndexOf('/') != -1)
    {
      fileName = fileName.substring(fileName.lastIndexOf('/') + 1);
    }

    if (!file.isDirectory() && fileName.endsWith(".anim") && file.size() > 0)
    {
      JsonObject obj = icons.createNestedObject();
      obj["type"] = "FS";
      obj["val"] = fileName;
      obj["name"] = fileName;
      count++;
    }
    file = root.openNextFile();
  }

  Serial.printf("[WS] Found %d icons in LittleFS\n", count);

  String output;
  serializeJson(doc, output);
  ws->sendTXT(num, output);
}

/**
 * @brief 初始化 WebSocket 服务器
 * @param websocket WebSocket 服务器实例指针
 */
void ServerManager_::setup(WebSocketsServer *websocket)
{
  this->ws = websocket;

  ws->begin();
  ws->onEvent(
      [this](uint8_t num, WStype_t type, uint8_t *payload, size_t length)
      {
        this->handleWebSocketEvent(num, type, payload, length);
      });

  Serial.println("WebSocket服务器已启动 (端口81)");
}

/**
 * @brief 主循环 - 处理 WebSocket 事件
 */
void ServerManager_::tick() { ws->loop(); }