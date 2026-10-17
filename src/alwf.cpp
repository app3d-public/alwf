#include <acul/log.hpp>
#include <acul/task.hpp>
#include <alwf/alwf.hpp>
#include <alwf/i18n.hpp>
#include "internal/framework.hpp"
#ifdef _WIN32
    #include <awin/native_access.hpp>
#endif

namespace alwf
{
    static struct Runtime
    {
        PLATFORM_WINDOW *window = nullptr;
#ifdef _WIN32
        acul::events::dispatcher ed;
        bool window_library_initialized = false;
#endif
        acul::task::service_dispatch sd;
        acul::log::log_service *logsvc = nullptr;
    } *rt = nullptr;

    static void setup_i18n(const Options &opt)
    {
        if (!opt.languages) return;
        if (opt.language_count > 0)
            alwf::locales::setup_i18n(alwf::locales::get_user_language(opt.languages, opt.language_count));
        alwf::locales::bind_domain(opt.gettext_domain, opt.locales_dir);
    }

#ifdef _WIN32
    static awin::WindowFlags map_win_flags(AlwfWindowFlags f)
    {
        using B = awin::WindowFlagBits;
        awin::WindowFlags out{};
        if (f & AlwfWindowFlagBits::decorated) out |= B::decorated | B::maximize_box;
        if (f & AlwfWindowFlagBits::resizable) out |= B::resizable;
        if (f & AlwfWindowFlagBits::minimize_box) out |= B::minimize_box;
        return out;
    }
#else
    static void apply_gtk_flags(GtkWidget *w, AlwfWindowFlags f)
    {
        gtk_window_set_decorated(GTK_WINDOW(w), (f & AlwfWindowFlagBits::decorated) != 0);
        gtk_window_set_resizable(GTK_WINDOW(w), (f & AlwfWindowFlagBits::resizable) != 0);
    }
#endif

    static PLATFORM_WINDOW *init_window(const Options &opt)
    {
#ifdef _WIN32
        awin::InitConfig cfg;
        cfg.events_dispatcher = &rt->ed;
        cfg.log_service = rt->logsvc;
        cfg.logger = acul::log::get_default_logger();
        awin::init_library(cfg);
        rt->window_library_initialized = true;
        awin::WindowFlags flags = map_win_flags(opt.flags);
        rt->window = acul::alloc<awin::Window>(opt.title, opt.width, opt.height, flags);

        rt->ed.bind_event(rt, awin::event_id::resize,
                          [=](const awin::PosEvent &e) { on_resize(e.window, e.position); });
        rt->ed.bind_event(rt, awin::event_id::move, [=](const awin::PosEvent &e) { on_move(); });
        awin::update_events();
#else
        gtk_init(nullptr, nullptr);
        GtkWidget *w = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        g_object_ref_sink(w);
        gtk_window_set_default_size(GTK_WINDOW(w), opt.width, opt.height);
        gtk_window_set_title(GTK_WINDOW(w), opt.title);
        apply_gtk_flags(w, opt.flags);
        rt->window = w;
#endif
        return rt->window;
    }

