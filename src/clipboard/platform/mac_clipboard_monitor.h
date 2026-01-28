/**
 * MacClipboardMonitor - macOS 剪贴板监听实现
 *
 * 使用 NSPasteboard 轮询 changeCount 检测剪贴板变化。
 * 支持文本和图片内容的读取与写入。
 *
 * 实现细节：
 * - 使用 QTimer 进行定时轮询（默认 500ms 间隔）
 * - 通过 NSPasteboard changeCount 检测变化
 * - 使用 QCryptographicHash 计算 SHA-256 哈希
 * - 通过 NSWorkspace 获取前台应用信息
 */

#ifndef SUYAN_CLIPBOARD_PLATFORM_MAC_CLIPBOARD_MONITOR_H
#define SUYAN_CLIPBOARD_PLATFORM_MAC_CLIPBOARD_MONITOR_H

#include "../clipboard_monitor.h"
#include <QObject>
#include <QTimer>
#include <memory>

namespace suyan {

/**
 * MacClipboardMonitor - macOS 剪贴板监听器
 *
 * 继承 QObject 以支持 Qt 信号槽机制。
 * 实现 IClipboardMonitor 接口。
 */
class MacClipboardMonitor : public QObject, public IClipboardMonitor {
    Q_OBJECT

public:
    /**
     * 构造函数
     *
     * @param parent Qt 父对象
     */
    explicit MacClipboardMonitor(QObject* parent = nullptr);
    
    /**
     * 析构函数
     */
    ~MacClipboardMonitor() override;

    // ========== IClipboardMonitor 接口实现 ==========
    
    bool start() override;
    void stop() override;
    bool isRunning() const override;
    void setCallback(ClipboardChangedCallback callback) override;
    bool writeToClipboard(const ClipboardContent& content) override;
    ClipboardContent readCurrentContent() override;
    std::string getCurrentFrontApp() override;
    int getPollInterval() const override;
    void setPollInterval(int intervalMs) override;

signals:
    /**
     * 剪贴板内容变化信号
     *
     * 当检测到剪贴板内容变化时发射。
     */
    void clipboardChanged(const ClipboardContent& content);

private slots:
    /**
     * 轮询定时器回调
     */
    void onPollTimer();

private:
    /**
     * 读取剪贴板内容
     *
     * @return 剪贴板内容
     */
    ClipboardContent readClipboard();
    
    /**
     * 计算内容哈希值
     *
     * 使用 SHA-256 算法计算内容的哈希值。
     *
     * @param content 剪贴板内容
     * @return 哈希值（十六进制字符串）
     */
    std::string calculateHash(const ClipboardContent& content);
    
    /**
     * 获取当前剪贴板 changeCount
     *
     * @return changeCount 值
     */
    int getChangeCount();
    
    /**
     * 写入文本到剪贴板
     *
     * @param text 文本内容
     * @return 是否成功
     */
    bool writeTextToClipboard(const std::string& text);
    
    /**
     * 写入图片到剪贴板
     *
     * @param imageData 图片数据
     * @param format 图片格式
     * @return 是否成功
     */
    bool writeImageToClipboard(const std::vector<uint8_t>& imageData, 
                                const std::string& format);
    
    /**
     * 确定图片格式
     *
     * 根据 NSPasteboard 类型确定图片格式。
     *
     * @param pasteboardType NSPasteboard 类型字符串
     * @return 图片格式（png, jpeg, gif, tiff）
     */
    std::string determineImageFormat(const std::string& pasteboardType);

    // 成员变量
    std::unique_ptr<QTimer> pollTimer_;         // 轮询定时器
    ClipboardChangedCallback callback_;          // 内容变化回调
    int lastChangeCount_ = 0;                    // 上次 changeCount
    bool running_ = false;                       // 运行状态
    int pollIntervalMs_ = 500;                   // 轮询间隔（毫秒）
    
    // 默认轮询间隔
    static constexpr int DEFAULT_POLL_INTERVAL_MS = 500;
    // 最小轮询间隔
    static constexpr int MIN_POLL_INTERVAL_MS = 100;
    // 最大轮询间隔
    static constexpr int MAX_POLL_INTERVAL_MS = 5000;
};

} // namespace suyan

#endif // SUYAN_CLIPBOARD_PLATFORM_MAC_CLIPBOARD_MONITOR_H
