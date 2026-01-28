/**
 * HotkeyManager - 全局快捷键管理器实现
 */

#include "hotkey_manager.h"
#include <QDebug>
#include <algorithm>
#include <sstream>
#include <cctype>

namespace suyan {

// ============================================================================
// Hotkey 结构体实现
// ============================================================================

std::string Hotkey::toString() const {
    if (!isValid()) {
        return "";
    }
    
    std::string result;
    
#ifdef Q_OS_MAC
    // macOS 使用 Cmd 而不是 Ctrl
    if (ctrl) {
        result += "Cmd+";
    }
    if (meta) {
        result += "Ctrl+";  // macOS 上 meta 是 Ctrl 键
    }
#else
    if (ctrl) {
        result += "Ctrl+";
    }
    if (meta) {
        result += "Win+";
    }
#endif
    
    if (shift) {
        result += "Shift+";
    }
    if (alt) {
        result += "Alt+";
    }
    
    // 转换键码为字符
    // 常见键码映射
    switch (keyCode) {
        // 字母键 A-Z
        case 0x00: result += "A"; break;
        case 0x0B: result += "B"; break;
        case 0x08: result += "C"; break;
        case 0x02: result += "D"; break;
        case 0x0E: result += "E"; break;
        case 0x03: result += "F"; break;
        case 0x05: result += "G"; break;
        case 0x04: result += "H"; break;
        case 0x22: result += "I"; break;
        case 0x26: result += "J"; break;
        case 0x28: result += "K"; break;
        case 0x25: result += "L"; break;
        case 0x2E: result += "M"; break;
        case 0x2D: result += "N"; break;
        case 0x1F: result += "O"; break;
        case 0x23: result += "P"; break;
        case 0x0C: result += "Q"; break;
        case 0x0F: result += "R"; break;
        case 0x01: result += "S"; break;
        case 0x11: result += "T"; break;
        case 0x20: result += "U"; break;
        case 0x09: result += "V"; break;
        case 0x0D: result += "W"; break;
        case 0x07: result += "X"; break;
        case 0x10: result += "Y"; break;
        case 0x06: result += "Z"; break;
        
        // 数字键 0-9
        case 0x1D: result += "0"; break;
        case 0x12: result += "1"; break;
        case 0x13: result += "2"; break;
        case 0x14: result += "3"; break;
        case 0x15: result += "4"; break;
        case 0x17: result += "5"; break;
        case 0x16: result += "6"; break;
        case 0x1A: result += "7"; break;
        case 0x1C: result += "8"; break;
        case 0x19: result += "9"; break;
        
        // 功能键
        case 0x7A: result += "F1"; break;
        case 0x78: result += "F2"; break;
        case 0x63: result += "F3"; break;
        case 0x76: result += "F4"; break;
        case 0x60: result += "F5"; break;
        case 0x61: result += "F6"; break;
        case 0x62: result += "F7"; break;
        case 0x64: result += "F8"; break;
        case 0x65: result += "F9"; break;
        case 0x6D: result += "F10"; break;
        case 0x67: result += "F11"; break;
        case 0x6F: result += "F12"; break;
        
        // 特殊键
        case 0x24: result += "Return"; break;
        case 0x30: result += "Tab"; break;
        case 0x31: result += "Space"; break;
        case 0x33: result += "Delete"; break;
        case 0x35: result += "Escape"; break;
        case 0x75: result += "ForwardDelete"; break;
        case 0x73: result += "Home"; break;
        case 0x77: result += "End"; break;
        case 0x74: result += "PageUp"; break;
        case 0x79: result += "PageDown"; break;
        case 0x7B: result += "Left"; break;
        case 0x7C: result += "Right"; break;
        case 0x7D: result += "Down"; break;
        case 0x7E: result += "Up"; break;
        
        default:
            // 未知键码，使用十六进制表示
            {
                std::ostringstream oss;
                oss << "0x" << std::hex << keyCode;
                result += oss.str();
            }
            break;
    }
    
    return result;
}

Hotkey Hotkey::fromString(const std::string& str) {
    Hotkey hotkey;
    
    if (str.empty()) {
        return hotkey;
    }
    
    // 转换为大写并分割
    std::string upper = str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    
    std::vector<std::string> parts;
    std::istringstream iss(upper);
    std::string part;
    while (std::getline(iss, part, '+')) {
        // 去除首尾空格
        size_t start = part.find_first_not_of(" \t");
        size_t end = part.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos) {
            parts.push_back(part.substr(start, end - start + 1));
        }
    }
    
