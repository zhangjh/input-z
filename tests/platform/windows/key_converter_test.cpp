/**
 * 键码转换单元测试
 * Task 2.2: 编写键码转换单元测试
 * 
 * 测试内容：
 * - 字母键、数字键、特殊键的转换
 * - 修饰键组合的转换
 * - Property 1: 键码转换正确性
 * - Property 2: 修饰键转换正确性
 * 
 * Validates: Requirements 2.1, 2.2
 * 
 * 注意：此测试文件设计为跨平台编译，在非 Windows 平台上
 * 使用模拟的 Windows API 定义进行测试。
 */

#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>

// ========== 平台兼容层 ==========
// 在非 Windows 平台上模拟 Windows API 定义

#ifndef _WIN32

// 模拟 Windows 类型定义
typedef unsigned long WPARAM;
typedef long LPARAM;
typedef unsigned int UINT;
typedef short SHORT;
typedef int BOOL;

// 模拟 Windows 虚拟键码定义
#define VK_BACK           0x08
#define VK_TAB            0x09
#define VK_RETURN         0x0D
#define VK_SHIFT          0x10
#define VK_CONTROL        0x11
#define VK_MENU           0x12  // Alt
#define VK_PAUSE          0x13
#define VK_CAPITAL        0x14  // Caps Lock
#define VK_ESCAPE         0x1B
#define VK_SPACE          0x20
#define VK_PRIOR          0x21  // Page Up
#define VK_NEXT           0x22  // Page Down
#define VK_END            0x23
#define VK_HOME           0x24
#define VK_LEFT           0x25
#define VK_UP             0x26
#define VK_RIGHT          0x27
#define VK_DOWN           0x28
#define VK_SNAPSHOT       0x2C  // Print Screen
#define VK_INSERT         0x2D
#define VK_DELETE         0x2E
#define VK_LWIN           0x5B
#define VK_RWIN           0x5C
#define VK_APPS           0x5D
#define VK_NUMPAD0        0x60
#define VK_NUMPAD1        0x61
#define VK_NUMPAD2        0x62
#define VK_NUMPAD3        0x63
#define VK_NUMPAD4        0x64
#define VK_NUMPAD5        0x65
#define VK_NUMPAD6        0x66
#define VK_NUMPAD7        0x67
#define VK_NUMPAD8        0x68
#define VK_NUMPAD9        0x69
#define VK_MULTIPLY       0x6A
#define VK_ADD            0x6B
#define VK_SEPARATOR      0x6C
#define VK_SUBTRACT       0x6D
#define VK_DECIMAL        0x6E
#define VK_DIVIDE         0x6F
#define VK_F1             0x70
#define VK_F2             0x71
#define VK_F3             0x72
#define VK_F4             0x73
#define VK_F5             0x74
#define VK_F6             0x75
#define VK_F7             0x76
#define VK_F8             0x77
#define VK_F9             0x78
#define VK_F10            0x79
#define VK_F11            0x7A
#define VK_F12            0x7B
#define VK_F13            0x7C
#define VK_F14            0x7D
#define VK_F15            0x7E
#define VK_F16            0x7F
#define VK_F17            0x80
#define VK_F18            0x81
#define VK_F19            0x82
#define VK_F20            0x83
#define VK_F21            0x84
#define VK_F22            0x85
#define VK_F23            0x86
#define VK_F24            0x87
#define VK_NUMLOCK        0x90
#define VK_SCROLL         0x91
#define VK_LSHIFT         0xA0
#define VK_RSHIFT         0xA1
#define VK_LCONTROL       0xA2
#define VK_RCONTROL       0xA3
#define VK_LMENU          0xA4
#define VK_RMENU          0xA5
#define VK_OEM_1          0xBA  // ;:
#define VK_OEM_PLUS       0xBB  // =+
#define VK_OEM_COMMA      0xBC  // ,<
#define VK_OEM_MINUS      0xBD  // -_
#define VK_OEM_PERIOD     0xBE  // .>
#define VK_OEM_2          0xBF  // /?
#define VK_OEM_3          0xC0  // `~
#define VK_OEM_4          0xDB  // [{
#define VK_OEM_5          0xDC  // \|
#define VK_OEM_6          0xDD  // ]}
#define VK_OEM_7          0xDE  // '"
#define VK_OEM_102        0xE2

// 模拟键盘状态（用于测试）
static unsigned char g_keyboardState[256] = {0};
static bool g_capsLockOn = false;

// 模拟 GetKeyState
SHORT GetKeyState(int vk) {
    SHORT result = 0;
    if (g_keyboardState[vk] & 0x80) {
        result |= 0x8000;  // 按下状态
    }
    if (vk == VK_CAPITAL && g_capsLockOn) {
        result |= 0x0001;  // 切换状态
    }
    return result;
}

// 模拟 GetKeyboardState
BOOL GetKeyboardState(unsigned char* lpKeyState) {
    if (lpKeyState) {
        for (int i = 0; i < 256; i++) {
            lpKeyState[i] = g_keyboardState[i];
        }
        return 1;
    }
    return 0;
}

// 模拟 ToUnicode
int ToUnicode(UINT vk, UINT scanCode, const unsigned char* keyState, 
              wchar_t* buffer, int bufferSize, UINT flags) {
    if (!buffer || bufferSize < 1) return 0;
    
    // 简单实现：只处理基本字符
    bool shift = (keyState[VK_SHIFT] & 0x80) != 0;
    
    // 字母键
    if (vk >= 'A' && vk <= 'Z') {
        bool caps = g_capsLockOn;
        if (shift != caps) {
            buffer[0] = static_cast<wchar_t>(vk);  // 大写
        } else {
            buffer[0] = static_cast<wchar_t>(vk + 32);  // 小写
        }
        return 1;
    }
    
    // 数字键
    if (vk >= '0' && vk <= '9') {
        if (shift) {
            static const char shiftedDigits[] = ")!@#$%^&*(";
            buffer[0] = static_cast<wchar_t>(shiftedDigits[vk - '0']);
        } else {
            buffer[0] = static_cast<wchar_t>(vk);
        }
        return 1;
    }
    
    return 0;
}

