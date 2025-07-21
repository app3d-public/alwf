#include <acul/string/string.hpp>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include "../framework.hpp"

static void app_scheme_request_cb(WebKitURISchemeRequest *request, gpointer)
{
    const gchar *uri = webkit_uri_scheme_request_get_uri(request);
    acul::string path = uri + 6; // skip "app://"

    auto it = env.routes.find(path);
    if (it != env.routes.end())
    {
        acul::string html = it->second();
        GInputStream *stream = g_memory_input_stream_new_from_data(html.c_str(), html.size(), nullptr);
        webkit_uri_scheme_request_finish(request, stream, html.size(), "text/html");
        g_object_unref(stream);
        return;
    }

    FileResponse file;
    if (load_static_file(path, file))
    {
        GInputStream *stream = g_memory_input_stream_new_from_data(file.content.c_str(), file.content.size(), nullptr);
        webkit_uri_scheme_request_finish(request, stream, file.content.size(), file.mime_type.c_str());
        g_object_unref(stream);
        return;
    }

    // 404
    const char *not_found = "<h1>404 Not Found</h1>";
    GInputStream *stream = g_memory_input_stream_new_from_data(not_found, strlen(not_found), nullptr);
    webkit_uri_scheme_request_finish(request, stream, strlen(not_found), "text/html");
    g_object_unref(stream);
}

static WebKitWebView *on_create_web_view(WebKitWebView *webview, WebKitNavigationAction *nav, gpointer /*user_data*/)
{
    WebKitURIRequest *request = webkit_navigation_action_get_request(nav);
    const gchar *uri = webkit_uri_request_get_uri(request);
    if (uri) gtk_show_uri_on_window(nullptr, uri, GDK_CURRENT_TIME, nullptr);
    return nullptr;
}

static void on_js_message(WebKitUserContentManager *mgr, WebKitJavascriptResult *result, gpointer)
{
    JSCValue *value = webkit_javascript_result_get_js_value(result);
    if (jsc_value_is_string(value))
    {
        char *json_str = jsc_value_to_string(value);
        rapidjson::Document doc;
        doc.Parse(json_str);
        if (!doc.HasParseError() && doc.HasMember("handler"))
        {
            acul::string handler = doc["handler"].GetString();
            auto it = env.handlers.find(handler);
            if (it != env.handlers.end()) it->second(doc);
        }
        g_free(json_str);
    }
}

static gboolean on_decide_policy(WebKitWebView *webview, WebKitPolicyDecision *decision, WebKitPolicyDecisionType type,
                                 gpointer user_data)
{
    if (type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION)
    {
        auto *nav_decision = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
        WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action(nav_decision);
        WebKitURIRequest *request = webkit_navigation_action_get_request(action);
        const gchar *uri = webkit_uri_request_get_uri(request);
        webkit_web_view_load_uri(webview, uri);
        webkit_policy_decision_ignore(decision);
        return TRUE;
    }

    return FALSE;
}

void init_web_view(GtkWidget *window)
{
    WebKitUserContentManager *ucm = webkit_user_content_manager_new();
    WebKitWebContext *context = webkit_web_context_new();
    webkit_web_context_register_uri_scheme(context, "app", app_scheme_request_cb, nullptr, nullptr);
    webkit_user_content_manager_register_script_message_handler(ucm, "handler");
    g_signal_connect(ucm, "script-message-received::handler", G_CALLBACK(on_js_message), nullptr);

    WebKitWebView *webview = WEBKIT_WEB_VIEW(
        g_object_new(WEBKIT_TYPE_WEB_VIEW, "web-context", context, "user-content-manager", ucm, nullptr));
    g_signal_connect(webview, "decide-policy", G_CALLBACK(on_decide_policy), nullptr);
    g_signal_connect(webview, "create", G_CALLBACK(on_create_web_view), nullptr);
    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(webview));
    webkit_web_view_load_uri(webview, "app:///");
}
