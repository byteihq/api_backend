#include "json_serialization.h"

#include <app/assets.h>
#include <util/util.h>

#include <algorithm>
#include <type_traits>

namespace boost::json {
object parse(const model::Road& road) {
    object obj;
    obj["x0"] = road.GetStart().x;
    obj["y0"] = road.GetStart().y;
    if (road.IsVertical())
        obj["y1"] = road.GetEnd().y;
    else
        obj["x1"] = road.GetEnd().x;
    return obj;
}

object parse(const model::Building& building) {
    json::object obj;
    obj["x"] = building.GetBounds().position.x;
    obj["y"] = building.GetBounds().position.y;
    obj["w"] = building.GetBounds().size.width;
    obj["h"] = building.GetBounds().size.height;
    return obj;
}

object parse(const model::Office& office) {
    json::object obj;
    obj["id"] = *office.GetId();
    obj["x"] = office.GetPosition().x;
    obj["y"] = office.GetPosition().y;
    obj["offsetX"] = office.GetOffset().dx;
    obj["offsetY"] = office.GetOffset().dy;
    return obj;
}

object parse(const model::Map& map) {
    json::object obj;
    obj["id"] = *map.GetId();
    obj["name"] = map.GetName();

    json::array json_arr;
    {
        auto roads = map.GetRoads();
        std::transform(roads.begin(), roads.end(), std::back_inserter(json_arr),
                       [](const model::Road& road) { return parse(road); });
    }
    obj["roads"] = std::move(json_arr);
    // json_arr можно использовать после перемещения, см
    // https://www.boost.org/doc/libs/1_83_0/libs/json/doc/html/json/ref/boost__json__array/array/overload9.html
    {
        auto buildings = map.GetBuildings();
        std::transform(
            buildings.begin(), buildings.end(), std::back_inserter(json_arr),
            [](const model::Building& building) { return parse(building); });
    }
    obj["buildings"] = std::move(json_arr);
    {
        auto offices = map.GetOffices();
        std::transform(
            offices.begin(), offices.end(), std::back_inserter(json_arr),
            [](const model::Office& office) { return parse(office); });
    }
    obj["offices"] = std::move(json_arr);
    return obj;
}

array parse(const model::Game::Maps& maps) {
    array arr;
    std::transform(maps.begin(), maps.end(), std::back_inserter(arr),
                   [](const model::Map& map) {
                       json::object obj;
                       obj["id"] = *map.GetId();
                       obj["name"] = map.GetName();
                       return obj;
                   });
    return arr;
}

std::string serialize(const model::Road& road) {
    return serialize(parse(road));
}

std::string serialize(const model::Building& building) {
    return serialize(parse(building));
}

std::string serialize(const model::Office& office) {
    return serialize(parse(office));
}

std::string serialize(const model::Map& map) { return serialize(parse(map)); }

std::string serialize(const model::Game::Maps& maps) {
    return serialize(parse(maps));
}
}  // namespace boost::json