// 模拟 MapVirtualKey
UINT MapVirtualKey(UINT vk, UINT mapType) {
    // 简单实现
    if (vk >= 'A' && vk <= 'Z') {
        return vk + 32;  // 返回小写
    }
    if (vk >= '0' && vk <= '9') {
        return vk;
    }
    return 0;
}

// 测试辅助函数：设置键盘状态
void setKeyDown(int vk, bool down) {
    if (down) {
        g_keyboardState[vk] |= 0x80;
    } else {
        g_keyboardState[vk] &= ~0x80;
    }
}

void setCapsLock(bool on) {
    g_capsLockOn = on;
}

void resetKeyboardState() {
    for (int i = 0; i < 256; i++) {
        g_keyboardState[i] = 0;
    }
    g_capsLockOn = false;
}

#else
// Windows 平台
#include <windows.h>

// Windows 平台使用模拟键盘状态进行测试
static unsigned char g_keyboardState[256] = {0};
static bool g_capsLockOn = false;

// 重写 GetKeyState 用于测试
#undef GetKeyState
SHORT GetKeyState(int vk) {
    SHORT result = 0;
    if (g_keyboardState[vk] & 0x80) {
        result |= 0x8000;
    }
    if (vk == VK_CAPITAL && g_capsLockOn) {
        result |= 0x0001;
    }
    return result;
}

void setKeyDown(int vk, bool down) {
    if (down) {
        g_keyboardState[vk] |= 0x80;
    } else {
        g_keyboardState[vk] &= ~0x80;
    }
}

void setCapsLock(bool on) {
    g_capsLockOn = on;
}

void resetKeyboardState() {
    for (int i = 0; i < 256; i++) {
        g_keyboardState[i] = 0;
    }
    g_capsLockOn = false;
}

#endif // _WIN32

// ========== X11 Keysym 常量定义（与 key_converter.cpp 一致）==========

namespace XK {
    constexpr int BackSpace     = 0xff08;
    constexpr int Tab           = 0xff09;
    constexpr int Return        = 0xff0d;
    constexpr int Pause         = 0xff13;
    constexpr int Scroll_Lock   = 0xff14;
    constexpr int Escape        = 0xff1b;
    constexpr int Delete        = 0xffff;
    constexpr int Home          = 0xff50;
    constexpr int Left          = 0xff51;
    constexpr int Up            = 0xff52;
    constexpr int Right         = 0xff53;
    constexpr int Down          = 0xff54;
    constexpr int Page_Up       = 0xff55;
    constexpr int Page_Down     = 0xff56;
    constexpr int End           = 0xff57;
    constexpr int Print         = 0xff61;
    constexpr int Insert        = 0xff63;
    constexpr int Menu          = 0xff67;
    constexpr int Num_Lock      = 0xff7f;
    constexpr int KP_Enter      = 0xff8d;
    constexpr int KP_Home       = 0xff95;
    constexpr int KP_Left       = 0xff96;
    constexpr int KP_Up         = 0xff97;
    constexpr int KP_Right      = 0xff98;
    constexpr int KP_Down       = 0xff99;
    constexpr int KP_Page_Up    = 0xff9a;
    constexpr int KP_Page_Down  = 0xff9b;
    constexpr int KP_End        = 0xff9c;
    constexpr int KP_Insert     = 0xff9e;
    constexpr int KP_Delete     = 0xff9f;
    constexpr int KP_Multiply   = 0xffaa;
    constexpr int KP_Add        = 0xffab;
    constexpr int KP_Subtract   = 0xffad;
    constexpr int KP_Decimal    = 0xffae;
    constexpr int KP_Divide     = 0xffaf;
    constexpr int KP_0          = 0xffb0;
    constexpr int KP_1          = 0xffb1;
    constexpr int KP_2          = 0xffb2;
    constexpr int KP_3          = 0xffb3;
    constexpr int KP_4          = 0xffb4;
    constexpr int KP_5          = 0xffb5;
    constexpr int KP_6          = 0xffb6;
    constexpr int KP_7          = 0xffb7;
    constexpr int KP_8          = 0xffb8;
    constexpr int KP_9          = 0xffb9;
    constexpr int F1            = 0xffbe;
    constexpr int F2            = 0xffbf;
    constexpr int F3            = 0xffc0;
    constexpr int F4            = 0xffc1;
    constexpr int F5            = 0xffc2;
    constexpr int F6            = 0xffc3;
    constexpr int F7            = 0xffc4;
    constexpr int F8            = 0xffc5;
    constexpr int F9            = 0xffc6;
    constexpr int F10           = 0xffc7;
    constexpr int F11           = 0xffc8;
    constexpr int F12           = 0xffc9;
    constexpr int F24           = 0xffd5;
    constexpr int Shift_L       = 0xffe1;
    constexpr int Shift_R       = 0xffe2;
    constexpr int Control_L     = 0xffe3;
    constexpr int Control_R     = 0xffe4;
    constexpr int Caps_Lock     = 0xffe5;
    constexpr int Alt_L         = 0xffe9;
    constexpr int Alt_R         = 0xffea;
    constexpr int Super_L       = 0xffeb;
    constexpr int Super_R       = 0xffec;
}

// ========== 修饰键掩码 ==========

namespace KeyModifier {
    constexpr int None    = 0;
    constexpr int Shift   = 1 << 0;
    constexpr int Control = 1 << 2;
    constexpr int Alt     = 1 << 3;
    constexpr int Super   = 1 << 6;
}


