/**
 * InputEngine - 输入引擎实现
 */

// Qt 头文件必须在 rime_api.h 之前包含，因为 rime_api.h 定义了 #define Bool int
// 这会与 Qt 的 qmetatype.h 冲突
#include "frequency_manager.h"  // 包含 QObject，必须在 rime_api.h 之前

// 取消 Bool 宏定义（如果已定义）
#ifdef Bool
#undef Bool
#endif

#include "input_engine.h"
#include "config_manager.h"
#include "platform_bridge.h"
#include "rime_wrapper.h"
#include <cctype>
#include <iostream>

// 再次取消 Bool 宏定义（rime_api.h 可能重新定义）
#ifdef Bool
#undef Bool
#endif

namespace suyan {

// ========== 构造与析构 ==========

InputEngine::InputEngine() = default;

InputEngine::~InputEngine() {
    shutdown();
}

// ========== 初始化与关闭 ==========

bool InputEngine::initialize(const std::string& userDataDir,
                              const std::string& sharedDataDir) {
    if (initialized_) {
        return true;
    }

    // 初始化 RimeWrapper
    auto& rime = RimeWrapper::instance();
    if (!rime.initialize(userDataDir, sharedDataDir, "SuYan")) {
        std::cerr << "InputEngine: Failed to initialize RimeWrapper" << std::endl;
        return false;
    }

    // 启动维护任务并等待完成
    rime.startMaintenance(false);
    rime.joinMaintenanceThread();

    // 创建会话
    sessionId_ = rime.createSession();
    if (sessionId_ == 0) {
        std::cerr << "InputEngine: Failed to create RIME session" << std::endl;
        return false;
    }

    initialized_ = true;
    mode_ = InputMode::Chinese;
    return true;
}

void InputEngine::shutdown() {
    if (!initialized_) {
        return;
    }

    auto& rime = RimeWrapper::instance();
    if (sessionId_ != 0) {
        rime.destroySession(sessionId_);
        sessionId_ = 0;
    }

    initialized_ = false;
}

// ========== 平台桥接 ==========

void InputEngine::setPlatformBridge(IPlatformBridge* bridge) {
    platformBridge_ = bridge;
}

// ========== 按键处理 ==========

bool InputEngine::processKeyEvent(int keyCode, int modifiers) {
    if (!initialized_) {
        return false;
    }

    // 根据当前模式分发处理
    bool handled = false;
    switch (mode_) {
        case InputMode::Chinese:
            handled = handleChineseMode(keyCode, modifiers);
            break;
        case InputMode::English:
            handled = handleEnglishMode(keyCode, modifiers);
            break;
        case InputMode::TempEnglish:
            handled = handleTempEnglishMode(keyCode, modifiers);
            break;
    }

    return handled;
}

bool InputEngine::handleChineseMode(int keyCode, int modifiers) {
    auto& rime = RimeWrapper::instance();

    // 检查是否应该进入临时英文模式（大写字母开头）
    if (!isComposing() && shouldEnterTempEnglish(keyCode, modifiers)) {
        mode_ = InputMode::TempEnglish;
        tempEnglishBuffer_.clear();
        // 添加首字母（大写）
        char c = static_cast<char>(keyCode);
        tempEnglishBuffer_ += c;
        notifyStateChanged();
        return true;
    }

    // 如果没有在输入状态，且按下的是数字键，直接提交数字
    // 这样可以正确跟踪 lastCommittedChar_ 用于数字后标点智能转换
    if (!isComposing() && isDigitKey(keyCode) && modifiers == 0) {
        std::string digit(1, static_cast<char>(keyCode));
        notifyCommitText(digit);
        return true;  // 我们处理了数字输入
    }
    
    // 数字后的标点智能转换（搜狗风格）
    // 如果上一个提交的字符是数字，且当前输入的是特定标点，则转换为英文标点
    // 注意：冒号需要 Shift 键，所以不能只检查 modifiers == 0
    if (!isComposing() && lastCommittedChar_ >= '0' && lastCommittedChar_ <= '9') {
        std::string englishPunct;
        // 不需要 Shift 的标点
        if (modifiers == 0) {
            switch (keyCode) {
                case '.': englishPunct = "."; break;  // 句号 -> 英文点
                case ',': englishPunct = ","; break;  // 逗号 -> 英文逗号
                case ';': englishPunct = ";"; break;  // 分号 -> 英文分号
                default: break;
            }
        }
        // 需要 Shift 的标点
        if ((modifiers & KeyModifier::Shift) && !(modifiers & ~KeyModifier::Shift)) {
            switch (keyCode) {
                case ':': englishPunct = ":"; break;  // Shift+; -> 冒号
                case ';': englishPunct = ":"; break;  // 有些系统 Shift+; 发送的是 ';' 而不是 ':'
                default: break;
            }
        }
        if (!englishPunct.empty()) {
            lastCommittedChar_ = static_cast<char>(keyCode);
            notifyCommitText(englishPunct);
            return true;
        }
    }
    
    // 处理箭头键导航（搜狗风格）
    if (isComposing() && modifiers == 0) {
        if (keyCode == KeyCode::Up || keyCode == KeyCode::Down ||
            keyCode == KeyCode::Left || keyCode == KeyCode::Right) {
            return handleArrowKeys(keyCode);
        }
    }
    
    // 如果在展开模式下按了空格或回车，选择当前高亮的候选词
    if (isExpanded_ && isComposing() && modifiers == 0) {
        if (keyCode == KeyCode::Space || keyCode == KeyCode::Return) {
            auto menu = rime.getCandidateMenu(sessionId_);
            int pageSize = menu.pageSize > 0 ? menu.pageSize : 9;
            int totalIndex = currentRow_ * pageSize + currentCol_;
            
            if (totalIndex < static_cast<int>(expandedCandidates_.size())) {
                std::string selectedText = expandedCandidates_[totalIndex].text;
                std::string currentPinyin = rime.getRawInput(sessionId_);
                
                // 使用 RIME 选择候选词
                // 需要计算在 RIME 中的实际索引
                int rimeIndex = totalIndex % pageSize;  // 当前页内的索引
                int targetPage = totalIndex / pageSize;
                
                // 先重置到第一页
                while (menu.pageIndex > 0) {
                    rime.changePage(sessionId_, true);  // 向前翻页
                    menu = rime.getCandidateMenu(sessionId_);
                }
                
                // 切换到目标页
                while (menu.pageIndex < targetPage && !menu.isLastPage) {
                    rime.changePage(sessionId_, false);  // 下一页
                    menu = rime.getCandidateMenu(sessionId_);
                }
                
                // 选择候选词
                bool success = rime.selectCandidateOnCurrentPage(sessionId_, rimeIndex);
                if (success) {
                    std::string commitText = rime.getCommitText(sessionId_);
                    if (!commitText.empty()) {
                        if (frequencyLearningEnabled_) {
                            updateFrequencyForSelectedCandidate(selectedText, currentPinyin);
                        }
                        notifyCommitText(commitText);
                    }
                }
                
                // 重置展开状态
                resetExpandedState();
                updateState();
                notifyStateChanged();
                return true;
            }
        }
        
        // Escape 键退出展开模式
        if (keyCode == KeyCode::Escape) {
            resetExpandedState();
            updateState();
            notifyStateChanged();
            return true;
        }
    }
    
    // 如果按了其他键，退出展开模式
    if (isExpanded_ && keyCode != KeyCode::BackSpace) {
        resetExpandedState();
    }
    
    // 如果输入了会改变候选词列表的按键（字母、退格等），重置导航状态
    // 这样下次使用方向键时，高亮会从第一个候选词开始
    if (isAlphaKey(keyCode) || keyCode == KeyCode::BackSpace) {
        // 清空导航状态，但不调用 resetExpandedState() 因为那会重置 RIME 页面
        expandedCandidates_.clear();
        currentRow_ = 0;
        currentCol_ = 0;
    }

    // 在处理按键之前，记录候选词信息用于词频更新
    std::string selectedCandidateText;
    std::string currentPinyin;
    
    if (frequencyLearningEnabled_ && isComposing()) {
        auto menu = rime.getCandidateMenu(sessionId_);
        currentPinyin = rime.getRawInput(sessionId_);
        
        // 空格键选择首选候选词（非展开模式）
        if (!isExpanded_ && keyCode == KeyCode::Space && !menu.candidates.empty()) {
            selectedCandidateText = menu.candidates[0].text;
            std::cout << "InputEngine: Space key, selecting first candidate: '" 
                      << selectedCandidateText << "' for pinyin: '" << currentPinyin << "'" << std::endl;
        }
        // 数字键选择对应候选词（1-9）
        else if (keyCode >= '1' && keyCode <= '9' && modifiers == 0) {
            int candidateIndex = keyCode - '1';  // 转换为 0-based
            if (candidateIndex < static_cast<int>(menu.candidates.size())) {
                selectedCandidateText = menu.candidates[candidateIndex].text;
                std::cout << "InputEngine: Number key " << (char)keyCode 
                          << ", selecting candidate[" << candidateIndex << "]: '" 
                          << selectedCandidateText << "' for pinyin: '" << currentPinyin << "'" << std::endl;
            }
            // 打印所有候选词用于调试
            std::cout << "InputEngine: All candidates for '" << currentPinyin << "':" << std::endl;
            for (size_t i = 0; i < menu.candidates.size(); ++i) {
                std::cout << "  [" << i << "] " << menu.candidates[i].text << std::endl;
            }
        }
    }

    // 将按键传递给 RIME
    bool processed = rime.processKey(sessionId_, keyCode, modifiers);

    // 检查是否有提交的文本
    std::string commitText = rime.getCommitText(sessionId_);
    if (!commitText.empty()) {
        // 更新选中候选词的词频
        if (frequencyLearningEnabled_ && !selectedCandidateText.empty()) {
            updateFrequencyForSelectedCandidate(selectedCandidateText, currentPinyin);
        }
        
        // 通过回调提交文本（回调会调用 MacOSBridge::commitText）
        notifyCommitText(commitText);
        
        // 提交后重置导航状态
        expandedCandidates_.clear();
        currentRow_ = 0;
        currentCol_ = 0;
        
        // 调试：检查提交后是否还有剩余输入
        std::string remainingInput = rime.getRawInput(sessionId_);
        if (!remainingInput.empty()) {
            std::cout << "InputEngine: After commit, remaining input: " << remainingInput << std::endl;
        }
    }

    // 更新状态
    updateState();
    notifyStateChanged();

    return processed;
}

bool InputEngine::handleEnglishMode(int keyCode, int modifiers) {
    // 英文模式：Shift 键切换回中文（由 IMKBridge 处理）
    // 英文模式下不处理任何按键，直接传递给应用
    // 返回 false 让系统/应用处理这个按键
    return false;
}

bool InputEngine::handleTempEnglishMode(int keyCode, int modifiers) {
    // 空格或回车：提交临时英文并退出
    if (keyCode == KeyCode::Space || keyCode == KeyCode::Return) {
        commitTempEnglishBuffer();
        exitTempEnglishMode();
        return true;
    }

    // Escape：取消临时英文
    if (keyCode == KeyCode::Escape) {
        tempEnglishBuffer_.clear();
        exitTempEnglishMode();
        notifyStateChanged();
        return true;
    }

    // Backspace：删除最后一个字符
    if (keyCode == KeyCode::BackSpace) {
        if (!tempEnglishBuffer_.empty()) {
            tempEnglishBuffer_.pop_back();
            if (tempEnglishBuffer_.empty()) {
                exitTempEnglishMode();
            }
            notifyStateChanged();
        }
        return true;
    }

    // 字母和数字：添加到缓冲区
    if (isAlphaKey(keyCode) || isDigitKey(keyCode)) {
        char c = static_cast<char>(keyCode);
        tempEnglishBuffer_ += c;
        notifyStateChanged();
        return true;
    }

    // 其他按键：提交当前缓冲区并退出临时英文模式
    if (!tempEnglishBuffer_.empty()) {
        commitTempEnglishBuffer();
    }
    exitTempEnglishMode();
    return false;  // 让后续按键由中文模式处理
}

// ========== 候选词操作 ==========

bool InputEngine::selectCandidate(int index) {
    if (!initialized_ || !isComposing()) {
        return false;
    }

    // index 是 1-based (1-9)，转换为 0-based
    if (index < 1 || index > 9) {
        return false;
    }

    auto& rime = RimeWrapper::instance();
    
    // 在选择之前，获取当前候选词信息用于词频更新
    std::string selectedText;
    std::string currentPinyin;
    
    if (frequencyLearningEnabled_) {
        auto menu = rime.getCandidateMenu(sessionId_);
        int candidateIndex = index - 1;  // 转换为 0-based
        
        if (candidateIndex >= 0 && candidateIndex < static_cast<int>(menu.candidates.size())) {
            selectedText = menu.candidates[candidateIndex].text;
        }
        
        // 获取当前输入的拼音
        currentPinyin = rime.getRawInput(sessionId_);
    }
    
    bool success = rime.selectCandidateOnCurrentPage(sessionId_, index - 1);

    if (success) {
        // 检查是否有提交的文本
        std::string commitText = rime.getCommitText(sessionId_);
        if (!commitText.empty()) {
            // 更新词频（在提交文本之前）
            if (frequencyLearningEnabled_ && !selectedText.empty()) {
                updateFrequencyForSelectedCandidate(selectedText, currentPinyin);
            }
            
            // 通过回调提交文本
            notifyCommitText(commitText);
            
            // 调试：检查提交后是否还有剩余输入
            std::string remainingInput = rime.getRawInput(sessionId_);
            std::cout << "InputEngine::selectCandidate: commitText='" << commitText 
                      << "', remainingInput='" << remainingInput << "'" << std::endl;
        }

        updateState();
        notifyStateChanged();
    }

    return success;
}

bool InputEngine::pageUp() {
    if (!initialized_ || !isComposing()) {
        return false;
    }

    auto& rime = RimeWrapper::instance();
    bool success = rime.changePage(sessionId_, true);  // backward = true

    if (success) {
        updateState();
        notifyStateChanged();
    }

    return success;
}

bool InputEngine::pageDown() {
    if (!initialized_ || !isComposing()) {
        return false;
    }

    auto& rime = RimeWrapper::instance();
    bool success = rime.changePage(sessionId_, false);  // backward = false

    if (success) {
        updateState();
        notifyStateChanged();
    }

    return success;
}

// ========== 模式切换 ==========

void InputEngine::toggleMode() {
    if (mode_ == InputMode::Chinese) {
        setMode(InputMode::English);
    } else {
        setMode(InputMode::Chinese);
    }
}

void InputEngine::setMode(InputMode mode) {
    if (mode_ == mode) {
        return;
    }

    // 如果正在输入，先重置
    if (isComposing()) {
        reset();
    }

    // 清空临时英文缓冲区
    if (mode_ == InputMode::TempEnglish) {
        tempEnglishBuffer_.clear();
    }

    mode_ = mode;

    // 同步到 RIME 的 ascii_mode 选项
    if (initialized_ && sessionId_ != 0) {
        auto& rime = RimeWrapper::instance();
        rime.setOption(sessionId_, "ascii_mode", mode == InputMode::English);
    }

    notifyStateChanged();
}

// ========== 状态管理 ==========

InputState InputEngine::getState() const {
    InputState state;
    state.mode = mode_;
    state.isComposing = false;
    state.highlightedIndex = 0;
    state.pageIndex = 0;
    state.pageSize = 9;
    state.hasMorePages = false;
    
    // 初始化展开状态字段
    state.isExpanded = isExpanded_;
    state.expandedRows = expandedRows_;
    state.currentRow = currentRow_;
    state.currentCol = currentCol_;
    state.totalCandidates = static_cast<int>(expandedCandidates_.size());

    if (!initialized_ || sessionId_ == 0) {
        return state;
    }

    // 临时英文模式
    if (mode_ == InputMode::TempEnglish) {
        state.preedit = tempEnglishBuffer_;
        state.rawInput = tempEnglishBuffer_;
        state.isComposing = !tempEnglishBuffer_.empty();
        return state;
    }

    // 中文模式：从 RIME 获取状态
    auto& rime = RimeWrapper::instance();

    // 获取组合信息
    auto composition = rime.getComposition(sessionId_);
    state.preedit = composition.preedit;
    state.rawInput = rime.getRawInput(sessionId_);

    // 获取候选词菜单
    auto menu = rime.getCandidateMenu(sessionId_);
    state.pageIndex = menu.pageIndex;
    state.pageSize = menu.pageSize > 0 ? menu.pageSize : 9;
    state.hasMorePages = !menu.isLastPage;
    
    // 展开模式下的高亮索引计算
    if (isExpanded_) {
        // 计算显示窗口（最多显示5行，当前行尽量在中间或靠下）
        int totalRows = (static_cast<int>(expandedCandidates_.size()) + state.pageSize - 1) / state.pageSize;
        int displayRows = std::min(5, totalRows);
        
        // 计算显示窗口的起始行
        int windowStartRow = 0;
        if (currentRow_ >= displayRows - 1) {
            // 当前行在窗口底部或超出，滚动窗口
            windowStartRow = currentRow_ - (displayRows - 1);
        }
        // 确保不超出范围
        windowStartRow = std::min(windowStartRow, std::max(0, totalRows - displayRows));
        
        // 在展开模式下，高亮索引是相对于显示窗口的
        state.highlightedIndex = currentRow_ * state.pageSize + currentCol_;
        
        // 只返回显示窗口内的候选词
        state.candidates.clear();
        int startIdx = windowStartRow * state.pageSize;
        int endIdx = std::min(startIdx + displayRows * state.pageSize, 
                              static_cast<int>(expandedCandidates_.size()));
        
        for (int i = startIdx; i < endIdx; ++i) {
            InputCandidate c = expandedCandidates_[i];
            c.index = i + 1;  // 保持全局索引
            state.candidates.push_back(c);
        }
        
        state.totalCandidates = static_cast<int>(expandedCandidates_.size());
        state.expandedRows = displayRows;
        // 调整 currentRow_ 为相对于显示窗口的行号
        state.currentRow = currentRow_ - windowStartRow;
        state.currentCol = currentCol_;
    } else {
        // 未展开模式：如果用户使用了方向键导航，使用我们维护的高亮索引
        // 否则使用 RIME 的高亮索引
        if (!expandedCandidates_.empty()) {
            // 用户已经使用过方向键，使用我们维护的索引
            state.highlightedIndex = currentCol_;
            state.currentRow = 0;
            state.currentCol = currentCol_;
        } else {
            state.highlightedIndex = menu.highlightedIndex;
        }
        
        // 转换候选词
        std::vector<InputCandidate> originalCandidates;
        for (size_t i = 0; i < menu.candidates.size(); ++i) {
            InputCandidate candidate;
            candidate.text = menu.candidates[i].text;
            candidate.comment = menu.candidates[i].comment;
            candidate.index = static_cast<int>(i + 1);  // 1-based
            originalCandidates.push_back(candidate);
        }

        // 注意：不再在 UI 层面重新排序候选词，因为这会导致显示和实际选择不一致
        // RIME 自己会根据用户选择学习词频
        state.candidates = originalCandidates;
    }

    // 检查是否正在输入
    auto rimeState = rime.getState(sessionId_);
    state.isComposing = rimeState.isComposing;

    return state;
}

void InputEngine::reset() {
    if (!initialized_ || sessionId_ == 0) {
        return;
    }

    // 清空临时英文缓冲区
    tempEnglishBuffer_.clear();

    // 清空 RIME 输入
    auto& rime = RimeWrapper::instance();
    rime.clearComposition(sessionId_);

    // 如果在临时英文模式，退出
    if (mode_ == InputMode::TempEnglish) {
        mode_ = InputMode::Chinese;
    }

    // 清除平台层的 preedit
    if (platformBridge_) {
        platformBridge_->clearPreedit();
    }

    updateState();
    notifyStateChanged();
}

void InputEngine::commit() {
    if (!initialized_ || sessionId_ == 0) {
        return;
    }

    // 临时英文模式
    if (mode_ == InputMode::TempEnglish && !tempEnglishBuffer_.empty()) {
        commitTempEnglishBuffer();
        exitTempEnglishMode();
        return;
    }

    // 中文模式
    if (!isComposing()) {
        return;
    }

    auto& rime = RimeWrapper::instance();

    // 提交当前输入
    rime.commitComposition(sessionId_);

    // 获取提交的文本
    std::string commitText = rime.getCommitText(sessionId_);
    if (!commitText.empty()) {
        // 通过回调提交文本
        notifyCommitText(commitText);
    }

    updateState();
    notifyStateChanged();
}

bool InputEngine::isComposing() const {
    if (!initialized_ || sessionId_ == 0) {
        return false;
    }

    // 临时英文模式
    if (mode_ == InputMode::TempEnglish) {
        return !tempEnglishBuffer_.empty();
    }

    // 中文模式
    auto& rime = RimeWrapper::instance();
    auto state = rime.getState(sessionId_);
    return state.isComposing;
}

// ========== 回调设置 ==========

void InputEngine::setStateChangedCallback(StateChangedCallback callback) {
    stateChangedCallback_ = std::move(callback);
}

void InputEngine::setCommitTextCallback(CommitTextCallback callback) {
    commitTextCallback_ = std::move(callback);
}

// ========== 激活/停用 ==========

void InputEngine::activate() {
    active_ = true;
    // 可以在这里恢复之前的状态
}

void InputEngine::deactivate() {
    // 停用前提交或清空当前输入
    if (isComposing()) {
        reset();
    }
    active_ = false;
}

// ========== 内部方法 ==========

void InputEngine::updateState() {
    cachedState_ = getState();
}

void InputEngine::notifyStateChanged() {
    if (stateChangedCallback_) {
        stateChangedCallback_(getState());
    }
}

void InputEngine::notifyCommitText(const std::string& text) {
    if (!text.empty()) {
        // 记录最后提交的字符（用于数字后标点智能转换）
        lastCommittedChar_ = text.back();
    }
    if (commitTextCallback_) {
        commitTextCallback_(text);
    }
}

bool InputEngine::isAlphaKey(int keyCode) const {
    return (keyCode >= 'a' && keyCode <= 'z') ||
           (keyCode >= 'A' && keyCode <= 'Z');
}

bool InputEngine::isDigitKey(int keyCode) const {
    return keyCode >= '0' && keyCode <= '9';
}

bool InputEngine::isPunctuationKey(int keyCode) const {
    // 常见标点符号
    return keyCode == ',' || keyCode == '.' || keyCode == ';' ||
           keyCode == '\'' || keyCode == '[' || keyCode == ']' ||
           keyCode == '/' || keyCode == '\\' || keyCode == '-' ||
           keyCode == '=' || keyCode == '`';
}

bool InputEngine::shouldEnterTempEnglish(int keyCode, int modifiers) const {
    // 条件：Shift + 字母键，且不在输入状态
    // 这会产生大写字母，触发临时英文模式
    if ((modifiers & KeyModifier::Shift) && isAlphaKey(keyCode)) {
        // 检查是否是大写字母（Shift + 小写字母）
        if (keyCode >= 'A' && keyCode <= 'Z') {
            return true;
        }
    }
    return false;
}

void InputEngine::exitTempEnglishMode() {
    mode_ = InputMode::Chinese;
    tempEnglishBuffer_.clear();
}

void InputEngine::commitTempEnglishBuffer() {
    if (tempEnglishBuffer_.empty()) {
        return;
    }

    // 通过回调提交文本
    notifyCommitText(tempEnglishBuffer_);
    tempEnglishBuffer_.clear();
}

// ========== 词频学习相关 ==========

void InputEngine::setFrequencyLearningEnabled(bool enabled) {
    frequencyLearningEnabled_ = enabled;
}

void InputEngine::setMinFrequencyForSorting(int minFrequency) {
    if (minFrequency >= 0) {
        minFrequencyForSorting_ = minFrequency;
    }
}

void InputEngine::updateFrequencyForSelectedCandidate(const std::string& text, 
                                                       const std::string& pinyin) {
    if (text.empty()) {
        return;
    }
    
    auto& freqMgr = FrequencyManager::instance();
    if (!freqMgr.isInitialized()) {
        // FrequencyManager 未初始化，跳过词频更新
        return;
    }
    
    // 更新词频
    bool success = freqMgr.updateFrequency(text, pinyin);
    if (success) {
        std::cout << "InputEngine: Updated frequency for '" << text 
                  << "' (pinyin: " << pinyin << ")" << std::endl;
    }
}

std::vector<InputCandidate> InputEngine::applySortingWithUserFrequency(
    const std::vector<InputCandidate>& candidates,
    const std::string& pinyin) const {
    
    if (candidates.empty()) {
        return candidates;
    }
    
    auto& freqMgr = FrequencyManager::instance();
    if (!freqMgr.isInitialized()) {
        // FrequencyManager 未初始化，返回原始顺序
        return candidates;
    }
    
    // 构建候选词列表用于排序
    std::vector<std::pair<std::string, std::string>> candidatePairs;
    for (const auto& c : candidates) {
        candidatePairs.emplace_back(c.text, c.comment);
    }
    
    // 调用 FrequencyManager 进行排序
    auto sortedInfo = freqMgr.mergeSortCandidates(candidatePairs, pinyin, minFrequencyForSorting_);
    
    // 根据排序结果重建候选词列表
    std::vector<InputCandidate> result;
    result.reserve(sortedInfo.size());
    
    for (size_t i = 0; i < sortedInfo.size(); ++i) {
        InputCandidate candidate;
        candidate.text = sortedInfo[i].text;
        candidate.comment = sortedInfo[i].comment;
        candidate.index = static_cast<int>(i + 1);  // 1-based，重新编号
        result.push_back(candidate);
    }
    
    return result;
}

// ========== 箭头键导航（搜狗风格） ==========

bool InputEngine::handleArrowKeys(int keyCode) {
    auto& rime = RimeWrapper::instance();
    auto menu = rime.getCandidateMenu(sessionId_);
    
    int pageSize = menu.pageSize > 0 ? menu.pageSize : 9;
    
    // 获取当前布局类型
    auto& configMgr = ConfigManager::instance();
    LayoutType layoutType = configMgr.getLayoutConfig().type;
    bool isVertical = (layoutType == LayoutType::Vertical);
    
    // 如果还没有加载候选词，先加载当前页的候选词
    // 注意：只在 expandedCandidates_ 为空时初始化，避免每次按键都重置
    if (expandedCandidates_.empty()) {
        expandedRows_ = 1;
        currentRow_ = 0;
        currentCol_ = 0;
        
        // 加载当前页的候选词
        for (const auto& c : menu.candidates) {
            InputCandidate candidate;
            candidate.text = c.text;
            candidate.comment = c.comment;
            candidate.index = static_cast<int>(expandedCandidates_.size() + 1);
            expandedCandidates_.push_back(candidate);
        }
    }
    
    // 辅助函数：加载更多候选词直到达到指定数量
    auto loadMoreCandidates = [&](int neededCandidates) {
        while (static_cast<int>(expandedCandidates_.size()) < neededCandidates) {
            auto currentMenu = rime.getCandidateMenu(sessionId_);
            if (currentMenu.isLastPage) {
                break;
            }
            
            bool changed = rime.changePage(sessionId_, false);
            if (!changed) {
                break;
            }
            
            currentMenu = rime.getCandidateMenu(sessionId_);
            for (const auto& c : currentMenu.candidates) {
                InputCandidate candidate;
                candidate.text = c.text;
                candidate.comment = c.comment;
                candidate.index = static_cast<int>(expandedCandidates_.size() + 1);
                expandedCandidates_.push_back(candidate);
            }
        }
    };
    
    // 辅助函数：获取当前组（行/列）的候选词数量
    // 横排模式：组 = 行，竖排模式：组 = 列
    auto getGroupSize = [&](int group) -> int {
        int groupStart = group * pageSize;
        int groupEnd = std::min(groupStart + pageSize, static_cast<int>(expandedCandidates_.size()));
        return groupEnd - groupStart;
    };
    
    // 辅助函数：获取总组数
    auto getTotalGroups = [&]() -> int {
        return (static_cast<int>(expandedCandidates_.size()) + pageSize - 1) / pageSize;
    };
    
    // ========== 未展开状态 ==========
    if (!isExpanded_) {
        // 判断是否是展开键
        // 横排模式：下键展开
        // 竖排模式：右键展开
        bool isExpandKey = isVertical ? (keyCode == KeyCode::Right) : (keyCode == KeyCode::Down);
        
        // 判断是否是组内移动键（未展开时在当前组内移动高亮）
        // 横排模式：左右键移动
        // 竖排模式：上下键移动
        bool isMoveNext = isVertical ? (keyCode == KeyCode::Down) : (keyCode == KeyCode::Right);
        bool isMovePrev = isVertical ? (keyCode == KeyCode::Up) : (keyCode == KeyCode::Left);
        
        if (isExpandKey) {
            // 展开候选词框
            isExpanded_ = true;
            currentRow_ = 1;  // 移动到第二组
            currentCol_ = 0;
            
            // 加载5组的候选词
            loadMoreCandidates(5 * pageSize);
            
            // 计算实际组数（最多显示5组）
            int totalGroups = getTotalGroups();
            expandedRows_ = std::min(5, totalGroups);
            
            // 确保有第二组，如果没有则不进入展开模式
            if (expandedRows_ < 2) {
                isExpanded_ = false;
                expandedRows_ = 1;
                currentRow_ = 0;
                currentCol_ = 0;
                return true;  // 消费按键但不展开
            }
        } else if (isMoveNext) {
            // 在当前组内向下/向右移动高亮
            int groupSize = getGroupSize(currentRow_);
            if (currentCol_ < groupSize - 1) {
                currentCol_++;
            }
            // 到末尾不做任何事
        } else if (isMovePrev) {
            // 在当前组内向上/向左移动高亮
            if (currentCol_ > 0) {
                currentCol_--;
            }
            // 到开头不做任何事
        } else {
            // 其他方向键（横排的上键，竖排的左键）在未展开时不做任何事
            return true;
        }
    }
    // ========== 已展开状态 ==========
    else {
        int totalGroups = getTotalGroups();
        
        // 判断按键类型
        // 横排模式：上下键切换组（行），左右键组内移动
        // 竖排模式：左右键切换组（列），上下键组内移动
        bool isNextGroup = isVertical ? (keyCode == KeyCode::Right) : (keyCode == KeyCode::Down);
        bool isPrevGroup = isVertical ? (keyCode == KeyCode::Left) : (keyCode == KeyCode::Up);
        bool isMoveNext = isVertical ? (keyCode == KeyCode::Down) : (keyCode == KeyCode::Right);
        bool isMovePrev = isVertical ? (keyCode == KeyCode::Up) : (keyCode == KeyCode::Left);
        
        if (isNextGroup) {
            // 移动到下一组
            if (currentRow_ < totalGroups - 1) {
                currentRow_++;
                
                // 如果需要，加载更多候选词
                int neededCandidates = (currentRow_ + 2) * pageSize;
                loadMoreCandidates(neededCandidates);
                
                // 确保组内索引有效
                int groupSize = getGroupSize(currentRow_);
                if (currentCol_ >= groupSize) {
                    currentCol_ = groupSize - 1;
                }
            } else {
                // 尝试加载更多
                int oldSize = static_cast<int>(expandedCandidates_.size());
                loadMoreCandidates(oldSize + pageSize);
                
                if (static_cast<int>(expandedCandidates_.size()) > oldSize) {
                    currentRow_++;
                    currentCol_ = 0;
                }
            }
        } else if (isPrevGroup) {
            // 移动到上一组
            if (currentRow_ > 0) {
                currentRow_--;
                
                // 确保组内索引有效
                int groupSize = getGroupSize(currentRow_);
                if (currentCol_ >= groupSize) {
                    currentCol_ = groupSize - 1;
                }
            }
            // 在第一组时不做任何事
        } else if (isMoveNext) {
            // 在当前组内向下/向右移动
            int groupSize = getGroupSize(currentRow_);
            
            if (currentCol_ < groupSize - 1) {
                currentCol_++;
            }
            // 到组尾不跳转到下一组，保持在当前位置
        } else if (isMovePrev) {
            // 在当前组内向上/向左移动
            if (currentCol_ > 0) {
                currentCol_--;
            }
            // 到组头不跳转到上一组，保持在当前位置
        }
    }
    
    updateState();
    notifyStateChanged();
    return true;
}

void InputEngine::resetExpandedState() {
    isExpanded_ = false;
    expandedRows_ = 1;
    currentRow_ = 0;
    currentCol_ = 0;
    expandedCandidates_.clear();
    
    // 重置 RIME 到第一页
    if (initialized_ && sessionId_ != 0) {
        auto& rime = RimeWrapper::instance();
        auto menu = rime.getCandidateMenu(sessionId_);
        while (menu.pageIndex > 0) {
            rime.changePage(sessionId_, true);  // 向前翻页
            menu = rime.getCandidateMenu(sessionId_);
        }
    }
}

} // namespace suyan