    if (parts.empty()) {
        return hotkey;
    }
    
    // 解析修饰键和主键
    for (size_t i = 0; i < parts.size(); ++i) {
        const std::string& p = parts[i];
        
        if (p == "CMD" || p == "COMMAND") {
            hotkey.ctrl = true;  // macOS 上 Cmd 映射到 ctrl
        } else if (p == "CTRL" || p == "CONTROL") {
#ifdef Q_OS_MAC
            hotkey.meta = true;  // macOS 上 Ctrl 映射到 meta
#else
            hotkey.ctrl = true;
#endif
        } else if (p == "SHIFT") {
            hotkey.shift = true;
        } else if (p == "ALT" || p == "OPTION" || p == "OPT") {
            hotkey.alt = true;
        } else if (p == "META" || p == "WIN" || p == "SUPER") {
            hotkey.meta = true;
        } else {
            // 主键
            // 字母键 A-Z（macOS 键码）
            if (p.length() == 1 && p[0] >= 'A' && p[0] <= 'Z') {
                // macOS 键码映射
                static const int keyMap[26] = {
                    0x00, // A
                    0x0B, // B
                    0x08, // C
                    0x02, // D
                    0x0E, // E
                    0x03, // F
                    0x05, // G
                    0x04, // H
                    0x22, // I
                    0x26, // J
                    0x28, // K
                    0x25, // L
                    0x2E, // M
                    0x2D, // N
                    0x1F, // O
                    0x23, // P
                    0x0C, // Q
                    0x0F, // R
                    0x01, // S
                    0x11, // T
                    0x20, // U
                    0x09, // V
                    0x0D, // W
                    0x07, // X
                    0x10, // Y
                    0x06  // Z
                };
                hotkey.keyCode = keyMap[p[0] - 'A'];
            }
            // 数字键 0-9
            else if (p.length() == 1 && p[0] >= '0' && p[0] <= '9') {
                static const int numKeyMap[10] = {
                    0x1D, // 0
                    0x12, // 1
                    0x13, // 2
                    0x14, // 3
                    0x15, // 4
                    0x17, // 5
                    0x16, // 6
                    0x1A, // 7
                    0x1C, // 8
                    0x19  // 9
                };
                hotkey.keyCode = numKeyMap[p[0] - '0'];
            }
            // 功能键 F1-F12
            else if (p[0] == 'F' && p.length() >= 2) {
                int fNum = std::stoi(p.substr(1));
                static const int fKeyMap[12] = {
                    0x7A, 0x78, 0x63, 0x76, 0x60, 0x61,
                    0x62, 0x64, 0x65, 0x6D, 0x67, 0x6F
                };
                if (fNum >= 1 && fNum <= 12) {
                    hotkey.keyCode = fKeyMap[fNum - 1];
                }
            }
            // 特殊键
            else if (p == "RETURN" || p == "ENTER") {
                hotkey.keyCode = 0x24;
            } else if (p == "TAB") {
                hotkey.keyCode = 0x30;
            } else if (p == "SPACE") {
                hotkey.keyCode = 0x31;
            } else if (p == "DELETE" || p == "BACKSPACE") {
                hotkey.keyCode = 0x33;
            } else if (p == "ESCAPE" || p == "ESC") {
                hotkey.keyCode = 0x35;
            } else if (p == "HOME") {
                hotkey.keyCode = 0x73;
            } else if (p == "END") {
                hotkey.keyCode = 0x77;
            } else if (p == "PAGEUP") {
                hotkey.keyCode = 0x74;
            } else if (p == "PAGEDOWN") {
                hotkey.keyCode = 0x79;
            } else if (p == "LEFT") {
                hotkey.keyCode = 0x7B;
            } else if (p == "RIGHT") {
                hotkey.keyCode = 0x7C;
            } else if (p == "DOWN") {
                hotkey.keyCode = 0x7D;
            } else if (p == "UP") {
                hotkey.keyCode = 0x7E;
            }
            // 十六进制键码
            else if (p.length() > 2 && p[0] == '0' && (p[1] == 'X' || p[1] == 'x')) {
                hotkey.keyCode = std::stoi(p.substr(2), nullptr, 16);
            }
        }
    }
    
