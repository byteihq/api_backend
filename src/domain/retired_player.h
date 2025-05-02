#pragma once

#include <util/tagged_uuid.h>

#include <chrono>
#include <string>
#include <string_view>

namespace domain {
namespace request {
using namespace std::literals;
static constexpr std::string_view add_new_player = "add_new_player"sv;
static constexpr std::string_view increase_player_scores =
    "increase_player_scores"sv;
static constexpr std::string_view increase_player_play_time =
    "increase_player_play_time"sv;
}  // namespace request

namespace detail {
struct RetiredPlayerTag {};
}  // namespace detail

using RetiredPlayerId = util::TaggedUUID<detail::RetiredPlayerTag>;

class RetiredPlayer final {
   public:
    RetiredPlayer(RetiredPlayerId id, std::string name, size_t scores,
                  std::chrono::milliseconds play_time);

    inline const RetiredPlayerId& GetId() const noexcept { return id_; }
    inline const std::string& GetName() const noexcept { return name_; }
    inline size_t GetScores() const noexcept { return scores_; }
    inline std::chrono::milliseconds GetPlayTime() const noexcept {
        return play_time_;
    }

   private:
    RetiredPlayerId id_;
    std::string name_;
    size_t scores_;
    std::chrono::milliseconds play_time_;
};

class RetiredPlayerRepository {
   public:
    virtual void CreateDB() = 0;
    virtual void CreateIndex() = 0;
    virtual std::vector<RetiredPlayer> GetTopKPlayers(size_t start,
                                                      size_t max_count) = 0;
    virtual bool Exists(std::string_view name) = 0;
    virtual void Save(const RetiredPlayer& retired_player) = 0;
    virtual void IncreaseScores(std::string_view name, size_t delta) = 0;
    virtual void IncreasePlayTime(std::string_view name, size_t delta) = 0;

   protected:
    ~RetiredPlayerRepository() = default;
};

}  // namespace domain
