#include "unit_of_work_impl.h"

#include <pqxx/transaction>

#include "retired_player_impl.h"

namespace postgres {
UnitOfWorkImpl::UnitOfWorkImpl(pqxx::connection& conn) noexcept : conn_{conn} {}

void UnitOfWorkImpl::InitPlayerDB() {
    TryExec([](pqxx::work& tx) { RetiredPlayerRepositoryImpl{tx}.CreateDB(); });
    TryExec(
        [](pqxx::work& tx) { RetiredPlayerRepositoryImpl{tx}.CreateIndex(); });
}

std::vector<domain::RetiredPlayer> UnitOfWorkImpl::GetTopKPlayers(
    size_t start, size_t max_count) {
    std::vector<domain::RetiredPlayer> result;
    TryExec([&result, start, max_count](pqxx::work& tx) {
        result =
            RetiredPlayerRepositoryImpl{tx}.GetTopKPlayers(start, max_count);
    });
    return result;
}

void UnitOfWorkImpl::IncreasePlayerStats(std::string_view name, size_t scores,
                                         size_t play_time) {
    TryExec([&name, scores, play_time](pqxx::work& tx) {
        RetiredPlayerRepositoryImpl player{tx};
        if (!player.Exists(name))
            player.Save(domain::RetiredPlayer{
                domain::RetiredPlayerId::New(),
                std::string(name.data(), name.size()), scores,
                std::chrono::milliseconds{play_time}});
        else {
            player.IncreaseScores(name, scores);
            player.IncreasePlayTime(name, play_time);
        }
    });
}
}  // namespace postgres