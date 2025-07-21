#pragma once

#include <acul/hash/hashmap.hpp>
#include <acul/string/string.hpp>
#include <rapidjson/document.h>

#ifdef _WIN32
    #include <awin/window.hpp>
    #define WINDOW_TYPE awin::Window
#else
    #include <gtk/gtk.h>
    #define WINDOW_TYPE GtkWidget
#endif

using EventHandler = std::function<void(const rapidjson::Value &)>;
using RouteHandler = std::function<acul::string()>;

struct FileResponse
{
    acul::string path;
    acul::string mime_type;
    acul::string content;
};

extern struct Enviroment
{
    acul::hashmap<acul::string, EventHandler> handlers;
    acul::hashmap<acul::string, RouteHandler> routes;
    acul::string static_folder;
} env;

void init_web_view(WINDOW_TYPE *window);

bool load_static_file(const acul::string &path, FileResponse &file);