#pragma once

#include <alwf/alwf.hpp>
#include <umbf/umbf.hpp>

#ifdef _WIN32
    #include <awin/awin.hpp>
using PLATFORM_WINDOW = awin::Window;
#else
    #include <gtk/gtk.h>
using PLATFORM_WINDOW = GtkWidget;
#endif

namespace alwf
{
    void init_web_view(PLATFORM_WINDOW *window);
    void destroy_platform();
    void on_resize(PLATFORM_WINDOW *window, acul::point2D<i32> size);
    void on_move();

    IResponse *load_static_file(const acul::string &path);

    void parse_request_url(const acul::string &uri, Request &request);

    using FileCache = acul::hashmap<acul::string, acul::unique_ptr<IResponse>>;

    extern struct Context
    {
        acul::string static_assets;
        Router *router;
        HandlerRouter *handler_router;
        FileCache file_cache;
        struct StaticResolver final : umbf::streams::Resolver
        {
            umbf::streams::HashResolver local;
            umbf::streams::Resolver *fallback = nullptr;

            const umbf::streams::Stream *get_stream(u32 signature) override
            {
                if (const auto *stream = local.get_stream(signature)) return stream;
                return fallback ? fallback->get_stream(signature) : nullptr;
            }
        } static_resolver;
        bool static_assets_loaded = false;
        umbf::LibraryMapData static_library;
    } *ctx;
} // namespace alwf
