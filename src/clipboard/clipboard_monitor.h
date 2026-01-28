/**
 * ClipboardMonitor - 剪贴板监听接口
 *
 * 定义跨平台的剪贴板监听抽象接口。
 * 平台特定实现（macOS/Windows）通过继承此接口实现。
 *
 * 功能：
 * - 剪贴板内容变化监听
 * - 剪贴板内容读取（文本/图片）
 * - 剪贴板内容写入
 * - 前台应用识别
 */

#ifndef SUYAN_CLIPBOARD_CLIPBOARD_MONITOR_H
#define SUYAN_CLIPBOARD_CLIPBOARD_MONITOR_H

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace suyan {

// 前向声明，避免循环依赖
// ClipboardContentType 在 clipboard_store.h 中定义
// 这里重新定义以保持监听模块的独立性
// 注意：两个定义必须保持一致

/**
 * 剪贴板内容类型（监听模块使用）
 * 
 * 注意：此枚举与 clipboard_store.h 中的定义相同，
 * 可以安全地进行类型转换。
 */
enum class MonitorContentType {
    Text = 0,       // 文本
    Image = 1,      // 图片
    Unknown = 2     // 未知/不支持
};

/**
 * 剪贴板内容数据
 */
struct ClipboardContent {
    MonitorContentType type = MonitorContentType::Unknown;  // 内容类型
    std::string textData;                   // 文本内容（type == Text 时有效）
    std::vector<uint8_t> imageData;         // 图片二进制数据（type == Image 时有效）
    std::string imageFormat;                // 图片格式（png, jpeg, gif 等）
    std::string sourceApp;                  // 来源应用标识（Bundle ID）
    std::string contentHash;                // SHA-256 哈希值
    
    /**
     * 检查内容是否有效
     */
    bool isValid() const {
        if (type == MonitorContentType::Text) {
            return !textData.empty();
        } else if (type == MonitorContentType::Image) {
            return !imageData.empty();
        }
        return false;
    }
    
    /**
     * 获取内容大小（字节）
     */
    size_t getSize() const {
        if (type == MonitorContentType::Text) {
            return textData.size();
        } else if (type == MonitorContentType::Image) {
            return imageData.size();
        }
        return 0;
    }
    
    /**
     * 检查是否为文本类型
     */
    bool isText() const {
        return type == MonitorContentType::Text;
    }
    
    /**
     * 检查是否为图片类型
     */
    bool isImage() const {
        return type == MonitorContentType::Image;
    }
};

/**
 * 剪贴板监听回调类型
 */
using ClipboardChangedCallback = std::function<void(const ClipboardContent&)>;

/**
 * IClipboardMonitor - 剪贴板监听接口
 *
 * 抽象基类，定义剪贴板监听的统一接口。
 * 平台特定实现需要继承此接口。
 */
class IClipboardMonitor {
public:
    virtual ~IClipboardMonitor() = default;
    
    /**
     * 启动监听
     *
     * @return 是否成功启动
     */
    virtual bool start() = 0;
    
    /**
     * 停止监听
     */
    virtual void stop() = 0;
    
    /**
     * 检查是否正在监听
     *
     * @return 是否正在运行
     */
    virtual bool isRunning() const = 0;
    
    /**
     * 设置内容变化回调
     *
     * 当剪贴板内容发生变化时，会调用此回调。
     *
     * @param callback 回调函数
     */
    virtual void setCallback(ClipboardChangedCallback callback) = 0;
    
    /**
     * 将内容写入系统剪贴板
     *
     * @param content 要写入的内容
     * @return 是否成功
     */
    virtual bool writeToClipboard(const ClipboardContent& content) = 0;
    
    /**
     * 读取当前剪贴板内容
     *
     * @return 剪贴板内容，如果无法读取则返回 Unknown 类型
     */
    virtual ClipboardContent readCurrentContent() = 0;
    
    /**
     * 获取当前前台应用标识
     *
     * @return 应用标识（macOS 为 Bundle ID，Windows 为进程名）
     */
    virtual std::string getCurrentFrontApp() = 0;
    
    /**
     * 获取监听间隔（毫秒）
     *
     * @return 轮询间隔
     */
    virtual int getPollInterval() const = 0;
    
    /**
     * 设置监听间隔（毫秒）
     *
     * @param intervalMs 轮询间隔
     */
    virtual void setPollInterval(int intervalMs) = 0;
};

/**
 * 创建平台特定的剪贴板监听器
 *
 * 工厂函数，根据当前平台创建对应的监听器实现。
 *
 * @return 监听器实例指针
 */
IClipboardMonitor* createClipboardMonitor();

} // namespace suyan

#endif // SUYAN_CLIPBOARD_CLIPBOARD_MONITOR_H
