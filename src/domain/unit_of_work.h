#pragma once

#include <string_view>

#include "retired_player.h"

namespace domain {
class UnitOfWork {
   public:
    virtual void InitPlayerDB() = 0;
    virtual std::vector<RetiredPlayer> GetTopKPlayers(size_t start,
                                                      size_t max_count) = 0;
    virtual void IncreasePlayerStats(std::string_view name, size_t scores,
                                     size_t play_time) = 0;

   protected:
    ~UnitOfWork() = default;
};
}  // namespace domain
