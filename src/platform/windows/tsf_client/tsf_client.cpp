#include "tsf_client.h"
#include <strsafe.h>
#include <shellapi.h>
#include <fstream>

namespace suyan {

static void DebugLog(const char* msg) {
    std::ofstream f("C:\\temp\\suyan32_debug.log", std::ios::app);
    if (f.is_open()) {
        f << "[TSF] " << msg << std::endl;
    }
}

// 与64位DLL使用相同的CLSID和Profile GUID
const CLSID CLSID_SuYanTextService = {0xA1B2C3D4, 0xE5F6, 0x7890, {0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x90}};
const GUID GUID_SuYanProfile = {0xB2C3D4E5, 0xF6A7, 0x8901, {0xBC, 0xDE, 0xF1, 0x23, 0x45, 0x67, 0x89, 0x01}};

static HMODULE g_hModule = nullptr;
TSFClientFactory g_factory;
LONG g_serverLocks = 0;

HMODULE GetModuleHandle() { return g_hModule; }
void SetModuleHandle(HMODULE hModule) { g_hModule = hModule; }

// ============================================
// TSFClient
// ============================================

TSFClient::TSFClient()
    : m_refCount(1)
    , m_threadMgr(nullptr)
    , m_clientId(TF_CLIENTID_NULL)
    , m_threadMgrEventSinkCookie(TF_INVALID_COOKIE)
    , m_activated(false) {
}

TSFClient::~TSFClient() {
    if (m_activated) {
        Deactivate();
    }
}

STDMETHODIMP TSFClient::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_INVALIDARG;
    
    *ppvObj = nullptr;
    
    if (IsEqualIID(riid, IID_IUnknown) || 
        IsEqualIID(riid, IID_ITfTextInputProcessor) ||
        IsEqualIID(riid, IID_ITfTextInputProcessorEx)) {
        *ppvObj = static_cast<ITfTextInputProcessorEx*>(this);
    } else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink)) {
        *ppvObj = static_cast<ITfThreadMgrEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
        *ppvObj = static_cast<ITfKeyEventSink*>(this);
    } else {
        return E_NOINTERFACE;
    }
    
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) TSFClient::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

STDMETHODIMP_(ULONG) TSFClient::Release() {
    LONG count = InterlockedDecrement(&m_refCount);
    if (count == 0) {
        delete this;
    }
    return count;
}

STDMETHODIMP TSFClient::Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId) {
    return ActivateEx(pThreadMgr, tfClientId, 0);
}

STDMETHODIMP TSFClient::ActivateEx(ITfThreadMgr* pThreadMgr, TfClientId tfClientId, DWORD dwFlags) {
    (void)dwFlags;
    
    DebugLog("ActivateEx called");
    
    if (m_activated) {
        DebugLog("ActivateEx: already activated");
        return S_OK;
    }
    
    m_threadMgr = pThreadMgr;
    m_threadMgr->AddRef();
    m_clientId = tfClientId;
    
    ITfSource* source = nullptr;
    if (SUCCEEDED(m_threadMgr->QueryInterface(IID_ITfSource, (void**)&source))) {
        source->AdviseSink(IID_ITfThreadMgrEventSink, 
                          static_cast<ITfThreadMgrEventSink*>(this),
                          &m_threadMgrEventSinkCookie);
        source->Release();
    }
    
    ITfKeystrokeMgr* keystrokeMgr = nullptr;
    if (SUCCEEDED(m_threadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&keystrokeMgr))) {
        keystrokeMgr->AdviseKeyEventSink(m_clientId, static_cast<ITfKeyEventSink*>(this), TRUE);
        keystrokeMgr->Release();
        DebugLog("ActivateEx: KeyEventSink registered");
    }
    
    DebugLog("ActivateEx: trying IPC connect");
    if (!m_ipc.connect()) {
        DebugLog("ActivateEx: IPC connect failed, starting server");
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(g_hModule, path, MAX_PATH);
        wchar_t* lastSlash = wcsrchr(path, L'\\');
        if (lastSlash) {
            wcscpy_s(lastSlash + 1, MAX_PATH - (lastSlash - path + 1), L"SuYanServer.exe");
            ShellExecuteW(nullptr, L"open", path, nullptr, nullptr, SW_HIDE);
            for (int i = 0; i < 20 && !m_ipc.connect(); ++i) {
                Sleep(100);
            }
        }
    }
    
    if (m_ipc.isConnected()) {
        DebugLog("ActivateEx: IPC connected, starting session");
        m_ipc.startSession();
        m_ipc.focusIn();
    } else {
        DebugLog("ActivateEx: IPC still not connected!");
    }
    
    m_activated = true;
    DebugLog("ActivateEx: done");
    return S_OK;
}

