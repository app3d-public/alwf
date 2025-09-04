#include <acul/io/path.hpp>
#include <acul/locales/locales.hpp>
#include <alwf/alwf.hpp>
#include <templates/api.hpp>
#include <templates/start.hpp>
#include "handlers.hpp"

int main(int argc, char **argv)
{
    acul::io::path current_path = acul::io::get_current_path();
    alwf::env.static_folder = current_path / "public";
    alwf::env.routes["/"] = []() { return ahtt::start::render(); };
    alwf::env.routes["/api"] = []() { return ahtt::api::render(); };
    alwf::env.handlers.emplace("api-demo", api_message);

    alwf::Options opt;
    opt.title = "Hello, Alwf!";
    opt.width = 1200;
    opt.height = 800;
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