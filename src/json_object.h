// ═══════════════════════════════════════════════════════════════════════════
// json_object.h — JsonObject COM + FyjsonFactory (entry point)
// Single type for objects, arrays and primitives. Backed by yyjson mutable.
// Root owns yyjson_mut_doc. Subnodes AddRef the root.
// ═══════════════════════════════════════════════════════════════════════════
#pragma once
#include "dispatch_base.h"

extern "C" {
#include "yyjson.h"
}

class JsonObject : public DispatchBase {
    yyjson_mut_doc* m_doc;      // Owned only by root (nullptr for subnodes)
    yyjson_mut_val* m_val;      // Node in the JSON tree
    JsonObject*     m_root;     // Root object (nullptr when this IS root)

    enum DispIds {
        DID_Item = DISPID_VALUE,    // 0 — default property
        DID_ToString = 1,
        DID_Count,
        DID_IsObject,
        DID_IsArray,
        DID_IsNull,
        DID_Exists,
        DID_Remove,
        DID_Add,
    };

    // ── Helpers ──────────────────────────────────────────────────────────

    yyjson_mut_doc* Doc() const { return m_root ? m_root->m_doc : m_doc; }

    HRESULT ValToVariant(yyjson_mut_val* val, _variant_t& result) {
        if (!val) { result.vt = VT_NULL; return S_OK; }

        if (yyjson_mut_is_obj(val) || yyjson_mut_is_arr(val))
            return ReturnSubNode(val, result);

        if (yyjson_mut_is_str(val)) {
            result = ToWide(yyjson_mut_get_str(val)).c_str();
            return S_OK;
        }
        if (yyjson_mut_is_int(val)) {
            int64_t n = yyjson_mut_get_sint(val);
            if (n >= LONG_MIN && n <= LONG_MAX)
                result = static_cast<long>(n);
            else
                result = static_cast<double>(n);
            return S_OK;
        }
        if (yyjson_mut_is_real(val)) {
            result = yyjson_mut_get_real(val);
            return S_OK;
        }
        if (yyjson_mut_is_bool(val)) {
            result = yyjson_mut_get_bool(val) ? VARIANT_TRUE : VARIANT_FALSE;
            result.vt = VT_BOOL;
            return S_OK;
        }
        result.vt = VT_NULL;
        return S_OK;
    }

    HRESULT ReturnSubNode(yyjson_mut_val* val, _variant_t& result) {
        JsonObject* root = m_root ? m_root : this;
        auto* sub = new JsonObject(val, root);
        ReturnComObject<JsonObject>(sub, result);
        return S_OK;
    }

    yyjson_mut_val* VariantToVal(const _variant_t& v) {
        yyjson_mut_doc* doc = Doc();
        switch (v.vt) {
        case VT_BSTR: {
            std::string u = ToUtf8(VariantToString(v));
            return yyjson_mut_strncpy(doc, u.c_str(), u.size());
        }
        case VT_I2:    return yyjson_mut_int(doc, static_cast<int64_t>(v.iVal));
        case VT_I4:    return yyjson_mut_int(doc, static_cast<int64_t>(v.lVal));
        case VT_R4:    return yyjson_mut_real(doc, static_cast<double>(v.fltVal));
        case VT_R8:    return yyjson_mut_real(doc, v.dblVal);
        case VT_BOOL:  return yyjson_mut_bool(doc, v.boolVal != VARIANT_FALSE);
        case VT_NULL:
        case VT_EMPTY: return yyjson_mut_null(doc);
        case VT_DISPATCH: {
            if (v.pdispVal) {
                IProvideClassInfo* pci = nullptr;
                if (SUCCEEDED(v.pdispVal->QueryInterface(IID_IProvideClassInfo, (void**)&pci))) {
                    pci->Release();
                    DispatchBase* base = static_cast<DispatchBase*>(v.pdispVal);
                    if (wcscmp(base->GetClassName(), L"JsonObject") == 0) {
                        auto* jobj = static_cast<JsonObject*>(base);
                        if (jobj->m_val)
                            return yyjson_mut_val_mut_copy(doc, jobj->m_val);
                    }
                }
            }
            return yyjson_mut_null(doc);
        }
        default: {
            // VBS Array() → JSON array
            if ((v.vt & VT_ARRAY) && (v.vt & VT_VARIANT)) {
                SAFEARRAY* sa = v.parray;
                if (sa) {
                    LONG lb = 0, ub = -1;
                    SafeArrayGetLBound(sa, 1, &lb);
                    SafeArrayGetUBound(sa, 1, &ub);
                    yyjson_mut_val* arr = yyjson_mut_arr(doc);
                    for (LONG i = lb; i <= ub; i++) {
                        VARIANT elem;
                        VariantInit(&elem);
                        SafeArrayGetElement(sa, &i, &elem);
                        yyjson_mut_val* jval = VariantToVal(_variant_t(elem));
                        VariantClear(&elem);
                        if (jval) yyjson_mut_arr_add_val(arr, jval);
                    }
                    return arr;
                }
            }
            // Coerce to string
            _variant_t conv;
            if (SUCCEEDED(VariantChangeType(&conv, const_cast<VARIANT*>(static_cast<const VARIANT*>(&v)), 0, VT_BSTR))) {
                std::string u = ToUtf8(VariantToString(conv));
                return yyjson_mut_strncpy(doc, u.c_str(), u.size());
            }
            return yyjson_mut_null(doc);
        }
        }
    }

