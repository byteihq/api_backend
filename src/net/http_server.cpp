#include "http_server.h"

#include <boost/asio/dispatch.hpp>

namespace http_server {
using namespace std::literals;

SessionBase::SessionBase(tcp::socket&& socket) : stream_(std::move(socket)) {}

void SessionBase::Run() {
    net::dispatch(
        stream_.get_executor(),
        beast::bind_front_handler(&SessionBase::Read, GetSharedFromThis()));
}

void SessionBase::Read() {
    request_ = {};
    stream_.expires_after(30s);

    http::async_read(
        stream_, buffer_, request_,
        beast::bind_front_handler(&SessionBase::OnRead, GetSharedFromThis()));
}

void SessionBase::OnRead(beast::error_code ec,
                         [[maybe_unused]] size_t bytes_read) {
    using namespace std::literals;

    if (ec == http::error::end_of_stream) {
        return Close();
    }

    if (ec) {
        LOG(error) << logging::add_value(app::log::additional_data,
                                         app::log::MakeNetError(ec, "read"sv))
                   << "error"sv;
        return;
    }

    HandleRequest(std::move(request_));
}

void SessionBase::OnWrite(bool close, beast::error_code ec,
                          [[maybe_unused]] size_t bytes_read) {
    if (ec) {
        LOG(error) << logging::add_value(app::log::additional_data,
                                         app::log::MakeNetError(ec, "write"sv))
                   << "error"sv;
        return;
    }

    if (close) {
        return Close();
    }

    Read();
}

void SessionBase::Close() {
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
}

}  // namespace http_server
