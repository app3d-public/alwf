#include <acul/log.hpp>
#include <acul/string/utils.hpp>
#include <awin/native_access.hpp>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <shlwapi.h>
#include "../framework.hpp"
#include "init.hpp"
#include "platform.hpp"

namespace alwf
{

    Win32PlatformData platform;

    // ----------------------------------------------------
    // WebMessageHandler
    // ----------------------------------------------------
    HRESULT STDMETHODCALLTYPE WebMessageHandler::Invoke(ICoreWebView2 *sender,
                                                        ICoreWebView2WebMessageReceivedEventArgs *args)
    {
        assert(ctx && ctx->handler_router);
        LPWSTR message = nullptr;
        args->get_WebMessageAsJson(&message);
        if (message)
        {
            auto u8message = acul::utf16_to_utf8(acul::u16string(reinterpret_cast<const char16_t *>(message)));
            rapidjson::Document doc;
            doc.Parse(u8message.c_str());
            if (!doc.HasParseError() && doc.HasMember("handler"))
            {
                acul::string handler = doc["handler"].GetString();
                auto it = ctx->handler_router->find(handler);
                if (it != ctx->handler_router->end()) it->second(doc);
                else LOG_ERROR("No such handler: %s", handler.c_str());
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
        const auto value = acul::utf16_to_utf8(acul::u16string(reinterpret_cast<const char16_t *>(url)));
        return value.compare(0, 16, "file://localhost") == 0 ? value.substr(16) : value;
    }

    void create_web_response(IResponse *res, Microsoft::WRL::ComPtr<ICoreWebView2WebResourceResponse> &res_raw)
    {
        Microsoft::WRL::ComPtr<IStream> stream;
        size_t size = res->size();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
        void *pMem = GlobalLock(hMem);
        memcpy(pMem, res->data(), size);
        GlobalUnlock(hMem);
        CreateStreamOnHGlobal(hMem, TRUE, &stream);
        acul::string headers = acul::format("Content-Type: %s\r\nContent-Length: %d", res->content_type, size);
        acul::u16string w_headers = acul::utf8_to_utf16(headers);
        platform.webViewEnvironment->CreateWebResourceResponse(stream.Get(), 200, L"OK", (LPWSTR)w_headers.c_str(),
                                                               &res_raw);
    }

    static inline Method parse_method(LPWSTR str)
    {
        static const acul::hashmap<acul::string, Method> methods = {
            {"GET", Method::get}, {"POST", Method::post}, {"PUT", Method::put}, {"DELETE", Method::del}};
        const auto method = acul::utf16_to_utf8(acul::u16string(reinterpret_cast<const char16_t *>(str)));
        auto it = methods.find(method);
        if (it != methods.end()) return it->second;
        LOG_ERROR("Unsupported method: %s", method.c_str());
        return Method::get;
    }

    static acul::string read_stream(IStream *s)
    {
        if (!s) return {};
        constexpr ULONG CHUNK = 1 << 14;
        acul::string out;
        char buf[CHUNK];
        for (;;)
        {
            ULONG read = 0;
            HRESULT hr = s->Read(buf, CHUNK, &read);
            if (FAILED(hr) || read == 0) break;
            out.append(buf, buf + read);
        }
        LARGE_INTEGER li{{0}};
        s->Seek(li, STREAM_SEEK_SET, nullptr);
        return out;
    }

    static bool wants_json_from(const Request &req)
    {
        auto accept = req.get_header(ACUL_C_STR("Accept"));
        if (!accept.empty() && acul::find_insensitive_case(accept, "application/json") != acul::string::npos)
            return true;
        auto xrw = req.get_header(ACUL_C_STR("X-Requested-With"));
        if (!xrw.empty() && (stricmp(xrw.data(), "fetch") || stricmp(xrw.data(), "XMLHttpRequest"))) return true;
        return false;
    }

    IResponse *emit_error(const Request &req, const char *err)
    {
        LOG_ERROR("%s", err);
        if (wants_json_from(req))
        {
            rapidjson::Document d;
            d.SetObject();
            auto &a = d.GetAllocator();
            d.AddMember("success", false, a);
            rapidjson::Value e(err, a);
            d.AddMember("error", e, a);
            return acul::alloc<JSONResponse>(std::move(d));
        }
        else return acul::alloc<TextResponse>(err, "text/plain");
    }

    // ----------------------------------------------------
    // WebResourceRequestedHandler
    // ----------------------------------------------------
    HRESULT STDMETHODCALLTYPE WebResourceRequestedHandler::Invoke(ICoreWebView2 *sender,
                                                                  ICoreWebView2WebResourceRequestedEventArgs *args)
    {
        assert(ctx && ctx->router);
        Microsoft::WRL::ComPtr<ICoreWebView2WebResourceRequest> request_raw;
        args->get_Request(&request_raw);

        LPWSTR uri;
        if (auto r = request_raw->get_Uri(&uri) != S_OK) return r;

        Microsoft::WRL::ComPtr<ICoreWebView2WebResourceResponse> response;
        Request req;

        LPWSTR method_str;
        if (auto r = request_raw->get_Method(&method_str) != S_OK) return r;
        req.method = parse_method(method_str);

        acul::string path = url_to_path(uri);
        parse_request_url(path, req);

        Microsoft::WRL::ComPtr<IStream> content;
        request_raw->get_Content(&content);
        if (content) req.body = read_stream(content.Get());

        Microsoft::WRL::ComPtr<ICoreWebView2HttpRequestHeaders> headers;
        request_raw->get_Headers(&headers);
        req.request_ctx = (void *)headers.Get();

        Router::route_store *router = nullptr;
        switch (req.method)
        {
            case Method::get:
                router = &ctx->router->get;
                break;
            case Method::post:
                router = &ctx->router->post;
                break;
            case Method::put:
                router = &ctx->router->put;
                break;
            case Method::del:
                router = &ctx->router->del;
                break;
            default:
                return S_OK;
        }

        auto it = router->find(path);
        if (it != router->end())
        {
            IResponse *res = nullptr;
            try
            {
                res = it->second(req);
                if (!res) res = emit_error(req, "Route handler returned null response");
            }
            catch (const std::exception &e)
            {
                res = emit_error(req, e.what());
            }
            create_web_response(res, response);
            acul::release(res);
        }
        else
        {
            if (auto *res = load_static_file(path)) create_web_response(res, response);
            else
                platform.webViewEnvironment->CreateWebResourceResponse(nullptr, 404, L"Not Found",
                                                                       L"Content-Type: text/html", &response);
        }

        args->put_Response(response.Get());
        return S_OK;
    }

    static acul::string get_header_case_insensitive(ICoreWebView2HttpRequestHeaders *headers, const wchar_t *header)
    {
        using Microsoft::WRL::ComPtr;
        ComPtr<ICoreWebView2HttpHeadersCollectionIterator> it;
        if (SUCCEEDED(headers->GetIterator(&it)) && it)
        {
            BOOL has = FALSE;
            while (SUCCEEDED(it->get_HasCurrentHeader(&has)) && has)
            {
                LPWSTR k = nullptr, v = nullptr;
                if (SUCCEEDED(it->GetCurrentHeader(&k, &v)))
                {
                    if (k && StrCmpIW(k, header) == 0)
                    {
                        acul::string out;
                        if (v) out = acul::utf16_to_utf8(acul::u16string(reinterpret_cast<const char16_t *>(v)));
                        if (k) CoTaskMemFree(k);
                        if (v) CoTaskMemFree(v);
                        return out;
                    }
                }
                if (k) CoTaskMemFree(k);
                if (v) CoTaskMemFree(v);
                BOOL moved = FALSE;
                it->MoveNext(&moved);
            }
        }
        return {};
    }

    acul::string Request::get_header(const ACUL_NATIVE_CHAR *header) const
    {
        using Microsoft::WRL::ComPtr;
        if (!request_ctx || !header) return {};

        auto *headers = static_cast<ICoreWebView2HttpRequestHeaders *>(request_ctx);
        if (LPWSTR val = nullptr; SUCCEEDED(headers->GetHeader((LPWSTR)header, &val)) && val)
        {
            acul::string out = acul::utf16_to_utf8(acul::u16string(reinterpret_cast<const char16_t *>(val)));
            CoTaskMemFree(val);
            return out;
        }
        return get_header_case_insensitive(headers, (LPWSTR)header);
    }

    // ----------------------------------------------------
    // WebView2ControllerHandler
    // ----------------------------------------------------
    HRESULT STDMETHODCALLTYPE WebView2ControllerHandler::Invoke(HRESULT result, ICoreWebView2Controller *controller)
    {
        if (!controller) return E_FAIL;
        if (_generation != platform.generation || !ctx || !IsWindow(_hwnd))
        {
            controller->Close();
            return S_OK;
        }

        platform.webViewController = controller;
        Microsoft::WRL::ComPtr<ICoreWebView2> webView;
        controller->get_CoreWebView2(&webView);
        Microsoft::WRL::ComPtr<ICoreWebView2Settings> settings;
        webView->get_Settings(&settings);
        settings->put_IsWebMessageEnabled(TRUE);
        platform.webView = webView.Get();
        EventRegistrationToken tokens[2];

        Microsoft::WRL::ComPtr<WebResourceRequestedHandler> handler;
        handler.Attach(acul::alloc<WebResourceRequestedHandler>());
        webView->AddWebResourceRequestedFilter(L"file://localhost/*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
        webView->add_WebResourceRequested(handler.Get(), &tokens[0]);

        Microsoft::WRL::ComPtr<WebMessageHandler> messageHandler;
        messageHandler.Attach(acul::alloc<WebMessageHandler>());
        webView->add_WebMessageReceived(messageHandler.Get(), &tokens[1]);

        auto uri = acul::utf8_to_utf16(acul::string("file://localhost") + ctx->start_path);
        webView->Navigate(reinterpret_cast<LPCWSTR>(uri.c_str()));

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
        if (_generation != platform.generation || !ctx || !IsWindow(_hwnd)) return S_OK;
        if (env)
        {
            Microsoft::WRL::ComPtr<WebView2ControllerHandler> handler;
            handler.Attach(acul::alloc<WebView2ControllerHandler>(_hwnd, _generation));
            env->CreateCoreWebView2Controller(_hwnd, handler.Get());
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

    void init_web_view(awin::Window *window)
    {
        HWND hwnd = awin::native_access::get_hwnd(*window);
        LPCWSTR cache = L"./cache";
        Microsoft::WRL::ComPtr<WebView2EnvHandler> handler;
        handler.Attach(acul::alloc<WebView2EnvHandler>(hwnd, ++platform.generation));
        const auto result = CreateCoreWebView2EnvironmentWithOptions(nullptr, cache, nullptr, handler.Get());
        if (FAILED(result)) throw acul::runtime_error("Failed to initialize WebView2.");
    }

    void destroy_platform()
    {
        ++platform.generation;
        if (platform.webViewController) platform.webViewController->Close();
        platform.webView.Reset();
        platform.webViewController.Reset();
        platform.webViewEnvironment.Reset();
    }

    void send_json_to_frontend(const rapidjson::Value &json)
    {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        json.Accept(writer);
        acul::u16string wJson = acul::utf8_to_utf16(buffer.GetString());
        platform.webView->PostWebMessageAsJson((LPCWSTR)wJson.c_str());
    }

    void on_resize(awin::Window *window, acul::point<i32> size)
    {
        if (!platform.webViewController) return;

        HWND hwnd = awin::native_access::get_hwnd(*window);
        if (!hwnd) return;

        if (size.x <= 0 || size.y <= 0)
        {
            RECT rc{0, 0, 1, 1};
            platform.webViewController->put_Bounds(rc);
            return;
        }

        RECT bounds{0, 0, size.x, size.y};
        platform.webViewController->put_Bounds(bounds);
        platform.webViewController->NotifyParentWindowPositionChanged();
    }

    void on_move()
    {
        if (!platform.webViewController) return;
        platform.webViewController->NotifyParentWindowPositionChanged();
    }
} // namespace alwf
