/**
 * ImageStorage 单元测试
 *
 * 测试图片存储层的文件存储、缩略图生成和存储管理功能。
 */

#include <iostream>
#include <cassert>
#include <filesystem>
#include <QCoreApplication>
#include <QImage>
#include <QBuffer>
#include "image_storage.h"

namespace fs = std::filesystem;

// 测试辅助宏
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

class ImageStorageTest {
public:
    ImageStorageTest() {
        // 使用临时目录进行测试
        testBaseDir_ = fs::temp_directory_path().string() + "/suyan_image_storage_test";
        
        // 清理之前的测试数据
        fs::remove_all(testBaseDir_);
    }
    
    ~ImageStorageTest() {
        // 关闭存储
        suyan::ImageStorage::instance().shutdown();
        // 清理测试数据
        fs::remove_all(testBaseDir_);
    }
    
    bool runAllTests() {
        std::cout << "=== ImageStorage 单元测试 ===" << std::endl;
        std::cout << "测试数据目录: " << testBaseDir_ << std::endl;
        std::cout << std::endl;
        
        bool allPassed = true;
        
        // 基础测试
        allPassed &= testGetInstance();
        allPassed &= testInitialize();
        allPassed &= testInitializeCreatesDirectories();
        
        // 存储测试
        allPassed &= testSaveImagePng();
        allPassed &= testSaveImageJpeg();
        allPassed &= testSaveImageDuplicate();
        allPassed &= testSaveImageEmptyData();
        allPassed &= testSaveImageEmptyHash();
        
        // 缩略图测试
        allPassed &= testThumbnailGeneration();
        allPassed &= testThumbnailSmallImage();
        allPassed &= testSetThumbnailSize();
        
        // 读取测试
        allPassed &= testLoadImage();
        allPassed &= testLoadImageNotExists();
        
        // 删除测试
        allPassed &= testDeleteImage();
        allPassed &= testDeleteImagePartial();
        
        // 存在性检查测试
        allPassed &= testImageExists();
        
        // 存储大小测试
        allPassed &= testGetStorageSize();
        
        std::cout << std::endl;
        if (allPassed) {
            std::cout << "=== 所有测试通过 ===" << std::endl;
        } else {
            std::cout << "=== 部分测试失败 ===" << std::endl;
        }
        
        return allPassed;
    }
    
private:
    std::string testBaseDir_;
    
    // 重置测试环境
    void resetTestEnvironment() {
        auto& storage = suyan::ImageStorage::instance();
        storage.shutdown();
        fs::remove_all(testBaseDir_);
        storage.initialize(testBaseDir_);
    }
    
    // 创建测试 PNG 图片数据
    std::vector<uint8_t> createTestPngImage(int width, int height, QColor color = Qt::red) {
        QImage image(width, height, QImage::Format_ARGB32);
        image.fill(color);
        
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        buffer.close();
        
        return std::vector<uint8_t>(byteArray.begin(), byteArray.end());
    }
    
    // 创建测试 JPEG 图片数据
    std::vector<uint8_t> createTestJpegImage(int width, int height, QColor color = Qt::blue) {
        QImage image(width, height, QImage::Format_RGB32);
        image.fill(color);
        
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "JPEG", 90);
        buffer.close();
        
