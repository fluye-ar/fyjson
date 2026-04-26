// ═══════════════════════════════════════════════════════════════════════════
// dispatch_base.h — Base IDispatch implementation (manual, no ATL)
// Descendants override MapName (name→dispid) and DoInvoke (dispatch).
// ═══════════════════════════════════════════════════════════════════════════
#pragma once
#include "framework.h"

// ── StubTypeInfo — minimal ITypeInfo for TypeName() support ──────────────

class StubTypeInfo : public ITypeInfo {
    LONG m_ref = 1;
    std::wstring m_name;
public:
    StubTypeInfo(const std::wstring& name) : m_name(name) {}

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_ITypeInfo) {
            *ppv = static_cast<ITypeInfo*>(this); AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_ref); }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG ref = InterlockedDecrement(&m_ref);
        if (ref == 0) delete this;
        return ref;
    }

    STDMETHODIMP GetDocumentation(MEMBERID memid, BSTR* pName, BSTR*, DWORD*, BSTR*) override {
        if (memid == MEMBERID_NIL && pName) { *pName = SysAllocString(m_name.c_str()); return S_OK; }
        return E_NOTIMPL;
    }

    STDMETHODIMP GetTypeAttr(TYPEATTR**) override { return E_NOTIMPL; }
    STDMETHODIMP GetTypeComp(ITypeComp**) override { return E_NOTIMPL; }
    STDMETHODIMP GetFuncDesc(UINT, FUNCDESC**) override { return E_NOTIMPL; }
    STDMETHODIMP GetVarDesc(UINT, VARDESC**) override { return E_NOTIMPL; }
    STDMETHODIMP GetNames(MEMBERID, BSTR*, UINT, UINT*) override { return E_NOTIMPL; }
    STDMETHODIMP GetRefTypeOfImplType(UINT, HREFTYPE*) override { return E_NOTIMPL; }
    STDMETHODIMP GetImplTypeFlags(UINT, INT*) override { return E_NOTIMPL; }
    STDMETHODIMP GetIDsOfNames(LPOLESTR*, UINT, MEMBERID*) override { return E_NOTIMPL; }
    STDMETHODIMP Invoke(PVOID, MEMBERID, WORD, DISPPARAMS*, VARIANT*, EXCEPINFO*, UINT*) override { return E_NOTIMPL; }
    STDMETHODIMP GetContainingTypeLib(ITypeLib**, UINT*) override { return E_NOTIMPL; }
    void STDMETHODCALLTYPE ReleaseTypeAttr(TYPEATTR*) override {}
    void STDMETHODCALLTYPE ReleaseFuncDesc(FUNCDESC*) override {}
    void STDMETHODCALLTYPE ReleaseVarDesc(VARDESC*) override {}
    STDMETHODIMP CreateInstance(IUnknown*, REFIID, PVOID*) override { return E_NOTIMPL; }
    STDMETHODIMP GetMops(MEMBERID, BSTR*) override { return E_NOTIMPL; }
    STDMETHODIMP GetRefTypeInfo(HREFTYPE, ITypeInfo**) override { return E_NOTIMPL; }
    STDMETHODIMP AddressOfMember(MEMBERID, INVOKEKIND, PVOID*) override { return E_NOTIMPL; }
    STDMETHODIMP GetDllEntry(MEMBERID, INVOKEKIND, BSTR*, BSTR*, WORD*) override { return E_NOTIMPL; }
};

// ── DispatchBase ────────────────────────────────────────────────────────

class DispatchBase : public IDispatch, public IProvideClassInfo {
    LONG m_ref;

protected:
    virtual DISPID MapName(const std::wstring& name) = 0;
    virtual HRESULT DoInvoke(DISPID id, WORD flags,
        const std::vector<_variant_t>& args,
        _variant_t& result) = 0;

public:
    static bool IsPut(WORD flags) {
        return (flags & (DISPATCH_PROPERTYPUT | DISPATCH_PROPERTYPUTREF)) != 0;
    }

    DispatchBase() : m_ref(0) { InterlockedIncrement(&g_moduleRefCount); }
    virtual ~DispatchBase() { InterlockedDecrement(&g_moduleRefCount); }
    virtual const wchar_t* GetClassName() const { return L"Object"; }

