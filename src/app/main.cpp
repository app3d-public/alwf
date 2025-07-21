#include <acul/locales.hpp>
#include <templates/about.hpp>
#include <templates/app.hpp>
#include "framework.hpp"
#include "handlers.hpp"

#ifdef _WIN32
    #include <awin/window.hpp>
#else
    #include <gtk/gtk.h>
#endif

Enviroment env;

int main(int argc, char **argv)
{
    const char *languages[] = {"en", "ru"};
    acul::locales::setup_i18n(acul::locales::get_user_language(languages, 2));
    bindtextdomain("app", "locales");
    bind_textdomain_codeset("app", "UTF-8");
    textdomain("app");

#ifdef _WIN32
    acul::events::dispatcher ed;
    awin::init_library(&ed);
    awin::CreationFlags flags =
        awin::CreationFlagsBits::snapped | awin::CreationFlagsBits::minimizebox | awin::CreationFlagsBits::decorated;
    awin::Window window{"WebView Sample", 800, 600, flags};
    awin::Window *pWindow = &window;
#else
    gtk_init(&argc, &argv);
    GtkWidget *pWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(pWindow), 800, 600);
    gtk_window_set_title(GTK_WINDOW(pWindow), "WebView Sample");
    gtk_window_set_resizable(GTK_WINDOW(pWindow), FALSE);
#endif
    env.static_folder = "public";
    env.routes["/"] = []() { return templates::app::render({_("mainpage")}); };
    env.routes["/secondPage"] = []() { return templates::app::render({_("second")}); };
    env.routes["/about"] = templates::about::render;
    env.handlers.emplace("btn_click", test_message);
    init_web_view(pWindow);

#ifdef _WIN32
    while (!window.ready_to_close()) awin::wait_events();
    window.destroy();
    awin::destroy_library();
#else
    g_signal_connect(pWindow, "destroy", G_CALLBACK(gtk_main_quit), nullptr);
    gtk_widget_show_all(pWindow);
    gtk_main();
#endif
    return 0;
}