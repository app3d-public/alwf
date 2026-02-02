#include "framework.hpp"
#include <acul/io/fs/file.hpp>
#include <acul/io/fs/path.hpp>
#include <acul/string/utils.hpp>


namespace alwf
{
    Context *ctx = nullptr;

    enum class ResponseKind
    {
        text,
        binary,
        json
    };

    struct MimeInfo
    {
        const char *ct;
        ResponseKind kind;
    };

    static const acul::hashmap<acul::string, MimeInfo> kMime{{"html", {"text/html", ResponseKind::text}},
                                                             {"css", {"text/css", ResponseKind::text}},
                                                             {"js", {"application/javascript", ResponseKind::text}},
                                                             {"txt", {"text/plain", ResponseKind::text}},
                                                             {"svg", {"image/svg+xml", ResponseKind::text}},
                                                             {"json", {"application/json", ResponseKind::json}},

                                                             {"jpg", {"image/jpeg", ResponseKind::binary}},
                                                             {"jpeg", {"image/jpeg", ResponseKind::binary}},
                                                             {"png", {"image/png", ResponseKind::binary}},
                                                             {"gif", {"image/gif", ResponseKind::binary}},
                                                             {"webp", {"image/webp", ResponseKind::binary}}};

    IResponse *load_static_file_from_disk(const acul::string &path)
    {
        assert(ctx && "Context is not initialized");
        acul::string ext = acul::fs::get_extension(path);
        if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
        ext = acul::to_lower(ext);

        acul::vector<char> buffer;
        const acul::string full = acul::format("%s/%s", ctx->static_folder, path.c_str());
        if (!acul::fs::read_binary(full, buffer)) return nullptr;

        const MimeInfo *mi = nullptr;
        if (auto it = kMime.find(ext); it != kMime.end()) mi = &it->second;
        const char *ct = mi ? mi->ct : "application/octet-stream";

        if (mi && mi->kind == ResponseKind::json)
        {
            acul::string s(buffer.data(), buffer.size());
            return acul::alloc<JSONResponse>(std::move(s), ct);
        }

        if (mi && mi->kind == ResponseKind::text)
        {
            acul::string s(buffer.data(), buffer.size());
            return acul::alloc<TextResponse>(std::move(s), ct);
        }

        return acul::alloc<BinaryResponse>(std::move(buffer), ct);
    }

    IResponse *load_static_file(const acul::string &path)
    {
        assert(ctx && "Context is not initialized");
        auto &cache = ctx->file_cache;
        if (auto it = cache.find(path); it != cache.end()) return it->second.get();

        IResponse *raw = load_static_file_from_disk(path);
        if (!raw) return nullptr;

        cache.emplace(path, acul::unique_ptr<IResponse>(raw));
        return raw;
    }
} // namespace alwf