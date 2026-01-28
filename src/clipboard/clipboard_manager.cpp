/**
 * ClipboardManager 实现
 *
 * 整合剪贴板监听、存储和检索功能。
 */

#include "clipboard_manager.h"
#include "clipboard_store.h"
#include "image_storage.h"
#include "clipboard_monitor.h"

#include <QDebug>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace suyan {

// ========== 单例实现 ==========

ClipboardManager& ClipboardManager::instance() {
    static ClipboardManager instance;
    return instance;
}

ClipboardManager::ClipboardManager()
    : QObject(nullptr)
{
}

ClipboardManager::~ClipboardManager() {
    shutdown();
}

// ========== 初始化和关闭 ==========

bool ClipboardManager::initialize(const std::string& dataDir) {
    if (initialized_) {
        if (dataDir_ == dataDir) {
            return true;
        }
        shutdown();
    }

    dataDir_ = dataDir;
    clipboardDir_ = dataDir + "/clipboard";
    dbPath_ = dataDir + "/clipboard.db";

    // 创建目录
    try {
        fs::create_directories(clipboardDir_);
    } catch (const std::exception& e) {
        qWarning() << "ClipboardManager: 创建目录失败:" << e.what();
        return false;
    }

    // 初始化 ClipboardStore
    if (!ClipboardStore::instance().initialize(dbPath_)) {
        qWarning() << "ClipboardManager: 初始化 ClipboardStore 失败";
        return false;
    }

    // 初始化 ImageStorage
    if (!ImageStorage::instance().initialize(clipboardDir_)) {
        qWarning() << "ClipboardManager: 初始化 ImageStorage 失败";
        ClipboardStore::instance().shutdown();
        return false;
    }

    // 创建剪贴板监听器
    monitor_.reset(createClipboardMonitor());
    if (!monitor_) {
        qWarning() << "ClipboardManager: 创建剪贴板监听器失败";
        ImageStorage::instance().shutdown();
        ClipboardStore::instance().shutdown();
        return false;
    }

    // 设置监听回调
    monitor_->setCallback([this](const ClipboardContent& content) {
        onClipboardChanged(content);
    });

    initialized_ = true;
    qDebug() << "ClipboardManager: 初始化成功";
    qDebug() << "  数据目录:" << QString::fromStdString(dataDir_);
    qDebug() << "  数据库路径:" << QString::fromStdString(dbPath_);
    qDebug() << "  剪贴板目录:" << QString::fromStdString(clipboardDir_);

    return true;
}

void ClipboardManager::shutdown() {
    if (!initialized_) {
        return;
    }

    // 停止监听
    stopMonitoring();

    // 释放监听器
    monitor_.reset();

    // 关闭存储
    ImageStorage::instance().shutdown();
    ClipboardStore::instance().shutdown();

    initialized_ = false;
    qDebug() << "ClipboardManager: 已关闭";
}

// ========== 监听控制 ==========

bool ClipboardManager::startMonitoring() {
    if (!initialized_) {
        qWarning() << "ClipboardManager: 未初始化，无法启动监听";
        return false;
    }

    if (!enabled_) {
        qDebug() << "ClipboardManager: 功能已禁用，不启动监听";
        return false;
    }

    if (monitor_->isRunning()) {
        return true;
    }

    bool success = monitor_->start();
    if (success) {
        qDebug() << "ClipboardManager: 监听已启动";
        emit monitoringStateChanged(true);
    } else {
        qWarning() << "ClipboardManager: 启动监听失败";
    }

    return success;
}

void ClipboardManager::stopMonitoring() {
    if (!initialized_ || !monitor_) {
        return;
    }

    if (!monitor_->isRunning()) {
        return;
    }

    monitor_->stop();
    qDebug() << "ClipboardManager: 监听已停止";
    emit monitoringStateChanged(false);
}

bool ClipboardManager::isMonitoring() const {
    if (!initialized_ || !monitor_) {
        return false;
    }
    return monitor_->isRunning();
}

// ========== 历史记录管理 ==========

std::vector<ClipboardRecord> ClipboardManager::getHistory(int limit, int offset) {
    if (!initialized_) {
        return {};
    }
    return ClipboardStore::instance().getAllRecords(limit, offset);
}

std::vector<ClipboardRecord> ClipboardManager::search(const std::string& keyword, int limit) {
    if (!initialized_) {
        return {};
    }
    return ClipboardStore::instance().searchText(keyword, limit);
}

