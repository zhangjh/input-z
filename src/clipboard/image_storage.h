/**
 * ImageStorage - 图片文件存储层
 *
 * 负责剪贴板图片的文件系统存储管理。
 * 支持原图存储和缩略图生成。
 *
 * 功能：
 * - 图片文件存储（以哈希值命名）
 * - 缩略图自动生成（120x80）
 * - 图片文件删除
 * - 存储空间统计
 */

#ifndef SUYAN_CLIPBOARD_IMAGE_STORAGE_H
#define SUYAN_CLIPBOARD_IMAGE_STORAGE_H

#include <string>
#include <vector>
#include <cstdint>

namespace suyan {

/**
 * 图片存储结果
 */
struct ImageStorageResult {
    bool success = false;           // 是否成功
    std::string imagePath;          // 原图路径
    std::string thumbnailPath;      // 缩略图路径
    int width = 0;                  // 图片宽度
    int height = 0;                 // 图片高度
    int64_t fileSize = 0;           // 文件大小（字节）
    std::string errorMessage;       // 错误信息（失败时）
};

/**
 * ImageStorage - 图片存储类
 *
 * 单例模式，管理剪贴板图片的文件系统存储。
 */
class ImageStorage {
public:
    /**
     * 获取单例实例
     */
    static ImageStorage& instance();

    // 禁止拷贝和移动
    ImageStorage(const ImageStorage&) = delete;
    ImageStorage& operator=(const ImageStorage&) = delete;
    ImageStorage(ImageStorage&&) = delete;
    ImageStorage& operator=(ImageStorage&&) = delete;

    /**
     * 初始化存储目录
     *
     * 创建 images/ 和 thumbnails/ 子目录。
     *
     * @param baseDir 基础目录路径（如 ~/Library/Application Support/SuYan/clipboard）
     * @return 是否成功
     */
    bool initialize(const std::string& baseDir);

    /**
     * 关闭存储
     */
    void shutdown();

    /**
     * 检查是否已初始化
     */
    bool isInitialized() const { return initialized_; }

    /**
     * 获取基础目录
     */
    std::string getBaseDir() const { return baseDir_; }

    /**
     * 获取图片目录
     */
    std::string getImagesDir() const { return imagesDir_; }

    /**
     * 获取缩略图目录
     */
    std::string getThumbnailsDir() const { return thumbnailsDir_; }

    // ========== 存储操作 ==========

    /**
     * 存储图片
     *
     * 保存原图并自动生成缩略图。
     * 文件名使用哈希值，避免重复存储。
     *
     * @param imageData 图片二进制数据
     * @param format 图片格式（png, jpeg, gif 等）
     * @param hash 内容哈希值（用作文件名）
     * @return 存储结果
     */
    ImageStorageResult saveImage(const std::vector<uint8_t>& imageData,
                                  const std::string& format,
                                  const std::string& hash);

    /**
     * 读取原图
     *
     * @param path 图片路径
     * @return 图片二进制数据，失败返回空 vector
     */
    std::vector<uint8_t> loadImage(const std::string& path);

    /**
     * 删除图片
     *
     * 同时删除原图和缩略图。
     *
     * @param imagePath 原图路径
     * @param thumbnailPath 缩略图路径
     * @return 是否成功
     */
    bool deleteImage(const std::string& imagePath, const std::string& thumbnailPath);

    /**
     * 检查图片是否存在
     *
     * @param path 图片路径
     * @return 是否存在
     */
    bool imageExists(const std::string& path);

    /**
     * 获取存储目录总大小
     *
     * @return 总大小（字节）
     */
    int64_t getStorageSize();

    // ========== 缩略图配置 ==========

    /**
     * 设置缩略图最大尺寸
     *
     * @param maxWidth 最大宽度
     * @param maxHeight 最大高度
     */
    void setThumbnailSize(int maxWidth, int maxHeight);

    /**
     * 获取缩略图最大宽度
     */
    int getThumbnailMaxWidth() const { return thumbnailMaxWidth_; }

    /**
     * 获取缩略图最大高度
     */
    int getThumbnailMaxHeight() const { return thumbnailMaxHeight_; }

private:
    ImageStorage();
    ~ImageStorage() = default;

    /**
     * 生成缩略图
     *
     * @param sourcePath 原图路径
     * @param thumbnailPath 缩略图保存路径
     * @param maxWidth 最大宽度
     * @param maxHeight 最大高度
     * @return 是否成功
     */
    bool generateThumbnail(const std::string& sourcePath,
                           const std::string& thumbnailPath,
                           int maxWidth,
                           int maxHeight);

    /**
     * 计算目录大小
     *
     * @param dirPath 目录路径
     * @return 目录大小（字节）
     */
    int64_t calculateDirectorySize(const std::string& dirPath);

    /**
     * 规范化图片格式
     *
     * @param format 输入格式
     * @return 规范化后的格式（小写，统一扩展名）
     */
    std::string normalizeFormat(const std::string& format);

    // 成员变量
    bool initialized_ = false;
    std::string baseDir_;
    std::string imagesDir_;
    std::string thumbnailsDir_;
    int thumbnailMaxWidth_ = 120;
    int thumbnailMaxHeight_ = 80;
};

} // namespace suyan

#endif // SUYAN_CLIPBOARD_IMAGE_STORAGE_H
