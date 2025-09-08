#include <acul/io/path.hpp>
#include <acul/locales/locales.hpp>
#include <alwf/alwf.hpp>
#include <templates/api.hpp>
#include <templates/start.hpp>
#include "handlers.hpp"

int main(int argc, char **argv)
{
    alwf::Router router;
    router.get["/"] = [](const alwf::Request &) { return acul::alloc<alwf::TextResponse>(ahtt::start::render()); };
    router.get["/api"] = [](const alwf::Request &) { return acul::alloc<alwf::TextResponse>(ahtt::api::render()); };

    alwf::HandlerRouter api_router;
    api_router.emplace("api-demo", api_message);

    alwf::Options opt;
    // Window
    opt.title = "Hello, Alwf!";
    opt.width = 1200;
    opt.height = 800;

    acul::io::path current_path = acul::io::get_current_path();
    // i18n (Actually not used in this example)
    const char *languages[] = {"en", "ru"};
    opt.languages = languages;
    opt.language_count = 2;
    opt.gettext_domain = "app";
    acul::string locales_path = current_path / "locales";
    opt.locales_dir = locales_path.c_str();

    // Public files
    acul::string static_folder = current_path / "public";
    opt.static_folder = static_folder.c_str();

    // Navigation
    opt.router = &router;
    opt.handler_router = &api_router;

    alwf::init(opt);
    alwf::run();
    alwf::shutdown();

    return 0;
}