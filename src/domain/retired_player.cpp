#include "retired_player.h"

namespace domain {
RetiredPlayer::RetiredPlayer(RetiredPlayerId id, std::string name,
                             size_t scores, std::chrono::milliseconds play_time)
    : id_{std::move(id)},
      name_{std::move(name)},
      scores_{scores},
      play_time_{play_time} {}
}  // namespace domain