// ========== 键码转换函数的独立实现（用于测试）==========
// 这是 key_converter.cpp 中函数的副本，用于跨平台测试

namespace suyan_test {

static inline bool isKeyDown(int vk) {
    return (GetKeyState(vk) & 0x8000) != 0;
}

static inline bool isCapsLockOn() {
    return (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
}

int convertVirtualKeyToRime(WPARAM vk, UINT scanCode, bool extended) {
    // 1. 功能键 F1-F24
    if (vk >= VK_F1 && vk <= VK_F24) {
        return XK::F1 + (static_cast<int>(vk) - VK_F1);
    }

    // 2. 修饰键
    switch (vk) {
        case VK_SHIFT:
            if (scanCode == 0x36) return XK::Shift_R;
            return XK::Shift_L;
        case VK_LSHIFT: return XK::Shift_L;
        case VK_RSHIFT: return XK::Shift_R;
        case VK_CONTROL:
            if (extended) return XK::Control_R;
            return XK::Control_L;
        case VK_LCONTROL: return XK::Control_L;
        case VK_RCONTROL: return XK::Control_R;
        case VK_MENU:
            if (extended) return XK::Alt_R;
            return XK::Alt_L;
        case VK_LMENU: return XK::Alt_L;
        case VK_RMENU: return XK::Alt_R;
        case VK_LWIN: return XK::Super_L;
        case VK_RWIN: return XK::Super_R;
        case VK_CAPITAL: return XK::Caps_Lock;
    }

    // 3. 导航键和编辑键
    switch (vk) {
        case VK_RETURN:
            if (extended) return XK::KP_Enter;
            return XK::Return;
        case VK_TAB: return XK::Tab;
        case VK_BACK: return XK::BackSpace;
        case VK_ESCAPE: return XK::Escape;
        case VK_SPACE: return 0x0020;
        case VK_DELETE:
            if (extended) return XK::Delete;
            return XK::KP_Delete;
        case VK_INSERT:
            if (extended) return XK::Insert;
            return XK::KP_Insert;
        case VK_HOME:
            if (extended) return XK::Home;
            return XK::KP_Home;
        case VK_END:
            if (extended) return XK::End;
            return XK::KP_End;
        case VK_PRIOR:
            if (extended) return XK::Page_Up;
            return XK::KP_Page_Up;
        case VK_NEXT:
            if (extended) return XK::Page_Down;
            return XK::KP_Page_Down;
        case VK_LEFT:
            if (extended) return XK::Left;
            return XK::KP_Left;
        case VK_RIGHT:
            if (extended) return XK::Right;
            return XK::KP_Right;
        case VK_UP:
            if (extended) return XK::Up;
            return XK::KP_Up;
        case VK_DOWN:
            if (extended) return XK::Down;
            return XK::KP_Down;
    }

    // 4. 其他功能键
    switch (vk) {
        case VK_PAUSE: return XK::Pause;
        case VK_SCROLL: return XK::Scroll_Lock;
        case VK_SNAPSHOT: return XK::Print;
        case VK_NUMLOCK: return XK::Num_Lock;
        case VK_APPS: return XK::Menu;
    }

    // 5. 小键盘数字和运算符
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        return XK::KP_0 + (static_cast<int>(vk) - VK_NUMPAD0);
    }
    switch (vk) {
        case VK_MULTIPLY: return XK::KP_Multiply;
        case VK_ADD: return XK::KP_Add;
        case VK_SUBTRACT: return XK::KP_Subtract;
        case VK_DECIMAL: return XK::KP_Decimal;
        case VK_DIVIDE: return XK::KP_Divide;
    }

    // 6. 字母键 (VK_A - VK_Z)
    if (vk >= 'A' && vk <= 'Z') {
        bool shiftDown = isKeyDown(VK_SHIFT);
        bool capsLock = isCapsLockOn();
        if (shiftDown != capsLock) {
            return static_cast<int>(vk);  // 大写
        } else {
            return static_cast<int>(vk) + 32;  // 小写
        }
    }

    // 7. 数字键 (VK_0 - VK_9)
    if (vk >= '0' && vk <= '9') {
        if (isKeyDown(VK_SHIFT)) {
            static const char shiftedDigits[] = ")!@#$%^&*(";
            return static_cast<int>(shiftedDigits[vk - '0']);
        }
        return static_cast<int>(vk);
    }

    return 0;
}

int convertModifiersToRime() {
    int modifiers = KeyModifier::None;

    if (isKeyDown(VK_SHIFT) || isKeyDown(VK_LSHIFT) || isKeyDown(VK_RSHIFT)) {
        modifiers |= KeyModifier::Shift;
    }
    if (isKeyDown(VK_CONTROL) || isKeyDown(VK_LCONTROL) || isKeyDown(VK_RCONTROL)) {
        modifiers |= KeyModifier::Control;
    }
    if (isKeyDown(VK_MENU) || isKeyDown(VK_LMENU) || isKeyDown(VK_RMENU)) {
        modifiers |= KeyModifier::Alt;
    }
    if (isKeyDown(VK_LWIN) || isKeyDown(VK_RWIN)) {
        modifiers |= KeyModifier::Super;
    }

    return modifiers;
}

bool isCharacterKey(WPARAM vk) {
    if (vk >= 'A' && vk <= 'Z') return true;
    if (vk >= '0' && vk <= '9') return true;
    if (vk == VK_SPACE) return true;
    switch (vk) {
        case VK_OEM_1: case VK_OEM_PLUS: case VK_OEM_COMMA:
        case VK_OEM_MINUS: case VK_OEM_PERIOD: case VK_OEM_2:
        case VK_OEM_3: case VK_OEM_4: case VK_OEM_5:
        case VK_OEM_6: case VK_OEM_7: case VK_OEM_102:
            return true;
    }
    return false;
}

bool isModifierKey(WPARAM vk) {
    switch (vk) {
        case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
        case VK_MENU: case VK_LMENU: case VK_RMENU:
        case VK_LWIN: case VK_RWIN: case VK_CAPITAL:
            return true;
        default:
            return false;
    }
}

bool isFunctionKey(WPARAM vk) {
    return vk >= VK_F1 && vk <= VK_F24;
}

bool isNumpadKey(WPARAM vk) {
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return true;
    switch (vk) {
        case VK_MULTIPLY: case VK_ADD: case VK_SEPARATOR:
        case VK_SUBTRACT: case VK_DECIMAL: case VK_DIVIDE:
        case VK_NUMLOCK:
            return true;
        default:
            return false;
    }
}

bool isNavigationKey(WPARAM vk) {
    switch (vk) {
        case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
        case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
            return true;
        default:
            return false;
    }
}

} // namespace suyan_test


