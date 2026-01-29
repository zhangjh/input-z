#pragma once

#include <windows.h>
#include <msctf.h>
#include <ctffunc.h>

namespace suyan {

// Language Bar Button GUID
// {C3D4E5F6-A7B8-9012-CDEF-123456789ABC}
extern const GUID GUID_LangBarButton;

class LanguageBarButton : public ITfLangBarItemButton,
                          public ITfSource {
public:
    LanguageBarButton();
    virtual ~LanguageBarButton();

    // Initialize with thread manager
    bool Initialize(ITfThreadMgr* pThreadMgr, TfClientId clientId, HMODULE hModule, REFCLSID clsidTextService);
    void Uninitialize();

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

private:
    LONG m_refCount;
    ITfLangBarItemMgr* m_pLangBarItemMgr;
    ITfLangBarItemSink* m_pLangBarItemSink;
    DWORD m_dwSinkCookie;
    TfClientId m_clientId;
    HMODULE m_hModule;
    CLSID m_clsidTextService;
    bool m_bAddedToLangBar;
};

} // namespace suyan
