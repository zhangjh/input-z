// src/platform/macos/squirrel_integration.cpp
// Squirrel 集成层实现 (macOS)

#ifdef PLATFORM_MACOS

#include "squirrel_integration.h"

#include <filesystem>
#include <sstream>
#include <cstdlib>
#include <pwd.h>
#include <unistd.h>

#include "core/storage/sqlite_storage.h"
#include "core/frequency/frequency_manager_impl.h"
#include "core/input/candidate_merger.h"
#include "core/learning/auto_learner_impl.h"

namespace fs = std::filesystem;

namespace ime {
namespace platform {
namespace macos {

// ==================== SquirrelIntegrationConfig ====================

SquirrelIntegrationConfig SquirrelIntegrationConfig::Default() {
    // 获取用户主目录
    std::string homeDir;
    if (const char* home = std::getenv("HOME")) {
        homeDir = home;
    } else if (struct passwd* pw = getpwuid(getuid())) {
        homeDir = pw->pw_dir;
    } else {
        homeDir = "/tmp";
    }

    return SquirrelIntegrationConfig{
        .userDataPath = homeDir + "/Library/Rime",
        .sharedDataPath = "/Library/Input Methods/Squirrel.app/Contents/SharedSupport",
        .logPath = "/tmp/rime.squirrel",
        .enableCloudSync = true,
        .enableAutoLearn = true,
        .pageSize = 9
    };
}

// ==================== SquirrelIntegration ====================

SquirrelIntegration& SquirrelIntegration::Instance() {
    static SquirrelIntegration instance;
    return instance;
}

SquirrelIntegration::SquirrelIntegration()
    : initialized_(false),
      currentMode_(input::InputMode::Chinese) {
}

SquirrelIntegration::~SquirrelIntegration() {
    Shutdown();
}

bool SquirrelIntegration::Initialize(const SquirrelIntegrationConfig& config) {
    if (initialized_) {
        return true;
    }

    config_ = config;

    // 初始化存储层
    if (!InitializeStorage(config.userDataPath)) {
        return false;
    }

    // 初始化词频管理器
    if (!InitializeFrequencyManager()) {
        return false;
    }

    // 初始化候选词合并器
    candidateMerger_ = std::make_unique<input::CandidateMerger>(storage_.get());
    input::MergeConfig mergeConfig = input::MergeConfig::Default();
    mergeConfig.pageSize = config.pageSize;
    candidateMerger_->SetConfig(mergeConfig);

    // 初始化自动学词
    if (config.enableAutoLearn) {
        if (!InitializeAutoLearner()) {
            // 自动学词失败不影响主功能
        }
    }

    // 从存储加载输入模式
    std::string modeStr = storage_->GetConfig("input.default_mode", "chinese");
    if (modeStr == "english") {
        currentMode_ = input::InputMode::English;
    } else {
        currentMode_ = input::InputMode::Chinese;
    }

    initialized_ = true;

    // 初始化完成后执行词频清理
    int cleaned = CleanupWordFrequencies();
    if (cleaned > 0) {
        // 可以添加日志记录
    }

    return true;
}

void SquirrelIntegration::Shutdown() {
    if (!initialized_) {
        return;
    }

    // 保存输入模式
    if (storage_) {
        std::string modeStr = (currentMode_ == input::InputMode::English) 
                              ? "english" : "chinese";
        storage_->SetConfig("input.default_mode", modeStr);
    }

    // 清理资源
    autoLearner_.reset();
    candidateMerger_.reset();
    frequencyManager_.reset();

    if (storage_) {
        storage_->Close();
        storage_.reset();
    }

    initialized_ = false;
}

bool SquirrelIntegration::InitializeStorage(const std::string& userDataPath) {
    // 构建数据库路径
    fs::path dbPath;
    if (!userDataPath.empty()) {
        dbPath = fs::path(userDataPath) / "ime_data.db";
    } else {
        // 使用默认路径
        std::string homeDir;
        if (const char* home = std::getenv("HOME")) {
            homeDir = home;
        } else {
            return false;
        }
        dbPath = fs::path(homeDir) / "Library" / "Rime" / "ime_data.db";
    }

    // 确保目录存在
    std::error_code ec;
    fs::create_directories(dbPath.parent_path(), ec);
    if (ec) {
        return false;
    }

    // 创建存储实例
    storage_ = std::make_unique<storage::SqliteStorage>(dbPath.string());
    if (!storage_->Initialize()) {
        storage_.reset();
        return false;
    }

    return true;
}

bool SquirrelIntegration::InitializeFrequencyManager() {
    frequencyManager_ = std::make_unique<frequency::FrequencyManagerImpl>(
        storage_.get());
    return frequencyManager_->Initialize();
}

bool SquirrelIntegration::InitializeAutoLearner() {
    autoLearner_ = std::make_unique<learning::AutoLearnerImpl>(storage_.get());
    return autoLearner_->Initialize();
}

std::vector<std::string> SquirrelIntegration::MergeCandidates(
    const std::vector<std::string>& rimeCandidates,
    const std::string& pinyin) {
    
    if (!initialized_ || !candidateMerger_) {
        return rimeCandidates;
    }

    // 将字符串列表转换为 CandidateWord 列表
    std::vector<input::CandidateWord> rimeWords;
    rimeWords.reserve(rimeCandidates.size());
    for (const auto& text : rimeCandidates) {
        input::CandidateWord word;
        word.text = text;
        word.pinyin = pinyin;
        word.isUserWord = false;
        rimeWords.push_back(word);
    }

    // 合并
    auto merged = candidateMerger_->Merge(rimeWords, pinyin);

    // 转换回字符串列表
    std::vector<std::string> result;
    result.reserve(merged.size());
    for (const auto& word : merged) {
        result.push_back(word.text);
    }

    return result;
}

std::vector<std::string> SquirrelIntegration::GetUserTopWords(
    const std::string& pinyin, int limit) {
    
    if (!initialized_ || !candidateMerger_) {
        return {};
    }

    auto userWords = candidateMerger_->QueryUserWords(pinyin, limit);
    
    std::vector<std::string> result;
    result.reserve(userWords.size());
    for (const auto& word : userWords) {
        result.push_back(word.text);
    }

    return result;
}

void SquirrelIntegration::RecordWordSelection(const std::string& word,
                                              const std::string& pinyin) {
    if (!initialized_) {
        return;
    }

    // 更新词频
    if (frequencyManager_) {
        frequencyManager_->RecordWordSelection(word, pinyin);
    }
}

void SquirrelIntegration::RecordConsecutiveSelection(const std::string& word,
                                                     const std::string& pinyin) {
    if (!initialized_) {
        return;
    }

    // 记录词频
    RecordWordSelection(word, pinyin);

    // 记录到自动学词器
    if (autoLearner_) {
        autoLearner_->RecordInput(word, pinyin);
    }
}

void SquirrelIntegration::OnCommitComplete() {
    if (!initialized_ || !autoLearner_) {
        return;
    }

    // 触发自动学词检查
    autoLearner_->ProcessCandidates();
}

input::InputMode SquirrelIntegration::GetInputMode() const {
    return currentMode_;
}

void SquirrelIntegration::SetInputMode(input::InputMode mode) {
    currentMode_ = mode;

    // 持久化
    if (initialized_ && storage_) {
        std::string modeStr;
        switch (mode) {
            case input::InputMode::English:
                modeStr = "english";
                break;
            case input::InputMode::TempEnglish:
                modeStr = "temp_english";
                break;
            default:
                modeStr = "chinese";
                break;
        }
        storage_->SetConfig("input.current_mode", modeStr);
    }
}

void SquirrelIntegration::ToggleInputMode() {
    if (currentMode_ == input::InputMode::Chinese) {
        SetInputMode(input::InputMode::English);
    } else {
        SetInputMode(input::InputMode::Chinese);
    }
}

std::string SquirrelIntegration::GetConfig(const std::string& key,
                                           const std::string& defaultValue) {
    if (!initialized_ || !storage_) {
        return defaultValue;
    }
    return storage_->GetConfig(key, defaultValue);
}

bool SquirrelIntegration::SetConfig(const std::string& key,
                                    const std::string& value) {
    if (!initialized_ || !storage_) {
        return false;
    }
    return storage_->SetConfig(key, value);
}

std::vector<std::string> SquirrelIntegration::GetEnabledDictionaries() {
    if (!initialized_ || !storage_) {
        return {};
    }

    auto dicts = storage_->GetEnabledDictionaries();
    std::vector<std::string> result;
    result.reserve(dicts.size());
    for (const auto& dict : dicts) {
        result.push_back(dict.id);
    }
    return result;
}

bool SquirrelIntegration::SetDictionaryEnabled(const std::string& dictId,
                                               bool enabled) {
    if (!initialized_ || !storage_) {
        return false;
    }
    return storage_->SetDictionaryEnabled(dictId, enabled);
}

int SquirrelIntegration::CleanupWordFrequencies() {
    if (!initialized_ || !storage_) {
        return 0;
    }

    int totalCleaned = 0;

    // 1. 清理低频旧词：frequency <= 1 且超过 30 天未更新
    int lowFreqCleaned = storage_->CleanupLowFrequencyWords(1, 30);
    totalCleaned += lowFreqCleaned;

    // 2. 限制总量：最多保留 500000 条
    int limitCleaned = storage_->EnforceFrequencyLimit(500000);
    totalCleaned += limitCleaned;

    return totalCleaned;
}

}  // namespace macos
}  // namespace platform
}  // namespace ime

