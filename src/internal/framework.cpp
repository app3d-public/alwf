#include "framework.hpp"
#include <acul/io/fs/path.hpp>
#include <acul/log.hpp>
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

    static const acul::hashmap<acul::string, MimeInfo> g_mime_types{{"html", {"text/html", ResponseKind::text}},
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

    static IResponse *build_response_from_buffer(const acul::string &path, acul::vector<char> &&buffer)
    {
        acul::string ext = acul::fs::get_extension(path);
        if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
        ext = acul::to_lower(ext);

        const MimeInfo *mi = nullptr;
        if (auto it = g_mime_types.find(ext); it != g_mime_types.end()) mi = &it->second;
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

    static IResponse *build_static_error_response(const char *message)
    {
        LOG_ERROR("%s", message);
        return acul::alloc<TextResponse>(message, "text/plain");
    }

    IResponse *load_static_file_from_umlib(const acul::string &path)
    {
        assert(ctx && "Context is not initialized");
        if (!ctx->static_assets_loaded || !ctx->static_library.library) return nullptr;

        acul::string normalized = path;
        while (!normalized.empty() && normalized.front() == '/') normalized.erase(normalized.begin());
        if (normalized.empty()) return nullptr;

        auto *node = ctx->static_library.library->get_node(acul::path(normalized));
        if (!node || node->is_folder) return nullptr;

        if (node->asset.blocks.empty())
            return build_static_error_response("Invalid static umlib: asset block list is empty");

        const auto &asset_block = node->asset.blocks.front();
        if (!asset_block) return build_static_error_response("Invalid static umlib: asset block is null");

        if (asset_block->signature() != umbf::sign_block::mapping)
            return build_static_error_response("Invalid static umlib: expected mapping asset block");

        auto mapping = acul::static_pointer_cast<umbf::Mapping>(asset_block);
        acul::vector<char> buffer;
        auto res = umbf::load_library_mapped_data(ctx->static_library, mapping, buffer);
        if (!res.success()) return build_static_error_response("Failed to read static asset data from umlib");
        return build_response_from_buffer(path, std::move(buffer));
    }

    IResponse *load_static_file(const acul::string &path)
    {
        assert(ctx && "Context is not initialized");
        auto &cache = ctx->file_cache;
        if (auto it = cache.find(path); it != cache.end()) return it->second.get();

        IResponse *raw = load_static_file_from_umlib(path);
        if (!raw) return nullptr;

        cache.emplace(path, acul::unique_ptr<IResponse>(raw));
        return raw;
    }
} // namespace alwf
