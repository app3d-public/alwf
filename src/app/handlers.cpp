#include <rapidjson/document.h>

#ifdef _WIN32
    #include <awin/popup.hpp>
void test_message(const rapidjson::Value &req)
{
    awin::popup::message_box(req["message"].GetString(), "Message Box Example");
}
#else
    #include <gtk/gtk.h>
void test_message(const rapidjson::Value &req)
{
    const char *message = req["message"].GetString();

    GtkWidget *dialog =
        gtk_message_dialog_new(nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", message);

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}
#endif