    // ── DoInvoke handlers ────────────────────────────────────────────────

    HRESULT DoItem(WORD flags, const std::vector<_variant_t>& args, _variant_t& result) {
        if (args.empty()) return E_INVALIDARG;
        if (IsPut(flags)) {
            if (args.size() < 2) return E_INVALIDARG;
            return DoSet(args.front(), args.back());
        }
        return DoGet(args[0], result);
    }

    HRESULT DoGet(const _variant_t& key, _variant_t& result) {
        if (yyjson_mut_is_obj(m_val)) {
            std::string k = ToUtf8(VariantToString(key));
            return ValToVariant(yyjson_mut_obj_get(m_val, k.c_str()), result);
        }
        if (yyjson_mut_is_arr(m_val)) {
            long idx = VariantToLong(key);
            return ValToVariant(yyjson_mut_arr_get(m_val, static_cast<size_t>(idx)), result);
        }
        return DISP_E_TYPEMISMATCH;
    }

    HRESULT DoSet(const _variant_t& key, const _variant_t& newVal) {
        yyjson_mut_doc* doc = Doc();
        yyjson_mut_val* jval = VariantToVal(newVal);
        if (!jval) return E_FAIL;

        if (yyjson_mut_is_obj(m_val)) {
            std::string k = ToUtf8(VariantToString(key));
            yyjson_mut_val* jkey = yyjson_mut_strncpy(doc, k.c_str(), k.size());
            return yyjson_mut_obj_put(m_val, jkey, jval) ? S_OK : E_FAIL;
        }
        if (yyjson_mut_is_arr(m_val)) {
            long idx = VariantToLong(key);
            size_t arrSize = yyjson_mut_arr_size(m_val);
            if (idx < 0 || static_cast<size_t>(idx) >= arrSize) return DISP_E_BADINDEX;
            yyjson_mut_arr_remove(m_val, static_cast<size_t>(idx));
            return yyjson_mut_arr_insert(m_val, jval, static_cast<size_t>(idx)) ? S_OK : E_FAIL;
        }
        return DISP_E_TYPEMISMATCH;
    }

    HRESULT DoToString(_variant_t& result) {
        size_t len = 0;
        char* str = yyjson_mut_val_write(m_val, YYJSON_WRITE_PRETTY, &len);
        if (!str) { result = L""; return S_OK; }
        result = ToWide(str).c_str();
        free(str);
        return S_OK;
    }

    HRESULT DoCount(_variant_t& result) {
        if (yyjson_mut_is_obj(m_val))
            result = static_cast<long>(yyjson_mut_obj_size(m_val));
        else if (yyjson_mut_is_arr(m_val))
            result = static_cast<long>(yyjson_mut_arr_size(m_val));
        else
            result = static_cast<long>(0);
        return S_OK;
    }

    HRESULT DoExists(const std::vector<_variant_t>& args, _variant_t& result) {
        if (args.empty()) return E_INVALIDARG;
        if (!yyjson_mut_is_obj(m_val)) { result = VARIANT_FALSE; result.vt = VT_BOOL; return S_OK; }
        std::string k = ToUtf8(VariantToString(args[0]));
        result = yyjson_mut_obj_get(m_val, k.c_str()) ? VARIANT_TRUE : VARIANT_FALSE;
        result.vt = VT_BOOL;
        return S_OK;
    }

    HRESULT DoRemove(const std::vector<_variant_t>& args) {
        if (args.empty()) return E_INVALIDARG;
        if (yyjson_mut_is_obj(m_val)) {
            std::string k = ToUtf8(VariantToString(args[0]));
            yyjson_mut_obj_remove_key(m_val, k.c_str());
            return S_OK;
        }
        if (yyjson_mut_is_arr(m_val)) {
            long idx = VariantToLong(args[0]);
            yyjson_mut_arr_remove(m_val, static_cast<size_t>(idx));
            return S_OK;
        }
        return DISP_E_TYPEMISMATCH;
    }

