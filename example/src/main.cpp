#include <acul/io/path.hpp>
#include <acul/locales/locales.hpp>
#include <alwf/alwf.hpp>
#include <templates/about.hpp>
#include <templates/app.hpp>
#include "handlers.hpp"

int main(int argc, char **argv)
{
    acul::io::path current_path = acul::io::get_current_path();
    alwf::env.static_folder = current_path / "public";
    alwf::env.routes["/"] = []() { return templates::app::render({_("mainpage")}); };
    alwf::env.routes["/secondPage"] = []() { return templates::app::render({_("second")}); };
    alwf::env.routes["/about"] = templates::about::render;
    alwf::env.handlers.emplace("btn_click", test_message);

    alwf::Options opt;
    opt.title = "Hello, Alwf!";
    opt.width = 800;
    opt.height = 600;
    const char *languages[] = {"en", "ru"};
    opt.languages = languages;
    opt.language_count = 2;
    opt.gettext_domain = "app";
    acul::string locales_path = current_path / "locales";
    opt.locales_dir = locales_path.c_str();

    alwf::init(opt);
    alwf::run();
    alwf::shutdown();

    return 0;
}