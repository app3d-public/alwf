#include <acul/log.hpp>
#include <acul/string/utils.hpp>
#include <awin/native_access.hpp>
#include <rapidjson/document.h>
#include "../framework.hpp"
#include "init.hpp"
#include "platform.hpp"


Win32PlatformData platform;

// ----------------------------------------------------
// WebMessageHandler
// ----------------------------------------------------
HRESULT STDMETHODCALLTYPE WebMessageHandler::Invoke(ICoreWebView2 *sender,
                                                    ICoreWebView2WebMessageReceivedEventArgs *args)
{
    LPWSTR message = nullptr;
    args->get_WebMessageAsJson(&message);
    if (message)
    {
        auto u8message = acul::utf16_to_utf8(reinterpret_cast<const acul::u16string::value_type *>(message));
        rapidjson::Document doc;
        doc.Parse(u8message.c_str());
        if (!doc.HasParseError() && doc.HasMember("handler"))
        {
            acul::string handler = doc["handler"].GetString();
            auto it = env.handlers.find(handler);
            if (it != env.handlers.end())
                it->second(doc);
            else
                LOG_ERROR("Failed to find handler: %s", handler.c_str());
        }
    }
    CoTaskMemFree(message);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebMessageHandler::QueryInterface(REFIID riid, void **ppvObject)
{
    if (!memcmp(&riid, &IID_IUnknown, sizeof(GUID)) ||
        !memcmp(&riid, &IID_ICoreWebView2WebMessageReceivedEventHandler, sizeof(GUID)))
    {
        *ppvObject = static_cast<ICoreWebView2WebMessageReceivedEventHandler *>(this);
        AddRef();
        return S_OK;
    }
    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

acul::string url_to_path(LPCWSTR url)
{
    acul::wstring wurl(url);
    size_t offset = wurl.compare(0, 16, L"file://localhost") == 0 ? 16 : 0;
    return acul::utf16_to_utf8(acul::u16string(wurl.begin() + offset, wurl.end()));
}

void create_web_response(const acul::string &content, LPWSTR headers, Microsoft::WRL::ComPtr<IStream> &stream,
                         Microsoft::WRL::ComPtr<ICoreWebView2WebResourceResponse> &response)
{
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, content.size());
    void *pMem = GlobalLock(hMem);
    memcpy(pMem, content.c_str(), content.size());
    GlobalUnlock(hMem);
    CreateStreamOnHGlobal(hMem, TRUE, &stream);
    platform.webViewEnvironment->CreateWebResourceResponse(stream.Get(), 200, L"OK", headers, &response);
}

// ----------------------------------------------------
// WebResourceRequestedHandler
// ----------------------------------------------------
HRESULT STDMETHODCALLTYPE WebResourceRequestedHandler::Invoke(ICoreWebView2 *sender,
                                                              ICoreWebView2WebResourceRequestedEventArgs *args)
{
    Microsoft::WRL::ComPtr<ICoreWebView2WebResourceRequest> request;
    args->get_Request(&request);

    LPWSTR uri;
    request->get_Uri(&uri);

    Microsoft::WRL::ComPtr<ICoreWebView2WebResourceResponse> response;
    acul::string path = url_to_path(uri);
    auto it = env.routes.find(path);
    if (it != env.routes.end())
    {
        Microsoft::WRL::ComPtr<IStream> htmlStream;
        acul::string html = it->second();
        acul::u16string headers = u"Content-Type: text/html\r\nContent-Length: " + acul::to_u16string(html.size());
        create_web_response(html, (LPWSTR)headers.c_str(), htmlStream, response);
    }
    else
    {
        FileResponse res;
        if (load_static_file(path, res))
        {
            Microsoft::WRL::ComPtr<IStream> stream;
            acul::string headers =
                acul::format("Content-Type: %s\r\nContent-Length: %d", res.mime_type.c_str(), res.content.size());
            acul::u16string wHeaders = acul::utf8_to_utf16(headers);
            create_web_response(res.content, (LPWSTR)wHeaders.c_str(), stream, response);
        }
        else
            platform.webViewEnvironment->CreateWebResourceResponse(nullptr, 404, L"Not Found",
                                                                   L"Content-Type: text/html", &response);
    }

    args->put_Response(response.Get());
    return S_OK;
}

// ----------------------------------------------------
// WebView2ControllerHandler
// ----------------------------------------------------
HRESULT STDMETHODCALLTYPE WebView2ControllerHandler::Invoke(HRESULT result, ICoreWebView2Controller *controller)
{
    if (!controller) return E_FAIL;

    controller->AddRef();
    Microsoft::WRL::ComPtr<ICoreWebView2> webView;
    controller->get_CoreWebView2(&webView);
    EventRegistrationToken tokens[2];

    Microsoft::WRL::ComPtr<WebResourceRequestedHandler> handler;
    handler.Attach(acul::alloc<WebResourceRequestedHandler>());
    webView->AddWebResourceRequestedFilter(L"file://localhost/*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    webView->add_WebResourceRequested(handler.Get(), &tokens[0]);

    Microsoft::WRL::ComPtr<WebMessageHandler> messageHandler;
    messageHandler.Attach(acul::alloc<WebMessageHandler>());
    webView->add_WebMessageReceived(messageHandler.Get(), &tokens[1]);

    webView->Navigate(L"file://localhost/");

    RECT bounds;
    GetClientRect(_hwnd, &bounds);
    controller->put_Bounds(bounds);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView2ControllerHandler::QueryInterface(REFIID riid, void **ppvObject)
{
    if (!memcmp(&riid, &IID_IUnknown, sizeof(GUID)) ||
        !memcmp(&riid, &IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler, sizeof(GUID)))
    {
        *ppvObject = static_cast<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *>(this);
        AddRef();
        return S_OK;
    }
    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

// ----------------------------------------------------
// WebView2EnvHandler
// ----------------------------------------------------
HRESULT STDMETHODCALLTYPE WebView2EnvHandler::Invoke(HRESULT result, ICoreWebView2Environment *env)
{
    if (env)
    {
        env->CreateCoreWebView2Controller(_hwnd, acul::alloc<WebView2ControllerHandler>(_hwnd));
        platform.webViewEnvironment = env;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView2EnvHandler::QueryInterface(REFIID riid, void **ppvObject)
{
    if (!memcmp(&riid, &IID_IUnknown, sizeof(GUID)) ||
        !memcmp(&riid, &IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler, sizeof(GUID)))
    {
        *ppvObject = static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *>(this);
        AddRef();
        return S_OK;
    }
    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

using CreateEnvFn = HRESULT(WINAPI *)(LPCWSTR, LPCWSTR, ICoreWebView2EnvironmentOptions *,
                                      ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *);

void init_web_view(awin::Window *window)
{
    HWND hwnd = awin::native_access::get_hwnd(*window);
    HMODULE hWebView2 = LoadLibraryExW(L"WebView2Loader.dll", NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!hWebView2)
    {
        MessageBoxW(hwnd, L"WebView2Loader.dll not found", L"Error", MB_OK);
        return;
    }

    auto create_env = (CreateEnvFn)GetProcAddress(hWebView2, "CreateCoreWebView2EnvironmentWithOptions");
    if (!create_env)
    {
        MessageBoxW(hwnd, L"CreateCoreWebView2EnvironmentWithOptions not found", L"Error", MB_OK);
        return;
    }
    LPCWSTR cache = L"./cache";
    create_env(nullptr, cache, nullptr, acul::alloc<WebView2EnvHandler>(hwnd));
}