    HRESULT DoAdd(const std::vector<_variant_t>& args, _variant_t& result) {
        if (args.empty()) return E_INVALIDARG;
        if (yyjson_mut_is_arr(m_val)) {
            yyjson_mut_val* jval = VariantToVal(args[0]);
            if (!jval) return E_FAIL;
            return yyjson_mut_arr_add_val(m_val, jval) ? S_OK : E_FAIL;
        }
        if (yyjson_mut_is_obj(m_val)) {
            if (args.size() < 2) return E_INVALIDARG;
            std::string k = ToUtf8(VariantToString(args[0]));
            yyjson_mut_doc* doc = Doc();
            yyjson_mut_val* jkey = yyjson_mut_strncpy(doc, k.c_str(), k.size());
            yyjson_mut_val* jval = VariantToVal(args[1]);
            if (!jkey || !jval) return E_FAIL;
            return yyjson_mut_obj_add(m_val, jkey, jval) ? S_OK : E_FAIL;
        }
        return DISP_E_TYPEMISMATCH;
    }

    HRESULT DoEnumerator(_variant_t& result) {
        std::vector<VARIANT> items;
        if (yyjson_mut_is_arr(m_val)) {
            size_t idx, max;
            yyjson_mut_val* val;
            yyjson_mut_arr_foreach(m_val, idx, max, val) {
                _variant_t v;
                ValToVariant(val, v);
                items.push_back(v);
                v.Detach();
            }
        }
        else if (yyjson_mut_is_obj(m_val)) {
            size_t idx, max;
            yyjson_mut_val* key;
            yyjson_mut_val* val;
            yyjson_mut_obj_foreach(m_val, idx, max, key, val) {
                _variant_t v = ToWide(yyjson_mut_get_str(key)).c_str();
                items.push_back(v);
                v.Detach();
            }
        }
        auto* enumerator = new VariantEnumerator(items);
        for (auto& v : items) VariantClear(&v);
        enumerator->AddRef();
        result.vt = VT_UNKNOWN;
        result.punkVal = static_cast<IUnknown*>(enumerator);
        return S_OK;
    }

protected:
    DISPID MapName(const std::wstring& name) override {
        if (_wcsicmp(name.c_str(), L"Item") == 0)     return DID_Item;
        if (_wcsicmp(name.c_str(), L"ToString") == 0)  return DID_ToString;
        if (_wcsicmp(name.c_str(), L"Count") == 0)     return DID_Count;
        if (_wcsicmp(name.c_str(), L"IsObject") == 0)  return DID_IsObject;
        if (_wcsicmp(name.c_str(), L"IsArray") == 0)   return DID_IsArray;
        if (_wcsicmp(name.c_str(), L"IsNull") == 0)    return DID_IsNull;
        if (_wcsicmp(name.c_str(), L"Exists") == 0)    return DID_Exists;
        if (_wcsicmp(name.c_str(), L"Remove") == 0)    return DID_Remove;
        if (_wcsicmp(name.c_str(), L"Add") == 0)       return DID_Add;
        return DISPID_UNKNOWN;
    }

    HRESULT DoInvoke(DISPID id, WORD flags,
        const std::vector<_variant_t>& args, _variant_t& result) override
    {
        switch (id) {
        case DID_Item:     return DoItem(flags, args, result);
        case DID_ToString: return DoToString(result);
        case DID_Count:    return DoCount(result);
        case DID_IsObject: result = yyjson_mut_is_obj(m_val) ? VARIANT_TRUE : VARIANT_FALSE;
                           result.vt = VT_BOOL; return S_OK;
        case DID_IsArray:  result = yyjson_mut_is_arr(m_val) ? VARIANT_TRUE : VARIANT_FALSE;
                           result.vt = VT_BOOL; return S_OK;
        case DID_IsNull:   result = yyjson_mut_is_null(m_val) ? VARIANT_TRUE : VARIANT_FALSE;
                           result.vt = VT_BOOL; return S_OK;
        case DID_Exists:   return DoExists(args, result);
        case DID_Remove:   return DoRemove(args);
        case DID_Add:      return DoAdd(args, result);
        case DISPID_NEWENUM: return DoEnumerator(result);
        }
        return DISP_E_MEMBERNOTFOUND;
    }

public:
    const wchar_t* GetClassName() const override { return L"JsonObject"; }

    // Root constructor — owns the document
    JsonObject(yyjson_mut_doc* doc, yyjson_mut_val* val)
        : m_doc(doc), m_val(val), m_root(nullptr) {}

