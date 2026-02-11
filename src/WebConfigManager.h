#ifndef WEB_CONFIG_MANAGER_H
#define WEB_CONFIG_MANAGER_H

#include <Arduino.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

// AP 模式配置
#define AP_SSID_PREFIX "NeoClock-" // AP 热点名称前缀 (后跟芯片ID后4位)
#define AP_PASSWORD ""             // AP 密码（空=开放热点）
#define AP_CHANNEL 1               // AP 信道
#define AP_MAX_CONN 4              // AP 最大连接数
#define AP_IP IPAddress(192, 168, 4, 1)
#define AP_GATEWAY IPAddress(192, 168, 4, 1)
#define AP_SUBNET IPAddress(255, 255, 255, 0)

// WiFi 连接配置
#define WIFI_CONNECT_TIMEOUT 15000    // WiFi 连接超时 (ms)
#define WIFI_RECONNECT_INTERVAL 30000 // 断线重连间隔 (ms)
#define DNS_PORT 53                   // DNS 服务端口

// WiFi 凭据最大保存数量
#define MAX_WIFI_CREDENTIALS 3

// WiFi 连接状态枚举
enum WiFiConnState {
  WIFI_STATE_IDLE,          // 空闲
  WIFI_STATE_CONNECTING,    // 正在连接
  WIFI_STATE_CONNECTED,     // 已连接
  WIFI_STATE_DISCONNECTED,  // 断开（等待重连）
  WIFI_STATE_AP_MODE,       // AP 配网模式
  WIFI_STATE_CONNECT_FAILED // 连接失败
};

// WiFi 扫描结果
struct WiFiScanResult {
  String ssid;
  int32_t rssi;
  uint8_t encType; // WIFI_AUTH_OPEN, WIFI_AUTH_WPA2_PSK, etc.
  String bssid;
  int32_t channel;
};

class WebConfigManager_ {
private:
  WebServer *httpServer;
  DNSServer *dnsServer;
  Preferences prefs;

  WiFiConnState connState;
  String apSSID; // 动态生成的 AP 名称

  // 保存的 WiFi 凭据
  String savedSSID;
  String savedPassword;

  // 连接状态追踪
  unsigned long connectStartTime;
  unsigned long lastReconnectAttempt;
  bool portalActive;         // 配网门户是否激活
  String connectingSSID;     // 正在尝试连接的 SSID
  String connectingPassword; // 正在尝试连接的密码
  String lastConnectMessage; // 最近一次连接结果消息

  // 内部方法
  void generateAPName();
  bool loadCredentials();
  void saveCredentials(const String &ssid, const String &password);
  void clearCredentials();

  bool tryConnect(const String &ssid, const String &password,
                  unsigned long timeout = WIFI_CONNECT_TIMEOUT);

  void startAPMode();
  void stopAPMode();
  void startHTTPServer();
  void stopHTTPServer();
  void startDNS();
  void stopDNS();

  // HTTP 路由处理
  void handleRoot();
  void handleScan();
  void handleConnect();
  void handleStatus();
  void handleRestart();
  void handleNotFound();

  // 生成配网 HTML 页面
  const char *getConfigPageHtml();

public:
  static WebConfigManager_ &getInstance();

  /**
   * 初始化配网管理器
   * 检查已保存凭据 → 尝试连接 → 失败则启动 AP 配网
   */
  void setup();

  /**
   * 主循环处理
   * 处理 HTTP 请求、DNS、断线重连等
   */
  void tick();

  /**
   * 获取当前 WiFi 连接状态
   */
  WiFiConnState getState() const { return connState; }

  /**
   * 是否处于 AP 配网模式
   */
  bool isAPMode() const { return connState == WIFI_STATE_AP_MODE; }

  /**
   * 是否已连接 WiFi
   */
  bool isConnected() const { return connState == WIFI_STATE_CONNECTED; }

  /**
   * 获取当前 IP 地址
   */
  String getIP() const;

  /**
   * 获取 AP 热点名称
   */
  String getAPName() const { return apSSID; }

  /**
   * 手动触发进入AP配网模式
   */
  void forceAPMode();
};

extern WebConfigManager_ &WebConfigManager;

#endif
