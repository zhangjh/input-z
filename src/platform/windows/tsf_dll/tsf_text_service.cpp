#include "tsf_text_service.h"
#include "language_bar_button.h"
#include <strsafe.h>
#include <imm.h>

#pragma comment(lib, "imm32.lib")

namespace suyan {

const CLSID CLSID_SuYanTextService = {
    0xA1B2C3D4, 0xE5F6, 0x7890,
    {0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x90}
};

const GUID GUID_SuYanProfile = {
    0xB2C3D4E5, 0xF6A7, 0x8901,
    {0xBC, 0xDE, 0xF1, 0x23, 0x45, 0x67, 0x89, 0x01}
};

TSFTextServiceFactory g_factory;
LONG g_serverLocks = 0;
HMODULE g_hModule = nullptr;

TSFTextService::TSFTextService()
    : m_refCount(1)
    , m_threadMgr(nullptr)
    , m_clientId(TF_CLIENTID_NULL)
    , m_threadMgrEventSinkCookie(TF_INVALID_COOKIE)
    , m_activated(false)
    , m_pLangBarButton(nullptr) {
}

TSFTextService::~TSFTextService() {
    if (m_activated) {
        Deactivate();
    }
}

STDMETHODIMP TSFTextService::QueryInterface(REFIID riid, void** ppvObj) {
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

STDMETHODIMP_(ULONG) TSFTextService::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

STDMETHODIMP_(ULONG) TSFTextService::Release() {
    LONG count = InterlockedDecrement(&m_refCount);
    if (count == 0) {
        delete this;
    }
    return count;
}

STDMETHODIMP TSFTextService::Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId) {
    return ActivateEx(pThreadMgr, tfClientId, 0);
}

STDMETHODIMP TSFTextService::ActivateEx(ITfThreadMgr* pThreadMgr, TfClientId tfClientId, DWORD) {
    if (m_activated) {
        return S_OK;
    }

    m_threadMgr = pThreadMgr;
    m_threadMgr->AddRef();
    m_clientId = tfClientId;

    ITfSource* source = nullptr;
    if (SUCCEEDED(m_threadMgr->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&source)))) {
        source->AdviseSink(IID_ITfThreadMgrEventSink,
                          static_cast<ITfThreadMgrEventSink*>(this),
                          &m_threadMgrEventSinkCookie);
        source->Release();
    }

    ITfKeystrokeMgr* keystrokeMgr = nullptr;
    if (SUCCEEDED(m_threadMgr->QueryInterface(IID_ITfKeystrokeMgr, reinterpret_cast<void**>(&keystrokeMgr)))) {
        keystrokeMgr->AdviseKeyEventSink(m_clientId, static_cast<ITfKeyEventSink*>(this), TRUE);
        keystrokeMgr->Release();
    }

    // Initialize Language Bar button
    m_pLangBarButton = new LanguageBarButton();
    if (!m_pLangBarButton->Initialize(m_threadMgr, m_clientId, g_hModule, CLSID_SuYanTextService)) {
        m_pLangBarButton->Release();
        m_pLangBarButton = nullptr;
    }

    if (m_ipc.ensureServer()) {
        m_ipc.startSession();
        m_ipc.focusIn();
    }

    m_activated = true;
    return S_OK;
}

STDMETHODIMP TSFTextService::Deactivate() {
    if (!m_activated) return S_OK;

    m_ipc.focusOut();
    m_ipc.disconnect();

    // Uninitialize Language Bar button
    if (m_pLangBarButton) {
        m_pLangBarButton->Uninitialize();
        m_pLangBarButton->Release();
        m_pLangBarButton = nullptr;
    }

    releaseSinks();

    if (m_threadMgr) {
        m_threadMgr->Release();
        m_threadMgr = nullptr;
    }

    m_clientId = TF_CLIENTID_NULL;
    m_activated = false;
    return S_OK;
}

void TSFTextService::releaseSinks() {
    if (!m_threadMgr) return;

    ITfKeystrokeMgr* keystrokeMgr = nullptr;
    if (SUCCEEDED(m_threadMgr->QueryInterface(IID_ITfKeystrokeMgr, reinterpret_cast<void**>(&keystrokeMgr)))) {
        keystrokeMgr->UnadviseKeyEventSink(m_clientId);
        keystrokeMgr->Release();
    }

    if (m_threadMgrEventSinkCookie != TF_INVALID_COOKIE) {
        ITfSource* source = nullptr;
        if (SUCCEEDED(m_threadMgr->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&source)))) {
            source->UnadviseSink(m_threadMgrEventSinkCookie);
            source->Release();
        }
        m_threadMgrEventSinkCookie = TF_INVALID_COOKIE;
    }
}

// ITfThreadMgrEventSink
STDMETHODIMP TSFTextService::OnInitDocumentMgr(ITfDocumentMgr*) { return S_OK; }
STDMETHODIMP TSFTextService::OnUninitDocumentMgr(ITfDocumentMgr*) { return S_OK; }
STDMETHODIMP TSFTextService::OnPushContext(ITfContext*) { return S_OK; }
STDMETHODIMP TSFTextService::OnPopContext(ITfContext*) { return S_OK; }

