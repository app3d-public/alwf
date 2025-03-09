#pragma once

#include <rapidjson/document.h>
#include <string>
#include <window/window.hpp>
#include "env.hpp"

using EventHandler = std::function<void(const rapidjson::Value &)>;

extern struct Enviroment
{
    std::string indexFile;
    astl::hashmap<std::string, EventHandler> handlers;
    platform_data_t platform;
} env;

void initWebView(awin::Window &window);