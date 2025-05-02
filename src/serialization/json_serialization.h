#pragma once

#include <app/api.h>
#include <app/assets.h>
#include <app/extra_data.h>
#include <entities/player.h>
#include <model/model.h>

#include <boost/json.hpp>
#include <string>

namespace boost::json {
object parse(const model::Road& road);
object parse(const model::Building& building);
object parse(const model::Office& office);
object parse(const model::Map& map);
array parse(const model::Game::Maps& maps);

std::string serialize(const model::Road& road);
std::string serialize(const model::Building& building);
std::string serialize(const model::Office& office);
std::string serialize(const model::Map& map);
std::string serialize(const model::Game::Maps& maps);
}  // namespace boost::json

namespace app {
namespace detail {
enum class player_serializer { Minimal, WithDogPosition };
}

boost::json::object parse(const API::game::Authorization& authorization);
boost::json::object parse(
    const app::Players::ConstPlayerRepr& player,
    detail::player_serializer serializer = detail::player_serializer::Minimal);
boost::json::object parse(
    const app::Players::ConstPlayerList& players,
    detail::player_serializer serializer = detail::player_serializer::Minimal);
boost::json::object parse(const app::entity::Dog& dog);
boost::json::object parse(const API::map::Info& map_info);
boost::json::object parse(
    const app::GameState::State::LostObjects& lost_objects);
boost::json::object parse(const API::game::State& state);
boost::json::object parse(const API::game::PlayerRecord& player_record);
boost::json::array parse(
    const std::vector<API::game::PlayerRecord>& player_records);

std::string serialize(API::Status status);
std::string serialize(const API::game::Authorization& authorization);
std::string serialize(const app::Players::ConstPlayerList& players);
std::string serialize(
    const app::Players::ConstPlayerList& players,
    detail::player_serializer serializer = detail::player_serializer::Minimal);
std::string serialize(const app::EmptyJson&);
std::string serialize(const API::map::Info& map_info);
std::string serialize(const API::game::State& state);
std::string serialize(const API::game::PlayerRecord& player_record);
std::string serialize(
    const std::vector<API::game::PlayerRecord>& player_records);
}  // namespace app
