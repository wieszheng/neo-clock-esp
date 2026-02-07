#ifndef WEATHER_MANAGER_H
#define WEATHER_MANAGER_H

#include <Arduino.h>

class WeatherManager_
{
public:
    static WeatherManager_ &getInstance();
    void setup();
    void tick();
    void startBackgroundTask();
    void stopBackgroundTask();
    void fetchOnce();

private:
    unsigned long lastUpdate = 0;
    void fetchWeather();
    TaskHandle_t taskHandle = NULL;
};

extern WeatherManager_ &WeatherManager;

#endif