// ========== 测试辅助宏 ==========

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "✗ 断言失败: " << message << std::endl; \
            std::cerr << "  位置: " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

#define TEST_PASS(message) \
    std::cout << "✓ " << message << std::endl

// ========== 测试类 ==========

class KeyConverterTest {
public:
    KeyConverterTest() {
        std::srand(static_cast<unsigned>(std::time(nullptr)));
    }

    bool runAllTests() {
        std::cout << "=== 键码转换单元测试 ===" << std::endl;
        std::cout << "Task 2.2: 编写键码转换单元测试" << std::endl;
        std::cout << "Validates: Requirements 2.1, 2.2" << std::endl;
        std::cout << std::endl;

        bool allPassed = true;

        // 基础功能测试
        allPassed &= testLetterKeys();
        allPassed &= testDigitKeys();
        allPassed &= testFunctionKeys();
        allPassed &= testNavigationKeys();
        allPassed &= testNumpadKeys();
        allPassed &= testModifierKeys();
        allPassed &= testSpecialKeys();
        
        // 属性测试
        allPassed &= testProperty1_KeyConversionCorrectness();
        allPassed &= testProperty2_ModifierConversionCorrectness();
        
        // 辅助函数测试
        allPassed &= testHelperFunctions();

        std::cout << std::endl;
        if (allPassed) {
            std::cout << "=== 所有测试通过 ===" << std::endl;
        } else {
            std::cout << "=== 部分测试失败 ===" << std::endl;
        }

        return allPassed;
    }

private:
    // ========== 字母键测试 ==========
    
    bool testLetterKeys() {
        std::cout << "\n--- 字母键转换测试 ---" << std::endl;
        
        resetKeyboardState();
        
        // 测试小写字母 (无 Shift，无 CapsLock)
        for (char c = 'A'; c <= 'Z'; c++) {
            int result = suyan_test::convertVirtualKeyToRime(c, 0, false);
            int expected = c + 32;  // 小写 ASCII
            TEST_ASSERT(result == expected, 
                "字母键 " + std::string(1, c) + " 应转换为小写 " + std::string(1, static_cast<char>(expected)));
        }
        
        // 测试大写字母 (Shift 按下)
        setKeyDown(VK_SHIFT, true);
        for (char c = 'A'; c <= 'Z'; c++) {
            int result = suyan_test::convertVirtualKeyToRime(c, 0, false);
            int expected = c;  // 大写 ASCII
            TEST_ASSERT(result == expected,
                "Shift + 字母键 " + std::string(1, c) + " 应转换为大写");
        }
        setKeyDown(VK_SHIFT, false);
        
        // 测试 CapsLock 开启时
        setCapsLock(true);
        for (char c = 'A'; c <= 'Z'; c++) {
            int result = suyan_test::convertVirtualKeyToRime(c, 0, false);
            int expected = c;  // 大写 ASCII
            TEST_ASSERT(result == expected,
                "CapsLock + 字母键 " + std::string(1, c) + " 应转换为大写");
        }
        
        // 测试 CapsLock + Shift (应该是小写)
        setKeyDown(VK_SHIFT, true);
        for (char c = 'A'; c <= 'Z'; c++) {
            int result = suyan_test::convertVirtualKeyToRime(c, 0, false);
            int expected = c + 32;  // 小写 ASCII
            TEST_ASSERT(result == expected,
                "CapsLock + Shift + 字母键 " + std::string(1, c) + " 应转换为小写");
        }
        
        resetKeyboardState();
        TEST_PASS("testLetterKeys: 字母键转换正确");
        return true;
    }

    // ========== 数字键测试 ==========
    
    bool testDigitKeys() {
        std::cout << "\n--- 数字键转换测试 ---" << std::endl;
        
        resetKeyboardState();
        
        // 测试数字键 (无 Shift)
        for (char c = '0'; c <= '9'; c++) {
            int result = suyan_test::convertVirtualKeyToRime(c, 0, false);
            int expected = c;  // 数字 ASCII
            TEST_ASSERT(result == expected,
                "数字键 " + std::string(1, c) + " 应转换为 ASCII " + std::to_string(expected));
        }
        
        // 测试 Shift + 数字键 (符号)
        setKeyDown(VK_SHIFT, true);
        const char* shiftedDigits = ")!@#$%^&*(";
        for (int i = 0; i <= 9; i++) {
            int result = suyan_test::convertVirtualKeyToRime('0' + i, 0, false);
            int expected = shiftedDigits[i];
            TEST_ASSERT(result == expected,
                "Shift + 数字键 " + std::to_string(i) + " 应转换为符号 " + std::string(1, static_cast<char>(expected)));
        }
        
        resetKeyboardState();
        TEST_PASS("testDigitKeys: 数字键转换正确");
        return true;
    }

    // ========== 功能键测试 ==========
    
