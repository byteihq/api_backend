#pragma once

#include <boost/beast/core.hpp>
#include <boost/date_time.hpp>
#include <boost/json.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/expressions/keyword.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <functional>

namespace logging = boost::log;
namespace json = boost::json;

namespace app::log {

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(line, "Line", uint32_t)
BOOST_LOG_ATTRIBUTE_KEYWORD(function, "Function", std::string)

#define LOG(level)                                              \
    BOOST_LOG_TRIVIAL(level)                                    \
        << logging::add_value(app::log::function, __FUNCTION__) \
        << logging::add_value(app::log::line, __LINE__)

void JsonFormatter(logging::record_view const& rec,
                   logging::formatting_ostream& strm);

void InitBoostLogFilter();

json::value MakeNetError(boost::beast::error_code ec, std::string_view where);
}  // namespace app::log
