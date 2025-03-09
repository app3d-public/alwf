#pragma once
#include <WebView2.h>

extern struct Win32PlatformData
{
    ICoreWebView2Controller *webViewController = nullptr;
    ICoreWebView2 *webView = nullptr;
    ICoreWebView2Environment *webViewEnvironment = nullptr;
} platform;