// ==================== C 接口实现 ====================

using namespace ime::platform::macos;

int ImeIntegration_Initialize(const char* userDataPath,
                               const char* sharedDataPath) {
    SquirrelIntegrationConfig config = SquirrelIntegrationConfig::Default();
    if (userDataPath && userDataPath[0] != '\0') {
        config.userDataPath = userDataPath;
    }
    if (sharedDataPath && sharedDataPath[0] != '\0') {
        config.sharedDataPath = sharedDataPath;
    }

    return SquirrelIntegration::Instance().Initialize(config) ? 0 : -1;
}

void ImeIntegration_Shutdown(void) {
    SquirrelIntegration::Instance().Shutdown();
}

int ImeIntegration_IsInitialized(void) {
    return SquirrelIntegration::Instance().IsInitialized() ? 1 : 0;
}

int ImeIntegration_MergeCandidates(const char** candidates,
                                    const char* pinyin,
                                    char** outBuffer,
                                    int bufferSize) {
    if (!candidates || !pinyin || !outBuffer || bufferSize <= 0) {
        return 0;
    }

    // 收集输入候选词
    std::vector<std::string> inputCandidates;
    for (int i = 0; candidates[i] != nullptr; ++i) {
        inputCandidates.push_back(candidates[i]);
    }

    // 合并
    auto merged = SquirrelIntegration::Instance().MergeCandidates(
        inputCandidates, pinyin);

    // 输出结果
    int count = std::min(static_cast<int>(merged.size()), bufferSize);
    for (int i = 0; i < count; ++i) {
        // 注意：调用者需要负责释放内存
        outBuffer[i] = strdup(merged[i].c_str());
    }

    return count;
}

