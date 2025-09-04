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

    struct FileResponse
    {
        acul::string path;
        acul::string mime_type;
        acul::string content;
    };

    bool load_static_file(const acul::string &path, FileResponse &file);
} // namespace alwf