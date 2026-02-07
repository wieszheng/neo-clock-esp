#ifndef SERVER_MANAGER_H
#define SERVER_MANAGER_H

#include <WebSocketsServer.h>

class ServerManager_ {
private:
    WebSocketsServer* ws;

    void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
    void broadcastConfig();
    void broadcastStats();
    void sendAck(uint8_t num, const String& requestId, const String& action, bool ok, const String& message = "");
    String rgb565ToHex(uint16_t rgb565);

    
public:
    static ServerManager_& getInstance();
    void setup(WebSocketsServer* websocket);
    void tick();
    void notifyClients(const String& event, const String& data);
    void sendLiveviewData(const char* data, size_t length);
};


extern ServerManager_& ServerManager;

#endif