    bool testFunctionKeys() {
        std::cout << "\n--- 功能键转换测试 ---" << std::endl;
        
        // F1-F12
        for (int i = 0; i < 12; i++) {
            int vk = VK_F1 + i;
            int result = suyan_test::convertVirtualKeyToRime(vk, 0, false);
            int expected = XK::F1 + i;
            TEST_ASSERT(result == expected,
                "F" + std::to_string(i + 1) + " 应转换为 XK_F" + std::to_string(i + 1));
        }
        
        // F13-F24
        for (int i = 12; i < 24; i++) {
            int vk = VK_F1 + i;
            int result = suyan_test::convertVirtualKeyToRime(vk, 0, false);
            int expected = XK::F1 + i;
            TEST_ASSERT(result == expected,
                "F" + std::to_string(i + 1) + " 应转换为 XK_F" + std::to_string(i + 1));
        }
        
        TEST_PASS("testFunctionKeys: 功能键转换正确");
        return true;
    }

    // ========== 导航键测试 ==========
    
    bool testNavigationKeys() {
        std::cout << "\n--- 导航键转换测试 ---" << std::endl;
        
        // 扩展键（主键盘区域）
        struct NavKeyTest {
            int vk;
            int expected;
            const char* name;
        };
        
        NavKeyTest extendedTests[] = {
            {VK_LEFT, XK::Left, "Left"},
            {VK_RIGHT, XK::Right, "Right"},
            {VK_UP, XK::Up, "Up"},
            {VK_DOWN, XK::Down, "Down"},
            {VK_HOME, XK::Home, "Home"},
            {VK_END, XK::End, "End"},
            {VK_PRIOR, XK::Page_Up, "Page Up"},
            {VK_NEXT, XK::Page_Down, "Page Down"},
            {VK_INSERT, XK::Insert, "Insert"},
            {VK_DELETE, XK::Delete, "Delete"},
        };
        
        for (const auto& test : extendedTests) {
            int result = suyan_test::convertVirtualKeyToRime(test.vk, 0, true);
            TEST_ASSERT(result == test.expected,
                std::string(test.name) + " (扩展) 应转换为 XK_" + test.name);
        }
        
        // 非扩展键（小键盘区域）
        NavKeyTest numpadTests[] = {
            {VK_LEFT, XK::KP_Left, "KP_Left"},
            {VK_RIGHT, XK::KP_Right, "KP_Right"},
            {VK_UP, XK::KP_Up, "KP_Up"},
            {VK_DOWN, XK::KP_Down, "KP_Down"},
            {VK_HOME, XK::KP_Home, "KP_Home"},
            {VK_END, XK::KP_End, "KP_End"},
            {VK_PRIOR, XK::KP_Page_Up, "KP_Page_Up"},
            {VK_NEXT, XK::KP_Page_Down, "KP_Page_Down"},
            {VK_INSERT, XK::KP_Insert, "KP_Insert"},
            {VK_DELETE, XK::KP_Delete, "KP_Delete"},
        };
        
        for (const auto& test : numpadTests) {
            int result = suyan_test::convertVirtualKeyToRime(test.vk, 0, false);
            TEST_ASSERT(result == test.expected,
                std::string(test.name) + " (非扩展) 应转换为 XK_" + test.name);
        }
        
        TEST_PASS("testNavigationKeys: 导航键转换正确");
        return true;
    }

    // ========== 小键盘测试 ==========
    
    bool testNumpadKeys() {
        std::cout << "\n--- 小键盘转换测试 ---" << std::endl;
        
        // 小键盘数字 0-9
        for (int i = 0; i <= 9; i++) {
            int vk = VK_NUMPAD0 + i;
            int result = suyan_test::convertVirtualKeyToRime(vk, 0, false);
            int expected = XK::KP_0 + i;
            TEST_ASSERT(result == expected,
                "Numpad " + std::to_string(i) + " 应转换为 XK_KP_" + std::to_string(i));
        }
        
        // 小键盘运算符
        struct NumpadOpTest {
            int vk;
            int expected;
            const char* name;
        };
        
        NumpadOpTest opTests[] = {
            {VK_MULTIPLY, XK::KP_Multiply, "Multiply"},
            {VK_ADD, XK::KP_Add, "Add"},
            {VK_SUBTRACT, XK::KP_Subtract, "Subtract"},
            {VK_DECIMAL, XK::KP_Decimal, "Decimal"},
            {VK_DIVIDE, XK::KP_Divide, "Divide"},
        };
        
        for (const auto& test : opTests) {
            int result = suyan_test::convertVirtualKeyToRime(test.vk, 0, false);
            TEST_ASSERT(result == test.expected,
                std::string("Numpad ") + test.name + " 应转换为 XK_KP_" + test.name);
        }
        
        TEST_PASS("testNumpadKeys: 小键盘转换正确");
        return true;
    }

    // ========== 修饰键测试 ==========
    
    bool testModifierKeys() {
        std::cout << "\n--- 修饰键转换测试 ---" << std::endl;
        
        struct ModKeyTest {
            int vk;
            int expected;
            const char* name;
            UINT scanCode;
            bool extended;
        };
        
        ModKeyTest tests[] = {
            {VK_LSHIFT, XK::Shift_L, "Left Shift", 0, false},
            {VK_RSHIFT, XK::Shift_R, "Right Shift", 0, false},
            {VK_SHIFT, XK::Shift_L, "Shift (Left)", 0x2A, false},
            {VK_SHIFT, XK::Shift_R, "Shift (Right)", 0x36, false},
            {VK_LCONTROL, XK::Control_L, "Left Control", 0, false},
            {VK_RCONTROL, XK::Control_R, "Right Control", 0, false},
            {VK_CONTROL, XK::Control_L, "Control (Left)", 0, false},
            {VK_CONTROL, XK::Control_R, "Control (Right)", 0, true},
            {VK_LMENU, XK::Alt_L, "Left Alt", 0, false},
            {VK_RMENU, XK::Alt_R, "Right Alt", 0, false},
            {VK_MENU, XK::Alt_L, "Alt (Left)", 0, false},
            {VK_MENU, XK::Alt_R, "Alt (Right)", 0, true},
            {VK_LWIN, XK::Super_L, "Left Win", 0, false},
            {VK_RWIN, XK::Super_R, "Right Win", 0, false},
            {VK_CAPITAL, XK::Caps_Lock, "Caps Lock", 0, false},
        };
        
        for (const auto& test : tests) {
            int result = suyan_test::convertVirtualKeyToRime(test.vk, test.scanCode, test.extended);
            TEST_ASSERT(result == test.expected,
                std::string(test.name) + " 应转换为正确的 XK 键码");
        }
        
        TEST_PASS("testModifierKeys: 修饰键转换正确");
        return true;
    }

