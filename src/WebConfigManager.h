/**
 * @file WebConfigManager.h
 * @brief WiFi 配网管理器 — AP 模式、DNS、HTTP 服务器
 *
 * 本文件定义：
 *   - WebConfigManager_ 单例类: 管理 WiFi 配网
 *   - WiFi 连接状态枚举
 *   - AP 模式配置常量
 *   - Captive Portal 功能
 */

#ifndef WEB_CONFIG_MANAGER_H
#define WEB_CONFIG_MANAGER_H

#include <Arduino.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

// ==================================================================
// AP 模式配置
// ==================================================================

/// AP 热点名称前缀 (后跟芯片ID后4位)
#define AP_SSID_PREFIX "NeoClock-"

/// AP 密码 (空=开放热点)
#define AP_PASSWORD ""

/// AP 信道
#define AP_CHANNEL 1

/// AP 最大连接数
#define AP_MAX_CONN 4

/// AP IP 地址
#define AP_IP IPAddress(192, 168, 4, 1)

/// AP 网关地址
#define AP_GATEWAY IPAddress(192, 168, 4, 1)

/// AP 子网掩码
#define AP_SUBNET IPAddress(255, 255, 255, 0)

// ==================================================================
// WiFi 连接配置
// ==================================================================

/// WiFi 连接超时 (毫秒)
#define WIFI_CONNECT_TIMEOUT 15000

/// WiFi 断线重连间隔 (毫秒)
#define WIFI_RECONNECT_INTERVAL 30000

/// DNS 服务端口
#define DNS_PORT 53

// ==================================================================
// WiFi 凭据配置
// ==================================================================

/// WiFi 凭据最大保存数量
#define MAX_WIFI_CREDENTIALS 3

// ==================================================================
// WiFi 连接状态枚举
// ==================================================================

/// WiFi 连接状态枚举
enum WiFiConnState {
  WIFI_STATE_IDLE,          ///< 空闲状态
  WIFI_STATE_CONNECTING,    ///< 正在连接 WiFi
  WIFI_STATE_CONNECTED,     ///< 已连接 WiFi
  WIFI_STATE_DISCONNECTED,  ///< 已断开 (等待重连)
  WIFI_STATE_AP_MODE,       ///< AP 配网模式
  WIFI_STATE_CONNECT_FAILED ///< 连接失败
};

// ==================================================================
// WiFi 扫描结果结构体
// ==================================================================

/// WiFi 扫描结果
struct WiFiScanResult {
  String ssid;          ///< WiFi 名称
  int32_t rssi;        ///< 信号强度
  uint8_t encType;     ///< 加密类型 (WIFI_AUTH_OPEN, WIFI_AUTH_WPA2_PSK 等)
  String bssid;        ///< MAC 地址
  int32_t channel;     ///< 信道
};

// ==================================================================
// Web 配网管理器类
// ==================================================================

/**
 * @class WebConfigManager_
 * @brief WiFi 配网管理器
 *
 * 功能：
 *   - 自动处理 WiFi 连接和 AP 模式切换
 *   - 提供 Web 界面进行 WiFi 配置
 *   - Captive Portal 功能 (DNS 劫持)
 *   - 自动重连机制
 *   - WiFi 凭据持久化存储
 */
class WebConfigManager_ {
private:
  /// HTTP 服务器实例
  WebServer *httpServer;

  /// DNS 服务器实例
  DNSServer *dnsServer;

  /// Preferences 实例 (存储 WiFi 凭据)
  Preferences prefs;

  // ==================================================================
  // 连接状态管理
  // ==================================================================

  /// 当前 WiFi 连接状态
  WiFiConnState connState;

  /// 动态生成的 AP 名称
  String apSSID;

  /// 保存的 WiFi 凭据
  String savedSSID;
  String savedPassword;

  // 连接状态追踪
  unsigned long connectStartTime;     ///< 连接开始时间
  unsigned long lastReconnectAttempt; ///< 上次重连尝试时间
  bool portalActive;                 ///< 配网门户是否激活
  String connectingSSID;              ///< 正在尝试连接的 SSID
  String connectingPassword;          ///< 正在尝试连接的密码
  String lastConnectMessage;          ///< 最近一次连接结果消息

  // ==================================================================
  // 内部方法
  // ==================================================================

  /// 生成 AP 热点名称 (芯片ID后4位)
  void generateAPName();

  /// 加载已保存的 WiFi 凭据
  bool loadCredentials();

  /// 保存 WiFi 凭据到 Flash
  void saveCredentials(const String &ssid, const String &password);

  /// 清除已保存的 WiFi 凭据
  void clearCredentials();

  /**
   * @brief 尝试连接 WiFi
   * @param ssid WiFi 名称
   * @param password WiFi 密码
   * @param timeout 连接超时 (毫秒)
   * @return true=连接成功, false=连接失败
   */
  bool tryConnect(const String &ssid, const String &password,
                  unsigned long timeout = WIFI_CONNECT_TIMEOUT);

  /// 启动 AP 配网模式
  void startAPMode();

  /// 停止 AP 配网模式
  void stopAPMode();

  /// 启动 HTTP 服务器
  void startHTTPServer();

  /// 停止 HTTP 服务器
  void stopHTTPServer();

  /// 启动 DNS 服务器 (Captive Portal)
  void startDNS();

  /// 停止 DNS 服务器
  void stopDNS();

  // ==================================================================
  // HTTP 路由处理
  // ==================================================================

  /// 处理根路径请求 (返回配网页面)
  void handleRoot();

  /// 处理 WiFi 扫描请求
  void handleScan();

  /// 处理 WiFi 连接请求
  void handleConnect();

  /// 处理 WiFi 状态查询
  void handleStatus();

  /// 处理设备重启请求
  void handleRestart();

  /// 处理未知路径 (重定向到配网页面)
  void handleNotFound();

  /// 生成配网 HTML 页面
  const char *getConfigPageHtml();

public:
  /**
   * @brief 获取单例实例
   * @return WebConfigManager_ 单例引用
   */
  static WebConfigManager_ &getInstance();

  /**
   * @brief 初始化配网管理器
   *
   * 检查已保存凭据 → 尝试连接 → 失败则启动 AP 配网
   */
  void setup();

  /**
   * @brief 主循环
   *
   * 处理 HTTP 请求、DNS、断线重连等
   */
  void tick();

  /**
   * @brief 获取当前 WiFi 连接状态
   * @return 当前 WiFiConnState
   */
  WiFiConnState getState() const { return connState; }

  /**
   * @brief 检查是否处于 AP 配网模式
   * @return true=AP 模式, false=STA 模式
   */
  bool isAPMode() const { return connState == WIFI_STATE_AP_MODE; }

  /**
   * @brief 检查是否已连接 WiFi
   * @return true=已连接, false=未连接
   */
  bool isConnected() const { return connState == WIFI_STATE_CONNECTED; }

  /**
   * @brief 获取当前 IP 地址
   * @return IP 地址字符串
   */
  String getIP() const;

  /**
   * @brief 获取 AP 热点名称
   * @return AP SSID
   */
  String getAPName() const { return apSSID; }

  /**
   * @brief 手动触发进入 AP 配网模式
   *
   * 强制断开当前连接并启动 AP 模式
   */
  void forceAPMode();
};

extern WebConfigManager_ &WebConfigManager;

#endif