    return hotkey;
}

bool Hotkey::isValid() const {
    // 必须有主键
    if (keyCode == 0) {
        return false;
    }
    // 必须至少有一个修饰键
    return ctrl || shift || alt || meta;
}

bool Hotkey::operator==(const Hotkey& other) const {
    return keyCode == other.keyCode &&
           ctrl == other.ctrl &&
           shift == other.shift &&
           alt == other.alt &&
           meta == other.meta;
}

bool Hotkey::operator!=(const Hotkey& other) const {
    return !(*this == other);
}

// ============================================================================
// HotkeyManager 实现
// ============================================================================

HotkeyManager& HotkeyManager::instance() {
    static HotkeyManager instance;
    return instance;
}

HotkeyManager::HotkeyManager() = default;

HotkeyManager::~HotkeyManager() {
    shutdown();
}

bool HotkeyManager::initialize() {
    if (initialized_) {
        qDebug() << "HotkeyManager: Already initialized";
        return true;
    }
    
    // 创建平台特定 handler
    handler_ = createHotkeyHandler();
    if (!handler_) {
        qWarning() << "HotkeyManager: Failed to create platform handler";
        return false;
    }
    
    // 初始化 handler
    if (!handler_->initialize(this)) {
        qWarning() << "HotkeyManager: Failed to initialize platform handler";
        handler_.reset();
        return false;
    }
    
    initialized_ = true;
    qDebug() << "HotkeyManager: Initialized successfully";
    return true;
}

void HotkeyManager::shutdown() {
    if (!initialized_) {
        return;
    }
    
    // 注销所有快捷键
    unregisterAllHotkeys();
    
    // 关闭 handler
    if (handler_) {
        handler_->shutdown();
        handler_.reset();
    }
    
    initialized_ = false;
    qDebug() << "HotkeyManager: Shutdown complete";
}

bool HotkeyManager::isInitialized() const {
    return initialized_;
}

bool HotkeyManager::registerHotkey(const std::string& name, const Hotkey& hotkey) {
    if (!initialized_) {
        qWarning() << "HotkeyManager: Not initialized";
        return false;
    }
    
    if (name.empty()) {
        qWarning() << "HotkeyManager: Empty hotkey name";
        return false;
    }
    
    if (!hotkey.isValid()) {
        qWarning() << "HotkeyManager: Invalid hotkey";
        return false;
    }
    
    // 检查是否已存在
    if (hotkeys_.find(name) != hotkeys_.end()) {
        qWarning() << "HotkeyManager: Hotkey already registered:" << QString::fromStdString(name);
        return false;
    }
    
    // 生成 ID
    int hotkeyId = generateHotkeyId();
    
    // 注册到平台 handler
    if (!handler_->registerHotkey(hotkey, hotkeyId)) {
        qWarning() << "HotkeyManager: Failed to register hotkey with platform handler";
        return false;
    }
    
    // 保存映射
    hotkeys_[name] = hotkey;
    hotkeyIds_[name] = hotkeyId;
    idToName_[hotkeyId] = name;
    
    qDebug() << "HotkeyManager: Registered hotkey" << QString::fromStdString(name)
             << "=" << QString::fromStdString(hotkey.toString());
    
    emit hotkeyRegistered(QString::fromStdString(name));
    return true;
}