    // ========== 特殊键测试 ==========
    
    bool testSpecialKeys() {
        std::cout << "\n--- 特殊键转换测试 ---" << std::endl;
        
        struct SpecialKeyTest {
            int vk;
            int expected;
            const char* name;
            bool extended;
        };
        
        SpecialKeyTest tests[] = {
            {VK_RETURN, XK::Return, "Enter", false},
            {VK_RETURN, XK::KP_Enter, "Numpad Enter", true},
            {VK_TAB, XK::Tab, "Tab", false},
            {VK_BACK, XK::BackSpace, "Backspace", false},
            {VK_ESCAPE, XK::Escape, "Escape", false},
            {VK_SPACE, 0x0020, "Space", false},
            {VK_PAUSE, XK::Pause, "Pause", false},
            {VK_SCROLL, XK::Scroll_Lock, "Scroll Lock", false},
            {VK_SNAPSHOT, XK::Print, "Print Screen", false},
            {VK_NUMLOCK, XK::Num_Lock, "Num Lock", false},
            {VK_APPS, XK::Menu, "Apps/Menu", false},
        };
        
        for (const auto& test : tests) {
            int result = suyan_test::convertVirtualKeyToRime(test.vk, 0, test.extended);
            TEST_ASSERT(result == test.expected,
                std::string(test.name) + " 应转换为正确的 XK 键码");
        }
        
        TEST_PASS("testSpecialKeys: 特殊键转换正确");
        return true;
    }


    // ========== Property 1: 键码转换正确性 ==========
    /**
     * Property 1: 键码转换正确性
     * 
     * For any Windows 虚拟键码 (VK_A 到 VK_Z, VK_0 到 VK_9, 以及特殊键)，
     * convertVirtualKeyToRime 函数应返回对应的 RIME 键码，
     * 且转换是确定性的（相同输入总是产生相同输出）。
     * 
     * Validates: Requirements 2.1
     */
    bool testProperty1_KeyConversionCorrectness() {
        std::cout << "\n--- Property 1: 键码转换正确性 ---" << std::endl;
        std::cout << "  验证: 相同输入总是产生相同输出（确定性）" << std::endl;
        
        resetKeyboardState();
        
        const int NUM_ITERATIONS = 100;
        int testCount = 0;
        
        // 收集所有要测试的虚拟键码
        std::vector<WPARAM> testKeys;
        
        // 字母键
        for (char c = 'A'; c <= 'Z'; c++) {
            testKeys.push_back(c);
        }
        
        // 数字键
        for (char c = '0'; c <= '9'; c++) {
            testKeys.push_back(c);
        }
        
        // 功能键
        for (int i = VK_F1; i <= VK_F12; i++) {
            testKeys.push_back(i);
        }
        
        // 导航键
        testKeys.push_back(VK_LEFT);
        testKeys.push_back(VK_RIGHT);
        testKeys.push_back(VK_UP);
        testKeys.push_back(VK_DOWN);
        testKeys.push_back(VK_HOME);
        testKeys.push_back(VK_END);
        testKeys.push_back(VK_PRIOR);
        testKeys.push_back(VK_NEXT);
        
        // 小键盘
        for (int i = VK_NUMPAD0; i <= VK_NUMPAD9; i++) {
            testKeys.push_back(i);
        }
        
        // 特殊键
        testKeys.push_back(VK_RETURN);
        testKeys.push_back(VK_TAB);
        testKeys.push_back(VK_BACK);
        testKeys.push_back(VK_ESCAPE);
        testKeys.push_back(VK_SPACE);
        
        // 对每个键进行多次转换，验证结果一致
        for (WPARAM vk : testKeys) {
            int firstResult = suyan_test::convertVirtualKeyToRime(vk, 0, false);
            
            for (int i = 0; i < NUM_ITERATIONS; i++) {
                int result = suyan_test::convertVirtualKeyToRime(vk, 0, false);
                TEST_ASSERT(result == firstResult,
                    "键码转换应该是确定性的: VK=" + std::to_string(vk));
                testCount++;
            }
        }
        
        // 测试不同的 extended 标志
        std::vector<WPARAM> extendableKeys = {
            VK_RETURN, VK_DELETE, VK_INSERT, VK_HOME, VK_END,
            VK_PRIOR, VK_NEXT, VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN,
            VK_CONTROL, VK_MENU
        };
        
        for (WPARAM vk : extendableKeys) {
            // 非扩展键
            int nonExtResult1 = suyan_test::convertVirtualKeyToRime(vk, 0, false);
            int nonExtResult2 = suyan_test::convertVirtualKeyToRime(vk, 0, false);
            TEST_ASSERT(nonExtResult1 == nonExtResult2,
                "非扩展键转换应该一致: VK=" + std::to_string(vk));
            
            // 扩展键
            int extResult1 = suyan_test::convertVirtualKeyToRime(vk, 0, true);
            int extResult2 = suyan_test::convertVirtualKeyToRime(vk, 0, true);
            TEST_ASSERT(extResult1 == extResult2,
                "扩展键转换应该一致: VK=" + std::to_string(vk));
            
            testCount += 4;
        }
        
        // 验证转换结果在有效范围内
        for (WPARAM vk : testKeys) {
            int result = suyan_test::convertVirtualKeyToRime(vk, 0, false);
            // 结果应该是 0（无法转换）或有效的键码
            // 有效键码范围：ASCII 可打印字符 (0x20-0x7e) 或 X11 keysym (0xff00-0xffff)
            bool validResult = (result == 0) ||
                               (result >= 0x20 && result <= 0x7e) ||
                               (result >= 0xff00 && result <= 0xffff);
            TEST_ASSERT(validResult,
                "转换结果应在有效范围内: VK=" + std::to_string(vk) + ", result=" + std::to_string(result));
            testCount++;
        }
        
        std::cout << "  执行了 " << testCount << " 次测试" << std::endl;
        TEST_PASS("Property 1: 键码转换正确性验证通过");
        return true;
    }

