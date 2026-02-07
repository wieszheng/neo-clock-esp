#include "PeripheryManager.h"
#include "Globals.h"
#include <math.h>

PeripheryManager_ &PeripheryManager_::getInstance()
{
    static PeripheryManager_ instance;
    return instance;
}

PeripheryManager_ &PeripheryManager = PeripheryManager_::getInstance();

void PeripheryManager_::setup()
{
    Serial.println("[Periphery] 初始化外设管理器...");

    // 初始化DHT22传感器
    dht = new DHT(DHT_PIN, DHT_TYPE);
    dht->begin();

    // 初始化数据缓存
    temperature = 0.0;
    humidity = 0.0;
    sensorAvailable = false;
    lastUpdate = 0;

    // 测试读取一次以验证传感器是否正常工作
    Serial.println("[Periphery] 测试读取DHT22传感器...");
    readDHT22();

    if (sensorAvailable) {
        Serial.printf("[Periphery] DHT22传感器初始化成功\n");
        Serial.printf("[Periphery] 初始室内温度: %.1f°C, 室内湿度: %.1f%%\n", temperature, humidity);

        // 更新室内温湿度变量
        INDOOR_TEMP = temperature;
        INDOOR_HUM = humidity;
    } else {
        Serial.println("[Periphery] 警告: DHT22传感器读取失败");
    }

    Serial.println("[Periphery] 外设管理器初始化完成");
}

void PeripheryManager_::tick()
{
    unsigned long currentTime = millis();

    // 检查是否到达读取间隔
    if (currentTime - lastUpdate >= READ_INTERVAL) {
        readDHT22();
        lastUpdate = currentTime;

        if (sensorAvailable) {
            // 更新室内温湿度变量
            INDOOR_TEMP = temperature;
            INDOOR_HUM = humidity;

            Serial.printf("[Periphery] 室内传感器数据已更新: %.1f°C, %.1f%%\n", temperature, humidity);
        }
    }
}

bool PeripheryManager_::isSensorAvailable()
{
    return sensorAvailable;
}

void PeripheryManager_::readDHT22()
{
    // 读取温湿度数据(最多尝试3次)
    for (int attempt = 0; attempt < 3; attempt++) {
        // 第一次读取不需要额外延迟,后续重试需要等待
        if (attempt > 0) {
            delay(READ_INTERVAL);
        }

        float temp = dht->readTemperature();
        float hum = dht->readHumidity();

        // 检查是否读取成功
        if (!isnan(temp) && !isnan(hum)) {
            temperature = temp;
            humidity = hum;
            sensorAvailable = true;

            // 合理性检查:温度 -40~80°C, 湿度 0~100%
            if (temperature >= -40 && temperature <= 80 &&
                humidity >= 0 && humidity <= 100) {
                return; // 读取成功且数据合理
            }
        }

        Serial.printf("[Periphery] 读取失败或数据无效, 尝试 %d/3\n", attempt + 1);
    }

    // 所有尝试都失败
    sensorAvailable = false;
    Serial.println("[Periphery] DHT22传感器读取失败");
}
