/**
 * ImageStorage 实现
 *
 * 使用 Qt QImage 进行图片处理和缩略图生成。
 */

#include "image_storage.h"
#include <QImage>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QBuffer>
#include <filesystem>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;

namespace suyan {

// ========== 单例实现 ==========

ImageStorage& ImageStorage::instance() {
    static ImageStorage instance;
    return instance;
}

ImageStorage::ImageStorage() = default;

// ========== 初始化和关闭 ==========

bool ImageStorage::initialize(const std::string& baseDir) {
    if (initialized_) {
        // 如果已初始化且路径相同，直接返回成功
        if (baseDir_ == baseDir) {
            return true;
        }
        // 路径不同，先关闭
        shutdown();
    }

    baseDir_ = baseDir;
    imagesDir_ = baseDir + "/images";
    thumbnailsDir_ = baseDir + "/thumbnails";

    // 创建目录
    try {
        fs::create_directories(imagesDir_);
        fs::create_directories(thumbnailsDir_);
    } catch (const std::exception& e) {
        std::cerr << "ImageStorage: 创建目录失败: " << e.what() << std::endl;
        return false;
    }

    // 验证目录存在
    if (!fs::exists(imagesDir_) || !fs::exists(thumbnailsDir_)) {
        std::cerr << "ImageStorage: 目录创建后不存在" << std::endl;
        return false;
    }

    initialized_ = true;
    return true;
}

void ImageStorage::shutdown() {
    initialized_ = false;
    baseDir_.clear();
    imagesDir_.clear();
    thumbnailsDir_.clear();
}

// ========== 存储操作 ==========

ImageStorageResult ImageStorage::saveImage(const std::vector<uint8_t>& imageData,
                                            const std::string& format,
                                            const std::string& hash) {
    ImageStorageResult result;
    result.success = false;

    if (!initialized_) {
        result.errorMessage = "ImageStorage 未初始化";
        return result;
    }

    if (imageData.empty()) {
        result.errorMessage = "图片数据为空";
        return result;
    }

    if (hash.empty()) {
        result.errorMessage = "哈希值为空";
        return result;
    }

    // 规范化格式
    std::string normalizedFormat = normalizeFormat(format);
    
    // 构建文件路径
    std::string imagePath = imagesDir_ + "/" + hash + "." + normalizedFormat;
    std::string thumbnailPath = thumbnailsDir_ + "/" + hash + "." + normalizedFormat;

    // 检查是否已存在
    if (fs::exists(imagePath)) {
        // 文件已存在，直接返回成功
        result.success = true;
        result.imagePath = imagePath;
        result.thumbnailPath = fs::exists(thumbnailPath) ? thumbnailPath : "";
        
        // 获取图片尺寸
        QImage existingImage(QString::fromStdString(imagePath));
        if (!existingImage.isNull()) {
            result.width = existingImage.width();
            result.height = existingImage.height();
        }
        result.fileSize = static_cast<int64_t>(fs::file_size(imagePath));
        return result;
    }

    // 从二进制数据加载图片
    QImage image;
    if (!image.loadFromData(imageData.data(), static_cast<int>(imageData.size()))) {
        result.errorMessage = "无法解析图片数据";
        return result;
    }

    // 保存原图
    QString qImagePath = QString::fromStdString(imagePath);
    if (!image.save(qImagePath)) {
        result.errorMessage = "保存原图失败";
        return result;
    }

    // 获取文件大小
    try {
        result.fileSize = static_cast<int64_t>(fs::file_size(imagePath));
    } catch (const std::exception& e) {
        std::cerr << "ImageStorage: 获取文件大小失败: " << e.what() << std::endl;
        result.fileSize = static_cast<int64_t>(imageData.size());
    }

    // 生成缩略图
    if (generateThumbnail(imagePath, thumbnailPath, thumbnailMaxWidth_, thumbnailMaxHeight_)) {
        result.thumbnailPath = thumbnailPath;
    } else {
        // 缩略图生成失败不是致命错误
        std::cerr << "ImageStorage: 缩略图生成失败，继续" << std::endl;
    }

    result.success = true;
    result.imagePath = imagePath;
    result.width = image.width();
    result.height = image.height();

    return result;
}

std::vector<uint8_t> ImageStorage::loadImage(const std::string& path) {
    std::vector<uint8_t> data;

    if (!initialized_) {
        return data;
    }

    if (path.empty() || !fs::exists(path)) {
        return data;
    }

    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly)) {
        std::cerr << "ImageStorage: 无法打开文件: " << path << std::endl;
        return data;
    }

    QByteArray fileData = file.readAll();
    file.close();

    data.resize(fileData.size());
    std::memcpy(data.data(), fileData.constData(), fileData.size());

    return data;
}