void ImeIntegration_FreeMergedCandidates(char** buffer, int count) {
    if (!buffer) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        if (buffer[i]) {
            free(buffer[i]);
            buffer[i] = nullptr;
        }
    }
}

void ImeIntegration_RecordSelection(const char* word, const char* pinyin) {
    if (!word || !pinyin) {
        return;
    }
    SquirrelIntegration::Instance().RecordWordSelection(word, pinyin);
}

void ImeIntegration_RecordConsecutive(const char* word, const char* pinyin) {
    if (!word || !pinyin) {
        return;
    }
    SquirrelIntegration::Instance().RecordConsecutiveSelection(word, pinyin);
}

void ImeIntegration_OnCommit(void) {
    SquirrelIntegration::Instance().OnCommitComplete();
}

int ImeIntegration_GetInputMode(void) {
    auto mode = SquirrelIntegration::Instance().GetInputMode();
    switch (mode) {
        case ime::input::InputMode::English:
            return 1;
        case ime::input::InputMode::TempEnglish:
            return 2;
        default:
            return 0;
    }
}

void ImeIntegration_SetInputMode(int mode) {
    ime::input::InputMode inputMode;
    switch (mode) {
        case 1:
            inputMode = ime::input::InputMode::English;
            break;
        case 2:
            inputMode = ime::input::InputMode::TempEnglish;
            break;
        default:
            inputMode = ime::input::InputMode::Chinese;
            break;
    }
    SquirrelIntegration::Instance().SetInputMode(inputMode);
}

void ImeIntegration_ToggleInputMode(void) {
    SquirrelIntegration::Instance().ToggleInputMode();
}

int ImeIntegration_GetConfig(const char* key, 
                              const char* defaultValue,
                              char* outBuffer, 
                              int bufferSize) {
    if (!key || !outBuffer || bufferSize <= 0) {
        return 0;
    }

    std::string value = SquirrelIntegration::Instance().GetConfig(
        key, defaultValue ? defaultValue : "");
    
    int len = static_cast<int>(value.length());
    int copyLen = std::min(len, bufferSize - 1);
    strncpy(outBuffer, value.c_str(), copyLen);
    outBuffer[copyLen] = '\0';
    
    return len;
}

int ImeIntegration_SetConfig(const char* key, const char* value) {
    if (!key || !value) {
        return -1;
    }
    return SquirrelIntegration::Instance().SetConfig(key, value) ? 0 : -1;
}

int ImeIntegration_GetUserTopWords(const char* pinyin,
                                    int limit,
                                    char** outBuffer,
                                    int bufferSize) {
    if (!pinyin || !outBuffer || bufferSize <= 0) {
        return 0;
    }

    auto words = SquirrelIntegration::Instance().GetUserTopWords(pinyin, limit);
    
    int count = std::min(static_cast<int>(words.size()), bufferSize);
    for (int i = 0; i < count; ++i) {
        outBuffer[i] = strdup(words[i].c_str());
    }
    
    return count;
}

void ImeIntegration_FreeUserTopWords(char** buffer, int count) {
    if (!buffer) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        if (buffer[i]) {
            free(buffer[i]);
            buffer[i] = nullptr;
        }
    }
}

#endif  // PLATFORM_MACOS