namespace app {
boost::json::object parse(const API::game::Authorization& authorization) {
    boost::json::object obj;
    obj["authToken"] = authorization.auth_token;
    obj["playerId"] = *authorization.player_id;

    return obj;
}

boost::json::object parse(const app::Players::ConstPlayerRepr& player,
                          detail::player_serializer serializer) {
    boost::json::object obj;
    if (serializer == detail::player_serializer::Minimal) {
        obj["name"] = player->GetName();
    } else if (serializer == detail::player_serializer::WithDogPosition) {
        obj = parse(player->GetDog());
    } else
        throw std::logic_error("unimplemented serializer");

    return obj;
}

boost::json::object parse(const app::Players::ConstPlayerList& players,
                          detail::player_serializer serializer) {
    boost::json::object obj;
    if (serializer == detail::player_serializer::Minimal) {
        std::for_each(players.begin(), players.end(),
                      [&obj, &serializer](const auto& player) {
                          obj[std::to_string(*player->GetId())] =
                              parse(player, serializer);
                      });
    } else if (serializer == detail::player_serializer::WithDogPosition) {
        obj["players"] = boost::json::object{};
        std::for_each(
            players.begin(), players.end(),
            [&obj, &serializer](const auto& player) {
                obj["players"].as_object()[std::to_string(*player->GetId())] =
                    parse(player, serializer);
            });
    } else
        throw std::logic_error("unimplemented serializer");

    return obj;
}

boost::json::object parse(const app::entity::Dog& dog) {
    static constexpr std::string_view map_direction[5] = {
        "L", "R", "U", "D", {}};

    boost::json::object obj{
        {"pos",
         boost::json::array{dog.GetCurPosition().x, dog.GetCurPosition().y}},
        {"speed", boost::json::array{dog.GetSpeed().x, dog.GetSpeed().y}}};
    if (auto pos = static_cast<
            std::underlying_type_t<app::entity::Dog::Movement::Direction>>(
            dog.GetDirection());
        pos < sizeof(map_direction))
        obj["dir"] = map_direction[pos];
    else
        throw std::runtime_error("unknown direction: " + std::to_string(pos));

    obj["bag"] = boost::json::array();
    const auto& bag = dog.GetBag();
    for (const auto& item : bag.items) {
        obj["bag"].as_array().push_back(
            boost::json::object{{"id", item.id}, {"type", item.type}});
    }
    obj["score"] = bag.score;

    return obj;
}

boost::json::object parse(const API::map::Info& map_info) {
    boost::json::object obj = boost::json::parse(map_info.map);
    boost::json::array loot_types;
    std::transform(map_info.extra_data.loot_types.begin(),
                   map_info.extra_data.loot_types.end(),
                   std::back_inserter(loot_types),
                   [](const app::extra::Data::Map::LootType& loot_type) {
                       return loot_type.obj;
                   });
    obj["lootTypes"] = std::move(loot_types);

    return obj;
}

boost::json::object parse(
    const app::GameState::State::LostObjects& lost_objects) {
    boost::json::object obj;
    for (const auto& lost_obj : lost_objects) {
        obj[std::to_string(lost_obj.id)] = boost::json::object{
            {"type", lost_obj.type},
            {"pos", boost::json::array{lost_obj.pos.x, lost_obj.pos.y}}};
    }
    return obj;
}

boost::json::object parse(const API::game::State& state) {
    auto obj = parse(state.players, detail::player_serializer::WithDogPosition);
    obj["lostObjects"] = parse(state.lost_objects);
    return obj;
}

boost::json::object parse(const API::game::PlayerRecord& player_record) {
    return boost::json::object{{"name", player_record.name},
                               {"score", player_record.score},
                               {"playTime", player_record.play_time_s}};
}

boost::json::array parse(
    const std::vector<API::game::PlayerRecord>& player_records) {
    boost::json::array array;
    std::ranges::transform(player_records, std::back_inserter(array),
                           [](const API::game::PlayerRecord& player_record) {
                               return parse(player_record);
                           });
    return array;
}

std::string serialize(API::Status status) {
    switch (status) {
        case API::Status::SUCCESS:
            return "SUCCESS";
        case API::Status::MAP_NOT_FOUND:
            return "MAP_NOT_FOUND";
        case API::Status::RESOURCE_NOT_FOUND:
            return "RESOURCE_NOT_FOUND";
        case API::Status::INVALID_RESOURCE_PATH:
            return "INVALID_RESOURCE_PATH";
        case API::Status::INVALID_USER_NAME:
            return "INVALID_USER_NAME";
        case API::Status::INVALID_JSON:
            return "INVALID_JSON";
        case API::Status::INVALID_TOKEN:
            return "INVALID_TOKEN";
        case API::Status::TOKEN_NOT_FOUND:
            return "TOKEN_NOT_FOUND";
        case API::Status::BAD_API_REQUEST:
            return "BAD_API_REQUEST";
        case API::Status::INVALID_QUERY_PARAM:
            return "INVALID_QUERY_PARAM";
        case API::Status::INTERNAL_SERVER_ERROR:
            return "INTERNAL_SERVER_ERROR";
        default:
            return "UNKNOWN";
    }
}

std::string serialize(const API::game::Authorization& authorization) {
    return boost::json::serialize(parse(authorization));
}

std::string serialize(const app::Players::ConstPlayerList& players,
                      detail::player_serializer serializer) {
    auto res = boost::json::serialize(parse(players, serializer));
    if (serializer == detail::player_serializer::WithDogPosition &&
        std::is_floating_point_v<app::Dimension>)
        return util::ScientificToFixed(res);
    return res;
}

std::string serialize(const app::EmptyJson&) { return "{}"; }

std::string serialize(const API::map::Info& map_info) {
    return boost::json::serialize(parse(map_info));
}

std::string serialize(const API::game::State& state) {
    auto res = boost::json::serialize(parse(state));
    if constexpr (std::is_floating_point_v<app::Dimension>)
        return util::ScientificToFixed(res);
    else
        return res;
}
std::string serialize(const API::game::PlayerRecord& player_record) {
    return boost::json::serialize(parse(player_record));
}
std::string serialize(
    const std::vector<API::game::PlayerRecord>& player_records) {
    return boost::json::serialize(parse(player_records));
}
}  // namespace app