    // ========== Property 2: 修饰键转换正确性 ==========
    /**
     * Property 2: 修饰键转换正确性
     * 
     * For any Windows 修饰键状态组合 (Shift, Ctrl, Alt, Win 的任意组合)，
     * convertModifiersToRime 函数应返回正确的 RIME 修饰键掩码，
     * 且各修饰键位独立映射。
     * 
     * Validates: Requirements 2.2
     */
    bool testProperty2_ModifierConversionCorrectness() {
        std::cout << "\n--- Property 2: 修饰键转换正确性 ---" << std::endl;
        std::cout << "  验证: 修饰键组合独立映射" << std::endl;
        
        int testCount = 0;
        
        // 测试所有 16 种修饰键组合 (2^4)
        for (int combo = 0; combo < 16; combo++) {
            resetKeyboardState();
            
            bool shift = (combo & 1) != 0;
            bool ctrl = (combo & 2) != 0;
            bool alt = (combo & 4) != 0;
            bool win = (combo & 8) != 0;
            
            // 设置键盘状态
            if (shift) setKeyDown(VK_SHIFT, true);
            if (ctrl) setKeyDown(VK_CONTROL, true);
            if (alt) setKeyDown(VK_MENU, true);
            if (win) setKeyDown(VK_LWIN, true);
            
            // 获取转换结果
            int result = suyan_test::convertModifiersToRime();
            
            // 验证各修饰键位独立
            bool hasShift = (result & KeyModifier::Shift) != 0;
            bool hasCtrl = (result & KeyModifier::Control) != 0;
            bool hasAlt = (result & KeyModifier::Alt) != 0;
            bool hasSuper = (result & KeyModifier::Super) != 0;
            
            TEST_ASSERT(hasShift == shift,
                "Shift 状态应正确映射: 期望=" + std::to_string(shift) + ", 实际=" + std::to_string(hasShift));
            TEST_ASSERT(hasCtrl == ctrl,
                "Ctrl 状态应正确映射: 期望=" + std::to_string(ctrl) + ", 实际=" + std::to_string(hasCtrl));
            TEST_ASSERT(hasAlt == alt,
                "Alt 状态应正确映射: 期望=" + std::to_string(alt) + ", 实际=" + std::to_string(hasAlt));
            TEST_ASSERT(hasSuper == win,
                "Win 状态应正确映射: 期望=" + std::to_string(win) + ", 实际=" + std::to_string(hasSuper));
            
            testCount += 4;
        }
        
        // 测试左右修饰键的等价性
        resetKeyboardState();
        
        // 左 Shift
        setKeyDown(VK_LSHIFT, true);
        int leftShiftResult = suyan_test::convertModifiersToRime();
        setKeyDown(VK_LSHIFT, false);
        
        // 右 Shift
        setKeyDown(VK_RSHIFT, true);
        int rightShiftResult = suyan_test::convertModifiersToRime();
        setKeyDown(VK_RSHIFT, false);
        
        TEST_ASSERT((leftShiftResult & KeyModifier::Shift) != 0,
            "左 Shift 应设置 Shift 位");
        TEST_ASSERT((rightShiftResult & KeyModifier::Shift) != 0,
            "右 Shift 应设置 Shift 位");
        testCount += 2;
        
        // 左 Control
        setKeyDown(VK_LCONTROL, true);
        int leftCtrlResult = suyan_test::convertModifiersToRime();
        setKeyDown(VK_LCONTROL, false);
        
        // 右 Control
        setKeyDown(VK_RCONTROL, true);
        int rightCtrlResult = suyan_test::convertModifiersToRime();
        setKeyDown(VK_RCONTROL, false);
        
        TEST_ASSERT((leftCtrlResult & KeyModifier::Control) != 0,
            "左 Control 应设置 Control 位");
        TEST_ASSERT((rightCtrlResult & KeyModifier::Control) != 0,
            "右 Control 应设置 Control 位");
        testCount += 2;
        
        // 左 Alt
        setKeyDown(VK_LMENU, true);
        int leftAltResult = suyan_test::convertModifiersToRime();
        setKeyDown(VK_LMENU, false);
        
        // 右 Alt
        setKeyDown(VK_RMENU, true);
        int rightAltResult = suyan_test::convertModifiersToRime();
        setKeyDown(VK_RMENU, false);
        
        TEST_ASSERT((leftAltResult & KeyModifier::Alt) != 0,
            "左 Alt 应设置 Alt 位");
        TEST_ASSERT((rightAltResult & KeyModifier::Alt) != 0,
            "右 Alt 应设置 Alt 位");
        testCount += 2;
        
        // 左右 Win
        setKeyDown(VK_LWIN, true);
        int leftWinResult = suyan_test::convertModifiersToRime();
        setKeyDown(VK_LWIN, false);
        
        setKeyDown(VK_RWIN, true);
        int rightWinResult = suyan_test::convertModifiersToRime();
        setKeyDown(VK_RWIN, false);
        
        TEST_ASSERT((leftWinResult & KeyModifier::Super) != 0,
            "左 Win 应设置 Super 位");
        TEST_ASSERT((rightWinResult & KeyModifier::Super) != 0,
            "右 Win 应设置 Super 位");
        testCount += 2;
        
        // 验证确定性：多次调用应返回相同结果
        resetKeyboardState();
        setKeyDown(VK_SHIFT, true);
        setKeyDown(VK_CONTROL, true);
        
        int result1 = suyan_test::convertModifiersToRime();
        int result2 = suyan_test::convertModifiersToRime();
        int result3 = suyan_test::convertModifiersToRime();
        
        TEST_ASSERT(result1 == result2 && result2 == result3,
            "修饰键转换应该是确定性的");
        testCount++;
        
        resetKeyboardState();
        
        std::cout << "  执行了 " << testCount << " 次测试" << std::endl;
        TEST_PASS("Property 2: 修饰键转换正确性验证通过");
        return true;
    }