STDMETHODIMP TSFClient::Deactivate() {
    if (!m_activated) return S_OK;
    
    m_ipc.disconnect();
    releaseSinks();
    
    if (m_threadMgr) {
        m_threadMgr->Release();
        m_threadMgr = nullptr;
    }
    
    m_clientId = TF_CLIENTID_NULL;
    m_activated = false;
    return S_OK;
}

bool TSFClient::setupSinks() {
    ITfSource* source = nullptr;
    if (FAILED(m_threadMgr->QueryInterface(IID_ITfSource, (void**)&source))) {
        return false;
    }
    
    HRESULT hr = source->AdviseSink(IID_ITfThreadMgrEventSink, 
                                     static_cast<ITfThreadMgrEventSink*>(this),
                                     &m_threadMgrEventSinkCookie);
    source->Release();
    
    if (FAILED(hr)) return false;
    
    ITfKeystrokeMgr* keystrokeMgr = nullptr;
    if (SUCCEEDED(m_threadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&keystrokeMgr))) {
        keystrokeMgr->AdviseKeyEventSink(m_clientId, static_cast<ITfKeyEventSink*>(this), TRUE);
        keystrokeMgr->Release();
    }
    
    return true;
}

void TSFClient::releaseSinks() {
    if (m_threadMgr) {
        ITfKeystrokeMgr* keystrokeMgr = nullptr;
        if (SUCCEEDED(m_threadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&keystrokeMgr))) {
            keystrokeMgr->UnadviseKeyEventSink(m_clientId);
            keystrokeMgr->Release();
        }
        
        if (m_threadMgrEventSinkCookie != TF_INVALID_COOKIE) {
            ITfSource* source = nullptr;
            if (SUCCEEDED(m_threadMgr->QueryInterface(IID_ITfSource, (void**)&source))) {
                source->UnadviseSink(m_threadMgrEventSinkCookie);
                source->Release();
            }
            m_threadMgrEventSinkCookie = TF_INVALID_COOKIE;
        }
    }
}

// ITfThreadMgrEventSink
STDMETHODIMP TSFClient::OnInitDocumentMgr(ITfDocumentMgr*) { return S_OK; }
STDMETHODIMP TSFClient::OnUninitDocumentMgr(ITfDocumentMgr*) { return S_OK; }
STDMETHODIMP TSFClient::OnPushContext(ITfContext*) { return S_OK; }
STDMETHODIMP TSFClient::OnPopContext(ITfContext*) { return S_OK; }

STDMETHODIMP TSFClient::OnSetFocus(ITfDocumentMgr* pDocMgrFocus, ITfDocumentMgr*) {
    (void)pDocMgrFocus;
    return S_OK;
}

// ITfKeyEventSink
STDMETHODIMP TSFClient::OnSetFocus(BOOL fForeground) {
    (void)fForeground;
    return S_OK;
}

