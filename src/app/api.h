#pragma once

#include <entities/player.h>
#include <logger/logger.h>
#include <model/model.h>
#include <postgres/connection_pool.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include "assets.h"
#include "extra_data.h"
#include "game_state.h"
#include "move_handler.h"
#include "resource_handler.h"

#ifndef API_VER
#define API_VER v1
#endif

#ifndef API
#define API api::API_VER
#endif

namespace api {
namespace v1 {
namespace fs = std::filesystem;

enum class Status : uint8_t {
    SUCCESS,
    MAP_NOT_FOUND,
    RESOURCE_NOT_FOUND,
    INVALID_RESOURCE_PATH,
    INVALID_USER_NAME,
    INVALID_JSON,
    INVALID_TOKEN,
    TOKEN_NOT_FOUND,
    BAD_API_REQUEST,
    INVALID_QUERY_PARAM,
    INTERNAL_SERVER_ERROR
};

template <typename T>
struct Result {
    std::optional<T> value;
    Status ec;

    constexpr operator bool() const noexcept { return ec == Status::SUCCESS; }
    constexpr const T& operator()() const { return value.value(); }
};

namespace map {
struct Info {
    model::Map map;
    app::extra::Data::Map extra_data;
};

Result<Info> Get(const model::Game& game, const app::extra::Data& extra_data,
                 std::string_view map_id);
Result<model::Game::Maps> List(const model::Game& game);
}  // namespace map

namespace resource {
Result<app::StaticResource> Get(const app::ResourceHandler& resource_handler,
                                const fs::path& path);
}

namespace game {
namespace detail {
using namespace std::literals;
static constexpr std::string_view authorizationType = "Bearer "sv;
static constexpr std::string_view timeDeltaField = "timeDelta"sv;
}  // namespace detail

struct Authorization {
    std::string auth_token;
    app::entity::Player::Id player_id;
};

struct State {
    app::Players::ConstPlayerList players;
    app::GameState::State::LostObjects lost_objects;
};

struct PlayerRecord {
    std::string name;
    size_t score;
    double play_time_s;
};

template <typename Res, typename Fn, typename Pred>
std::variant<Status, Res> ExecuteAuthorized(std::string_view token, Fn&& action,
                                            Pred&& pred) {
    std::unique_ptr<app::Token> ptoken;
    try {
        if (!token.starts_with(detail::authorizationType))
            return Status::INVALID_TOKEN;
        token.remove_prefix(detail::authorizationType.size());
        ptoken = std::make_unique<app::Token>(
            std::string{token.begin(), token.end()});
        if (!ptoken) return Status::INTERNAL_SERVER_ERROR;
        if (!pred(*ptoken)) return Status::TOKEN_NOT_FOUND;
    } catch (const std::exception& e) {
        LOG(error) << logging::add_value(app::log::additional_data,
                                         json::value{{"exception", e.what()}})
                   << "authorization check failure";
        return Status::INVALID_TOKEN;
    }
    return action(*ptoken);
}

namespace util {
std::optional<std::chrono::milliseconds> TryExtractTimeDelta(
    std::string_view request);
}
Result<app::EmptyJson> Tick(app::GameState& game_state,
                            const model::move::MoveHandler& move_handler,
                            app::Players& players,
                            postgres::ConnectionPool& connection_pool,
                            std::chrono::milliseconds delta);
Result<State> GetState(const app::GameState& game_state,
                       const app::Players& players, std::string_view token);
Result<std::vector<PlayerRecord>> GetRecords(
    postgres::ConnectionPool& connection_pool,
    const std::optional<size_t>& start = std::nullopt,
    const std::optional<size_t>& max_count = std::nullopt);

namespace player {
namespace detail {
using namespace std::literals;
static constexpr std::string_view authorizationType = "Bearer "sv;
static constexpr std::string_view userNameFiled = "userName"sv;
static constexpr std::string_view mapIdField = "mapId"sv;
static constexpr std::string_view moveField = "move"sv;
}  // namespace detail

Result<Authorization> Join(
    const model::Game& game, app::Players& players, std::string_view request,
    bool randomize_position,
    [[maybe_unused]] const model::move::MoveHandler& move_handler);
Result<app::Players::ConstPlayerList> List(const app::Players& players,
                                           std::string_view token);
Result<app::EmptyJson> Action(app::Players& players, std::string_view request,
                              std::string_view token);
}  // namespace player
}  // namespace game
}  // namespace v1
}  // namespace api