    void init(const Options &opt)
    {
        if (rt) throw acul::runtime_error("ALWF is already initialized. Call shutdown before init.");
        rt = acul::alloc<Runtime>();
        try
        {
            rt->sd.run();
            rt->logsvc = acul::alloc<acul::log::log_service>();
            rt->sd.register_service(rt->logsvc);
#ifdef DNDEBUG
            rt->logsvc->level = acul::log::level::trace;
#else
            rt->logsvc->level = acul::log::level::info;
#endif
            if (opt.log_file)
            {
                auto *logger = rt->logsvc->add_logger<acul::log::file_logger>(
                    "app", opt.log_file, std::ios_base::out | std::ios_base::trunc);
                logger->set_pattern("%(ascii_time) %(level_name) %(message)\n");
                acul::log::set_default_logger(logger);
            }
            else
            {
                auto *logger = rt->logsvc->add_logger<acul::log::console_logger>("app");
                logger->set_pattern("%(ascii_time) %(level_name) %(message)\n");
                acul::log::set_default_logger(logger);
            }

            ctx = acul::alloc<Context>();
            ctx->start_path = opt.start_path ? opt.start_path : "/";
            ctx->router = opt.router;
            ctx->handler_router = opt.handler_router;
            if (opt.static_assets) ctx->static_assets = opt.static_assets;

            if (!ctx->static_assets.empty())
            {
                umbf::insert_default_block_streams(ctx->static_resolver.local);
                umbf::insert_default_segment_codecs(ctx->static_resolver.local);
                ctx->static_resolver.fallback = umbf::registry::resolver;

                auto *prev_resolver = umbf::registry::resolver;
                umbf::registry::resolver = &ctx->static_resolver;
                try
                {
                    auto result = umbf::create_read_descriptor(ctx->static_assets.c_str(), ctx->static_map);
                    if (result.success() && ctx->static_map.file->type_sign == umbf::sign_block::format::library)
                    {
                        auto entry = ctx->static_map.begin();
                        if (entry && entry->signature == umbf::sign_block::library)
                        {
                            auto block = entry.read();
                            ctx->static_library.reset(static_cast<umbf::Library *>(block.release()));
                        }
                    }
                }
                catch (...)
                {
                    umbf::registry::resolver = prev_resolver;
                    throw;
                }
                umbf::registry::resolver = prev_resolver;
                ctx->static_assets_loaded = static_cast<bool>(ctx->static_library);
                if (!ctx->static_assets_loaded) LOG_WARN("Failed to load static umlib: %s", ctx->static_assets.c_str());
            }

            LOG_INFO("Setup i18n");
            setup_i18n(opt);

            LOG_INFO("Init window");
            PLATFORM_WINDOW *w = init_window(opt);

            LOG_INFO("Init web view");
            init_web_view(w);

            LOG_INFO("Alwf inited successfully");
        }
        catch (...)
        {
            shutdown();
            throw;
        }
    }

    void run()
    {
        LOG_INFO("Run main loop");
#ifdef _WIN32
        PLATFORM_WINDOW *w = rt->window;
        while (w && !w->ready_to_close()) awin::wait_events();
#else
        g_signal_connect(rt->window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);
        gtk_widget_show_all(rt->window);
        gtk_main();
#endif
    }

    void close_window()
    {
#ifdef _WIN32
        PLATFORM_WINDOW *w = rt->window;
        if (w) w->ready_to_close(true);
#else
        gtk_main_quit();
#endif
    }

    void set_window_size(int width, int height)
    {
        if (!rt || !rt->window) throw acul::runtime_error("ALWF window is not initialized.");
        if (width <= 0 || height <= 0) throw acul::runtime_error("Invalid ALWF window size.");
#ifdef _WIN32
        const HWND hwnd = awin::native_access::get_hwnd(*rt->window);
        RECT outer{}, client{};
        if (!GetWindowRect(hwnd, &outer) || !GetClientRect(hwnd, &client))
            throw acul::runtime_error("Could not read ALWF window size.");
        const int current_width = client.right - client.left, current_height = client.bottom - client.top;
        if (width == current_width && height == current_height) return;
        if (!SetWindowPos(hwnd, nullptr, 0, 0, width + outer.right - outer.left - current_width,
                          height + outer.bottom - outer.top - current_height,
                          SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE))
            throw acul::runtime_error("Could not resize ALWF window.");
#else
        gtk_window_resize(GTK_WINDOW(rt->window), width, height);
#endif
    }

    void set_window_title(const acul::string &title)
    {
        if (!rt || !rt->window) throw acul::runtime_error("ALWF window is not initialized.");
#ifdef _WIN32
        rt->window->title(title);
#else
        gtk_window_set_title(GTK_WINDOW(rt->window), title.c_str());
#endif
    }

    void shutdown()
    {
        if (!rt) return;
        LOG_INFO("Shutdown alwf");
        destroy_platform();
#ifdef _WIN32
        if (rt->window)
        {
            rt->window->destroy();
            acul::release(rt->window);
        }
        if (rt->window_library_initialized) awin::destroy_library();
#else
        if (rt->window)
        {
            g_signal_handlers_disconnect_by_func(rt->window, (gpointer)gtk_main_quit, nullptr);
            gtk_widget_destroy(rt->window);
            g_object_unref(rt->window);
        }
#endif
        if (ctx) umbf::close_read_descriptor(ctx->static_map);
        acul::release(rt);
        rt = nullptr;
        acul::release(ctx);
        ctx = nullptr;
    }

    void parse_request_url(const acul::string &uri, Request &request)
    {
        acul::string path;
        acul::string query;

        if (auto qpos = uri.find('?'); qpos != acul::string::npos)
        {
            path = uri.substr(0, qpos);
            query = uri.substr(qpos + 1);
        }
        else
        {
            path = uri;
            query = "";
        }

        request.path = std::move(path);
        request.query = std::move(query);
    }

} // namespace alwf
