// ═══════════════════════════════════════════════════════════════════════════
// dllmain.cpp — DLL entry, COM exports, manual registry
// yyjson-com — CreateObject("yyjson")
// ═══════════════════════════════════════════════════════════════════════════
#include "json_object.h"

// ── Globals ─────────────────────────────────────────────────────────────

LONG g_moduleRefCount = 0;
HMODULE g_hModule = nullptr;

static YyjsonClassFactory g_factory;

// ── DLL Entry ───────────────────────────────────────────────────────────

BOOL WINAPI DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}

// ── COM Exports ─────────────────────────────────────────────────────────

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (rclsid == CLSID_YyjsonFactory)
        return g_factory.QueryInterface(riid, ppv);
    *ppv = nullptr;
    return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow() {
    return (g_moduleRefCount == 0) ? S_OK : S_FALSE;
}

// ── Registry helpers ────────────────────────────────────────────────────

static HRESULT SetRegKey(HKEY root, const std::wstring& path,
    const wchar_t* valueName, const std::wstring& value)
{
    HKEY hKey;
    LONG res = RegCreateKeyExW(root, path.c_str(), 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
    if (res != ERROR_SUCCESS) return SELFREG_E_CLASS;
    RegSetValueExW(hKey, valueName, 0, REG_SZ,
        (const BYTE*)value.c_str(), (DWORD)((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
    return S_OK;
}

static void DeleteRegTree(HKEY root, const std::wstring& path) {
    RegDeleteTreeW(root, path.c_str());
}

STDAPI DllRegisterServer() {
    wchar_t dllPath[MAX_PATH];
    GetModuleFileNameW(g_hModule, dllPath, MAX_PATH);

    std::wstring clsidStr = GuidToString(CLSID_YyjsonFactory);

    // HKCR\CLSID\{guid}
    std::wstring clsidKey = L"CLSID\\" + clsidStr;
    HRESULT hr = SetRegKey(HKEY_CLASSES_ROOT, clsidKey, nullptr, DESCRIPTION);
    if (FAILED(hr)) return hr;

    // HKCR\CLSID\{guid}\InprocServer32
    hr = SetRegKey(HKEY_CLASSES_ROOT, clsidKey + L"\\InprocServer32", nullptr, dllPath);
    if (FAILED(hr)) return hr;
    hr = SetRegKey(HKEY_CLASSES_ROOT, clsidKey + L"\\InprocServer32",
        L"ThreadingModel", L"Apartment");
    if (FAILED(hr)) return hr;

    // HKCR\CLSID\{guid}\ProgID
    hr = SetRegKey(HKEY_CLASSES_ROOT, clsidKey + L"\\ProgID", nullptr, PROGID_YYJSON);
    if (FAILED(hr)) return hr;

    // HKCR\yyjson
    hr = SetRegKey(HKEY_CLASSES_ROOT, PROGID_YYJSON, nullptr, DESCRIPTION);
    if (FAILED(hr)) return hr;

    // HKCR\yyjson\CLSID
    hr = SetRegKey(HKEY_CLASSES_ROOT,
        std::wstring(PROGID_YYJSON) + L"\\CLSID", nullptr, clsidStr);

    return hr;
}

STDAPI DllUnregisterServer() {
    std::wstring clsidStr = GuidToString(CLSID_YyjsonFactory);
    DeleteRegTree(HKEY_CLASSES_ROOT, L"CLSID\\" + clsidStr);
    DeleteRegTree(HKEY_CLASSES_ROOT, PROGID_YYJSON);
    return S_OK;
}
