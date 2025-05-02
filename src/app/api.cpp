#include "api.h"

#include <config/config.h>
#include <domain/retired_player.h>
#include <postgres/unit_of_work_impl.h>

#include <boost/json.hpp>

namespace api {
namespace v1 {
namespace json = boost::json;

namespace map {
Result<Info> Get(const model::Game& game, const app::extra::Data& extra_data,
                 std::string_view map_id) {
    model::Map::Id model_map_id{std::string(map_id.data(), map_id.size())};
    auto map = game.FindMap(model_map_id);
    if (!map) return {std::nullopt, Status::MAP_NOT_FOUND};
    auto map_extra_data = extra_data.GetMapExtraData(model_map_id);
    if (!map_extra_data) {
        LOG(error) << "failed to get " << *model_map_id << " extra data";
        return {std::nullopt, Status::INTERNAL_SERVER_ERROR};
    }
    return {Info{*map, *map_extra_data}, Status::SUCCESS};
}

Result<model::Game::Maps> List(const model::Game& game) {
    return {game.GetMaps(), Status::SUCCESS};
}
}  // namespace map

namespace resource {
Result<app::StaticResource> Get(const app::ResourceHandler& resource_handler,
                                const fs::path& path) {
    try {
        if (!resource_handler.IsSubPath(path))
            return {std::nullopt, Status::INVALID_RESOURCE_PATH};
        return {app::StaticResource{resource_handler.GetAbsolutePath(path)},
                Status::SUCCESS};
    } catch (const std::exception& e) {
        LOG(error) << logging::add_value(app::log::additional_data,
                                         json::value{{"exception", e.what()}})
                   << "static resource failure";
    }
    return {std::nullopt, Status::RESOURCE_NOT_FOUND};
}
}  // namespace resource

namespace game {
namespace util {
std::optional<std::chrono::milliseconds> TryExtractTimeDelta(
    std::string_view request) {
    boost::system::error_code ec;
    auto jv = json::parse(request, ec);
    if (ec) {
        return std::nullopt;
    }
    int64_t time_delta{};
    if (auto delta = jv.as_object().if_contains(detail::timeDeltaField);
        delta) {
        try {
            time_delta = delta->as_int64();
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::chrono::milliseconds{time_delta};
}
}  // namespace util

Result<app::EmptyJson> Tick(app::GameState& game_state,
                            const model::move::MoveHandler& move_handler,
                            app::Players& players,
                            postgres::ConnectionPool& connection_pool,
                            std::chrono::milliseconds delta) {
    auto players_list = players.GetPlayerList();
    std::ranges::for_each(players_list, [&players, &connection_pool, delta](
                                            app::Players::PlayerRepr player) {
        auto& dog = player->GetDog();
        dog.IncreaseInactiveTime(delta);
        if (dog.GetInactiveTime() >=
            app::Config::GetInstance().GetDogRetirementTime()) {
            auto conn = connection_pool.GetConnection();
            postgres::UnitOfWorkImpl{*conn}.IncreasePlayerStats(
                dog.GetName(), dog.GetBag().score,
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    dog.GetLastActiveTimePoint() + dog.GetInactiveTime() -
                    dog.GetJoinGameTimePoint())
                    .count());
            players.RemovePlayer(player->GetToken());
        }
    });
    players_list = players.GetPlayerList();
    std::ranges::for_each(
        players_list, [&move_handler, delta](app::Players::PlayerRepr player) {
            move_handler.Tick(player, delta);
        });
    game_state.Tick(delta, players_list, move_handler);
    return {app::EmptyJson{}, Status::SUCCESS};
}

Result<State> GetState(const app::GameState& game_state,
                       const app::Players& players, std::string_view token) {
    using ReturnType = Result<State>;
    auto res = ExecuteAuthorized<ReturnType>(
        token,
        [&players, &game_state](const app::Token& token) -> ReturnType {
            auto map_id = players.GetPlayer(token)->GetMapId();
            auto map_state = game_state.GetState(map_id);
            if (!map_state) {
                LOG(error) << "unknown state on map " << *map_id;
                return {std::nullopt, Status::INTERNAL_SERVER_ERROR};
            }
            return {
                State{players.GetPlayerList(map_id), map_state->lost_objects},
                Status::SUCCESS};
        },
        [&players](const app::Token& token) {
            return players.TokenExists(token);
        });
    if (!std::holds_alternative<ReturnType>(res))
        return {std::nullopt, std::get<Status>(res)};
    return std::get<ReturnType>(res);
}

Result<std::vector<PlayerRecord>> GetRecords(
    postgres::ConnectionPool& connection_pool,
    const std::optional<size_t>& start,
    const std::optional<size_t>& max_count) {
    static constexpr uint8_t max_top_players = 100;

    size_t count = max_top_players;
    if (max_count) {
        if (*max_count > max_top_players)
            return {std::nullopt, API::Status::INVALID_QUERY_PARAM};
        count = *max_count;
    }

    std::vector<domain::RetiredPlayer> retired_players;
    {
        auto conn = connection_pool.GetConnection();
        retired_players = postgres::UnitOfWorkImpl{*conn}.GetTopKPlayers(
            start.value_or(0), count);
    }
    std::vector<PlayerRecord> result;
    result.reserve(retired_players.size());
    std::ranges::transform(
        retired_players, std::back_inserter(result),
        [](const domain::RetiredPlayer& reitred_player) {
            return PlayerRecord{reitred_player.GetName(),
                                reitred_player.GetScores(),
                                reitred_player.GetPlayTime().count() / 1000.0};
        });
    return {result, API::Status::SUCCESS};
}

namespace player {
Result<Authorization> Join(
    const model::Game& game, app::Players& players, std::string_view request,
    bool randomize_position,
    [[maybe_unused]] const model::move::MoveHandler& move_handler) {
    using enum api::v1::Status;
    boost::system::error_code ec;
    auto jv = json::parse(request, ec);
    if (ec) {
        return {std::nullopt, INVALID_JSON};
    }

    if (auto obj = jv.as_object(); !obj.if_contains(detail::userNameFiled) ||
                                   !obj.if_contains(detail::mapIdField))
        return {std::nullopt, INVALID_JSON};

    std::string user_name = jv.at(detail::userNameFiled).as_string().c_str();
    model::Map::Id map_id{jv.at(detail::mapIdField).as_string().c_str()};

    if (user_name.empty()) return {std::nullopt, INVALID_USER_NAME};

    {
        auto map = game.FindMap(map_id);
        if (!map) return {std::nullopt, MAP_NOT_FOUND};
    }

    auto player = players.AddPlayer(
        user_name, map_id,
        (randomize_position ? move_handler.GetRandomPos(map_id)
                            : move_handler.GetStartPos(map_id)));
    if (!player) return {std::nullopt, INTERNAL_SERVER_ERROR};

    return {Authorization{player->GetToken().Hex(), player->GetId()}, SUCCESS};
}

Result<app::Players::ConstPlayerList> List(const app::Players& players,
                                           std::string_view token) {
    using ReturnType = Result<app::Players::ConstPlayerList>;
    auto res = ExecuteAuthorized<ReturnType>(
        token,
        [&players]([[maybe_unused]] const app::Token& token) {
            return Result<app::Players::ConstPlayerList>{
                players.GetPlayerList(players.GetPlayer(token)->GetMapId()),
                Status::SUCCESS};
        },
        [&players](const app::Token& token) -> bool {
            return players.TokenExists(token);
        });
    if (!std::holds_alternative<ReturnType>(res))
        return {std::nullopt, std::get<Status>(res)};
    return std::get<ReturnType>(res);
}

Result<app::EmptyJson> Action(app::Players& players, std::string_view request,
                              std::string_view token) {
    using ReturnType = Result<app::EmptyJson>;
    auto res = ExecuteAuthorized<ReturnType>(
        token,
        [&players, request](const app::Token& token) -> ReturnType {
            boost::system::error_code ec;
            auto jv = json::parse(request, ec);
            if (ec) return {std::nullopt, Status::INVALID_JSON};
            app::entity::Dog::Movement::Direction dir{};
            try {
                if (auto move = jv.as_object().if_contains(detail::moveField);
                    move) {
                    if (auto opt_dir = app::entity::StrToDogDirection(
                            move->as_string().c_str());
                        opt_dir)
                        dir = *opt_dir;
                    else
                        return {std::nullopt, Status::INVALID_JSON};
                } else
                    return {std::nullopt, Status::INVALID_JSON};
            } catch (const std::exception&) {
                return {std::nullopt, Status::INVALID_JSON};
            }
            auto player = players.GetPlayer(token);
            if (!player) return {std::nullopt, Status::TOKEN_NOT_FOUND};
            player->GetDog().MoveTo(dir);

            return {app::EmptyJson{}, Status::SUCCESS};
        },
        [&players](const app::Token& token) -> bool {
            return players.TokenExists(token);
        });

    if (!std::holds_alternative<ReturnType>(res))
        return {std::nullopt, std::get<Status>(res)};
    return std::get<ReturnType>(res);
}
}  // namespace player
}  // namespace game
}  // namespace v1
}  // namespace api