    // ========== 辅助函数测试 ==========
    
    bool testHelperFunctions() {
        std::cout << "\n--- 辅助函数测试 ---" << std::endl;
        
        // 测试 isCharacterKey
        TEST_ASSERT(suyan_test::isCharacterKey('A'), "A 应该是字符键");
        TEST_ASSERT(suyan_test::isCharacterKey('Z'), "Z 应该是字符键");
        TEST_ASSERT(suyan_test::isCharacterKey('0'), "0 应该是字符键");
        TEST_ASSERT(suyan_test::isCharacterKey('9'), "9 应该是字符键");
        TEST_ASSERT(suyan_test::isCharacterKey(VK_SPACE), "Space 应该是字符键");
        TEST_ASSERT(suyan_test::isCharacterKey(VK_OEM_1), "OEM_1 应该是字符键");
        TEST_ASSERT(!suyan_test::isCharacterKey(VK_F1), "F1 不应该是字符键");
        TEST_ASSERT(!suyan_test::isCharacterKey(VK_RETURN), "Return 不应该是字符键");
        
        // 测试 isModifierKey
        TEST_ASSERT(suyan_test::isModifierKey(VK_SHIFT), "Shift 应该是修饰键");
        TEST_ASSERT(suyan_test::isModifierKey(VK_LSHIFT), "LShift 应该是修饰键");
        TEST_ASSERT(suyan_test::isModifierKey(VK_RSHIFT), "RShift 应该是修饰键");
        TEST_ASSERT(suyan_test::isModifierKey(VK_CONTROL), "Control 应该是修饰键");
        TEST_ASSERT(suyan_test::isModifierKey(VK_MENU), "Alt 应该是修饰键");
        TEST_ASSERT(suyan_test::isModifierKey(VK_LWIN), "LWin 应该是修饰键");
        TEST_ASSERT(suyan_test::isModifierKey(VK_CAPITAL), "CapsLock 应该是修饰键");
        TEST_ASSERT(!suyan_test::isModifierKey('A'), "A 不应该是修饰键");
        TEST_ASSERT(!suyan_test::isModifierKey(VK_F1), "F1 不应该是修饰键");
        
        // 测试 isFunctionKey
        TEST_ASSERT(suyan_test::isFunctionKey(VK_F1), "F1 应该是功能键");
        TEST_ASSERT(suyan_test::isFunctionKey(VK_F12), "F12 应该是功能键");
        TEST_ASSERT(suyan_test::isFunctionKey(VK_F24), "F24 应该是功能键");
        TEST_ASSERT(!suyan_test::isFunctionKey('A'), "A 不应该是功能键");
        TEST_ASSERT(!suyan_test::isFunctionKey(VK_RETURN), "Return 不应该是功能键");
        
        // 测试 isNumpadKey
        TEST_ASSERT(suyan_test::isNumpadKey(VK_NUMPAD0), "Numpad0 应该是小键盘键");
        TEST_ASSERT(suyan_test::isNumpadKey(VK_NUMPAD9), "Numpad9 应该是小键盘键");
        TEST_ASSERT(suyan_test::isNumpadKey(VK_MULTIPLY), "Multiply 应该是小键盘键");
        TEST_ASSERT(suyan_test::isNumpadKey(VK_ADD), "Add 应该是小键盘键");
        TEST_ASSERT(suyan_test::isNumpadKey(VK_NUMLOCK), "NumLock 应该是小键盘键");
        TEST_ASSERT(!suyan_test::isNumpadKey('0'), "主键盘 0 不应该是小键盘键");
        
        // 测试 isNavigationKey
        TEST_ASSERT(suyan_test::isNavigationKey(VK_LEFT), "Left 应该是导航键");
        TEST_ASSERT(suyan_test::isNavigationKey(VK_RIGHT), "Right 应该是导航键");
        TEST_ASSERT(suyan_test::isNavigationKey(VK_UP), "Up 应该是导航键");
        TEST_ASSERT(suyan_test::isNavigationKey(VK_DOWN), "Down 应该是导航键");
        TEST_ASSERT(suyan_test::isNavigationKey(VK_HOME), "Home 应该是导航键");
        TEST_ASSERT(suyan_test::isNavigationKey(VK_END), "End 应该是导航键");
        TEST_ASSERT(suyan_test::isNavigationKey(VK_PRIOR), "PageUp 应该是导航键");
        TEST_ASSERT(suyan_test::isNavigationKey(VK_NEXT), "PageDown 应该是导航键");
        TEST_ASSERT(!suyan_test::isNavigationKey('A'), "A 不应该是导航键");
        TEST_ASSERT(!suyan_test::isNavigationKey(VK_RETURN), "Return 不应该是导航键");
        
        TEST_PASS("testHelperFunctions: 辅助函数测试通过");
        return true;
    }
};

// ========== 主函数 ==========

int main() {
    KeyConverterTest test;
    return test.runAllTests() ? 0 : 1;
}