STDMETHODIMP TSFTextService::OnSetFocus(ITfDocumentMgr*, ITfDocumentMgr*) {
    return S_OK;
}

// ITfKeyEventSink
STDMETHODIMP TSFTextService::OnSetFocus(BOOL) {
    return S_OK;
}

STDMETHODIMP TSFTextService::OnTestKeyDown(ITfContext*, WPARAM wParam, LPARAM, BOOL* pfEaten) {
    *pfEaten = m_ipc.testKey(static_cast<uint32_t>(wParam), getModifiers()) ? TRUE : FALSE;
    return S_OK;
}

STDMETHODIMP TSFTextService::OnTestKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* pfEaten) {
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP TSFTextService::OnKeyDown(ITfContext*, WPARAM wParam, LPARAM, BOOL* pfEaten) {
    *pfEaten = FALSE;

    // Update cursor position BEFORE processing key to ensure candidate window
    // appears at the correct position
    updateCursorPosition();

    if (m_ipc.processKey(static_cast<uint32_t>(wParam), getModifiers())) {
        *pfEaten = TRUE;

        std::wstring text;
        if (m_ipc.getCommitText(text) && !text.empty()) {
            commitText(text);
        }
    }

    return S_OK;
}

STDMETHODIMP TSFTextService::OnKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* pfEaten) {
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP TSFTextService::OnPreservedKey(ITfContext*, REFGUID, BOOL* pfEaten) {
    *pfEaten = FALSE;
    return S_OK;
}

uint32_t TSFTextService::getModifiers() {
    uint32_t mod = SUYAN_MOD_NONE;
    if (GetKeyState(VK_SHIFT) & 0x8000)   mod |= SUYAN_MOD_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000) mod |= SUYAN_MOD_CONTROL;
    if (GetKeyState(VK_MENU) & 0x8000)    mod |= SUYAN_MOD_ALT;
    return mod;
}

void TSFTextService::commitText(const std::wstring& text) {
    for (wchar_t ch : text) {
        INPUT input[2] = {};
        input[0].type = INPUT_KEYBOARD;
        input[0].ki.wScan = ch;
        input[0].ki.dwFlags = KEYEVENTF_UNICODE;

        input[1].type = INPUT_KEYBOARD;
        input[1].ki.wScan = ch;
        input[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;

        SendInput(2, input, sizeof(INPUT));
    }
}

void TSFTextService::updateCursorPosition() {
    HWND hwnd = GetFocus();
    if (!hwnd) {
        hwnd = GetForegroundWindow();
    }
    if (!hwnd) return;

    GUITHREADINFO gti = { sizeof(GUITHREADINFO) };
    DWORD threadId = GetWindowThreadProcessId(hwnd, nullptr);
    if (GetGUIThreadInfo(threadId, &gti)) {
        if (gti.hwndCaret && !IsRectEmpty(&gti.rcCaret)) {
            POINT pt = { gti.rcCaret.left, gti.rcCaret.top };
            ClientToScreen(gti.hwndCaret, &pt);
            int height = gti.rcCaret.bottom - gti.rcCaret.top;
            if (height <= 0) height = 20;
            m_ipc.updatePosition(pt.x, pt.y + height, height);
            return;
        }
        if (gti.hwndFocus) {
            hwnd = gti.hwndFocus;
        }
    }

    POINT caretPos = {};
    if (GetCaretPos(&caretPos)) {
        ClientToScreen(hwnd, &caretPos);
        m_ipc.updatePosition(caretPos.x, caretPos.y + 20, 20);
        return;
    }

    HIMC hIMC = ImmGetContext(hwnd);
    if (hIMC) {
        COMPOSITIONFORM cf = {};
        if (ImmGetCompositionWindow(hIMC, &cf)) {
            POINT pt = cf.ptCurrentPos;
            ClientToScreen(hwnd, &pt);
            ImmReleaseContext(hwnd, hIMC);
            m_ipc.updatePosition(pt.x, pt.y + 20, 20);
            return;
        }
        ImmReleaseContext(hwnd, hIMC);
    }
}

// TSFTextServiceFactory
STDMETHODIMP TSFTextServiceFactory::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_INVALIDARG;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
        *ppvObj = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }

    *ppvObj = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) TSFTextServiceFactory::AddRef() { return 1; }
STDMETHODIMP_(ULONG) TSFTextServiceFactory::Release() { return 1; }

STDMETHODIMP TSFTextServiceFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObj) {
    if (pUnkOuter) return CLASS_E_NOAGGREGATION;

    TSFTextService* service = new TSFTextService();
    HRESULT hr = service->QueryInterface(riid, ppvObj);
    service->Release();
    return hr;
}

STDMETHODIMP TSFTextServiceFactory::LockServer(BOOL fLock) {
    if (fLock) {
        InterlockedIncrement(&g_serverLocks);
    } else {
        InterlockedDecrement(&g_serverLocks);
    }
    return S_OK;
}

} // namespace suyan
