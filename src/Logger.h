/**
 * @file Logger.h
 * @brief 可配置日志系统 — 支持日志级别、时间戳
 *
 * 功能特性:
 *   - 日志级别: DEBUG, INFO, WARN, ERROR
 *   - 时间戳: 可选是否显示时间戳
 *   - 编译时优化: 高级别日志未启用时零开销
 *
 * 使用方法:
 *   LOG_DEBUG("调试信息: %d", value);   // 调试级别
 *   LOG_INFO("一般信息: %s", str);    // 信息级别
 *   LOG_WARN("警告: %s", str);        // 警告级别
 *   LOG_ERROR("错误: %d", code);      // 错误级别
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

// ==================================================================
// 日志级别配置 (在 platformio.ini 或 build_flags 中覆盖)
// ==================================================================

// 默认日志级别: DEBUG=0, INFO=1, WARN=2, ERROR=3, OFF=4
#ifndef LOG_LEVEL
#define LOG_LEVEL 1  // 默认: INFO 级别
#endif

// 输出目标配置
#ifndef LOG_TO_SERIAL
#define LOG_TO_SERIAL 1  // 默认启用 Serial 输出
#endif

#ifndef LOG_TIMESTAMP
#define LOG_TIMESTAMP 1  // 默认显示时间戳
#endif

// ==================================================================
// 日志级别枚举
// ==================================================================

enum LogLevel {
  LOG_LEVEL_DEBUG = 0,
  LOG_LEVEL_INFO  = 1,
  LOG_LEVEL_WARN  = 2,
  LOG_LEVEL_ERROR = 3,
  LOG_LEVEL_OFF   = 4
};

// ==================================================================
// 内部宏 (请勿直接使用)
// ==================================================================

#define _LOG_LEVEL_ENABLED(level) ((level) >= LOG_LEVEL)

#define _LOG_OUTPUT_SERIAL(...) \
  do { if (LOG_TO_SERIAL) { Serial.print(__VA_ARGS__); } } while(0)

#define _LOG_TIMESTAMP() \
  do { if (LOG_TIMESTAMP) { \
    char _ts[16]; \
    snprintf(_ts, sizeof(_ts), "[%06lu] ", millis()); \
    _LOG_OUTPUT_SERIAL(_ts); \
  }} while(0)

#define _LOG_MSG(fmt, ...) \
  do { char _buf[256]; \
       snprintf(_buf, sizeof(_buf), fmt, ##__VA_ARGS__); \
       _LOG_OUTPUT_SERIAL(_buf); } while(0)

// ==================================================================
// 公开日志宏
// ==================================================================

/**
 * @brief 调试级别日志
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
#define LOG_DEBUG(fmt, ...) \
  do { if (_LOG_LEVEL_ENABLED(LOG_LEVEL_DEBUG)) { \
    _LOG_TIMESTAMP(); \
    _LOG_MSG(fmt, ##__VA_ARGS__); \
    _LOG_OUTPUT_SERIAL("\n"); \
  }} while(0)

/**
 * @brief 信息级别日志
 */
#define LOG_INFO(fmt, ...) \
  do { if (_LOG_LEVEL_ENABLED(LOG_LEVEL_INFO)) { \
    _LOG_TIMESTAMP(); \
    _LOG_MSG(fmt, ##__VA_ARGS__); \
    _LOG_OUTPUT_SERIAL("\n"); \
  }} while(0)

/**
 * @brief 警告级别日志
 */
#define LOG_WARN(fmt, ...) \
  do { if (_LOG_LEVEL_ENABLED(LOG_LEVEL_WARN)) { \
    _LOG_TIMESTAMP(); \
    _LOG_MSG(fmt, ##__VA_ARGS__); \
    _LOG_OUTPUT_SERIAL("\n"); \
  }} while(0)

/**
 * @brief 错误级别日志
 */
#define LOG_ERROR(fmt, ...) \
  do { if (_LOG_LEVEL_ENABLED(LOG_LEVEL_ERROR)) { \
    _LOG_TIMESTAMP(); \
    _LOG_MSG(fmt, ##__VA_ARGS__); \
    _LOG_OUTPUT_SERIAL("\n"); \
  }} while(0)

// ==================================================================
// 便捷宏
// ==================================================================

/**
 * @brief 简单日志 (无时间戳，用于需要紧凑输出的场景)
 */
#define LOG(fmt, ...) LOG_INFO(fmt, ##__VA_ARGS__)

/**
 * @brief 条件日志
 */
#define LOG_IF(condition, fmt, ...) \
  do { if (condition) { LOG_INFO(fmt, ##__VA_ARGS__); }} while(0)

/**
 * @brief 结果日志 (成功输出 INFO，失败输出 ERROR)
 */
#define LOG_RESULT(success, ok_msg, fail_msg) \
  do { if (success) { LOG_INFO(ok_msg); } else { LOG_ERROR(fail_msg); }} while(0)

// ==================================================================
// 初始化函数声明
// ==================================================================

/**
 * @brief 初始化日志系统
 * @note 在 setup() 中调用
 */
void Logger_Init();

/**
 * @brief 设置日志级别
 * @param level 新的日志级别
 */
void Logger_SetLevel(LogLevel level);

/**
 * @brief 获取当前日志级别
 * @return 当前日志级别
 */
LogLevel Logger_GetLevel();

#endif // LOGGER_H
