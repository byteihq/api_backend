#include "request_handler.h"

namespace http_handler {
std::string MakeResponseBody(std::string_view ec, std::string_view msg) {
    json::object obj;
    obj["code"] = ec;
    obj["message"] = msg;
    return json::serialize(obj);
}
}  // namespace http_handler