bool ClipboardManager::pasteRecord(int64_t recordId) {
    if (!initialized_) {
        qWarning() << "ClipboardManager: 未初始化，无法粘贴";
        emit pasteCompleted(recordId, false);
        return false;
    }

    // 获取记录
    auto record = ClipboardStore::instance().getRecord(recordId);
    if (!record) {
        qWarning() << "ClipboardManager: 记录不存在:" << recordId;
        emit pasteCompleted(recordId, false);
        return false;
    }

    // 构建剪贴板内容
    ClipboardContent content;

    if (record->type == ClipboardContentType::Text) {
        content.type = MonitorContentType::Text;
        content.textData = record->content;
    } else if (record->type == ClipboardContentType::Image) {
        content.type = MonitorContentType::Image;
        // 从文件加载图片数据
        content.imageData = ImageStorage::instance().loadImage(record->content);
        if (content.imageData.empty()) {
            qWarning() << "ClipboardManager: 加载图片失败:" 
                       << QString::fromStdString(record->content);
            emit pasteCompleted(recordId, false);
            return false;
        }
        content.imageFormat = record->imageFormat;
    } else {
        qWarning() << "ClipboardManager: 不支持的内容类型";
        emit pasteCompleted(recordId, false);
        return false;
    }

    // 写入系统剪贴板
    if (!monitor_->writeToClipboard(content)) {
        qWarning() << "ClipboardManager: 写入剪贴板失败";
        emit pasteCompleted(recordId, false);
        return false;
    }

    // 更新最后使用时间
    ClipboardStore::instance().updateLastUsedTime(recordId);

    qDebug() << "ClipboardManager: 粘贴成功，记录 ID:" << recordId;
    
    // 发射粘贴完成信号
    emit pasteCompleted(recordId, true);
    
    return true;
}

bool ClipboardManager::deleteRecord(int64_t recordId) {
    if (!initialized_) {
        return false;
    }

    // 获取记录（用于删除图片文件）
    auto record = ClipboardStore::instance().getRecord(recordId);
    if (!record) {
        return false;
    }

    // 删除图片文件
    if (record->type == ClipboardContentType::Image) {
        deleteImageFiles(*record);
    }

    // 删除数据库记录
    bool success = ClipboardStore::instance().deleteRecord(recordId);
    if (success) {
        emit recordDeleted(recordId);
        qDebug() << "ClipboardManager: 删除记录成功:" << recordId;
    }

    return success;
}

bool ClipboardManager::clearHistory() {
    if (!initialized_) {
        return false;
    }

    // 获取所有记录（用于删除图片文件）
    auto records = ClipboardStore::instance().clearAll();

    // 删除所有图片文件
    for (const auto& record : records) {
        if (record.type == ClipboardContentType::Image) {
            deleteImageFiles(record);
        }
    }

    emit historyCleared();
    qDebug() << "ClipboardManager: 历史记录已清空，删除" << records.size() << "条记录";

    return true;
}

int64_t ClipboardManager::getRecordCount() {
    if (!initialized_) {
        return 0;
    }
    return ClipboardStore::instance().getRecordCount();
}

// ========== 清理管理 ==========

void ClipboardManager::performCleanup() {
    if (!initialized_) {
        return;
    }

    qDebug() << "ClipboardManager: 执行清理，maxAgeDays=" << maxAgeDays_ 
             << ", maxCount=" << maxCount_;

    // 删除过期记录
    auto deletedRecords = ClipboardStore::instance().deleteExpiredRecords(maxAgeDays_, maxCount_);

    // 删除关联的图片文件
    for (const auto& record : deletedRecords) {
        if (record.type == ClipboardContentType::Image) {
            deleteImageFiles(record);
        }
    }

    if (!deletedRecords.empty()) {
        qDebug() << "ClipboardManager: 清理完成，删除" << deletedRecords.size() << "条记录";
    }
}

// ========== 配置 ==========

void ClipboardManager::setEnabled(bool enabled) {
    if (enabled_ == enabled) {
        return;
    }

    enabled_ = enabled;

    if (initialized_) {
        if (enabled_) {
            startMonitoring();
        } else {
            stopMonitoring();
        }
    }

    qDebug() << "ClipboardManager: 功能" << (enabled_ ? "已启用" : "已禁用");
}

void ClipboardManager::setMaxAgeDays(int days) {
    maxAgeDays_ = std::max(0, days);
    qDebug() << "ClipboardManager: 最大保留天数设置为" << maxAgeDays_;
}

void ClipboardManager::setMaxCount(int count) {
    maxCount_ = std::max(0, count);
    qDebug() << "ClipboardManager: 最大保留条数设置为" << maxCount_;
}