STDMETHODIMP TSFClient::OnTestKeyDown(ITfContext*, WPARAM wParam, LPARAM, BOOL* pfEaten) {
    *pfEaten = FALSE;
    
    DWORD modifiers = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000) modifiers |= 1;
    if (GetKeyState(VK_CONTROL) & 0x8000) modifiers |= 2;
    if (GetKeyState(VK_MENU) & 0x8000) modifiers |= 4;
    
    *pfEaten = m_ipc.testKey(static_cast<DWORD>(wParam), modifiers) ? TRUE : FALSE;
    return S_OK;
}

STDMETHODIMP TSFClient::OnTestKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* pfEaten) {
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP TSFClient::OnKeyDown(ITfContext* pContext, WPARAM wParam, LPARAM, BOOL* pfEaten) {
    *pfEaten = FALSE;
    
    DWORD modifiers = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000) modifiers |= 1;
    if (GetKeyState(VK_CONTROL) & 0x8000) modifiers |= 2;
    if (GetKeyState(VK_MENU) & 0x8000) modifiers |= 4;
    
    if (m_ipc.processKey(static_cast<DWORD>(wParam), modifiers)) {
        *pfEaten = TRUE;
        
        updateCursorPosition(pContext);
        
        std::wstring commitText;
        if (m_ipc.getCommitText(commitText) && !commitText.empty()) {
            this->commitText(pContext, commitText);
        }
    }
    
    return S_OK;
}

STDMETHODIMP TSFClient::OnKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* pfEaten) {
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP TSFClient::OnPreservedKey(ITfContext*, REFGUID, BOOL* pfEaten) {
    *pfEaten = FALSE;
    return S_OK;
}

void TSFClient::commitText(ITfContext* pContext, const std::wstring& text) {
    if (!pContext || text.empty()) return;
    
    TfEditCookie ec;
    for (wchar_t ch : text) {
        INPUT input[2] = {};
        input[0].type = INPUT_KEYBOARD;
        input[0].ki.wVk = 0;
        input[0].ki.wScan = ch;
        input[0].ki.dwFlags = KEYEVENTF_UNICODE;
        
        input[1].type = INPUT_KEYBOARD;
        input[1].ki.wVk = 0;
        input[1].ki.wScan = ch;
        input[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        
        SendInput(2, input, sizeof(INPUT));
    }
}

void TSFClient::updateCursorPosition(ITfContext* pContext) {
    if (!pContext) return;
    
    HWND hwnd = GetFocus();
    if (!hwnd) return;
    
    GUITHREADINFO gti = { sizeof(GUITHREADINFO) };
    if (GetGUIThreadInfo(0, &gti) && gti.hwndCaret) {
        POINT pt = { gti.rcCaret.left, gti.rcCaret.bottom };
        ClientToScreen(gti.hwndCaret, &pt);
        RECT rc = { pt.x, pt.y - (gti.rcCaret.bottom - gti.rcCaret.top), 
                    pt.x + 1, pt.y };
        m_ipc.updatePosition(rc);
        return;
    }
    
    POINT caretPos;
    if (GetCaretPos(&caretPos)) {
        ClientToScreen(hwnd, &caretPos);
        RECT rc = { caretPos.x, caretPos.y, caretPos.x + 1, caretPos.y + 20 };
        m_ipc.updatePosition(rc);
    }
}

// ============================================
// TSFClientFactory
// ============================================

STDMETHODIMP TSFClientFactory::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_INVALIDARG;
    
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
        *ppvObj = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }
    
    *ppvObj = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) TSFClientFactory::AddRef() { return 1; }
STDMETHODIMP_(ULONG) TSFClientFactory::Release() { return 1; }

STDMETHODIMP TSFClientFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObj) {
    if (pUnkOuter) return CLASS_E_NOAGGREGATION;
    
    TSFClient* client = new TSFClient();
    HRESULT hr = client->QueryInterface(riid, ppvObj);
    client->Release();
    return hr;
}

STDMETHODIMP TSFClientFactory::LockServer(BOOL fLock) {
    if (fLock) {
        InterlockedIncrement(&g_serverLocks);
    } else {
        InterlockedDecrement(&g_serverLocks);
    }
    return S_OK;
}

} // namespace suyan

