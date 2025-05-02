#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <chrono>
#include <functional>

namespace app {
namespace net = boost::asio;

class Ticker : public std::enable_shared_from_this<Ticker> {
   public:
    using Strand = net::strand<net::io_context::executor_type>;
    using Handler = std::function<void(std::chrono::milliseconds)>;

    Ticker(Strand strand, std::chrono::milliseconds period, Handler handler);

    void Start();

   private:
    void SheduleTick();
    void OnTick(boost::system::error_code ec);

   private:
    Strand strand_;
    std::chrono::milliseconds period_;
    net::steady_timer timer_{strand_};
    Handler handler_;
    std::chrono::steady_clock::time_point last_tick_;
};
}  // namespace app
