#include "framework.hpp"
#include <acul/io/file.hpp>
#include <acul/io/path.hpp>
#include <acul/string/utils.hpp>

namespace alwf
{
    Context *ctx = nullptr;

    enum class RespKind
    {
        Text,
        Binary,
        Json
    };

    struct MimeInfo
    {
        const char *ct;
        RespKind kind;
    };

    static const acul::hashmap<acul::string, MimeInfo> kMime{{"html", {"text/html", RespKind::Text}},
                                                             {"css", {"text/css", RespKind::Text}},
                                                             {"js", {"application/javascript", RespKind::Text}},
                                                             {"txt", {"text/plain", RespKind::Text}},
                                                             {"svg", {"image/svg+xml", RespKind::Text}},
                                                             {"json", {"application/json", RespKind::Json}},

                                                             {"jpg", {"image/jpeg", RespKind::Binary}},
                                                             {"jpeg", {"image/jpeg", RespKind::Binary}},
                                                             {"png", {"image/png", RespKind::Binary}},
                                                             {"gif", {"image/gif", RespKind::Binary}},
                                                             {"webp", {"image/webp", RespKind::Binary}}};

    IResponse *load_static_file_from_disk(const acul::string &path)
    {
        assert(ctx && "Context is not initialized");
        acul::string ext = acul::io::get_extension(path);
        if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
        ext = acul::to_lower(ext);

        acul::vector<char> buffer;
        const acul::string full = acul::format("%s/%s", ctx->static_folder, path.c_str());
        auto ok = acul::io::file::read_binary(full, buffer) == acul::io::file::op_state::success;
        if (!ok) return nullptr;

        const MimeInfo *mi = nullptr;
        if (auto it = kMime.find(ext); it != kMime.end()) mi = &it->second;
        const char *ct = mi ? mi->ct : "application/octet-stream";

        if (mi && mi->kind == RespKind::Json)
        {
            acul::string s(buffer.data(), buffer.size());
            return acul::alloc<JSONResponse>(std::move(s), 200, ct);
        }

        if (mi && mi->kind == RespKind::Text)
        {
            acul::string s(buffer.data(), buffer.size());
            return acul::alloc<TextResponse>(std::move(s), 200, ct);
        }

        return acul::alloc<BinaryResponse>(std::move(buffer), 200, ct);
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