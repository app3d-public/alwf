#pragma once

#include <webkit2/webkit2.h>

namespace alwf
{
    extern struct LinuxPlatformData
    {
        WebKitWebView *web_view = nullptr;
    } platform;
} // namespace alwf