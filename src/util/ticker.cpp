#include "ticker.h"

namespace app {
Ticker::Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
    : strand_{strand}, period_{period}, handler_{handler} {}

void Ticker::Start() {
    net::dispatch(strand_, [self = shared_from_this()] {
        self->last_tick_ = std::chrono::steady_clock::now();
        self->SheduleTick();
    });
}

void Ticker::SheduleTick() {
    timer_.expires_after(period_);
    timer_.async_wait([self = shared_from_this()](
                          boost::system::error_code ec) { self->OnTick(ec); });
}

void Ticker::OnTick(boost::system::error_code ec) {
    if (!ec) {
        auto this_tick = std::chrono::steady_clock::now();
        auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
            this_tick - last_tick_);
        last_tick_ = this_tick;
        try {
            handler_(delta);
        } catch (...) {
        }
        SheduleTick();
    }
}
}  // namespace app
