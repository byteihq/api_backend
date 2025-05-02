#include "json_loader.h"

#include <fmt/core.h>

#include <algorithm>
#include <boost/json.hpp>
#include <fstream>
#include <sstream>
#include <string_view>

namespace json_loader {

using namespace std::literals;
namespace json = boost::json;

struct Fields {
    static constexpr std::string_view maps = "maps"sv;

    struct Map {
        static constexpr std::string_view id = "id"sv;
        static constexpr std::string_view name = "name"sv;
        static constexpr std::string_view roads = "roads"sv;
        static constexpr std::string_view buildings = "buildings"sv;
        static constexpr std::string_view offices = "offices"sv;
        static constexpr std::string_view lootTypes = "lootTypes"sv;
    };
    struct Road {
        static constexpr std::string_view x0 = "x0"sv;
        static constexpr std::string_view x1 = "x1"sv;
        static constexpr std::string_view y0 = "y0"sv;
        static constexpr std::string_view y1 = "y1"sv;
    };
    struct Building {
        static constexpr std::string_view x = "x"sv;
        static constexpr std::string_view y = "y"sv;
        static constexpr std::string_view w = "w"sv;
        static constexpr std::string_view h = "h"sv;
    };
    struct Office {
        static constexpr std::string_view id = "id"sv;
        static constexpr std::string_view x = "x"sv;
        static constexpr std::string_view y = "y"sv;
        static constexpr std::string_view offsetX = "offsetX"sv;
        static constexpr std::string_view offsetY = "offsetY"sv;
    };
    struct LootType {
        static constexpr std::string_view name = "name"sv;
        static constexpr std::string_view file = "file"sv;
        static constexpr std::string_view type = "type"sv;
        static constexpr std::string_view rotation = "rotation"sv;
        static constexpr std::string_view color = "color"sv;
        static constexpr std::string_view scale = "scale"sv;
    };
};

template <typename T>
T json_int64_as(const json::value& obj, std::string_view field) {
    return static_cast<T>(obj.at(field).as_int64());
}

static std::vector<model::Road> LoadRoads(const json::array& jroads) {
    std::vector<model::Road> roads;
    for (const auto& jroad : jroads) {
        model::Point start_point{
            json_int64_as<model::Dimension>(jroad, Fields::Road::x0),
            json_int64_as<model::Dimension>(jroad, Fields::Road::y0)};
        if (jroad.as_object().if_contains(Fields::Road::x1))  // horizontal road
            roads.emplace_back(
                model::Road::HORIZONTAL, start_point,
                json_int64_as<model::Dimension>(jroad, Fields::Road::x1));
        else
            roads.emplace_back(
                model::Road::VERTICAL, start_point,
                json_int64_as<model::Dimension>(jroad, Fields::Road::y1));
    }
    return roads;
}

static std::vector<model::Building> LoadBuildings(
    const json::array& jbuildings) {
    std::vector<model::Building> buildings;
    for (const auto& jbuilding : jbuildings) {
        buildings.emplace_back(model::Rectangle{
            model::Point{
                json_int64_as<model::Dimension>(jbuilding, Fields::Building::x),
                json_int64_as<model::Dimension>(jbuilding,
                                                Fields::Building::y)},
            model::Size{
                json_int64_as<model::Dimension>(jbuilding, Fields::Building::w),
                json_int64_as<model::Dimension>(jbuilding,
                                                Fields::Building::h)}});
    }
    return buildings;
}

static std::vector<model::Office> LoadOffices(const json::array& joffices) {
    std::vector<model::Office> offices;
    for (const auto& joffice : joffices) {
        offices.emplace_back(
            model::Office::Id{
                joffice.at(Fields::Office::id).as_string().c_str()},
            model::Point{
                json_int64_as<model::Dimension>(joffice, Fields::Office::x),
                json_int64_as<model::Dimension>(joffice, Fields::Office::y)},
            model::Offset{json_int64_as<model::Dimension>(
                              joffice, Fields::Office::offsetX),
                          json_int64_as<model::Dimension>(
                              joffice, Fields::Office::offsetY)});
    }
    return offices;
}

model::Game LoadGame(const std::filesystem::path& json_path) {
    // Загрузить содержимое файла json_path, например, в виде строки
    // Распарсить строку как JSON, используя boost::json::parse
    // Загрузить модель игры из файла
    json::value json_cfg;
    {
        std::ifstream cfg(json_path);
        if (!cfg)
            throw std::runtime_error(fmt::format(
                "LoadGame: can't open file: {}", json_path.string()));
        std::stringstream buf;
        buf << cfg.rdbuf();
        cfg.close();
        json_cfg = json::parse(buf.str());
    }
    auto maps = json_cfg.at(Fields::maps);
    if (!maps.is_array())
        throw std::logic_error(fmt::format(
            "LoadGame: field '{}' should be an array", Fields::maps));

    model::Game game;
    for (const auto& map : maps.as_array()) {
        model::Map gmap{
            model::Map::Id{map.at(Fields::Map::id).as_string().c_str()},
            map.at(Fields::Map::name).as_string().c_str()};
        {
            auto roads = LoadRoads(map.at(Fields::Map::roads).as_array());
            std::for_each(
                roads.begin(), roads.end(),
                [&gmap](const model::Road& road) { gmap.AddRoad(road); });
        }
        if (auto jbuildings =
                map.as_object().if_contains(Fields::Map::buildings);
            jbuildings) {
            auto buildings = LoadBuildings(jbuildings->as_array());
            std::for_each(buildings.begin(), buildings.end(),
                          [&gmap](const model::Building& building) {
                              gmap.AddBuilding(building);
                          });
        }
        if (auto joffices = map.as_object().if_contains(Fields::Map::offices);
            joffices) {
            auto offices = LoadOffices(joffices->as_array());
            std::for_each(offices.begin(), offices.end(),
                          [&gmap](const model::Office& office) {
                              gmap.AddOffice(office);
                          });
        }
        game.AddMap(std::move(gmap));
    }

    return game;
}

void LoadMapLootTypes(const std::filesystem::path& json_path,
                      app::extra::Data& extra) {
    json::value json_cfg;
    {
        std::ifstream cfg(json_path);
        if (!cfg)
            throw std::runtime_error(fmt::format(
                "LoadMapLootTypes: can't open file: {}", json_path.string()));
        std::stringstream buf;
        buf << cfg.rdbuf();
        cfg.close();
        json_cfg = json::parse(buf.str());
    }

    auto maps = json_cfg.at(Fields::maps);
    if (!maps.is_array())
        throw std::logic_error(fmt::format(
            "LoadMapLootTypes: field '{}' should be an array", Fields::maps));

    for (const auto& map : maps.as_array()) {
        auto loot_types = map.at(Fields::Map::lootTypes).as_array();
        for (const auto& loot_type : loot_types) {
            extra.AddMapLootType(
                model::Map::Id{map.at(Fields::Map::id).as_string().c_str()},
                app::extra::Data::Map::LootType{loot_type.as_object()});
        }
    }
}

}  // namespace json_loader
