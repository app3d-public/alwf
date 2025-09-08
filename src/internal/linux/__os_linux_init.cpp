#include <acul/log.hpp>
#include <acul/string/string.hpp>
#include <acul/string/utils.hpp>
#include <gtk/gtk.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include "../framework.hpp"
#include "platform.hpp"

namespace alwf
{
    LinuxPlatformData platform;

    static Method parse_method(const gchar *m)
    {
        if (!m) return Method::get;
        if (g_ascii_strcasecmp(m, "GET") == 0) return Method::get;
        if (g_ascii_strcasecmp(m, "POST") == 0) return Method::post;
        if (g_ascii_strcasecmp(m, "PUT") == 0) return Method::put;
        if (g_ascii_strcasecmp(m, "DELETE") == 0) return Method::del;
        LOG_ERROR("Unsupported method: %s", m);
        return Method::get;
    }

    static bool wants_json_from(const Request &req)
    {
        auto accept = req.get_header(ACUL_C_STR("Accept"));
        if (!accept.empty() && acul::find_insensitive_case(accept, "application/json") != acul::string::npos)
            return true;

        auto xrw = req.get_header(ACUL_C_STR("X-Requested-With"));
        if (!xrw.empty())
        {
            auto ieq = [](const char *a, const char *b) {
                for (; *a && *b; ++a, ++b)
                {
                    unsigned ca = (unsigned char)*a, cb = (unsigned char)*b;
                    if (tolower(ca) != tolower(cb)) return false;
                }
                return *a == 0 && *b == 0;
            };
            if (ieq(xrw.c_str(), "fetch") || ieq(xrw.c_str(), "XMLHttpRequest")) return true;
        }
        return false;
    }

    static IResponse *emit_error(const Request &req, const char *err)
    {
        LOG_ERROR("%s", err);
        if (wants_json_from(req))
        {
            rapidjson::Document d;
            d.SetObject();
            auto &a = d.GetAllocator();
            d.AddMember("success", false, a);
            rapidjson::Value e(err, a);
            d.AddMember("error", e, a);
            return acul::alloc<JSONResponse>(std::move(d), 500);
        }
        else { return acul::alloc<TextResponse>(err, 500, "text/plain"); }
    }

    static void finish_404(WebKitURISchemeRequest *request)
    {
        GError *err = g_error_new_literal(g_quark_from_static_string("alwf"), 404, "Not Found");
        webkit_uri_scheme_request_finish_error(request, err);
        g_error_free(err);
    }

    static void finish_with_response(WebKitURISchemeRequest *request, IResponse *res)
    {
        const char *mime = res->content_type ? res->content_type : "application/octet-stream";

        GBytes *bytes = g_bytes_new(res->data(), res->size());
        GInputStream *stream = g_memory_input_stream_new_from_bytes(bytes);
        g_bytes_unref(bytes);

        webkit_uri_scheme_request_finish(request, stream, res->size(), mime);
        g_object_unref(stream);
    }

    acul::string Request::get_header(const ACUL_NATIVE_CHAR *header) const
    {
        if (!request_ctx || !header) return {};
        auto *hdrs = static_cast<SoupMessageHeaders *>(request_ctx);
        const char *val = soup_message_headers_get_one(hdrs, (const char *)header);
        if (!val) return {};
        return acul::string(val);
    }

    static void app_scheme_request_cb(WebKitURISchemeRequest *request_raw, gpointer)
    {
        assert(ctx && "Context is not initialized");
        const gchar *uri = webkit_uri_scheme_request_get_uri(request_raw);
        if (!uri)
        {
            finish_404(request_raw);
            return;
        }
        acul::string path = uri + 6; // skip "app://"
        Request req;
#if WEBKIT_CHECK_VERSION(2, 40, 0)
        const gchar *method = webkit_uri_scheme_request_get_http_method(request_raw);
        req.method = parse_method(method);
#else
        req.method = Method::get;
#endif

        SoupMessageHeaders *hdrs = nullptr;
#if WEBKIT_CHECK_VERSION(2, 40, 0)
        hdrs = webkit_uri_scheme_request_get_http_headers(request_raw);
#endif
        req.request_ctx = (void *)hdrs;

#if WEBKIT_CHECK_VERSION(2, 40, 0)
        if (GInputStream *body = webkit_uri_scheme_request_get_http_body(request_raw))
        {
            char buf[1 << 14];
            gssize n = 0;
            GError *err = nullptr;
            do {
                n = g_input_stream_read(body, buf, sizeof(buf), nullptr, &err);
                if (n > 0) req.body.append(buf, buf + n);
            } while (n > 0 && err == nullptr);
            if (err) g_error_free(err);
        }
#endif

        Router::route_store *store = nullptr;
        switch (req.method)
        {
            case Method::get:
                store = &ctx->router->get;
                break;
            case Method::post:
                store = &ctx->router->post;
                break;
            case Method::put:
                store = &ctx->router->put;
                break;
            case Method::del:
                store = &ctx->router->del;
                break;
            default:
                store = &ctx->router->get;
                break;
        }

        parse_request_url(path, req);

        auto it = store->find(req.path);
        if (it != store->end())
        {
            IResponse *res = nullptr;
            try
            {
                res = it->second(req);
                if (!res) res = emit_error(req, "Route handler returned null response");
            }
            catch (const std::exception &e)
            {
                res = emit_error(req, e.what());
            }
            catch (...)
            {
                res = emit_error(req, "Unknown error");
            }

            finish_with_response(request_raw, res);
            acul::release(res);
            return;
        }

        if (auto *res = load_static_file(path))
        {
            finish_with_response(request_raw, res);
            return;
        }

        // 404
        finish_404(request_raw);
    }

