/**
 * @file DisplayManager.h
 * @brief 显示管理器 — LED 矩阵显示控制与状态系统
 *
 * 本文件定义：
 *   - DisplayStatus 枚举: 显示模式 (正常/AP/连接中/连接成功/失败)
 *   - DisplayManager_ 单例类: 管理矩阵显示、亮度、颜色
 *   - 状态显示系统: 用于显示配网、连接等状态画面
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "Globals.h"
#include "MatrixDisplayUi.h"
#include <Arduino.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>
#include <vector>

// ==================================================================
// 显示状态枚举
// ==================================================================

/// 显示状态枚举
enum DisplayStatus
{
  DISPLAY_NORMAL = 0,    ///< 正常应用显示模式
  DISPLAY_AP_MODE,       ///< AP 配网模式 - 显示热点名称和 IP
  DISPLAY_CONNECTING,    ///< WiFi 连接中动画
  DISPLAY_CONNECTED,     ///< 连接成功 - 短暂显示 IP 地址
  DISPLAY_CONNECT_FAILED ///< 连接失败提示
};

// ==================================================================
// 显示管理器类
// ==================================================================

/**
 * @class DisplayManager_
 * @brief LED 矩阵显示管理器
 *
 * 功能：
 *   - 初始化和配置 LED 矩阵 (FastLED + FastLED_NeoMatrix)
 *   - 管理显示亮度、颜色、布局
 *   - 管理应用列表和应用切换
 *   - 状态显示系统 (AP 配网、连接状态等)
 *   - 文字渲染与滚动显示
 */
class DisplayManager_
{
private:
  // ==================================================================
  // 状态显示相关
  // ==================================================================
  struct DisplayState
  {
    DisplayStatus status = DISPLAY_NORMAL; ///< 当前显示状态
    String line1;                          ///< 主显示文字
    String line2;                          ///< 副显示文字 (滚动)
    int16_t scrollX = 0;                   ///< 滚动位置
    int16_t scrollTextWidth = 0;           ///< 滚动文字像素宽度
    unsigned long lastScrollTime = 0;      ///< 上次滚动时间
    unsigned long startTime = 0;           ///< 状态开始时间
    uint8_t animFrame = 0;                 ///< 动画帧索引
    unsigned long lastAnimTime = 0;        ///< 上次动画更新时间
  } _state;

  // ==================================================================
  // 状态画面渲染方法 (私有)
  // ==================================================================

  /// 渲染 AP 配网模式画面
  void _renderAPMode();

  /// 渲染 WiFi 连接中动画
  void _renderConnecting();

  /// 渲染 WiFi 连接成功画面
  void _renderConnected();

  /// 渲染 WiFi 连接失败画面
  void _renderConnectFailed();

  /// 绘制 WiFi 图标
  void _drawWiFiIcon(int16_t x, int16_t y, uint16_t color);

  /// 绘制滚动文字
  void _drawScrollText(int16_t y, const String &text, uint16_t color);

public:
  /**
   * @brief 获取单例实例
   * @return DisplayManager_ 单例引用
   */
  static DisplayManager_ &getInstance();

  // ==================================================================
  // 初始化与主循环
  // ==================================================================

  /// 初始化显示管理器 (设置 FastLED、矩阵参数)
  void setup();

  /// 主循环 - 渲染当前帧
  void tick();

  /// 清空显示缓冲区
  void clear();

  /// 将缓冲区数据输出到 LED
  void show();

  // ==================================================================
  // 亮度与颜色设置
  // ==================================================================

  /**
   * @brief 设置矩阵布局
   * @param layout 布局模式 (0-5)
   */
  void setMatrixLayout(int layout);

  /**
   * @brief 设置显示亮度
   * @param bri 亮度值 (0-255)
   */
  void setBrightness(uint8_t bri);

  /**
   * @brief 设置文字颜色
   * @param color RGB565 颜色值
   */
  void setTextColor(uint16_t color);

  /**
   * @brief 设置矩阵电源状态
   * @param on true=开启, false=关闭
   */
  void setMatrixState(bool on);

  // ==================================================================
  // 文字渲染
  // ==================================================================

  /**
   * @brief 打印文字到指定位置
   * @param x X 坐标
   * @param y Y 坐标
   * @param text 要显示的文字
   * @param centered 是否居中显示
   * @param ignoreUppercase 是否忽略转大写
   */
  void printText(int16_t x, int16_t y, const char *text, bool centered,
                 bool ignoreUppercase);

  /**
   * @brief 恢复默认文字颜色
   */
  void defaultTextColor();
  void gammaCorrection();
  // ==================================================================
  // 设置应用
  // ==================================================================

  /// 应用所有设置 (帧率、亮度、自动切换等)
  void applyAllSettings();

  /// 加载内置应用列表
  void loadNativeApps();

  /**
   * @brief 通过 JSON 更新应用列表
   * @param json JSON 字符串
   */
  void updateAppVector(const char *json);

  /**
   * @brief 通过 JSON 更新系统设置
   * @param json JSON 字符串
   */
  void setNewSettings(const char *json);

  // ==================================================================
  // 应用切换
  // ==================================================================

  /// 切换到下一个应用
  void nextApp();

  /// 切换到上一个应用
  void previousApp();

  // ==================================================================
  // 按钮处理
  // ==================================================================

  /// 左按钮 - 切换到上一个应用
  void leftButton();

  /// 选择按钮 - 当前未实现
  void selectButton();

  /// 右按钮 - 切换到下一个应用
  void rightButton();

  // ==================================================================
  // 状态显示公开方法
  // ==================================================================

  /**
   * @brief 设置显示状态
   * @param status 显示状态枚举
   * @param line1 主文字 (如 AP 名称)
   * @param line2 副文字 (如 IP 地址，会滚动显示)
   */
  void setDisplayStatus(DisplayStatus status, const String &line1 = "",
                        const String &line2 = "");

  /**
   * @brief 获取当前显示状态
   * @return 当前 DisplayStatus
   */
  DisplayStatus getDisplayStatus() const { return _state.status; }
};

extern DisplayManager_ &DisplayManager;

#endif