void HotkeyManager::unregisterHotkey(const std::string& name) {
    if (!initialized_) {
        return;
    }
    
    auto it = hotkeyIds_.find(name);
    if (it == hotkeyIds_.end()) {
        return;
    }
    
    int hotkeyId = it->second;
    
    // 从平台 handler 注销
    handler_->unregisterHotkey(hotkeyId);
    
    // 移除映射
    hotkeys_.erase(name);
    hotkeyIds_.erase(name);
    idToName_.erase(hotkeyId);
    
    qDebug() << "HotkeyManager: Unregistered hotkey" << QString::fromStdString(name);
    
    emit hotkeyUnregistered(QString::fromStdString(name));
}

void HotkeyManager::unregisterAllHotkeys() {
    if (!initialized_) {
        return;
    }
    
    // 复制名称列表（因为 unregisterHotkey 会修改 map）
    std::vector<std::string> names;
    for (const auto& pair : hotkeys_) {
        names.push_back(pair.first);
    }
    
    for (const auto& name : names) {
        unregisterHotkey(name);
    }
}

bool HotkeyManager::updateHotkey(const std::string& name, const Hotkey& hotkey) {
    if (!initialized_) {
        qWarning() << "HotkeyManager: Not initialized";
        return false;
    }
    
    if (!hotkey.isValid()) {
        qWarning() << "HotkeyManager: Invalid hotkey";
        return false;
    }
    
    // 检查是否存在
    auto it = hotkeys_.find(name);
    if (it == hotkeys_.end()) {
        // 不存在则直接注册
        return registerHotkey(name, hotkey);
    }
    
    // 如果快捷键相同，无需更新
    if (it->second == hotkey) {
        return true;
    }
    
    // 先注销旧的
    int oldId = hotkeyIds_[name];
    handler_->unregisterHotkey(oldId);
    
    // 注册新的（使用相同 ID）
    if (!handler_->registerHotkey(hotkey, oldId)) {
        // 注册失败，尝试恢复旧的
        handler_->registerHotkey(it->second, oldId);
        qWarning() << "HotkeyManager: Failed to update hotkey";
        return false;
    }
    
    // 更新映射
    hotkeys_[name] = hotkey;
    
    qDebug() << "HotkeyManager: Updated hotkey" << QString::fromStdString(name)
             << "=" << QString::fromStdString(hotkey.toString());
    
    return true;
}

std::optional<Hotkey> HotkeyManager::getHotkey(const std::string& name) const {
    auto it = hotkeys_.find(name);
    if (it != hotkeys_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool HotkeyManager::hasHotkey(const std::string& name) const {
    return hotkeys_.find(name) != hotkeys_.end();
}

bool HotkeyManager::isHotkeyAvailable(const Hotkey& hotkey) const {
    if (!initialized_ || !handler_) {
        return false;
    }
    
    // 检查是否与已注册的快捷键冲突
    for (const auto& pair : hotkeys_) {
        if (pair.second == hotkey) {
            return false;
        }
    }
    
    // 检查平台级别的可用性
    return handler_->isHotkeyAvailable(hotkey);
}

const std::map<std::string, Hotkey>& HotkeyManager::getAllHotkeys() const {
    return hotkeys_;
}

void HotkeyManager::onHotkeyTriggered(int hotkeyId) {
    auto it = idToName_.find(hotkeyId);
    if (it != idToName_.end()) {
        qDebug() << "HotkeyManager: Hotkey triggered:" << QString::fromStdString(it->second);
        emit hotkeyTriggered(QString::fromStdString(it->second));
    }
}

int HotkeyManager::generateHotkeyId() {
    return nextHotkeyId_++;
}

} // namespace suyan