    // ── IUnknown ────────────────────────────────────────────────────────

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDispatch) {
            *ppv = static_cast<IDispatch*>(this); AddRef(); return S_OK;
        }
        if (riid == IID_IProvideClassInfo) {
            *ppv = static_cast<IProvideClassInfo*>(this); AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_ref); }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG ref = InterlockedDecrement(&m_ref);
        if (ref == 0) delete this;
        return ref;
    }

    // ── IProvideClassInfo ─────────────────────────────────────────────────

    STDMETHODIMP GetClassInfo(ITypeInfo** ppTI) override {
        if (!ppTI) return E_POINTER;
        *ppTI = new StubTypeInfo(GetClassName());
        return S_OK;
    }

    // ── IDispatch ───────────────────────────────────────────────────────

    STDMETHODIMP GetTypeInfoCount(UINT* count) override {
        *count = 0;
        return S_OK;
    }

    STDMETHODIMP GetTypeInfo(UINT, LCID, ITypeInfo** ppTI) override {
        *ppTI = nullptr;
        return DISP_E_BADINDEX;
    }

    STDMETHODIMP GetIDsOfNames(REFIID, LPOLESTR* names, UINT count,
        LCID, DISPID* ids) override
    {
        HRESULT hr = S_OK;
        for (UINT i = 0; i < count; i++) {
            ids[i] = MapName(names[i]);
            if (ids[i] == DISPID_UNKNOWN) hr = DISP_E_UNKNOWNNAME;
        }
        return hr;
    }

    STDMETHODIMP Invoke(DISPID id, REFIID, LCID, WORD flags,
        DISPPARAMS* dp, VARIANT* result,
        EXCEPINFO* pExcepInfo, UINT*) override
    {
        SetErrorInfo(0, nullptr);

        bool isPut = (flags & (DISPATCH_PROPERTYPUT | DISPATCH_PROPERTYPUTREF)) != 0;

        int posCount = dp->cArgs - dp->cNamedArgs;
        std::vector<_variant_t> args;
        for (int i = posCount - 1; i >= 0; i--)
            args.push_back(dp->rgvarg[dp->cNamedArgs + i]);

        if (isPut && dp->cNamedArgs > 0)
            args.push_back(dp->rgvarg[0]);

        _variant_t vr;
        HRESULT hr;
        try {
            hr = DoInvoke(id, flags, args, vr);
        } catch (const std::exception& ex) {
            if (pExcepInfo) {
                memset(pExcepInfo, 0, sizeof(EXCEPINFO));
                std::wstring msg = ToWide(std::string(ex.what()));
                pExcepInfo->bstrDescription = SysAllocString(msg.c_str());
                pExcepInfo->bstrSource = SysAllocString(L"yyjson");
                pExcepInfo->scode = E_FAIL;
            }
            return DISP_E_EXCEPTION;
        } catch (...) {
            return E_FAIL;
        }

        if (SUCCEEDED(hr) && result) {
            VariantInit(result);
            VariantCopy(result, &vr);
        }

        if (FAILED(hr) && pExcepInfo) {
            memset(pExcepInfo, 0, sizeof(EXCEPINFO));
            IErrorInfo* pEI = nullptr;
            if (SUCCEEDED(GetErrorInfo(0, &pEI)) && pEI) {
                BSTR desc = nullptr, src = nullptr;
                pEI->GetDescription(&desc);
                pEI->GetSource(&src);
                pExcepInfo->bstrDescription = desc;
                pExcepInfo->bstrSource = src;
                pExcepInfo->scode = hr;
                pEI->Release();
                hr = DISP_E_EXCEPTION;
            }
        }

        return hr;
    }
};

// ── ReturnComObject ─────────────────────────────────────────────────────

template<typename T>
inline void ReturnComObject(T* obj, _variant_t& result) {
    IDispatch* disp = nullptr;
    obj->QueryInterface(IID_IDispatch, (void**)&disp);
    if (disp) { result = disp; disp->Release(); }
    else { delete obj; }
}

// ── RaiseComError ───────────────────────────────────────────────────────

inline HRESULT RaiseComError(const std::wstring& desc,
    const std::wstring& source = L"yyjson", HRESULT hr = E_FAIL)
{
    ICreateErrorInfo* pCEI = nullptr;
    if (SUCCEEDED(CreateErrorInfo(&pCEI))) {
        pCEI->SetDescription(_bstr_t(desc.c_str()));
        pCEI->SetSource(_bstr_t(source.c_str()));
        IErrorInfo* pEI = nullptr;
        pCEI->QueryInterface(IID_IErrorInfo, (void**)&pEI);
        SetErrorInfo(0, pEI);
        if (pEI) pEI->Release();
        pCEI->Release();
    }
    return hr;
}

// ── VariantEnumerator ───────────────────────────────────────────────────

class VariantEnumerator : public IEnumVARIANT {
    LONG m_ref;
    std::vector<VARIANT> m_items;
    size_t m_index;

public:
    VariantEnumerator(const std::vector<VARIANT>& items)
        : m_ref(0), m_index(0)
    {
        m_items.resize(items.size());
        for (size_t i = 0; i < items.size(); i++) {
            VariantInit(&m_items[i]);
            VariantCopy(&m_items[i], &items[i]);
        }
        InterlockedIncrement(&g_moduleRefCount);
    }

    ~VariantEnumerator() {
        for (auto& v : m_items) VariantClear(&v);
        InterlockedDecrement(&g_moduleRefCount);
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IEnumVARIANT) {
            *ppv = static_cast<IEnumVARIANT*>(this); AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_ref); }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG ref = InterlockedDecrement(&m_ref);
        if (ref == 0) delete this;
        return ref;
    }

    STDMETHODIMP Next(ULONG celt, VARIANT* rgVar, ULONG* pFetched) override {
        ULONG fetched = 0;
        while (fetched < celt && m_index < m_items.size()) {
            VariantInit(&rgVar[fetched]);
            VariantCopy(&rgVar[fetched], &m_items[m_index]);
            m_index++; fetched++;
        }
        if (pFetched) *pFetched = fetched;
        return (fetched == celt) ? S_OK : S_FALSE;
    }

    STDMETHODIMP Skip(ULONG celt) override {
        m_index += celt;
        return (m_index <= m_items.size()) ? S_OK : S_FALSE;
    }

    STDMETHODIMP Reset() override { m_index = 0; return S_OK; }

    STDMETHODIMP Clone(IEnumVARIANT** ppEnum) override {
        auto* c = new VariantEnumerator(m_items);
        c->AddRef(); c->m_index = m_index;
        *ppEnum = c; return S_OK;
    }
};
