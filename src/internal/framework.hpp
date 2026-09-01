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
    void on_resize(PLATFORM_WINDOW *window, awin::IPoint size);
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
        struct StaticResolver final : umbf::registry::Resolver
        {
            umbf::registry::HashResolver local;
            umbf::registry::Resolver *fallback = nullptr;

            const umbf::registry::BlockStream *get_block_stream(u32 signature) override
            {
                if (const auto *stream = local.get_block_stream(signature)) return stream;
                return fallback ? fallback->get_block_stream(signature) : nullptr;
            }
            const umbf::registry::SegmentCodec *get_segment_codec(u32 signature) override
            {
                if (const auto *codec = local.get_segment_codec(signature)) return codec;
                return fallback ? fallback->get_segment_codec(signature) : nullptr;
            }
        } static_resolver;
        bool static_assets_loaded = false;
        umbf::ReadDescriptor static_file;
        acul::shared_ptr<umbf::Library> static_library;
    } *ctx;
} // namespace alwf
