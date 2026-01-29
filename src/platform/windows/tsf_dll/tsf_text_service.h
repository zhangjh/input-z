#pragma once

#include <windows.h>
#include <msctf.h>
#include <olectl.h>
#include "ipc_client.h"
#include "language_bar_button.h"

namespace suyan {

// {A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
extern const CLSID CLSID_SuYanTextService;
// {B2C3D4E5-F6A7-8901-BCDE-F12345678901}
extern const GUID GUID_SuYanProfile;

class TSFTextService : public ITfTextInputProcessorEx,
                       public ITfThreadMgrEventSink,
                       public ITfKeyEventSink {
public:
    TSFTextService();
    virtual ~TSFTextService();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfTextInputProcessor
    STDMETHODIMP Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId) override;
    STDMETHODIMP Deactivate() override;

    // ITfTextInputProcessorEx
    STDMETHODIMP ActivateEx(ITfThreadMgr* pThreadMgr, TfClientId tfClientId, DWORD dwFlags) override;

    // ITfThreadMgrEventSink
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr* pDocMgr) override;
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr* pDocMgr) override;
    STDMETHODIMP OnSetFocus(ITfDocumentMgr* pDocMgrFocus, ITfDocumentMgr* pDocMgrPrevFocus) override;
    STDMETHODIMP OnPushContext(ITfContext* pContext) override;
    STDMETHODIMP OnPopContext(ITfContext* pContext) override;

    // ITfKeyEventSink
    STDMETHODIMP OnSetFocus(BOOL fForeground) override;
    STDMETHODIMP OnTestKeyDown(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnTestKeyUp(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnKeyDown(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnKeyUp(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnPreservedKey(ITfContext* pContext, REFGUID rguid, BOOL* pfEaten) override;

private:
    void releaseSinks();
    void commitText(const std::wstring& text);
    void updateCursorPosition();
    uint32_t getModifiers();

    LONG m_refCount;
    ITfThreadMgr* m_threadMgr;
    TfClientId m_clientId;
    DWORD m_threadMgrEventSinkCookie;
    IPCClient m_ipc;
    bool m_activated;
    LanguageBarButton* m_pLangBarButton;
};

class TSFTextServiceFactory : public IClassFactory {
public:
    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IClassFactory
    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObj) override;
    STDMETHODIMP LockServer(BOOL fLock) override;
};

extern TSFTextServiceFactory g_factory;
extern LONG g_serverLocks;
extern HMODULE g_hModule;

} // namespace suyan
