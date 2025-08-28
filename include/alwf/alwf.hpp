#pragma once

#include <acul/enum.hpp>
#include <acul/hash/hashmap.hpp>
#include <acul/string/string.hpp>
#include <rapidjson/document.h>

namespace alwf
{
    using EventHandler = std::function<void(const rapidjson::Value &)>;
    using RouteHandler = std::function<acul::string()>;

    extern struct Enviroment
    {
        acul::hashmap<acul::string, EventHandler> handlers;
        acul::hashmap<acul::string, RouteHandler> routes;
        acul::string static_folder;
    } env;

    struct AlwfWindowFlagBits
    {
        enum enum_type : uint32_t
        {
            decorated = 1u << 0,
            resizable = 1u << 1,
            minimize_box = 1u << 2
        };
        using flag_bitmask = std::true_type;
    };
    using AlwfWindowFlags = acul::flags<AlwfWindowFlagBits>;

    struct Options
    {
        // Window
        const char *title = "Alwf App";
        int width = 800;
        int height = 600;
        AlwfWindowFlags flags =
            AlwfWindowFlagBits::decorated | AlwfWindowFlagBits::resizable | AlwfWindowFlagBits::minimize_box;

        // i18n
        const char **languages = nullptr;
        size_t language_count = 0;
        const char *gettext_domain = "app";
        const char *locales_dir = "locales";

        // Log
        const char *log_file = nullptr;
    };

    void init(const Options &opt);
    void run();
    void shutdown();
    void send_json_to_frontend(const rapidjson::Value &json);
} // namespace alwf