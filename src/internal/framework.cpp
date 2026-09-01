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
        if (!ctx->static_assets_loaded || !ctx->static_library) return nullptr;

        acul::string normalized = path;
        while (!normalized.empty() && normalized.front() == '/') normalized.erase(normalized.begin());
        if (normalized.empty()) return nullptr;

        auto *node = ctx->static_library->get_node(acul::path(normalized));
        if (!node || node->is_directory()) return nullptr;
        if (node->index == umbf::Library::Node::invalid_index)
            return build_static_error_response("Invalid static umlib: asset index is missing");

        umbf::BlockIterator entry(ctx->static_file, node->index);
        if (!entry) return build_static_error_response("Invalid static umlib: asset block is unreadable");
        // ALWF packs its public directory as a Library plus ordinary raw blocks.
        if (entry->signature != umbf::sign_block::raw)
        {
            LOG_ERROR("Invalid static asset '%s': expected raw block, signature=0x%08x",
                      path.c_str(), entry->signature);
            return acul::alloc<TextResponse>("Invalid static asset type", "text/plain");
        }
        acul::vector<char> buffer;
        if (entry->size) buffer.assign(entry->data, entry->data + static_cast<size_t>(entry->size));
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
