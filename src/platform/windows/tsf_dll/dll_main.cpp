#include "tsf_text_service.h"
#include <strsafe.h>

extern "C" {

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
    wchar_t dllPath[MAX_PATH];
    GetModuleFileNameW(suyan::g_hModule, dllPath, MAX_PATH);

    wchar_t clsidStr[64];
    StringFromGUID2(suyan::CLSID_SuYanTextService, clsidStr, 64);

    wchar_t keyPath[256];
    StringCchPrintfW(keyPath, 256, L"CLSID\\%s", clsidStr);

    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, keyPath, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        const wchar_t* desc = L"SuYan Input Method";
        RegSetValueExW(hKey, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(desc), static_cast<DWORD>((wcslen(desc) + 1) * sizeof(wchar_t)));

        HKEY hSubKey;
        if (RegCreateKeyExW(hKey, L"InprocServer32", 0, nullptr, 0, KEY_WRITE, nullptr, &hSubKey, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hSubKey, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(dllPath), static_cast<DWORD>((wcslen(dllPath) + 1) * sizeof(wchar_t)));
            const wchar_t* threading = L"Apartment";
            RegSetValueExW(hSubKey, L"ThreadingModel", 0, REG_SZ, reinterpret_cast<const BYTE*>(threading), static_cast<DWORD>((wcslen(threading) + 1) * sizeof(wchar_t)));
            RegCloseKey(hSubKey);
        }
        RegCloseKey(hKey);
    }

    ITfInputProcessorProfileMgr* profileMgr = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_ITfInputProcessorProfileMgr, reinterpret_cast<void**>(&profileMgr)))) {
        profileMgr->RegisterProfile(
            suyan::CLSID_SuYanTextService,
            MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED),
            suyan::GUID_SuYanProfile,
            L"素言输入法",
            static_cast<ULONG>(wcslen(L"素言输入法")),
            dllPath,
            static_cast<ULONG>(wcslen(dllPath)),
            0,  // Icon index (0 = first icon in DLL, as per RIME Weasel implementation)
            nullptr,
            0,
            TRUE,
            0
        );
        profileMgr->Release();
    }

    ITfCategoryMgr* catMgr = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_ITfCategoryMgr, reinterpret_cast<void**>(&catMgr)))) {
        catMgr->RegisterCategory(suyan::CLSID_SuYanTextService, GUID_TFCAT_TIP_KEYBOARD, suyan::CLSID_SuYanTextService);
        catMgr->RegisterCategory(suyan::CLSID_SuYanTextService, GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT, suyan::CLSID_SuYanTextService);
        catMgr->RegisterCategory(suyan::CLSID_SuYanTextService, GUID_TFCAT_TIPCAP_UIELEMENTENABLED, suyan::CLSID_SuYanTextService);
        catMgr->RegisterCategory(suyan::CLSID_SuYanTextService, GUID_TFCAT_TIPCAP_SECUREMODE, suyan::CLSID_SuYanTextService);
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
                                    IID_ITfInputProcessorProfileMgr, reinterpret_cast<void**>(&profileMgr)))) {
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
                                    IID_ITfCategoryMgr, reinterpret_cast<void**>(&catMgr)))) {
        catMgr->UnregisterCategory(suyan::CLSID_SuYanTextService, GUID_TFCAT_TIP_KEYBOARD, suyan::CLSID_SuYanTextService);
        catMgr->UnregisterCategory(suyan::CLSID_SuYanTextService, GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT, suyan::CLSID_SuYanTextService);
        catMgr->UnregisterCategory(suyan::CLSID_SuYanTextService, GUID_TFCAT_TIPCAP_UIELEMENTENABLED, suyan::CLSID_SuYanTextService);
        catMgr->UnregisterCategory(suyan::CLSID_SuYanTextService, GUID_TFCAT_TIPCAP_SECUREMODE, suyan::CLSID_SuYanTextService);
        catMgr->Release();
    }

    return S_OK;
}

} // extern "C"

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            suyan::g_hModule = hinstDLL;
            DisableThreadLibraryCalls(hinstDLL);
            break;
    }
    return TRUE;
}