bool ImageStorage::deleteImage(const std::string& imagePath, const std::string& thumbnailPath) {
    if (!initialized_) {
        return false;
    }

    bool success = true;

    // 删除原图
    if (!imagePath.empty()) {
        try {
            if (fs::exists(imagePath)) {
                fs::remove(imagePath);
            }
        } catch (const std::exception& e) {
            std::cerr << "ImageStorage: 删除原图失败: " << e.what() << std::endl;
            success = false;
        }
    }

    // 删除缩略图
    if (!thumbnailPath.empty()) {
        try {
            if (fs::exists(thumbnailPath)) {
                fs::remove(thumbnailPath);
            }
        } catch (const std::exception& e) {
            std::cerr << "ImageStorage: 删除缩略图失败: " << e.what() << std::endl;
            success = false;
        }
    }

    return success;
}

bool ImageStorage::imageExists(const std::string& path) {
    if (!initialized_ || path.empty()) {
        return false;
    }

    return fs::exists(path);
}

int64_t ImageStorage::getStorageSize() {
    if (!initialized_) {
        return 0;
    }

    int64_t totalSize = 0;
    totalSize += calculateDirectorySize(imagesDir_);
    totalSize += calculateDirectorySize(thumbnailsDir_);
    return totalSize;
}

// ========== 缩略图配置 ==========

void ImageStorage::setThumbnailSize(int maxWidth, int maxHeight) {
    if (maxWidth > 0) {
        thumbnailMaxWidth_ = maxWidth;
    }
    if (maxHeight > 0) {
        thumbnailMaxHeight_ = maxHeight;
    }
}

// ========== 私有方法 ==========

bool ImageStorage::generateThumbnail(const std::string& sourcePath,
                                      const std::string& thumbnailPath,
                                      int maxWidth,
                                      int maxHeight) {
    QImage source(QString::fromStdString(sourcePath));
    if (source.isNull()) {
        std::cerr << "ImageStorage: 无法加载源图片: " << sourcePath << std::endl;
        return false;
    }

    // 计算缩放比例，保持宽高比
    QSize targetSize = source.size();
    targetSize.scale(maxWidth, maxHeight, Qt::KeepAspectRatio);

    // 如果原图已经小于目标尺寸，直接复制
    if (source.width() <= maxWidth && source.height() <= maxHeight) {
        return source.save(QString::fromStdString(thumbnailPath));
    }

    // 使用高质量缩放
    QImage thumbnail = source.scaled(
        targetSize,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );

    if (thumbnail.isNull()) {
        std::cerr << "ImageStorage: 缩放图片失败" << std::endl;
        return false;
    }

    return thumbnail.save(QString::fromStdString(thumbnailPath));
}

int64_t ImageStorage::calculateDirectorySize(const std::string& dirPath) {
    int64_t totalSize = 0;

    try {
        if (!fs::exists(dirPath)) {
            return 0;
        }

        for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
            if (entry.is_regular_file()) {
                totalSize += static_cast<int64_t>(entry.file_size());
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "ImageStorage: 计算目录大小失败: " << e.what() << std::endl;
    }

    return totalSize;
}

std::string ImageStorage::normalizeFormat(const std::string& format) {
    std::string normalized = format;
    
    // 转小写
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // 统一格式名称
    if (normalized == "jpeg" || normalized == "jpg") {
        return "jpg";
    }
    if (normalized == "tiff" || normalized == "tif") {
        return "tiff";
    }
    if (normalized.empty()) {
        return "png";  // 默认格式
    }

    return normalized;
}

} // namespace suyan
