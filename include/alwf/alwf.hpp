#pragma once

#include <acul/enum.hpp>
#include <acul/hash/hashmap.hpp>
#include <acul/memory/smart_ptr.hpp>
#include <acul/string/utils.hpp>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace alwf
{
    enum class Method
    {
        get,
        post,
        put,
        del
    };

    struct Request
    {
        Method method;
        acul::string path;
        acul::string query;
        acul::string body;
        void *request_ctx;

        acul::string get_header(const ACUL_NATIVE_CHAR *header) const;
    };

    class IResponse
    {
    public:
        const char *content_type;

        virtual ~IResponse() = default;

        IResponse(const char *content_type) : content_type(content_type) {}

        virtual const char *data() const = 0;
        virtual size_t size() const = 0;
    };

    class TextResponse final : public IResponse
    {
    public:
        acul::string content;

        TextResponse(const acul::string &content, const char *content_type = "text/html")
            : IResponse(content_type), content(content)
        {
        }

        virtual const char *data() const override { return content.c_str(); }

        virtual size_t size() const override { return content.size(); }
    };

    class BinaryResponse final : public IResponse
    {
    public:
        acul::vector<char> content;

        BinaryResponse(const acul::vector<char> &content, const char *content_type = "application/octet-stream")
            : IResponse(content_type), content(content)
        {
        }

        BinaryResponse(acul::vector<char> &&content, const char *content_type = "application/octet-stream")
            : IResponse(content_type), content(std::move(content))
        {
        }

        BinaryResponse(const char *content_type = "application/octet-stream") : IResponse(content_type) {}

        virtual const char *data() const override { return content.data(); }
        virtual size_t size() const override { return content.size(); }
    };

    class BinaryViewResponse final : public IResponse
    {
    public:
        const char *buf;
        size_t len;

        virtual const char *data() const override { return buf; }
        virtual size_t size() const override { return len; }

        BinaryViewResponse(const char *buf, size_t len, const char *content_type = "application/octet-stream")
            : IResponse(content_type), buf(buf), len(len)
        {
        }

        BinaryViewResponse(const char *content_type = "application/octet-stream")
            : IResponse(content_type), buf(nullptr), len(0)
        {
        }
    };

    class JSONResponse final : public IResponse
    {
    public:
        acul::string json;

        JSONResponse(rapidjson::Document &&d, const char *content_type = "application/json") : IResponse(content_type)
        {
            rapidjson::StringBuffer buf;
            rapidjson::Writer<rapidjson::StringBuffer> w(buf);
            d.Accept(w);
            json.assign(buf.GetString(), buf.GetSize());
        }

        JSONResponse(const acul::string &json, const char *content_type = "application/json")
            : IResponse(content_type), json(json)
        {
        }

        const char *data() const override { return json.c_str(); }
        size_t size() const override { return json.size(); }
    };

    using EventHandler = std::function<void(const rapidjson::Value &)>;
    using RouteHandler = std::function<IResponse *(const Request &)>;

    struct Router
    {
        using route_store = acul::hashmap<acul::string, RouteHandler>;

        route_store get;
        route_store post;
        route_store put;
        route_store del;
    };

    using HandlerRouter = acul::hashmap<acul::string, EventHandler>;

    struct AlwfWindowFlagBits
    {
        enum enum_type : uint32_t
        {
            decorated = 1u << 0,
            resizable = 1u << 1,
            minimize_box = 1u << 2
        };
        using flag_bitmask = std::true_type;
    };
    using AlwfWindowFlags = acul::flags<AlwfWindowFlagBits>;

    struct Options
    {
        // Window
        const char *title = "Alwf App";
        int width = 800;
        int height = 600;
        AlwfWindowFlags flags =
            AlwfWindowFlagBits::decorated | AlwfWindowFlagBits::resizable | AlwfWindowFlagBits::minimize_box;

        // i18n
        const char **languages = nullptr;
        size_t language_count = 0;
        const char *gettext_domain = "app";
        const char *locales_dir = "locales";

        // Log
        const char *log_file = nullptr;

        // Static files
        const char *static_folder = nullptr;

        // Navigation
        Router *router = nullptr;
        HandlerRouter *handler_router = nullptr;
    };

    void init(const Options &opt);
    void run();
    void close_window();
    void shutdown();
    void send_json_to_frontend(const rapidjson::Value &json);
} // namespace alwf