#include <rapidjson/document.h>
#include <awin/popup.hpp>

void test_message(const rapidjson::Value &req)
{
    awin::popup::msgBox(req["message"].GetString(), "Message Box Example");
}