/**
 * @file WeatherManager.h
 * @brief 天气管理器 — OpenWeatherMap API 数据获取
 *
 * 本文件定义：
 *   - WeatherManager_ 单例类: 管理天气数据获取
 *   - 后台任务定时更新天气
 *   - 支持手动触发单次获取
 */

#ifndef WEATHER_MANAGER_H
#define WEATHER_MANAGER_H

#include <Arduino.h>

// ==================================================================
// 天气管理器类
// ==================================================================

/**
 * @class WeatherManager_
 * @brief 天气数据获取管理器
 *
 * 功能：
 *   - 使用 FreeRTOS 后台任务定时从 OpenWeatherMap 获取天气
 *   - 更新室外温度、湿度、风速等数据
 *   - 支持 API 密钥和城市配置
 */
class WeatherManager_
{
public:
    /**
     * @brief 获取单例实例
     * @return WeatherManager_ 单例引用
     */
    static WeatherManager_ &getInstance();

    /**
     * @brief 初始化天气管理器
     *
     * 启动后台任务，开始定时获取天气数据
     */
    void setup();

    /**
     * @brief 主循环 (已由后台任务接管，此函数为空)
     */
    void tick();

    /**
     * @brief 启动后台获取任务
     *
     * 创建 FreeRTOS 任务，定时调用 fetchOnce()
     */
    void startBackgroundTask();

    /**
     * @brief 停止后台获取任务
     *
     * 删除 FreeRTOS 任务
     */
    void stopBackgroundTask();

    /**
     * @brief 手动触发一次天气获取
     *
     * 主动获取一次天气数据 (通常用于测试或手动刷新)
     */
    void fetchOnce();

private:
    unsigned long lastUpdate = 0;  ///< 上次更新时间戳
    TaskHandle_t taskHandle = NULL; ///< FreeRTOS 任务句柄

    /**
     * @brief 实际执行天气数据获取
     *
     * 使用 HTTPClient 从 OpenWeatherMap API 获取数据，
     * 使用流式 JSON 解析减少内存分配
     */
    void fetchWeather();
};

extern WeatherManager_ &WeatherManager;

#endif
