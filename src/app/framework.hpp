#pragma once

#include <acul/string/string.hpp>
#include <awin/window.hpp>
#include <rapidjson/document.h>

using EventHandler = std::function<void(const rapidjson::Value &)>;
using RouteHandler = std::function<acul::string()>;

struct FileResponse
{
    acul::string path;
    acul::string mimeType;
    acul::string content;
};

extern struct Enviroment
{
    acul::hashmap<acul::string, EventHandler> handlers;
    acul::hashmap<acul::string, RouteHandler> routes;
    acul::string staticFolder;
} env;

void initWebView(awin::Window &window);

bool loadStaticFile(const acul::string &path, FileResponse &file);