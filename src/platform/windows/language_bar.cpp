/**
 * LanguageBar 实现
 * 
 * TSF 语言栏按钮实现，用于显示输入法状态
 */

#ifdef _WIN32

#include "language_bar.h"
#include "tsf_bridge.h"
#include "input_engine.h"
#include <strsafe.h>
#include <olectl.h>

namespace suyan {

// Language Bar Item GUID
const GUID GUID_LBI_SuYanButton = {
    0xC3D4E5F6, 0xA7B8, 0x9012,
    { 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC }
};

// ============================================
// LanguageBarButton 实现
// ============================================

LanguageBarButton::LanguageBarButton()
    : currentMode_(InputMode::Chinese) {
}

LanguageBarButton::~LanguageBarButton() {
    if (sink_) {
        sink_->Release();
        sink_ = nullptr;
    }
}

// IUnknown
STDMETHODIMP LanguageBarButton::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ITfLangBarItem) ||
        IsEqualIID(riid, IID_ITfLangBarItemButton)) {
        *ppvObj = static_cast<ITfLangBarItemButton*>(this);
    } else if (IsEqualIID(riid, IID_ITfSource)) {
        *ppvObj = static_cast<ITfSource*>(this);
    } else {
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) LanguageBarButton::AddRef() {
    return ++refCount_;
}

STDMETHODIMP_(ULONG) LanguageBarButton::Release() {
    ULONG count = --refCount_;
    if (count == 0) {
        delete this;
    }
    return count;
}

// ITfLangBarItem
STDMETHODIMP LanguageBarButton::GetInfo(TF_LANGBARITEMINFO* pInfo) {
    if (!pInfo) return E_INVALIDARG;

    pInfo->clsidService = CLSID_SuYanTextService;
    pInfo->guidItem = GUID_LBI_SuYanButton;
    pInfo->dwStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_SHOWNINTRAY;
    pInfo->ulSort = 0;
    StringCchCopyW(pInfo->szDescription, ARRAYSIZE(pInfo->szDescription), L"素言输入法");

    return S_OK;
}

STDMETHODIMP LanguageBarButton::GetStatus(DWORD* pdwStatus) {
    if (!pdwStatus) return E_INVALIDARG;

    *pdwStatus = 0;
    if (!enabled_) {
        *pdwStatus |= TF_LBI_STATUS_DISABLED;
    }
    if (!visible_) {
        *pdwStatus |= TF_LBI_STATUS_HIDDEN;
    }

    return S_OK;
}

STDMETHODIMP LanguageBarButton::Show(BOOL fShow) {
    visible_ = (fShow != FALSE);
    notifySinkUpdate(TF_LBI_STATUS);
    return S_OK;
}

STDMETHODIMP LanguageBarButton::GetTooltipString(BSTR* pbstrToolTip) {
    if (!pbstrToolTip) return E_INVALIDARG;

    const wchar_t* tooltip = (currentMode_ == InputMode::Chinese) 
        ? L"素言输入法 - 中文" 
        : L"素言输入法 - 英文";
    
    *pbstrToolTip = SysAllocString(tooltip);
    return (*pbstrToolTip) ? S_OK : E_OUTOFMEMORY;
}

// ITfLangBarItemButton
STDMETHODIMP LanguageBarButton::OnClick(TfLBIClick click, POINT pt, const RECT* prcArea) {
    (void)pt;
    (void)prcArea;

    if (click == TF_LBI_CLK_LEFT) {
        // 左键点击：切换中英文模式
        InputEngine* engine = GetInputEngine();
        if (engine) {
            engine->toggleMode();
        }
    }

    return S_OK;
}

STDMETHODIMP LanguageBarButton::InitMenu(ITfMenu* pMenu) {
    if (!pMenu) return E_INVALIDARG;

    // 添加菜单项
    pMenu->AddMenuItem(1, 0, nullptr, nullptr, L"切换中/英文", 6, nullptr);
    pMenu->AddMenuItem(0, TF_LBMENUF_SEPARATOR, nullptr, nullptr, nullptr, 0, nullptr);
    pMenu->AddMenuItem(2, 0, nullptr, nullptr, L"关于素言", 4, nullptr);

    return S_OK;
}

STDMETHODIMP LanguageBarButton::OnMenuSelect(UINT wID) {
    switch (wID) {
        case 1: {
            // 切换中英文
            InputEngine* engine = GetInputEngine();
            if (engine) {
                engine->toggleMode();
            }
            break;
        }
        case 2:
            // 关于
            MessageBoxW(nullptr, L"素言输入法 v1.0.0\n\n基于 RIME 引擎", 
                       L"关于素言", MB_OK | MB_ICONINFORMATION);
            break;
    }

    return S_OK;
}

STDMETHODIMP LanguageBarButton::GetIcon(HICON* phIcon) {
    if (!phIcon) return E_INVALIDARG;

    *phIcon = loadIconForMode(currentMode_);
    return (*phIcon) ? S_OK : E_FAIL;
}

STDMETHODIMP LanguageBarButton::GetText(BSTR* pbstrText) {
    if (!pbstrText) return E_INVALIDARG;

    const wchar_t* text = (currentMode_ == InputMode::Chinese) ? L"中" : L"英";
    *pbstrText = SysAllocString(text);
    return (*pbstrText) ? S_OK : E_OUTOFMEMORY;
}

