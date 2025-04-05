#include "framework.hpp"
#include <acul/io/file.hpp>

bool loadStaticFileFromDisk(const acul::string &path, FileResponse &file)
{
    acul::hashmap<acul::string, acul::string> mimeTypes{
        {"html", "text/html"}, {"css", "text/css"},  {"js", "text/javascript"}, {"json", "application/json"},
        {"jpg", "image/jpeg"}, {"png", "image/png"}, {"gif", "image/gif"},      {"svg", "image/svg+xml"}};

    acul::string extension = acul::io::get_extension(path);
    if (!extension.empty() && extension[0] == '.') extension.erase(0, 1);
    file.mimeType = mimeTypes.contains(extension) ? mimeTypes.at(extension) : "application/octet-stream";
    file.path = path;

    acul::vector<char> buffer;
    auto res = acul::io::file::read_binary(acul::format("%s/%s", env.staticFolder.c_str(), path.c_str()), buffer);
    if (res != acul::io::file::op_state::success) return false;
    file.content.assign(buffer.begin(), buffer.end());
    return true;
}

bool loadStaticFile(const acul::string &path, FileResponse &file)
{
    static acul::hashmap<acul::string, FileResponse> files;
    auto it = files.find(path);
    if (it != files.end())
    {
        file = it->second;
        return true;
    }
    else if (loadStaticFileFromDisk(path, file))
    {
        files[path] = file;
        return true;
    }
    return false;
}