// ============================================
// DLL 导出函数
// ============================================

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (!ppv) return E_INVALIDARG;
    
    if (IsEqualCLSID(rclsid, suyan::CLSID_SuYanTextService)) {
        return suyan::g_factory.QueryInterface(riid, ppv);
    }
    
    *ppv = nullptr;
    return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow() {
    return (suyan::g_serverLocks == 0) ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer() {
    // 32位DLL注册到WOW6432Node
    wchar_t dllPath[MAX_PATH];
    GetModuleFileNameW(suyan::g_hModule, dllPath, MAX_PATH);
    
    wchar_t clsidStr[64];
    StringFromGUID2(suyan::CLSID_SuYanTextService, clsidStr, 64);
    
    // 注册COM服务器
    wchar_t keyPath[256];
    StringCchPrintfW(keyPath, 256, L"CLSID\\%s", clsidStr);
    
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, keyPath, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, nullptr, 0, REG_SZ, (BYTE*)L"SuYan Input Method (32-bit)", 56);
        
        HKEY hSubKey;
        if (RegCreateKeyExW(hKey, L"InprocServer32", 0, nullptr, 0, KEY_WRITE, nullptr, &hSubKey, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hSubKey, nullptr, 0, REG_SZ, (BYTE*)dllPath, (wcslen(dllPath) + 1) * sizeof(wchar_t));
            RegSetValueExW(hSubKey, L"ThreadingModel", 0, REG_SZ, (BYTE*)L"Apartment", 20);
            RegCloseKey(hSubKey);
        }
        RegCloseKey(hKey);
    }
    
    // 注册TSF配置
    ITfInputProcessorProfileMgr* profileMgr = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_ITfInputProcessorProfileMgr, (void**)&profileMgr))) {
        profileMgr->RegisterProfile(
            suyan::CLSID_SuYanTextService,
            MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED),
            suyan::GUID_SuYanProfile,
            L"素言输入法",
            wcslen(L"素言输入法"),
            dllPath,
            wcslen(dllPath),
            0,
            nullptr,
            0,
            TRUE,
            0
        );
        profileMgr->Release();
    }
    
    // 注册分类
    ITfCategoryMgr* catMgr = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_ITfCategoryMgr, (void**)&catMgr))) {
        catMgr->RegisterCategory(suyan::CLSID_SuYanTextService, GUID_TFCAT_TIP_KEYBOARD, suyan::CLSID_SuYanTextService);
        catMgr->Release();
    }
    
    return S_OK;
}

STDAPI DllUnregisterServer() {
    wchar_t clsidStr[64];
    StringFromGUID2(suyan::CLSID_SuYanTextService, clsidStr, 64);
    
    wchar_t keyPath[256];
    StringCchPrintfW(keyPath, 256, L"CLSID\\%s", clsidStr);
    RegDeleteTreeW(HKEY_CLASSES_ROOT, keyPath);
    
    ITfInputProcessorProfileMgr* profileMgr = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_ITfInputProcessorProfileMgr, (void**)&profileMgr))) {
        profileMgr->UnregisterProfile(
            suyan::CLSID_SuYanTextService,
            MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED),
            suyan::GUID_SuYanProfile,
            0
        );
        profileMgr->Release();
    }
    
    ITfCategoryMgr* catMgr = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_ITfCategoryMgr, (void**)&catMgr))) {
        catMgr->UnregisterCategory(suyan::CLSID_SuYanTextService, GUID_TFCAT_TIP_KEYBOARD, suyan::CLSID_SuYanTextService);
        catMgr->Release();
    }
    
    return S_OK;
}

// DLL入口
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            suyan::SetModuleHandle(hinstDLL);
            DisableThreadLibraryCalls(hinstDLL);
            break;
    }
    return TRUE;
}
