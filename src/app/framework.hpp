#pragma once

#include <awin/window.hpp>
#include <rapidjson/document.h>
#include <string>

using EventHandler = std::function<void(const rapidjson::Value &)>;
using RouteHandler = std::function<std::string()>;

struct FileResponse
{
    std::string path;
    std::string mimeType;
    std::string content;
};

extern struct Enviroment
{
    astl::hashmap<std::string, EventHandler> handlers;
    astl::hashmap<std::string, RouteHandler> routes;
    std::string staticFolder;
} env;

void initWebView(awin::Window &window);

bool loadStaticFile(const std::string &path, FileResponse &file);