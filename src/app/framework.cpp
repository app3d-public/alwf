#include "framework.hpp"
#include <acul/io/file.hpp>
#include <filesystem>

bool loadStaticFileFromDisk(const std::string &path, FileResponse &file)
{
    astl::hashmap<std::string, std::string> mimeTypes{
        {"html", "text/html"}, {"css", "text/css"},  {"js", "text/javascript"}, {"json", "application/json"},
        {"jpg", "image/jpeg"}, {"png", "image/png"}, {"gif", "image/gif"},      {"svg", "image/svg+xml"}};

    std::string extension = std::filesystem::path(path).extension().string();
    if (!extension.empty() && extension[0] == '.') extension.erase(0, 1);
    file.mimeType = mimeTypes.contains(extension) ? mimeTypes.at(extension) : "application/octet-stream";
    file.path = path;

    astl::vector<char> buffer;
    auto res = io::file::readBinary(astl::format("%s/%s", env.staticFolder.c_str(), path.c_str()), buffer);
    if (res != io::file::ReadState::Success) return false;
    file.content.assign(buffer.begin(), buffer.end());
    return true;
}

bool loadStaticFile(const std::string &path, FileResponse &file)
{
    static astl::hashmap<std::string, FileResponse> files;
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