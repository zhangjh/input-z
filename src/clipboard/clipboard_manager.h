/**
 * ClipboardManager - 剪贴板管理核心类
 *
 * 整合 ClipboardStore、ImageStorage、IClipboardMonitor，
 * 提供剪贴板历史记录的完整管理功能。
 *
 * 功能：
 * - 剪贴板监听控制
 * - 历史记录管理（添加、查询、删除）
 * - 内容去重（基于 SHA-256 哈希）
 * - 粘贴操作（写入系统剪贴板）
 * - 自动清理（根据保留策略）
 */

#ifndef SUYAN_CLIPBOARD_CLIPBOARD_MANAGER_H
#define SUYAN_CLIPBOARD_CLIPBOARD_MANAGER_H

#include <QObject>
#include <QString>
#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "clipboard_store.h"
#include "clipboard_monitor.h"

namespace suyan {

// 前向声明
class ImageStorage;
class IClipboardMonitor;

/**
 * 文本长度阈值（64KB）
 * 超过此长度的文本不记录
 */
constexpr size_t MAX_TEXT_LENGTH = 65536;

/**
 * ClipboardManager - 剪贴板管理核心类
 *
 * 单例模式，协调剪贴板监听、存储和检索。
 */
class ClipboardManager : public QObject {
    Q_OBJECT

public:
    /**
     * 获取单例实例
     */
    static ClipboardManager& instance();

    // 禁止拷贝和移动
    ClipboardManager(const ClipboardManager&) = delete;
    ClipboardManager& operator=(const ClipboardManager&) = delete;
    ClipboardManager(ClipboardManager&&) = delete;
    ClipboardManager& operator=(ClipboardManager&&) = delete;

    /**
     * 初始化剪贴板管理器
     *
     * 初始化所有组件：ClipboardStore、ImageStorage、IClipboardMonitor。
     *
     * @param dataDir 数据目录路径（如 ~/Library/Application Support/SuYan）
     * @return 是否成功
     */
    bool initialize(const std::string& dataDir);

    /**
     * 关闭剪贴板管理器
     *
     * 停止监听并释放资源。
     */
    void shutdown();

    /**
     * 检查是否已初始化
     */
    bool isInitialized() const { return initialized_; }

    /**
     * 获取数据目录
     */
    std::string getDataDir() const { return dataDir_; }

    // ========== 监听控制 ==========

    /**
     * 启动剪贴板监听
     *
     * @return 是否成功
     */
    bool startMonitoring();

    /**
     * 停止剪贴板监听
     */
    void stopMonitoring();

    /**
     * 检查是否正在监听
     */
    bool isMonitoring() const;

    // ========== 历史记录管理 ==========

    /**
     * 获取历史记录
     *
     * @param limit 最大返回数量
     * @param offset 偏移量（用于分页）
     * @return 记录列表（按最后使用时间降序）
     */
    std::vector<ClipboardRecord> getHistory(int limit = 100, int offset = 0);

    /**
     * 搜索记录
     *
     * 仅搜索文本记录。
     *
     * @param keyword 搜索关键词
     * @param limit 最大返回数量
     * @return 匹配的记录列表
     */
    std::vector<ClipboardRecord> search(const std::string& keyword, int limit = 100);

    /**
     * 粘贴指定记录
     *
     * 将记录内容写入系统剪贴板，并更新最后使用时间。
     *
     * @param recordId 记录 ID
     * @return 是否成功
     */
    bool pasteRecord(int64_t recordId);

    /**
     * 删除记录
     *
     * 同时删除关联的图片文件。
     *
     * @param recordId 记录 ID
     * @return 是否成功
     */
    bool deleteRecord(int64_t recordId);

    /**
     * 清空历史记录
     *
     * 删除所有记录和关联的图片文件。
     *
     * @return 是否成功
     */
    bool clearHistory();

    /**
     * 获取记录总数
     */
    int64_t getRecordCount();

    // ========== 清理管理 ==========

    /**
     * 执行清理
     *
     * 根据保留策略删除过期记录。
     */
    void performCleanup();

    // ========== 配置 ==========

    /**
     * 设置启用状态
     *
     * @param enabled 是否启用
     */
    void setEnabled(bool enabled);

    /**
     * 检查是否启用
     */
    bool isEnabled() const { return enabled_; }

    /**
     * 设置最大保留天数
     *
     * @param days 天数（0 表示不限制）
     */
    void setMaxAgeDays(int days);

    /**
     * 获取最大保留天数
     */
    int getMaxAgeDays() const { return maxAgeDays_; }

    /**
     * 设置最大保留条数
     *
     * @param count 条数（0 表示不限制）
     */
    void setMaxCount(int count);

    /**
     * 获取最大保留条数
     */
    int getMaxCount() const { return maxCount_; }

signals:
    /**
     * 新记录添加信号
     *
     * @param record 新添加的记录
     */
    void recordAdded(const ClipboardRecord& record);

    /**
     * 记录删除信号
     *
     * @param recordId 被删除的记录 ID
     */
    void recordDeleted(int64_t recordId);

    /**
     * 历史清空信号
     */
    void historyCleared();

    /**
     * 监听状态变化信号
     *
     * @param running 是否正在监听
     */
    void monitoringStateChanged(bool running);

    /**
     * 粘贴完成信号
     *
     * 当 pasteRecord() 成功完成后发射此信号。
     * 可用于通知 UI 隐藏窗口。
     *
     * @param recordId 被粘贴的记录 ID
     * @param success 是否成功
     */
    void pasteCompleted(int64_t recordId, bool success);

private:
    ClipboardManager();
    ~ClipboardManager() override;

    /**
     * 处理剪贴板内容变化
     *
     * 由监听器回调调用。
     *
     * @param content 新的剪贴板内容
     */
    void onClipboardChanged(const ClipboardContent& content);

    /**
     * 处理文本内容
     *
     * @param content 剪贴板内容
     * @return 是否成功处理
     */
    bool handleTextContent(const ClipboardContent& content);

    /**
     * 处理图片内容
     *
     * @param content 剪贴板内容
     * @return 是否成功处理
     */
    bool handleImageContent(const ClipboardContent& content);

    /**
     * 删除图片文件
     *
     * @param record 包含图片路径的记录
     */
    void deleteImageFiles(const ClipboardRecord& record);

    // 成员变量
    bool initialized_ = false;
    bool enabled_ = true;
    int maxAgeDays_ = 30;
    int maxCount_ = 1000;
    std::string dataDir_;
    std::string clipboardDir_;
    std::string dbPath_;

    std::unique_ptr<IClipboardMonitor> monitor_;
};

} // namespace suyan

#endif // SUYAN_CLIPBOARD_CLIPBOARD_MANAGER_H
