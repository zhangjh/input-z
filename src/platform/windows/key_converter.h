/**
 * 键码转换
 * Task 2.1: 实现键码转换函数
 * 
 * Windows 虚拟键码到 RIME 键码的转换
 * 
 * RIME 使用 X11 keysym 风格的键码：
 * - 可打印 ASCII 字符直接使用其 ASCII 值 (0x20-0x7e)
 * - 功能键使用 0xff00-0xffff 范围
 * - 修饰键使用 0xffe0-0xffef 范围
 * 
 * Requirements: 2.1, 2.2
 */

#ifndef SUYAN_WINDOWS_KEY_CONVERTER_H
#define SUYAN_WINDOWS_KEY_CONVERTER_H

#ifdef _WIN32

// Qt 头文件必须在 Windows 头文件之前包含
// 因为 Windows 头文件定义了 Bool 宏，与 Qt 的 qmetatype.h 冲突
#include <QtCore/qglobal.h>

#include <windows.h>

namespace suyan {

/**
 * 将 Windows 虚拟键码转换为 RIME 键码
 * 
 * 转换规则：
 * - 字母键 (VK_A-VK_Z): 根据 Shift 状态返回大写或小写 ASCII
 * - 数字键 (VK_0-VK_9): 返回对应的 ASCII 数字或符号
 * - 功能键: 返回对应的 X11 keysym
 * - 小键盘: 返回对应的 X11 keypad keysym
 * 
 * @param vk Windows 虚拟键码 (VK_*)
 * @param scanCode 扫描码
 * @param extended 是否为扩展键
 * @return RIME 键码，如果无法转换返回 0
 */
int convertVirtualKeyToRime(WPARAM vk, UINT scanCode, bool extended);

/**
 * 将当前修饰键状态转换为 RIME 修饰键掩码
 * 
 * 检查当前按下的修饰键并返回对应的掩码：
 * - Shift: KeyModifier::Shift (1 << 0)
 * - Control: KeyModifier::Control (1 << 2)
 * - Alt: KeyModifier::Alt (1 << 3)
 * - Win: KeyModifier::Super (1 << 6)
 * 
 * @return RIME 修饰键掩码
 */
int convertModifiersToRime();

/**
 * 检查是否为字符键
 * 
 * 字符键包括：
 * - 字母键 (VK_A-VK_Z)
 * - 数字键 (VK_0-VK_9)
 * - OEM 键（标点符号等）
 * - 空格键
 * 
 * @param vk 虚拟键码
 * @return 是否为字符键
 */
bool isCharacterKey(WPARAM vk);

/**
 * 获取键的字符值
 * 
 * 使用 ToUnicode 函数获取按键对应的字符，
 * 考虑当前键盘布局和修饰键状态。
 * 
 * @param vk 虚拟键码
 * @param scanCode 扫描码
 * @return 字符值，如果不是字符键返回 0
 */
wchar_t getCharacterFromKey(WPARAM vk, UINT scanCode);

/**
 * 检查是否为修饰键
 * 
 * @param vk 虚拟键码
 * @return 是否为修饰键 (Shift, Ctrl, Alt, Win)
 */
bool isModifierKey(WPARAM vk);

/**
 * 检查是否为功能键
 * 
 * @param vk 虚拟键码
 * @return 是否为功能键 (F1-F24)
 */
bool isFunctionKey(WPARAM vk);

/**
 * 检查是否为小键盘键
 * 
 * @param vk 虚拟键码
 * @return 是否为小键盘键
 */
bool isNumpadKey(WPARAM vk);

/**
 * 检查是否为导航键
 * 
 * @param vk 虚拟键码
 * @return 是否为导航键 (方向键、Home、End、PageUp、PageDown)
 */
bool isNavigationKey(WPARAM vk);

} // namespace suyan

#endif // _WIN32

#endif // SUYAN_WINDOWS_KEY_CONVERTER_H
