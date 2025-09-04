#include <acul/log.hpp>
#include <alwf/alwf.hpp>
#include <rapidjson/document.h>

void api_message(const rapidjson::Value &req)
{
    const char *msg = nullptr;
    if (req.IsString())
        msg = req.GetString();
    else if (req.IsObject() && req.HasMember("message") && req["message"].IsString())
        msg = req["message"].GetString();

    if (msg)
        LOG_INFO("Message: %s", msg);
    else
        LOG_WARN("api-demo: payload has no 'message' string");

    rapidjson::Document doc;
    doc.SetObject();
    auto &a = doc.GetAllocator();
    doc.AddMember("handler", rapidjson::Value("api-demo", a), a);

    if (msg)
        doc.AddMember("message", rapidjson::Value(msg, a), a);
    else
        doc.AddMember("error", rapidjson::Value("invalid payload", a), a);

    alwf::send_json_to_frontend(doc);
}
