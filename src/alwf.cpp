#include <acul/locales/locales.hpp>
#include <acul/log.hpp>
#include <acul/task.hpp>
#include <alwf/alwf.hpp>
#include "internal/framework.hpp"

namespace alwf
{
    static struct Runtime
    {
        PLATFORM_WINDOW *window = nullptr;
#ifdef _WIN32
        acul::events::dispatcher ed;
#endif
        acul::task::service_dispatch sd;
        acul::log::log_service *logsvc = nullptr;
    } *rt = nullptr;

    static void setup_i18n(const Options &opt)
    {
        if (!opt.languages) return;
        if (opt.language_count > 0)
            acul::locales::setup_i18n(acul::locales::get_user_language(opt.languages, opt.language_count));
        bindtextdomain(opt.gettext_domain, opt.locales_dir);
        bind_textdomain_codeset(opt.gettext_domain, "UTF-8");
        textdomain(opt.gettext_domain);
    }

#ifdef _WIN32
    static awin::WindowFlags map_win_flags(AlwfWindowFlags f)
    {
        using B = awin::WindowFlagBits;
        awin::WindowFlags out = awin::WindowFlagBits::NATIVE_RESERVE_FLAG;
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
        awin::init_library(&rt->ed);
        awin::WindowFlags flags = map_win_flags(opt.flags);
        static awin::Window window{opt.title, opt.width, opt.height, flags};
        rt->window = &window;

        rt->ed.bind_event(rt, awin::event_id::resize,
                          [=](const awin::PosEvent &e) { on_resize(e.window, e.position); });
        rt->ed.bind_event(rt, awin::event_id::move, [=](const awin::PosEvent &e) { on_move(); });
        awin::update_events();
#else
        gtk_init(nullptr, nullptr);
        GtkWidget *w = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_default_size(GTK_WINDOW(w), opt.width, opt.height);
        gtk_window_set_title(GTK_WINDOW(w), opt.title);
        apply_gtk_flags(w, opt.flags);
        rt->window = w;
#endif
        return rt->window;
    }

    void init(const Options &opt)
    {
        rt = acul::alloc<Runtime>();
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
            auto *logger = rt->logsvc->add_logger<acul::log::file_logger>("app", opt.log_file,
                                                                          std::ios_base::out | std::ios_base::trunc);
            logger->set_pattern("%(ascii_time) %(level_name) %(message)\n");
            rt->logsvc->default_logger = logger;
        }
        else
        {
            auto *logger = rt->logsvc->add_logger<acul::log::console_logger>("app");
            logger->set_pattern("%(ascii_time) %(level_name) %(message)\n");
            rt->logsvc->default_logger = logger;
        }

        ctx = acul::alloc<Context>();
        ctx->router = opt.router;
        ctx->handler_router = opt.handler_router;
        ctx->static_folder = opt.static_folder;

        LOG_INFO("Setup i18n");
        setup_i18n(opt);

        LOG_INFO("Init window");
        PLATFORM_WINDOW *w = init_window(opt);

        LOG_INFO("Init web view");
        init_web_view(w);

        LOG_INFO("Alwf inited successfully");
    }

    void run()
    {
        LOG_INFO("Run main loop");
#ifdef _WIN32
        PLATFORM_WINDOW *w = rt->window;
        while (w && !w->ready_to_close()) awin::wait_events();
        if (w) w->destroy();
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

    void shutdown()
    {
        LOG_INFO("Shutdown alwf");
        destroy_platform();
#ifdef _WIN32
        awin::destroy_library();
#endif
        acul::release(rt);
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