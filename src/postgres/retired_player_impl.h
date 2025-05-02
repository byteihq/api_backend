#pragma once

#include <domain/retired_player.h>

#include <pqxx/transaction>
#include <string_view>

namespace postgres {
namespace request {
using namespace std::literals;
static constexpr std::string_view add_new_player =
    "INSERT INTO retired_players (id, name, score, play_time_ms) VALUES ($1, $2, $3, $4)"sv;
static constexpr std::string_view increase_player_scores =
    "UPDATE retired_players SET score = score + $2 WHERE name = $1"sv;
static constexpr std::string_view increase_player_play_time =
    "UPDATE retired_players SET play_time_ms = play_time_ms + $2 WHERE name = $1"sv;
}  // namespace request

class RetiredPlayerRepositoryImpl final
    : public domain::RetiredPlayerRepository {
   public:
    explicit RetiredPlayerRepositoryImpl(pqxx::work& tx);
    void CreateDB() override;
    void CreateIndex() override;
    std::vector<domain::RetiredPlayer> GetTopKPlayers(
        size_t start, size_t max_count) override;
    bool Exists(std::string_view name) override;
    void Save(const domain::RetiredPlayer& retired_player) override;
    void IncreaseScores(std::string_view name, size_t delta) override;
    void IncreasePlayTime(std::string_view name, size_t delta) override;

   private:
    pqxx::work& tx_;
};
}  // namespace postgres
