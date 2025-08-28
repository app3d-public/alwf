#include "framework.hpp"
#include <acul/io/file.hpp>
#include <acul/io/path.hpp>
#include <acul/string/utils.hpp>

namespace alwf
{
    bool load_static_file_from_disk(const acul::string &path, FileResponse &file)
    {
        acul::hashmap<acul::string, acul::string> mime_types{
            {"html", "text/html"}, {"css", "text/css"},  {"js", "text/javascript"}, {"json", "application/json"},
            {"jpg", "image/jpeg"}, {"png", "image/png"}, {"gif", "image/gif"},      {"svg", "image/svg+xml"}};

        acul::string extension = acul::io::get_extension(path);
        if (!extension.empty() && extension[0] == '.') extension.erase(0, 1);
        file.mime_type = mime_types.contains(extension) ? mime_types.at(extension) : "application/octet-stream";
        file.path = path;

        acul::vector<char> buffer;
        auto res = acul::io::file::read_binary(acul::format("%s/%s", env.static_folder.c_str(), path.c_str()), buffer);
        if (res != acul::io::file::op_state::success) return false;
        file.content.assign(buffer.begin(), buffer.end());
        return true;
    }

    bool load_static_file(const acul::string &path, FileResponse &file)
    {
        static acul::hashmap<acul::string, FileResponse> files;
        auto it = files.find(path);
        if (it != files.end())
        {
            file = it->second;
            return true;
        }
        else if (load_static_file_from_disk(path, file))
        {
            files[path] = file;
            return true;
        }
        return false;
    }
} // namespace alwf