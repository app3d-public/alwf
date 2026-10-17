#pragma once
#include <WebView2.h>
#include <cstdint>
#include <wrl/client.h>

namespace alwf
{
    extern struct Win32PlatformData
    {
        uint64_t generation = 0;
        Microsoft::WRL::ComPtr<ICoreWebView2Controller> webViewController = nullptr;
        Microsoft::WRL::ComPtr<ICoreWebView2> webView = nullptr;
        Microsoft::WRL::ComPtr<ICoreWebView2Environment> webViewEnvironment = nullptr;
    } platform;
} // namespace alwf