/**
 * MacClipboardMonitor - macOS 剪贴板监听实现
 *
 * 使用 NSPasteboard 轮询 changeCount 检测剪贴板变化。
 */

#include "mac_clipboard_monitor.h"
#include <QCryptographicHash>
#include <QDebug>

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

namespace suyan {

// ========== 构造与析构 ==========

MacClipboardMonitor::MacClipboardMonitor(QObject* parent)
    : QObject(parent)
    , pollTimer_(std::make_unique<QTimer>(this))
    , pollIntervalMs_(DEFAULT_POLL_INTERVAL_MS)
{
    // 连接定时器信号
    connect(pollTimer_.get(), &QTimer::timeout, this, &MacClipboardMonitor::onPollTimer);
}

MacClipboardMonitor::~MacClipboardMonitor()
{
    stop();
}

// ========== IClipboardMonitor 接口实现 ==========

bool MacClipboardMonitor::start()
{
    if (running_) {
        qDebug() << "MacClipboardMonitor: Already running";
        return true;
    }
    
    // 获取初始 changeCount
    lastChangeCount_ = getChangeCount();
    
    // 启动轮询定时器
    pollTimer_->start(pollIntervalMs_);
    running_ = true;
    
    qDebug() << "MacClipboardMonitor: Started with interval" << pollIntervalMs_ << "ms";
    return true;
}

void MacClipboardMonitor::stop()
{
    if (!running_) {
        return;
    }
    
    pollTimer_->stop();
    running_ = false;
    
    qDebug() << "MacClipboardMonitor: Stopped";
}

bool MacClipboardMonitor::isRunning() const
{
    return running_;
}

void MacClipboardMonitor::setCallback(ClipboardChangedCallback callback)
{
    callback_ = std::move(callback);
}

bool MacClipboardMonitor::writeToClipboard(const ClipboardContent& content)
{
    if (content.type == MonitorContentType::Text) {
        return writeTextToClipboard(content.textData);
    } else if (content.type == MonitorContentType::Image) {
        return writeImageToClipboard(content.imageData, content.imageFormat);
    }
    
    qWarning() << "MacClipboardMonitor: Cannot write unknown content type";
    return false;
}

ClipboardContent MacClipboardMonitor::readCurrentContent()
{
    return readClipboard();
}

std::string MacClipboardMonitor::getCurrentFrontApp()
{
    @autoreleasepool {
        NSRunningApplication* frontApp = [[NSWorkspace sharedWorkspace] frontmostApplication];
        if (frontApp) {
            NSString* bundleId = [frontApp bundleIdentifier];
            if (bundleId) {
                return [bundleId UTF8String];
            }
        }
    }
    return "";
}

int MacClipboardMonitor::getPollInterval() const
{
    return pollIntervalMs_;
}

void MacClipboardMonitor::setPollInterval(int intervalMs)
{
    // 限制在合理范围内
    pollIntervalMs_ = std::max(MIN_POLL_INTERVAL_MS, 
                               std::min(intervalMs, MAX_POLL_INTERVAL_MS));
    
    // 如果正在运行，重启定时器
    if (running_) {
        pollTimer_->stop();
        pollTimer_->start(pollIntervalMs_);
    }
    
    qDebug() << "MacClipboardMonitor: Poll interval set to" << pollIntervalMs_ << "ms";
}

// ========== 私有方法 ==========

void MacClipboardMonitor::onPollTimer()
{
    int currentChangeCount = getChangeCount();
    
    if (currentChangeCount != lastChangeCount_) {
        lastChangeCount_ = currentChangeCount;
        
        // 读取剪贴板内容
        ClipboardContent content = readClipboard();
        
        if (content.isValid()) {
            // 计算哈希
            content.contentHash = calculateHash(content);
            
            // 获取来源应用
            content.sourceApp = getCurrentFrontApp();
            
            // 触发回调
            if (callback_) {
                callback_(content);
            }
            
            // 发射信号
            emit clipboardChanged(content);
        }
    }
}

ClipboardContent MacClipboardMonitor::readClipboard()
{
    ClipboardContent content;
    content.type = MonitorContentType::Unknown;
    
    @autoreleasepool {
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        
        // 支持的图片类型（按优先级排序）
        NSArray* imageTypes = @[
            NSPasteboardTypePNG,
            NSPasteboardTypeTIFF,
            @"public.jpeg",
            @"com.compuserve.gif"
        ];
        
        // 优先检查图片
        for (NSString* type in imageTypes) {
            NSData* data = [pasteboard dataForType:type];
            if (data && data.length > 0) {
                content.type = MonitorContentType::Image;
                content.imageData = std::vector<uint8_t>(
                    static_cast<const uint8_t*>(data.bytes),
                    static_cast<const uint8_t*>(data.bytes) + data.length
                );
                content.imageFormat = determineImageFormat([type UTF8String]);
                return content;
            }
        }
        
        // 检查文本
        NSString* text = [pasteboard stringForType:NSPasteboardTypeString];
        if (text && text.length > 0) {
            content.type = MonitorContentType::Text;
            content.textData = [text UTF8String];
            return content;
        }
    }
    
    return content;
}

std::string MacClipboardMonitor::calculateHash(const ClipboardContent& content)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    
    if (content.type == MonitorContentType::Text) {
        hash.addData(QByteArray::fromRawData(content.textData.data(), 
                                              static_cast<int>(content.textData.size())));
    } else if (content.type == MonitorContentType::Image) {
        hash.addData(QByteArray::fromRawData(
            reinterpret_cast<const char*>(content.imageData.data()),
            static_cast<int>(content.imageData.size())
        ));
    }
    
