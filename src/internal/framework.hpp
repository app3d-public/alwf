#pragma once

#include <alwf/alwf.hpp>

#ifdef _WIN32
    #include <awin/window.hpp>
using PLATFORM_WINDOW = awin::Window;
#else
    #include <gtk/gtk.h>
using PLATFORM_WINDOW = GtkWidget;
#endif

namespace alwf
{
    void init_web_view(PLATFORM_WINDOW *window);
    void destroy_platform();
    void on_resize(PLATFORM_WINDOW *window, acul::point2D<i32> size);
    void on_move();

    IResponse *load_static_file(const acul::string &path);

    void parse_request_url(const acul::string &uri, Request &request);
} // namespace alwf