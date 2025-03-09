#include <acul/locales.hpp>
#include <templates/app.hpp>
#include <templates/about.hpp>
#include <awin/window.hpp>
#include "framework.hpp"
#include "handlers.hpp"

Enviroment env;

int main()
{
    events::Manager ed;
    locales::setup_i18n(locales::getUserLanguage());
    bindtextdomain("app", "locales");
    bind_textdomain_codeset("app", "UTF-8");
    textdomain("app");

    awin::initLibrary(&ed);
    {
        awin::CreationFlags flags = awin::CreationFlagsBits::snapped | awin::CreationFlagsBits::minimizebox |
                                      awin::CreationFlagsBits::decorated;
        awin::Window window{"WebView Sample", 800, 600, flags};
        env.staticFolder = "public";
        env.routes["/"] = []() { return templates::app::render({_("mainpage")}); };
        env.routes["/secondPage"] = []() { return templates::app::render({_("second")}); };
        env.routes["/about"] = templates::about::render;

        env.handlers.emplace("btn_click", test_message);
        initWebView(window);
        while (!window.readyToClose()) awin::waitEvents();
        window.destroy();
    }
    awin::destroyLibrary();
    return 0;
}