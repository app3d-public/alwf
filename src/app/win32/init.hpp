#pragma once
#include <WebView2.h>
#include <acul/memory.hpp>
#include <windows.h>
#include <wrl.h>
#include <wrl/client.h>

// ----------------------------------------------------
// WebMessageHandler
// ----------------------------------------------------
class WebMessageHandler final : public ICoreWebView2WebMessageReceivedEventHandler
{
public:
    WebMessageHandler() : _refCount(1) {}

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args) override;

    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&_refCount); }
    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG ref = InterlockedDecrement(&_refCount);
        if (ref == 0) acul::release(this);
        return ref;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override;

private:
    LONG _refCount;
};

// ----------------------------------------------------
// WebResourceRequestedHandler
// ----------------------------------------------------
class WebResourceRequestedHandler : public ICoreWebView2WebResourceRequestedEventHandler
{
public:
    WebResourceRequestedHandler() : _refCount(1) {}

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2 *sender, ICoreWebView2WebResourceRequestedEventArgs *args) override;

    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&_refCount); }
    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG ref = InterlockedDecrement(&_refCount);
        if (ref == 0) acul::release(this);
        return ref;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override
    {
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2WebResourceRequestedEventHandler)
        {
            *ppvObject = static_cast<ICoreWebView2WebResourceRequestedEventHandler *>(this);
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

private:
    LONG _refCount;
};

// ----------------------------------------------------
// WebView2ControllerHandler
// ----------------------------------------------------
class WebView2ControllerHandler final : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler
{
public:
    WebView2ControllerHandler(HWND hwnd) : _hwnd(hwnd), _refCount(1) {}

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Controller *controller) override;
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&_refCount); }
    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG ref = InterlockedDecrement(&_refCount);
        if (ref == 0) acul::release(this);
        return ref;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override;

private:
    HWND _hwnd;
    ULONG _refCount;
};

// ----------------------------------------------------
// WebView2EnvHandler
// ----------------------------------------------------
class WebView2EnvHandler final : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
{
public:
    WebView2EnvHandler(HWND hwnd) : _hwnd(hwnd), _refCount(1) {}

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Environment *env) override;
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&_refCount); }
    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG ref = InterlockedDecrement(&_refCount);
        if (ref == 0) acul::release(this);
        return ref;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override;

private:
    HWND _hwnd;
    ULONG _refCount;
};