    return hash.result().toHex().toStdString();
}

int MacClipboardMonitor::getChangeCount()
{
    @autoreleasepool {
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        return static_cast<int>([pasteboard changeCount]);
    }
}

bool MacClipboardMonitor::writeTextToClipboard(const std::string& text)
{
    @autoreleasepool {
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        
        NSString* nsText = [NSString stringWithUTF8String:text.c_str()];
        if (!nsText) {
            qWarning() << "MacClipboardMonitor: Failed to convert text to NSString";
            return false;
        }
        
        BOOL success = [pasteboard setString:nsText forType:NSPasteboardTypeString];
        
        if (success) {
            // 更新 changeCount 以避免触发自己的回调
            lastChangeCount_ = static_cast<int>([pasteboard changeCount]);
            qDebug() << "MacClipboardMonitor: Text written to clipboard";
        } else {
            qWarning() << "MacClipboardMonitor: Failed to write text to clipboard";
        }
        
        return success == YES;
    }
}

bool MacClipboardMonitor::writeImageToClipboard(const std::vector<uint8_t>& imageData,
                                                  const std::string& format)
{
    @autoreleasepool {
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        
        NSData* data = [NSData dataWithBytes:imageData.data() 
                                      length:imageData.size()];
        if (!data) {
            qWarning() << "MacClipboardMonitor: Failed to create NSData from image";
            return false;
        }
        
        // 确定 pasteboard 类型
        NSString* pasteboardType = NSPasteboardTypePNG;  // 默认 PNG
        
        if (format == "jpeg" || format == "jpg") {
            pasteboardType = @"public.jpeg";
        } else if (format == "gif") {
            pasteboardType = @"com.compuserve.gif";
        } else if (format == "tiff" || format == "tif") {
            pasteboardType = NSPasteboardTypeTIFF;
        }
        
        BOOL success = [pasteboard setData:data forType:pasteboardType];
        
        if (success) {
            // 更新 changeCount 以避免触发自己的回调
            lastChangeCount_ = static_cast<int>([pasteboard changeCount]);
            qDebug() << "MacClipboardMonitor: Image written to clipboard";
        } else {
            qWarning() << "MacClipboardMonitor: Failed to write image to clipboard";
        }
        
        return success == YES;
    }
}

std::string MacClipboardMonitor::determineImageFormat(const std::string& pasteboardType)
{
    if (pasteboardType == "public.png" || 
        pasteboardType == [NSPasteboardTypePNG UTF8String]) {
        return "png";
    } else if (pasteboardType == "public.jpeg") {
        return "jpeg";
    } else if (pasteboardType == "com.compuserve.gif") {
        return "gif";
    } else if (pasteboardType == "public.tiff" || 
               pasteboardType == [NSPasteboardTypeTIFF UTF8String]) {
        return "tiff";
    }
    
    // 默认返回 png
    return "png";
}

// ========== 工厂函数 ==========

IClipboardMonitor* createClipboardMonitor()
{
    return new MacClipboardMonitor();
}

} // namespace suyan