    static WebKitWebView *on_create_web_view(WebKitWebView *webview, WebKitNavigationAction *nav, gpointer)
    {
        WebKitURIRequest *req = webkit_navigation_action_get_request(nav);
        const gchar *uri = webkit_uri_request_get_uri(req);
        if (uri && g_str_has_prefix(uri, "app:///"))
            webkit_web_view_load_uri(webview, uri);
        else if (uri)
            gtk_show_uri_on_window(nullptr, uri, GDK_CURRENT_TIME, nullptr);
        return nullptr;
    }

    static void on_js_message(WebKitUserContentManager *mgr, WebKitJavascriptResult *result, gpointer)
    {
        assert(ctx && ctx->handler_router);
        JSCValue *value = webkit_javascript_result_get_js_value(result);
        if (!jsc_value_is_string(value)) return;
        char *json_str = jsc_value_to_string(value);

        rapidjson::Document doc;
        doc.Parse(json_str);
        if (!doc.HasParseError() && doc.HasMember("handler"))
        {
            acul::string handler = doc["handler"].GetString();
            auto it = ctx->handler_router->find(handler);
            if (it != ctx->handler_router->end()) it->second(doc);
        }

        g_free(json_str);
    }

    static gboolean on_decide_policy(WebKitWebView *webview, WebKitPolicyDecision *decision,
                                     WebKitPolicyDecisionType type, gpointer)
    {
        if (type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION &&
            type != WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION)
            return FALSE;

        WebKitNavigationPolicyDecision *nav_decision = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
        WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action(nav_decision);
        WebKitURIRequest *req = webkit_navigation_action_get_request(action);
        const gchar *uri = webkit_uri_request_get_uri(req);
        if (!uri) return FALSE;

        if (!g_str_has_prefix(uri, "app:///"))
        {
            gtk_show_uri_on_window(nullptr, uri, GDK_CURRENT_TIME, nullptr);
            webkit_policy_decision_ignore(decision);
            return TRUE;
        }

        webkit_policy_decision_use(decision);
        return TRUE;
    }

    void init_web_view(GtkWidget *window)
    {
        WebKitUserContentManager *ucm = webkit_user_content_manager_new();
        WebKitWebContext *context = webkit_web_context_new();

        webkit_web_context_register_uri_scheme(
            context, "app", [](WebKitURISchemeRequest *r, gpointer d) { app_scheme_request_cb(r, d); }, nullptr,
            nullptr);

        webkit_user_content_manager_register_script_message_handler(ucm, "handler");
        g_signal_connect(ucm, "script-message-received::handler", G_CALLBACK(on_js_message), nullptr);

        platform.web_view = WEBKIT_WEB_VIEW(
            g_object_new(WEBKIT_TYPE_WEB_VIEW, "web-context", context, "user-content-manager", ucm, nullptr));

        g_signal_connect(platform.web_view, "decide-policy", G_CALLBACK(on_decide_policy), nullptr);
        g_signal_connect(platform.web_view, "create", G_CALLBACK(on_create_web_view), nullptr);
        gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(platform.web_view));

        webkit_web_view_load_uri(platform.web_view, "app:///");

#ifndef NDEBUG
        g_signal_connect(window, "key-press-event", G_CALLBACK(+[](GtkWidget *, GdkEventKey *e, gpointer) -> gboolean {
                             const guint ctrl = (e->state & GDK_CONTROL_MASK);
                             const guint shift = (e->state & GDK_SHIFT_MASK);
                             if (e->keyval == GDK_KEY_F12 ||
                                 (ctrl && shift && (e->keyval == GDK_KEY_i || e->keyval == GDK_KEY_I)))
                             {
                                 WebKitWebInspector *inspector = webkit_web_view_get_inspector(platform.web_view);
                                 webkit_web_inspector_show(inspector);
                                 return TRUE;
                             }
                             return FALSE;
                         }),
                         nullptr);
#endif
    }

    void send_json_to_frontend(const rapidjson::Value &json)
    {
        rapidjson::StringBuffer buf;
        rapidjson::Writer<rapidjson::StringBuffer> w(buf);
        json.Accept(w);
        acul::string script = acul::format("window.__alwf_receive(%s);", buf.GetString());
        webkit_web_view_evaluate_javascript(platform.web_view, script.c_str(), -1, nullptr, nullptr, nullptr, nullptr,
                                            nullptr);
    }

    void destroy_platform() {};
} // namespace alwf