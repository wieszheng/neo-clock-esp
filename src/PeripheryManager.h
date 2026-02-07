#ifndef PERIPHERY_MANAGER_H
#define PERIPHERY_MANAGER_H

#include <Arduino.h>
#include <DHT.h>

class PeripheryManager_
{
public:
    static PeripheryManager_ &getInstance();
    void setup();
    void tick();
    
    bool isSensorAvailable();

private:
    // DHT22传感器配置
    void readDHT22();

    // DHT22实例
    DHT *dht;

    // 传感器数据缓存
    float temperature;
    float humidity;
    bool sensorAvailable;
    unsigned long lastUpdate;


    // 传感器配置常量
    static const int DHT_PIN = 4;        // DHT22数据引脚(GPIO4)
    static const int DHT_TYPE = DHT22;    // DHT22传感器类型
    static const unsigned long READ_INTERVAL = 2000; // 读取间隔(毫秒),DHT22需要至少2秒
};

extern PeripheryManager_ &PeripheryManager;

#endif