        return std::vector<uint8_t>(byteArray.begin(), byteArray.end());
    }
    
    // ========== 基础测试 ==========
    
    bool testGetInstance() {
        auto& storage1 = suyan::ImageStorage::instance();
        auto& storage2 = suyan::ImageStorage::instance();
        TEST_ASSERT(&storage1 == &storage2, "单例实例应该相同");
        TEST_PASS("testGetInstance: 单例模式正常");
        return true;
    }
    
    bool testInitialize() {
        auto& storage = suyan::ImageStorage::instance();
        storage.shutdown();
        
        // 首次初始化
        bool result = storage.initialize(testBaseDir_);
        TEST_ASSERT(result, "初始化应该成功");
        TEST_ASSERT(storage.isInitialized(), "初始化后 isInitialized 应该返回 true");
        
        // 重复初始化应该返回 true（幂等）
        result = storage.initialize(testBaseDir_);
        TEST_ASSERT(result, "重复初始化应该返回 true");
        
        TEST_PASS("testInitialize: 初始化成功");
        return true;
    }
    
    bool testInitializeCreatesDirectories() {
        auto& storage = suyan::ImageStorage::instance();
        storage.shutdown();
        fs::remove_all(testBaseDir_);
        
        storage.initialize(testBaseDir_);
        
        // 检查目录已创建
        TEST_ASSERT(fs::exists(testBaseDir_), "基础目录应该已创建");
        TEST_ASSERT(fs::exists(storage.getImagesDir()), "images 目录应该已创建");
        TEST_ASSERT(fs::exists(storage.getThumbnailsDir()), "thumbnails 目录应该已创建");
        
        std::cout << "  基础目录: " << storage.getBaseDir() << std::endl;
        std::cout << "  图片目录: " << storage.getImagesDir() << std::endl;
        std::cout << "  缩略图目录: " << storage.getThumbnailsDir() << std::endl;
        
        TEST_PASS("testInitializeCreatesDirectories: 目录创建正常");
        return true;
    }
    
    // ========== 存储测试 ==========
    
    bool testSaveImagePng() {
        resetTestEnvironment();
        auto& storage = suyan::ImageStorage::instance();
        
        auto imageData = createTestPngImage(200, 150);
        auto result = storage.saveImage(imageData, "png", "test_hash_png_001");
        
        TEST_ASSERT(result.success, "保存 PNG 应该成功");
        TEST_ASSERT(!result.imagePath.empty(), "图片路径不应为空");
        TEST_ASSERT(fs::exists(result.imagePath), "图片文件应该存在");
        TEST_ASSERT(result.width == 200, "宽度应该是 200");
        TEST_ASSERT(result.height == 150, "高度应该是 150");
        TEST_ASSERT(result.fileSize > 0, "文件大小应该大于 0");
        
        std::cout << "  图片路径: " << result.imagePath << std::endl;
        std::cout << "  文件大小: " << result.fileSize << " bytes" << std::endl;
        
        TEST_PASS("testSaveImagePng: 保存 PNG 图片正常");
        return true;
    }
    
    bool testSaveImageJpeg() {
        resetTestEnvironment();
        auto& storage = suyan::ImageStorage::instance();
        
        auto imageData = createTestJpegImage(300, 200);
        auto result = storage.saveImage(imageData, "jpeg", "test_hash_jpeg_001");
        
        TEST_ASSERT(result.success, "保存 JPEG 应该成功");
        TEST_ASSERT(!result.imagePath.empty(), "图片路径不应为空");
        TEST_ASSERT(fs::exists(result.imagePath), "图片文件应该存在");
        TEST_ASSERT(result.width == 300, "宽度应该是 300");
        TEST_ASSERT(result.height == 200, "高度应该是 200");
        
        // 验证格式规范化（jpeg -> jpg）
        TEST_ASSERT(result.imagePath.find(".jpg") != std::string::npos, "文件扩展名应该是 .jpg");
        
        TEST_PASS("testSaveImageJpeg: 保存 JPEG 图片正常");
        return true;
    }
    
    bool testSaveImageDuplicate() {
        resetTestEnvironment();
        auto& storage = suyan::ImageStorage::instance();
        
        auto imageData = createTestPngImage(100, 100);
        
        // 第一次保存
        auto result1 = storage.saveImage(imageData, "png", "test_hash_dup_001");
        TEST_ASSERT(result1.success, "第一次保存应该成功");
        
        // 第二次保存相同哈希
        auto result2 = storage.saveImage(imageData, "png", "test_hash_dup_001");
        TEST_ASSERT(result2.success, "第二次保存应该成功（返回已存在的文件）");
        TEST_ASSERT(result1.imagePath == result2.imagePath, "路径应该相同");
        
        TEST_PASS("testSaveImageDuplicate: 重复保存处理正常");
        return true;
    }
    
    bool testSaveImageEmptyData() {
        resetTestEnvironment();
        auto& storage = suyan::ImageStorage::instance();
        
        std::vector<uint8_t> emptyData;
        auto result = storage.saveImage(emptyData, "png", "test_hash_empty");
        
        TEST_ASSERT(!result.success, "空数据保存应该失败");
        TEST_ASSERT(!result.errorMessage.empty(), "应该有错误信息");
        
        TEST_PASS("testSaveImageEmptyData: 空数据处理正常");
        return true;
    }
    
    bool testSaveImageEmptyHash() {
        resetTestEnvironment();
        auto& storage = suyan::ImageStorage::instance();
        
        auto imageData = createTestPngImage(100, 100);
        auto result = storage.saveImage(imageData, "png", "");
        
        TEST_ASSERT(!result.success, "空哈希保存应该失败");
        TEST_ASSERT(!result.errorMessage.empty(), "应该有错误信息");
        
        TEST_PASS("testSaveImageEmptyHash: 空哈希处理正常");
        return true;
    }
    
    // ========== 缩略图测试 ==========
    
    bool testThumbnailGeneration() {
        resetTestEnvironment();
        auto& storage = suyan::ImageStorage::instance();
        
        // 创建一个大图片
        auto imageData = createTestPngImage(800, 600);
        auto result = storage.saveImage(imageData, "png", "test_hash_thumb_001");
        
        TEST_ASSERT(result.success, "保存应该成功");
        TEST_ASSERT(!result.thumbnailPath.empty(), "缩略图路径不应为空");
        TEST_ASSERT(fs::exists(result.thumbnailPath), "缩略图文件应该存在");
        
        // 验证缩略图尺寸
        QImage thumbnail(QString::fromStdString(result.thumbnailPath));
        TEST_ASSERT(!thumbnail.isNull(), "缩略图应该能加载");
        TEST_ASSERT(thumbnail.width() <= 120, "缩略图宽度应该 <= 120");
        TEST_ASSERT(thumbnail.height() <= 80, "缩略图高度应该 <= 80");
        
        std::cout << "  原图尺寸: 800x600" << std::endl;
        std::cout << "  缩略图尺寸: " << thumbnail.width() << "x" << thumbnail.height() << std::endl;
        
        TEST_PASS("testThumbnailGeneration: 缩略图生成正常");
        return true;
    }
    
    bool testThumbnailSmallImage() {
        resetTestEnvironment();
        auto& storage = suyan::ImageStorage::instance();
        
        // 创建一个小于缩略图尺寸的图片
        auto imageData = createTestPngImage(50, 40);
        auto result = storage.saveImage(imageData, "png", "test_hash_small_001");
        
        TEST_ASSERT(result.success, "保存应该成功");
        TEST_ASSERT(!result.thumbnailPath.empty(), "缩略图路径不应为空");
        
        // 验证缩略图尺寸（应该保持原尺寸）
        QImage thumbnail(QString::fromStdString(result.thumbnailPath));
        TEST_ASSERT(thumbnail.width() == 50, "小图片缩略图宽度应该保持原尺寸");
        TEST_ASSERT(thumbnail.height() == 40, "小图片缩略图高度应该保持原尺寸");
        
        TEST_PASS("testThumbnailSmallImage: 小图片缩略图处理正常");
        return true;
    }
    
    bool testSetThumbnailSize() {
        resetTestEnvironment();
        auto& storage = suyan::ImageStorage::instance();
        
        // 设置自定义缩略图尺寸
        storage.setThumbnailSize(200, 150);
        TEST_ASSERT(storage.getThumbnailMaxWidth() == 200, "最大宽度应该是 200");
        TEST_ASSERT(storage.getThumbnailMaxHeight() == 150, "最大高度应该是 150");
        
        // 创建图片并验证缩略图尺寸
        auto imageData = createTestPngImage(1000, 800);
        auto result = storage.saveImage(imageData, "png", "test_hash_custom_size");
        
        QImage thumbnail(QString::fromStdString(result.thumbnailPath));
        TEST_ASSERT(thumbnail.width() <= 200, "缩略图宽度应该 <= 200");
        TEST_ASSERT(thumbnail.height() <= 150, "缩略图高度应该 <= 150");
        
        // 恢复默认尺寸
        storage.setThumbnailSize(120, 80);
        
        TEST_PASS("testSetThumbnailSize: 自定义缩略图尺寸正常");
        return true;
    }
    
    // ========== 读取测试 ==========
    
    bool testLoadImage() {
        resetTestEnvironment();
        auto& storage = suyan::ImageStorage::instance();
        
        // 保存图片
        auto originalData = createTestPngImage(100, 100);
        auto saveResult = storage.saveImage(originalData, "png", "test_hash_load_001");
        TEST_ASSERT(saveResult.success, "保存应该成功");
        
        // 读取图片
        auto loadedData = storage.loadImage(saveResult.imagePath);
        TEST_ASSERT(!loadedData.empty(), "读取的数据不应为空");
        
        // 验证数据可以解析为图片
        QImage loadedImage;
        bool loaded = loadedImage.loadFromData(loadedData.data(), static_cast<int>(loadedData.size()));
        TEST_ASSERT(loaded, "读取的数据应该能解析为图片");
        TEST_ASSERT(loadedImage.width() == 100, "宽度应该正确");
        TEST_ASSERT(loadedImage.height() == 100, "高度应该正确");
        
        TEST_PASS("testLoadImage: 读取图片正常");
        return true;
    }
    
    bool testLoadImageNotExists() {
        resetTestEnvironment();
        auto& storage = suyan::ImageStorage::instance();
        
        auto data = storage.loadImage("/nonexistent/path/image.png");
        TEST_ASSERT(data.empty(), "不存在的文件应该返回空数据");
        
        data = storage.loadImage("");
        TEST_ASSERT(data.empty(), "空路径应该返回空数据");
        
        TEST_PASS("testLoadImageNotExists: 不存在文件处理正常");
        return true;
    }
    
    // ========== 删除测试 ==========
    
    bool testDeleteImage() {
        resetTestEnvironment();
        auto& storage = suyan::ImageStorage::instance();
        
        // 保存图片
        auto imageData = createTestPngImage(100, 100);
        auto result = storage.saveImage(imageData, "png", "test_hash_delete_001");
        TEST_ASSERT(result.success, "保存应该成功");
        TEST_ASSERT(fs::exists(result.imagePath), "原图应该存在");
        TEST_ASSERT(fs::exists(result.thumbnailPath), "缩略图应该存在");
        
        // 删除图片
        bool deleted = storage.deleteImage(result.imagePath, result.thumbnailPath);
        TEST_ASSERT(deleted, "删除应该成功");
        TEST_ASSERT(!fs::exists(result.imagePath), "原图应该已删除");
        TEST_ASSERT(!fs::exists(result.thumbnailPath), "缩略图应该已删除");
        
        TEST_PASS("testDeleteImage: 删除图片正常");
        return true;
    }
    
    bool testDeleteImagePartial() {
        resetTestEnvironment();
        auto& storage = suyan::ImageStorage::instance();
        
        // 保存图片
        auto imageData = createTestPngImage(100, 100);
        auto result = storage.saveImage(imageData, "png", "test_hash_partial_001");
        
        // 只删除原图
        bool deleted = storage.deleteImage(result.imagePath, "");
        TEST_ASSERT(deleted, "部分删除应该成功");
        TEST_ASSERT(!fs::exists(result.imagePath), "原图应该已删除");
        TEST_ASSERT(fs::exists(result.thumbnailPath), "缩略图应该仍存在");
        
        // 删除缩略图
        deleted = storage.deleteImage("", result.thumbnailPath);
        TEST_ASSERT(deleted, "删除缩略图应该成功");
        TEST_ASSERT(!fs::exists(result.thumbnailPath), "缩略图应该已删除");
        
        TEST_PASS("testDeleteImagePartial: 部分删除正常");
        return true;
    }
    
    // ========== 存在性检查测试 ==========
    
    bool testImageExists() {
        resetTestEnvironment();
        auto& storage = suyan::ImageStorage::instance();
        
        // 不存在的文件
        TEST_ASSERT(!storage.imageExists("/nonexistent/path.png"), "不存在的文件应该返回 false");
        TEST_ASSERT(!storage.imageExists(""), "空路径应该返回 false");
        
        // 保存图片
        auto imageData = createTestPngImage(100, 100);
        auto result = storage.saveImage(imageData, "png", "test_hash_exists_001");
        
        // 存在的文件
        TEST_ASSERT(storage.imageExists(result.imagePath), "存在的文件应该返回 true");
        TEST_ASSERT(storage.imageExists(result.thumbnailPath), "缩略图应该返回 true");
        
        TEST_PASS("testImageExists: 存在性检查正常");
        return true;
    }
    
    // ========== 存储大小测试 ==========
    
    bool testGetStorageSize() {
        resetTestEnvironment();
        auto& storage = suyan::ImageStorage::instance();
        
        // 初始大小应该是 0
        int64_t initialSize = storage.getStorageSize();
        TEST_ASSERT(initialSize == 0, "初始存储大小应该是 0");
        
        // 保存一些图片
        auto imageData1 = createTestPngImage(200, 200);
        storage.saveImage(imageData1, "png", "test_hash_size_001");
        
        auto imageData2 = createTestPngImage(300, 300);
        storage.saveImage(imageData2, "png", "test_hash_size_002");
        
        // 存储大小应该增加
        int64_t newSize = storage.getStorageSize();
        TEST_ASSERT(newSize > 0, "存储大小应该大于 0");
        
        std::cout << "  存储大小: " << newSize << " bytes" << std::endl;
        
        TEST_PASS("testGetStorageSize: 存储大小统计正常");
        return true;
    }
};

int main(int argc, char* argv[]) {
    // 需要 QCoreApplication 来支持 Qt 图片处理
    QCoreApplication app(argc, argv);
    
    ImageStorageTest test;
    return test.runAllTests() ? 0 : 1;
}
