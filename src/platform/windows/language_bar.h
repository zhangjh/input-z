/**
 * LanguageBar - TSF 语言栏实现
 * 
 * 实现 ITfLangBarItemButton 接口，在 Windows 语言栏显示输入法状态图标。
 * 这是 Windows 输入法的标准方式，而不是使用普通的系统托盘图标。
 */

#ifndef SUYAN_WINDOWS_LANGUAGE_BAR_H
#define SUYAN_WINDOWS_LANGUAGE_BAR_H

#ifdef _WIN32

#include <windows.h>
#include <msctf.h>
#include <atomic>

namespace suyan {

// 前向声明
enum class InputMode;

// Language Bar Item GUID
// {C3D4E5F6-A7B8-9012-CDEF-123456789ABC}
extern const GUID GUID_LBI_SuYanButton;

/**
 * LanguageBarButton - 语言栏按钮实现
 * 
 * 实现 ITfLangBarItemButton 接口，显示中/英文状态图标
 */
class LanguageBarButton : public ITfLangBarItemButton,
                          public ITfSource {
public:
    LanguageBarButton();
    virtual ~LanguageBarButton();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfLangBarItem
    STDMETHODIMP GetInfo(TF_LANGBARITEMINFO* pInfo) override;
    STDMETHODIMP GetStatus(DWORD* pdwStatus) override;
    STDMETHODIMP Show(BOOL fShow) override;
    STDMETHODIMP GetTooltipString(BSTR* pbstrToolTip) override;

    // ITfLangBarItemButton
    STDMETHODIMP OnClick(TfLBIClick click, POINT pt, const RECT* prcArea) override;
    STDMETHODIMP InitMenu(ITfMenu* pMenu) override;
    STDMETHODIMP OnMenuSelect(UINT wID) override;
    STDMETHODIMP GetIcon(HICON* phIcon) override;
    STDMETHODIMP GetText(BSTR* pbstrText) override;

    // ITfSource
    STDMETHODIMP AdviseSink(REFIID riid, IUnknown* punk, DWORD* pdwCookie) override;
    STDMETHODIMP UnadviseSink(DWORD dwCookie) override;

    // 状态更新
    void updateIcon(InputMode mode);
    void setEnabled(bool enabled);

private:
    void notifySinkUpdate(DWORD dwFlags);
    HICON loadIconForMode(InputMode mode);

    std::atomic<ULONG> refCount_{1};
    ITfLangBarItemSink* sink_ = nullptr;
    DWORD sinkCookie_ = 0;
    InputMode currentMode_;
    bool enabled_ = true;
    bool visible_ = true;
};

/**
 * LanguageBar - 语言栏管理器
 * 
 * 管理语言栏按钮的创建、注册和更新
 */
class LanguageBar {
public:
    static LanguageBar& instance();

    bool initialize(ITfThreadMgr* threadMgr);
    void shutdown();
    void updateIcon(InputMode mode);
    bool isInitialized() const { return initialized_; }

private:
    LanguageBar();
    ~LanguageBar();
    LanguageBar(const LanguageBar&) = delete;
    LanguageBar& operator=(const LanguageBar&) = delete;

    bool initialized_ = false;
    ITfThreadMgr* threadMgr_ = nullptr;
    ITfLangBarItemMgr* langBarItemMgr_ = nullptr;
    LanguageBarButton* button_ = nullptr;
};

} // namespace suyan

#endif // _WIN32

#endif // SUYAN_WINDOWS_LANGUAGE_BAR_H