// ========== 私有方法 ==========

void ClipboardManager::onClipboardChanged(const ClipboardContent& content) {
    if (!initialized_ || !enabled_) {
        return;
    }

    if (!content.isValid()) {
        return;
    }

    bool success = false;

    if (content.isText()) {
        success = handleTextContent(content);
    } else if (content.isImage()) {
        success = handleImageContent(content);
    }

    if (!success) {
        qDebug() << "ClipboardManager: 处理剪贴板内容失败";
    }
}

bool ClipboardManager::handleTextContent(const ClipboardContent& content) {
    // 检查文本长度是否超过阈值
    if (content.textData.size() > MAX_TEXT_LENGTH) {
        qDebug() << "ClipboardManager: 文本长度超过阈值，忽略"
                 << "(" << content.textData.size() << " > " << MAX_TEXT_LENGTH << ")";
        return false;
    }

    // 检查是否为空或仅包含空白字符
    bool isEmpty = true;
    for (char c : content.textData) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            isEmpty = false;
            break;
        }
    }
    if (isEmpty) {
        qDebug() << "ClipboardManager: 文本为空或仅包含空白，忽略";
        return false;
    }

    // 创建记录
    ClipboardRecord record;
    record.type = ClipboardContentType::Text;
    record.content = content.textData;
    record.contentHash = content.contentHash;
    record.sourceApp = content.sourceApp;

    // 添加到存储（ClipboardStore 会处理去重）
    auto result = ClipboardStore::instance().addRecord(record);
    if (result.id <= 0) {
        qWarning() << "ClipboardManager: 添加文本记录失败";
        return false;
    }

    // 只有新记录才发射 recordAdded 信号
    if (result.isNew) {
        // 获取完整记录（包含时间戳等）
        auto fullRecord = ClipboardStore::instance().getRecord(result.id);
        if (fullRecord) {
            emit recordAdded(*fullRecord);
            qDebug() << "ClipboardManager: 添加文本记录成功，ID:" << result.id
                     << ", 长度:" << content.textData.size();
        }
    } else {
        qDebug() << "ClipboardManager: 文本记录已存在，更新时间戳，ID:" << result.id;
    }

    return true;
}

bool ClipboardManager::handleImageContent(const ClipboardContent& content) {
    // 先检查是否已存在相同哈希的记录（避免重复保存图片）
    auto existing = ClipboardStore::instance().findByHash(content.contentHash);
    if (existing) {
        // 更新时间戳
        ClipboardStore::instance().updateLastUsedTime(existing->id);
        qDebug() << "ClipboardManager: 图片记录已存在，更新时间戳，ID:" << existing->id;
        return true;
    }

    // 保存图片
    auto result = ImageStorage::instance().saveImage(
        content.imageData,
        content.imageFormat,
        content.contentHash
    );

    if (!result.success) {
        qWarning() << "ClipboardManager: 保存图片失败:" 
                   << QString::fromStdString(result.errorMessage);
        return false;
    }

    // 创建记录
    ClipboardRecord record;
    record.type = ClipboardContentType::Image;
    record.content = result.imagePath;
    record.contentHash = content.contentHash;
    record.sourceApp = content.sourceApp;
    record.thumbnailPath = result.thumbnailPath;
    record.imageFormat = content.imageFormat;
    record.imageWidth = result.width;
    record.imageHeight = result.height;
    record.fileSize = result.fileSize;

    // 添加到存储
    auto addResult = ClipboardStore::instance().addRecord(record);
    if (addResult.id <= 0) {
        qWarning() << "ClipboardManager: 添加图片记录失败";
        // 删除已保存的图片文件
        ImageStorage::instance().deleteImage(result.imagePath, result.thumbnailPath);
        return false;
    }

    // 只有新记录才发射 recordAdded 信号
    if (addResult.isNew) {
        // 获取完整记录
        auto fullRecord = ClipboardStore::instance().getRecord(addResult.id);
        if (fullRecord) {
            emit recordAdded(*fullRecord);
            qDebug() << "ClipboardManager: 添加图片记录成功，ID:" << addResult.id
                     << ", 尺寸:" << result.width << "x" << result.height;
        }
    }

    return true;
}

void ClipboardManager::deleteImageFiles(const ClipboardRecord& record) {
    if (record.type != ClipboardContentType::Image) {
        return;
    }

    ImageStorage::instance().deleteImage(record.content, record.thumbnailPath);
    qDebug() << "ClipboardManager: 删除图片文件:" 
             << QString::fromStdString(record.content);
}

} // namespace suyan
