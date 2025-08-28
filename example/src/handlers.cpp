#include <acul/log.hpp>
#include <alwf/alwf.hpp>
#include <rapidjson/document.h>

void test_message(const rapidjson::Value &req)
{
    auto message = req["message"].GetString();
    LOG_INFO("Message: %s", message);

    rapidjson::Document doc;
    doc.SetObject();
    auto &allocator = doc.GetAllocator();
    doc.AddMember("handler", "btn_click", allocator); 
    doc.AddMember("message", rapidjson::Value(message, allocator), allocator);
    alwf::send_json_to_frontend(doc);
}
