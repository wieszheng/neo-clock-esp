/**
 * @file Logger.cpp
 * @brief 可配置日志系统实现
 */

#include "Logger.h"

// ==================================================================
// 私有变量
// ==================================================================

static LogLevel _currentLevel = LOG_LEVEL_INFO;

// ==================================================================
// 公开实现
// ==================================================================

void Logger_Init()
{
#if LOG_TO_SERIAL
  Serial.begin(115200);
  delay(100);  // 等待 Serial 就绪
#endif

#if LOG_LEVEL <= LOG_LEVEL_DEBUG
  // DEBUG 模式: 打印编译信息
  Serial.print("\n=== NeoClock 编译信息 ===\n");
  Serial.print("日志级别: ");
  switch (LOG_LEVEL) {
    case LOG_LEVEL_DEBUG: Serial.print("DEBUG"); break;
    case LOG_LEVEL_INFO:  Serial.print("INFO"); break;
    case LOG_LEVEL_WARN:  Serial.print("WARN"); break;
    case LOG_LEVEL_ERROR: Serial.print("ERROR"); break;
    default:              Serial.print("未知"); break;
  }
  Serial.print("\n");
  Serial.print("时间戳: ");
  Serial.print(LOG_TIMESTAMP ? "启用" : "禁用");
  Serial.print("\n");
  Serial.print("===========================\n");
#endif
}

void Logger_SetLevel(LogLevel level)
{
  _currentLevel = level;
}

LogLevel Logger_GetLevel()
{
  return _currentLevel;
}
