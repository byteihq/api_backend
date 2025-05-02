#include "logger.h"

#include <boost/log/utility/setup/console.hpp>

namespace app::log {
void JsonFormatter(logging::record_view const& rec,
                   logging::formatting_ostream& strm) {
    using namespace std::literals;

    json::object out;
    out["timestamp"] = to_iso_extended_string(*rec[timestamp]);
    if (rec[additional_data]) out["data"] = *rec[additional_data];
    out["message"] = *rec[logging::expressions::smessage];
    out["severity"] =
        logging::trivial::to_string(*rec[logging::trivial::severity]);
    out["location"] = *rec[function] + ":"s + std::to_string(*rec[line]);
    strm << out;
}

void InitBoostLogFilter() {
    logging::add_common_attributes();

    logging::add_console_log(
        std::cout, logging::keywords::format = &app::log::JsonFormatter,
        logging::keywords::auto_flush = true);
}

json::value MakeNetError(boost::beast::error_code ec, std::string_view where) {
    return json::value{
        {"code", ec.value()}, {"text", ec.message()}, {"where", where}};
}
}  // namespace app::log
