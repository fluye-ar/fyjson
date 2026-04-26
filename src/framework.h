// ═══════════════════════════════════════════════════════════════════════════
// framework.h — Common includes, GUIDs, globals
// yyjson-com — Fast JSON parser for COM (yyjson engine, IDispatch native)
// ═══════════════════════════════════════════════════════════════════════════
#pragma once

#include <windows.h>
#include <objbase.h>
#include <oaidl.h>
#include <ocidl.h>
#include <comutil.h>
#include <olectl.h>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "comsuppw.lib")
#pragma comment(lib, "advapi32.lib")

// ── CLSID — same for x86 and x64, ProgID "yyjson" ─────────────────────

// {D6947387-FAF4-464B-BA04-DA783F57B845}
static const CLSID CLSID_YyjsonFactory =
    {0xD6947387, 0xFAF4, 0x464B, {0xBA,0x04,0xDA,0x78,0x3F,0x57,0xB8,0x45}};

static const wchar_t* PROGID_YYJSON   = L"yyjson";
static const wchar_t* DESCRIPTION     = L"yyjson-com — Fast JSON parser for COM";

// Module globals (defined in dllmain.cpp)
extern LONG g_moduleRefCount;
extern HMODULE g_hModule;

// ── Helpers ─────────────────────────────────────────────────────────────

inline std::wstring GuidToString(const GUID& guid) {
    wchar_t buf[64];
    StringFromGUID2(guid, buf, 64);
    return buf;
}

inline std::wstring ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring r(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &r[0], len);
    return r;
}

inline std::wstring ToWide(const char* s) {
    return s ? ToWide(std::string(s)) : L"";
}

inline std::string ToUtf8(const std::wstring& s) {
    if (s.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string r(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &r[0], len, nullptr, nullptr);
    return r;
}

inline std::wstring VariantToString(const _variant_t& v) {
    if (v.vt == VT_BSTR && v.bstrVal) return v.bstrVal;
    try { return (const wchar_t*)_bstr_t(v); } catch (...) { return L""; }
}

inline long VariantToLong(const _variant_t& v) {
    try { return (long)v; } catch (...) { return 0; }
}