    // Subnode constructor — AddRefs root
    JsonObject(yyjson_mut_val* val, JsonObject* root)
        : m_doc(nullptr), m_val(val), m_root(root)
    {
        if (m_root) m_root->AddRef();
    }

    ~JsonObject() {
        if (m_root) m_root->Release();
        else if (m_doc) yyjson_mut_doc_free(m_doc);
    }

    // Parse UTF-8 → root JsonObject
    static JsonObject* Parse(const char* json, size_t len) {
        yyjson_doc* idoc = yyjson_read(json, len, 0);
        if (!idoc) return nullptr;
        yyjson_mut_doc* mdoc = yyjson_doc_mut_copy(idoc, nullptr);
        yyjson_doc_free(idoc);
        if (!mdoc) return nullptr;
        yyjson_mut_val* root = yyjson_mut_doc_get_root(mdoc);
        if (!root) { yyjson_mut_doc_free(mdoc); return nullptr; }
        return new JsonObject(mdoc, root);
    }

    static JsonObject* Parse(const wchar_t* json) {
        std::string u = ToUtf8(json);
        return Parse(u.c_str(), u.size());
    }
};


// ═══════════════════════════════════════════════════════════════════════════
// FyjsonFactory — CreateObject("fyjson") entry point
// Methods: Parse, NewObject, NewArray
// ═══════════════════════════════════════════════════════════════════════════

class FyjsonFactory : public DispatchBase {
    enum DispIds {
        DID_Parse = 1,
        DID_NewObject,
        DID_NewArray,
        DID_Version,
    };

protected:
    DISPID MapName(const std::wstring& name) override {
        if (_wcsicmp(name.c_str(), L"Parse") == 0)      return DID_Parse;
        if (_wcsicmp(name.c_str(), L"NewObject") == 0)   return DID_NewObject;
        if (_wcsicmp(name.c_str(), L"NewArray") == 0)    return DID_NewArray;
        if (_wcsicmp(name.c_str(), L"Version") == 0)     return DID_Version;
        return DISPID_UNKNOWN;
    }

    HRESULT DoInvoke(DISPID id, WORD flags,
        const std::vector<_variant_t>& args, _variant_t& result) override
    {
        switch (id) {
        case DID_Parse: {
            if (args.empty()) return E_INVALIDARG;
            std::wstring json = VariantToString(args[0]);
            auto* obj = JsonObject::Parse(json.c_str());
            if (!obj) return RaiseComError(L"Invalid JSON");
            ReturnComObject<JsonObject>(obj, result);
            return S_OK;
        }
        case DID_NewObject: {
            yyjson_mut_doc* doc = yyjson_mut_doc_new(nullptr);
            yyjson_mut_val* root = yyjson_mut_obj(doc);
            yyjson_mut_doc_set_root(doc, root);
            auto* obj = new JsonObject(doc, root);
            ReturnComObject<JsonObject>(obj, result);
            return S_OK;
        }
        case DID_NewArray: {
            yyjson_mut_doc* doc = yyjson_mut_doc_new(nullptr);
            yyjson_mut_val* root = yyjson_mut_arr(doc);
            yyjson_mut_doc_set_root(doc, root);
            auto* obj = new JsonObject(doc, root);
            ReturnComObject<JsonObject>(obj, result);
            return S_OK;
        }
        case DID_Version:
            result = L"1.0.0";
            return S_OK;
        }
        return DISP_E_MEMBERNOTFOUND;
    }

public:
    const wchar_t* GetClassName() const override { return L"fyjson"; }
};


// ═══════════════════════════════════════════════════════════════════════════
// COM Class Factory — creates FyjsonFactory instances
// ═══════════════════════════════════════════════════════════════════════════

class FyjsonClassFactory : public IClassFactory {
    LONG m_ref;
public:
    FyjsonClassFactory() : m_ref(0) {}
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *ppv = static_cast<IClassFactory*>(this); AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_ref); }
    STDMETHODIMP_(ULONG) Release() override { return InterlockedDecrement(&m_ref); }
    STDMETHODIMP CreateInstance(IUnknown* pOuter, REFIID riid, void** ppv) override {
        if (pOuter) return CLASS_E_NOAGGREGATION;
        auto* f = new FyjsonFactory(); f->AddRef();
        HRESULT hr = f->QueryInterface(riid, ppv);
        f->Release(); return hr;
    }
    STDMETHODIMP LockServer(BOOL fLock) override {
        if (fLock) InterlockedIncrement(&g_moduleRefCount);
        else InterlockedDecrement(&g_moduleRefCount);
        return S_OK;
    }
};
