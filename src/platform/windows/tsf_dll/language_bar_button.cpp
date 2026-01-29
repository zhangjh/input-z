#include "language_bar_button.h"
#include <strsafe.h>
#include <olectl.h>

namespace suyan {

// {C3D4E5F6-A7B8-9012-CDEF-123456789ABC}
const GUID GUID_LangBarButton = {
    0xC3D4E5F6, 0xA7B8, 0x9012,
    {0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC}
};

LanguageBarButton::LanguageBarButton()
    : m_refCount(1)
    , m_pLangBarItemMgr(nullptr)
    , m_pLangBarItemSink(nullptr)
    , m_dwSinkCookie(0)
    , m_clientId(TF_CLIENTID_NULL)
    , m_hModule(nullptr)
    , m_clsidTextService(CLSID_NULL)
    , m_bAddedToLangBar(false) {
}

LanguageBarButton::~LanguageBarButton() {
    Uninitialize();
}

bool LanguageBarButton::Initialize(ITfThreadMgr* pThreadMgr, TfClientId clientId, HMODULE hModule, REFCLSID clsidTextService) {
    if (!pThreadMgr) return false;

    m_clientId = clientId;
    m_hModule = hModule;
    m_clsidTextService = clsidTextService;

    // Get Language Bar Item Manager
    HRESULT hr = pThreadMgr->QueryInterface(IID_ITfLangBarItemMgr,
                                             reinterpret_cast<void**>(&m_pLangBarItemMgr));
    if (FAILED(hr) || !m_pLangBarItemMgr) {
        return false;
    }

    // Add this button to the language bar
    hr = m_pLangBarItemMgr->AddItem(static_cast<ITfLangBarItemButton*>(this));
    if (FAILED(hr)) {
        m_pLangBarItemMgr->Release();
        m_pLangBarItemMgr = nullptr;
        return false;
    }

    m_bAddedToLangBar = true;
    return true;
}

void LanguageBarButton::Uninitialize() {
    if (m_pLangBarItemMgr) {
        if (m_bAddedToLangBar) {
            m_pLangBarItemMgr->RemoveItem(static_cast<ITfLangBarItemButton*>(this));
            m_bAddedToLangBar = false;
        }
        m_pLangBarItemMgr->Release();
        m_pLangBarItemMgr = nullptr;
    }

    if (m_pLangBarItemSink) {
        m_pLangBarItemSink->Release();
        m_pLangBarItemSink = nullptr;
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
    return InterlockedIncrement(&m_refCount);
}

STDMETHODIMP_(ULONG) LanguageBarButton::Release() {
    LONG count = InterlockedDecrement(&m_refCount);
    if (count == 0) {
        delete this;
    }
    return count;
}

// ITfLangBarItem
STDMETHODIMP LanguageBarButton::GetInfo(TF_LANGBARITEMINFO* pInfo) {
    if (!pInfo) return E_INVALIDARG;

    pInfo->clsidService = m_clsidTextService;
    pInfo->guidItem = GUID_LangBarButton;
    pInfo->dwStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_SHOWNINTRAY;
    pInfo->ulSort = 0;
    StringCchCopyW(pInfo->szDescription, ARRAYSIZE(pInfo->szDescription), L"SuYan Input Method");

    return S_OK;
}

STDMETHODIMP LanguageBarButton::GetStatus(DWORD* pdwStatus) {
    if (!pdwStatus) return E_INVALIDARG;
    *pdwStatus = 0;  // Enabled
    return S_OK;
}

STDMETHODIMP LanguageBarButton::Show(BOOL fShow) {
    (void)fShow;
    return S_OK;
}

STDMETHODIMP LanguageBarButton::GetTooltipString(BSTR* pbstrToolTip) {
    if (!pbstrToolTip) return E_INVALIDARG;
    *pbstrToolTip = SysAllocString(L"素言输入法");
    return (*pbstrToolTip) ? S_OK : E_OUTOFMEMORY;
}

// ITfLangBarItemButton
STDMETHODIMP LanguageBarButton::OnClick(TfLBIClick click, POINT pt, const RECT* prcArea) {
    (void)click;
    (void)pt;
    (void)prcArea;
    // Can handle click events here if needed
    return S_OK;
}

STDMETHODIMP LanguageBarButton::InitMenu(ITfMenu* pMenu) {
    if (!pMenu) return E_INVALIDARG;

    // Add menu items
    pMenu->AddMenuItem(1, 0, nullptr, nullptr, L"设置(&S)", 6, nullptr);
    pMenu->AddMenuItem(0, TF_LBMENUF_SEPARATOR, nullptr, nullptr, nullptr, 0, nullptr);
    pMenu->AddMenuItem(2, 0, nullptr, nullptr, L"退出(&Q)", 6, nullptr);

    return S_OK;
}

STDMETHODIMP LanguageBarButton::OnMenuSelect(UINT wID) {
    switch (wID) {
        case 1:  // Settings
            // TODO: Open settings
            break;
        case 2:  // Quit
            // TODO: Send quit command via IPC
            break;
    }
    return S_OK;
}

STDMETHODIMP LanguageBarButton::GetIcon(HICON* phIcon) {
    if (!phIcon) return E_INVALIDARG;

    // Load icon from module resources
    if (m_hModule) {
        *phIcon = static_cast<HICON>(LoadImageW(m_hModule, MAKEINTRESOURCEW(101),
                                                 IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
        if (*phIcon) return S_OK;
    }

    // Fallback: use system icon
    *phIcon = static_cast<HICON>(LoadImageW(nullptr, IDI_APPLICATION,
                                             IMAGE_ICON, 16, 16, LR_SHARED));
    return (*phIcon) ? S_OK : E_FAIL;
}

STDMETHODIMP LanguageBarButton::GetText(BSTR* pbstrText) {
    if (!pbstrText) return E_INVALIDARG;
    *pbstrText = SysAllocString(L"素言");
    return (*pbstrText) ? S_OK : E_OUTOFMEMORY;
}

// ITfSource
STDMETHODIMP LanguageBarButton::AdviseSink(REFIID riid, IUnknown* punk, DWORD* pdwCookie) {
    if (!punk || !pdwCookie) return E_INVALIDARG;

    if (!IsEqualIID(riid, IID_ITfLangBarItemSink)) {
        return CONNECT_E_CANNOTCONNECT;
    }

    if (m_pLangBarItemSink) {
        return CONNECT_E_ADVISELIMIT;
    }

    HRESULT hr = punk->QueryInterface(IID_ITfLangBarItemSink,
                                       reinterpret_cast<void**>(&m_pLangBarItemSink));
    if (FAILED(hr)) {
        return hr;
    }

    *pdwCookie = ++m_dwSinkCookie;
    return S_OK;
}

STDMETHODIMP LanguageBarButton::UnadviseSink(DWORD dwCookie) {
    if (dwCookie != m_dwSinkCookie || !m_pLangBarItemSink) {
        return CONNECT_E_NOCONNECTION;
    }

    m_pLangBarItemSink->Release();
    m_pLangBarItemSink = nullptr;
    return S_OK;
}

} // namespace suyan
