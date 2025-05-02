#include "retired_player_impl.h"

#include <fmt/core.h>
#include <util/util.h>

namespace postgres {
RetiredPlayerRepositoryImpl::RetiredPlayerRepositoryImpl(pqxx::work& tx)
    : tx_{tx} {}

void RetiredPlayerRepositoryImpl::CreateDB() {
    tx_.exec(R"(
CREATE TABLE IF NOT EXISTS retired_players (
    id UUID CONSTRAINT author_id_constraint PRIMARY KEY,
    name varchar(100) UNIQUE NOT NULL,
    score integer,
    play_time_ms integer
);
)");
}

void RetiredPlayerRepositoryImpl::CreateIndex() {
    if (auto index_exists = tx_.query01<int>(
            R"(SELECT 1 FROM pg_indexes WHERE indexname = 'player_records';)");
        !index_exists) {
        tx_.exec(R"(
        CREATE INDEX player_records ON retired_players (score DESC, play_time_ms, name);
        )");
    }
}

std::vector<domain::RetiredPlayer> RetiredPlayerRepositoryImpl::GetTopKPlayers(
    size_t start, size_t max_count) {
    std::vector<domain::RetiredPlayer> retired_players;
    for (const auto& [id, name, score, play_time_ms] :
         tx_.query<std::string, std::string, uint32_t, uint64_t>(fmt::format(
             "SELECT id, name, score, play_time_ms FROM retired_players ORDER "
             "BY score DESC, play_time_ms, name LIMIT {} OFFSET {};",
             max_count, start))) {
        retired_players.emplace_back(domain::RetiredPlayerId::FromString(id),
                                     name, score,
                                     std::chrono::milliseconds{play_time_ms});
    }
    return retired_players;
}

bool RetiredPlayerRepositoryImpl::Exists(std::string_view name) {
    return tx_
        .query01<int>("SELECT 1 FROM retired_players WHERE name = " +
                      tx_.quote(name))
        .has_value();
}

void RetiredPlayerRepositoryImpl::Save(
    const domain::RetiredPlayer& retired_player) {
    tx_.exec_prepared(util::postgres::ToZview(domain::request::add_new_player),
                      retired_player.GetId().ToString(),
                      retired_player.GetName(), retired_player.GetScores(),
                      retired_player.GetPlayTime().count());
}

void RetiredPlayerRepositoryImpl::IncreaseScores(std::string_view name,
                                                 size_t delta) {
    tx_.exec_prepared(
        util::postgres::ToZview(domain::request::increase_player_scores), name,
        delta);
}

void RetiredPlayerRepositoryImpl::IncreasePlayTime(std::string_view name,
                                                   size_t delta) {
    tx_.exec_prepared(
        util::postgres::ToZview(domain::request::increase_player_play_time),
        name, delta);
}

}  // namespace postgres
