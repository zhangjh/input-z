#pragma once

#include <windows.h>
#include <msctf.h>
#include <olectl.h>
#include "ipc_client.h"

namespace suyan {

// {A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
// 与64位DLL使用相同的CLSID
extern const CLSID CLSID_SuYanTextService;
extern const GUID GUID_SuYanProfile;

HMODULE GetModuleHandle();
void SetModuleHandle(HMODULE hModule);

class TSFClient : public ITfTextInputProcessorEx,
                  public ITfThreadMgrEventSink,
                  public ITfKeyEventSink {
public:
    TSFClient();
    virtual ~TSFClient();
    
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
    bool setupSinks();
    void releaseSinks();
    void commitText(ITfContext* pContext, const std::wstring& text);
    void updateCursorPosition(ITfContext* pContext);
    
    LONG m_refCount;
    ITfThreadMgr* m_threadMgr;
    TfClientId m_clientId;
    DWORD m_threadMgrEventSinkCookie;
    IPCClient m_ipc;
    bool m_activated;
};

class TSFClientFactory : public IClassFactory {
public:
    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    
    // IClassFactory
    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObj) override;
    STDMETHODIMP LockServer(BOOL fLock) override;
};

extern TSFClientFactory g_factory;
extern LONG g_serverLocks;

} // namespace suyan

extern "C" {
    STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv);
    STDAPI DllCanUnloadNow();
    STDAPI DllRegisterServer();
    STDAPI DllUnregisterServer();
}
