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
        int status_code;
        const char *content_type;

        virtual ~IResponse() = default;

        IResponse(int status_code, const char *content_type) : status_code(status_code), content_type(content_type) {}

        virtual const char *data() const = 0;
        virtual size_t size() const = 0;
    };

    class TextResponse final : public IResponse
    {
    public:
        acul::string content;

        TextResponse(const acul::string &content, int status_code = 200, const char *content_type = "text/html")
            : IResponse(status_code, content_type), content(content)
        {
        }

        virtual const char *data() const override { return content.c_str(); }

        virtual size_t size() const override { return content.size(); }
    };

    class BinaryResponse final : public IResponse
    {
    public:
        acul::vector<char> content;

        BinaryResponse(const acul::vector<char> &content, int status_code = 200,
                       const char *content_type = "application/octet-stream")
            : IResponse(status_code, content_type), content(content)
        {
        }

        BinaryResponse(acul::vector<char> &&content, int status_code = 200,
                       const char *content_type = "application/octet-stream")
            : IResponse(status_code, content_type), content(std::move(content))
        {
        }

        BinaryResponse(int status_code = 200, const char *content_type = "application/octet-stream")
            : IResponse(status_code, content_type)
        {
        }

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

        BinaryViewResponse(const char *buf, size_t len, int status_code = 200,
                           const char *content_type = "application/octet-stream")
            : IResponse(status_code, content_type), buf(buf), len(len)
        {
        }

        BinaryViewResponse(int status_code = 200, const char *content_type = "application/octet-stream")
            : IResponse(status_code, content_type), buf(nullptr), len(0)
        {
        }
    };

    class JSONResponse final : public IResponse
    {
    public:
        acul::string json;

        JSONResponse(rapidjson::Document &&d, int status_code = 200, const char *content_type = "application/json")
            : IResponse(status_code, content_type)
        {
            rapidjson::StringBuffer buf;
            rapidjson::Writer<rapidjson::StringBuffer> w(buf);
            d.Accept(w);
            json.assign(buf.GetString(), buf.GetSize());
        }

        JSONResponse(const acul::string &json, int status_code = 200, const char *content_type = "application/json")
            : IResponse(status_code, content_type), json(json)
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

    extern struct Enviroment
    {
        acul::hashmap<acul::string, EventHandler> handlers;
        Router router;
        acul::string static_folder;
    } env;

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
    };

    void init(const Options &opt);
    void run();
    void shutdown();
    void send_json_to_frontend(const rapidjson::Value &json);
} // namespace alwf