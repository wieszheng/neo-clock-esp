/**
 * @file ServerManager.h
 * @brief WebSocket 服务器管理器 — 远程控制与实时通信
 *
 * 本文件定义：
 *   - ServerManager_ 单例类: 管理 WebSocket 通信
 *   - 支持 JSON 协议的远程控制命令
 *   - 实时预览数据推送
 *   - 图标上传功能
 */

#ifndef SERVER_MANAGER_H
#define SERVER_MANAGER_H

#include <WebSocketsServer.h>

// ==================================================================
// WebSocket 服务器管理器类
// ==================================================================

/**
 * @class ServerManager_
 * @brief WebSocket 服务器管理器
 *
 * 功能：
 *   - 管理 WebSocket 连接 (端口 81)
 *   - 处理客户端发送的 JSON 控制命令
 *   - 广播配置和状态更新
 *   - 推送 Liveview 实时预览数据
 *   - 处理用户图标上传
 */
class ServerManager_
{
private:
    /// WebSocket 服务器实例
    WebSocketsServer *ws;

    // ==================================================================
    // 私有方法
    // ==================================================================

    /// 处理 WebSocket 事件 (连接/断开/消息)
    void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);

    /// 广播当前配置到所有客户端
    void broadcastConfig();

    /// 广播当前状态 (温湿度、亮度等)
    void broadcastStats();

    /**
     * @brief 发送 ACK/错误响应
     * @param num 客户端 ID
     * @param requestId 请求 ID
     * @param action 动作名称
     * @param ok 是否成功
     * @param message 错误消息 (失败时)
     */
    void sendAck(uint8_t num, const String &requestId, const String &action, bool ok, const String &message = "");

    /**
     * @brief RGB565 转换为十六进制颜色字符串
     * @param rgb565 RGB565 颜色值
     * @return HEX 格式颜色字符串 (如 "#FFFFFF")
     */
    String rgb565ToHex(uint16_t rgb565);

    /**
     * @brief 发送用户图标列表
     * @param num 客户端 ID
     */
    void sendIconList(uint8_t num);

public:
    /**
     * @brief 获取单例实例
     * @return ServerManager_ 单例引用
     */
    static ServerManager_ &getInstance();

    /**
     * @brief 初始化 WebSocket 服务器
     * @param websocket WebSocket 服务器实例指针
     */
    void setup(WebSocketsServer *websocket);

    /**
     * @brief 主循环 - 处理 WebSocket 事件
     */
    void tick();

    /**
     * @brief 通知所有客户端事件
     * @param event 事件类型
     * @param data 事件数据
     */
    void notifyClients(const String &event, const String &data);

    /**
     * @brief 发送 Liveview 实时预览数据
     * @param data 像素数据
     * @param length 数据长度
     */
    void sendLiveviewData(const char *data, size_t length);
};

extern ServerManager_ &ServerManager;

#endif