// ITfSource
STDMETHODIMP LanguageBarButton::AdviseSink(REFIID riid, IUnknown* punk, DWORD* pdwCookie) {
    if (!punk || !pdwCookie) return E_INVALIDARG;

    if (!IsEqualIID(riid, IID_ITfLangBarItemSink)) {
        return CONNECT_E_CANNOTCONNECT;
    }

    if (sink_) {
        return CONNECT_E_ADVISELIMIT;
    }

    HRESULT hr = punk->QueryInterface(IID_ITfLangBarItemSink, 
                                       reinterpret_cast<void**>(&sink_));
    if (FAILED(hr)) {
        return hr;
    }

    *pdwCookie = ++sinkCookie_;
    return S_OK;
}

STDMETHODIMP LanguageBarButton::UnadviseSink(DWORD dwCookie) {
    if (dwCookie != sinkCookie_ || !sink_) {
        return CONNECT_E_NOCONNECTION;
    }

    sink_->Release();
    sink_ = nullptr;
    sinkCookie_ = 0;

    return S_OK;
}

void LanguageBarButton::updateIcon(InputMode mode) {
    if (currentMode_ != mode) {
        currentMode_ = mode;
        notifySinkUpdate(TF_LBI_ICON | TF_LBI_TEXT | TF_LBI_TOOLTIP);
    }
}

void LanguageBarButton::setEnabled(bool enabled) {
    if (enabled_ != enabled) {
        enabled_ = enabled;
        notifySinkUpdate(TF_LBI_STATUS);
    }
}

void LanguageBarButton::notifySinkUpdate(DWORD dwFlags) {
    if (sink_) {
        sink_->OnUpdate(dwFlags);
    }
}

HICON LanguageBarButton::loadIconForMode(InputMode mode) {
    // 从资源加载图标
    HMODULE hModule = GetModuleHandle();
    if (!hModule) return nullptr;

    // 使用资源 ID（在 resource.h 中定义）
    // IDI_CHINESE_MODE = 102, IDI_ENGLISH_MODE = 103
    int iconId = (mode == InputMode::Chinese) ? 102 : 103;
    
    HICON hIcon = LoadIconW(hModule, MAKEINTRESOURCEW(iconId));
    if (hIcon) return hIcon;

    // 如果资源加载失败，创建一个简单的文字图标
    HDC hdc = GetDC(nullptr);
    if (!hdc) return nullptr;

    int size = GetSystemMetrics(SM_CXSMICON);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdc, size, size);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(memDC, hBitmap);

    // 填充背景
    RECT rect = { 0, 0, size, size };
    HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(memDC, &rect, hBrush);
    DeleteObject(hBrush);

    // 绘制文字
    const wchar_t* text = (mode == InputMode::Chinese) ? L"中" : L"英";
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, (mode == InputMode::Chinese) ? RGB(0, 100, 200) : RGB(100, 100, 100));
    
    HFONT hFont = CreateFontW(size - 2, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
    HFONT hOldFont = (HFONT)SelectObject(memDC, hFont);
    DrawTextW(memDC, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(memDC, hOldFont);
    DeleteObject(hFont);

    SelectObject(memDC, hOldBitmap);

    // 创建掩码位图
    HBITMAP hMask = CreateBitmap(size, size, 1, 1, nullptr);
    HDC maskDC = CreateCompatibleDC(hdc);
    HBITMAP hOldMask = (HBITMAP)SelectObject(maskDC, hMask);
    RECT maskRect = { 0, 0, size, size };
    FillRect(maskDC, &maskRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
    SelectObject(maskDC, hOldMask);
    DeleteDC(maskDC);

    // 创建图标
    ICONINFO iconInfo = {};
    iconInfo.fIcon = TRUE;
    iconInfo.hbmMask = hMask;
    iconInfo.hbmColor = hBitmap;
    hIcon = CreateIconIndirect(&iconInfo);

    DeleteObject(hBitmap);
    DeleteObject(hMask);
    DeleteDC(memDC);
    ReleaseDC(nullptr, hdc);

    return hIcon;
}

// ============================================
// LanguageBar 实现
// ============================================

LanguageBar& LanguageBar::instance() {
    static LanguageBar instance;
    return instance;
}

LanguageBar::LanguageBar() = default;

LanguageBar::~LanguageBar() {
    shutdown();
}

bool LanguageBar::initialize(ITfThreadMgr* threadMgr) {
    if (initialized_) return true;
    if (!threadMgr) return false;

    threadMgr_ = threadMgr;
    threadMgr_->AddRef();

    // 获取 ITfLangBarItemMgr
    HRESULT hr = threadMgr_->QueryInterface(IID_ITfLangBarItemMgr,
                                             reinterpret_cast<void**>(&langBarItemMgr_));
    if (FAILED(hr) || !langBarItemMgr_) {
        threadMgr_->Release();
        threadMgr_ = nullptr;
        return false;
    }

    // 创建并添加按钮
    button_ = new LanguageBarButton();
    hr = langBarItemMgr_->AddItem(button_);
    if (FAILED(hr)) {
        button_->Release();
        button_ = nullptr;
        langBarItemMgr_->Release();
        langBarItemMgr_ = nullptr;
        threadMgr_->Release();
        threadMgr_ = nullptr;
        return false;
    }

    initialized_ = true;
    return true;
}

void LanguageBar::shutdown() {
    if (!initialized_) return;

    if (langBarItemMgr_ && button_) {
        langBarItemMgr_->RemoveItem(button_);
    }

    if (button_) {
        button_->Release();
        button_ = nullptr;
    }

    if (langBarItemMgr_) {
        langBarItemMgr_->Release();
        langBarItemMgr_ = nullptr;
    }

    if (threadMgr_) {
        threadMgr_->Release();
        threadMgr_ = nullptr;
    }

    initialized_ = false;
}

void LanguageBar::updateIcon(InputMode mode) {
    if (button_) {
        button_->updateIcon(mode);
    }
}

} // namespace suyan

#endif // _WIN32
