#include "config.h"

#include <fmt/core.h>
#include <logger/logger.h>

#include <boost/json.hpp>
#include <fstream>
#include <sstream>
#include <type_traits>

namespace app {
namespace json = boost::json;

static Dimension cast_to_dimension(const json::value& v,
                                   std::string_view field) {
    if constexpr (std::is_floating_point_v<Dimension>) {
        if constexpr (std::is_same_v<Dimension, double>)
            return v.at(field).as_double();
        else
            return static_cast<Dimension>(v.at(field).as_double());
    } else {
        if constexpr (std::is_same_v<Dimension, int64_t>)
            return v.at(field).as_int64();
        else
            return static_cast<Dimension>(v.at(field).as_int64());
    }
}

void Config::LoadFromFile(const fs::path& path) {
    if (!fs::exists(path) || !fs::is_regular_file(path))
        throw std::invalid_argument(fmt::format(
            "config: {} file not found or has invalid type", path.string()));

    std::ifstream file(path);
    if (!file)
        throw std::invalid_argument(
            fmt::format("config: failed to open file: {}", path.string()));

    std::stringstream ss;
    ss << file.rdbuf();
    file.close();

    boost::system::error_code ec;
    auto jv = json::parse(ss.str(), ec);
    if (ec) {
        LOG(error) << logging::add_value(app::log::additional_data,
                                         json::value{{"code", ec.value()},
                                                     {"text", ec.what()}})
                   << "failed to parse config as json";
        throw std::invalid_argument(fmt::format(
            "config: file: {} contains invalid json", path.string()));
    }

    if (jv.as_object().if_contains(Fields::defaultDogSpeed)) {
        dog_attributes_.default_speed =
            cast_to_dimension(jv, Fields::defaultDogSpeed);
        LOG(debug) << Fields::defaultDogSpeed << " = "
                   << dog_attributes_.default_speed;
    }

    if (auto dog_retirement_time =
            jv.as_object().if_contains(Fields::dogRetirementTime);
        dog_retirement_time) {
        dog_attributes_.retirement_time = std::chrono::milliseconds{
            static_cast<uint64_t>(dog_retirement_time->as_double() * 1000)};
        LOG(debug) << Fields::dogRetirementTime << " = "
                   << dog_attributes_.retirement_time.count();
    }

    if (jv.as_object().if_contains(Fields::defaultBagCapacity)) {
        bag_attributes_.default_capacity =
            jv.at(Fields::defaultBagCapacity).as_uint64();
        LOG(debug) << Fields::defaultBagCapacity << " = "
                   << bag_attributes_.default_capacity;
    }

    if (auto maps = jv.as_object().if_contains(Fields::maps); maps) {
        for (const auto& map : maps->as_array()) {
            if (map.as_object().if_contains(Fields::Map::dogSpeed)) {
                auto speed = cast_to_dimension(map, Fields::Map::dogSpeed);
                dog_attributes_
                    .speed_on_map[map.at(Fields::Map::id).as_string().c_str()] =
                    speed;
                LOG(debug) << Fields::Map::dogSpeed << " on "
                           << map.at(Fields::Map::id).as_string() << " = "
                           << speed;
            }
            if (map.as_object().if_contains(Fields::Map::bagCapacity)) {
                auto capacity = map.at(Fields::Map::bagCapacity).as_uint64();
                bag_attributes_.capacity_on_map
                    [map.at(Fields::Map::id).as_string().c_str()] = capacity;
                LOG(debug) << Fields::Map::bagCapacity << " on "
                           << map.at(Fields::Map::id).as_string() << " = "
                           << capacity;
            }
        }
    } else
        throw std::out_of_range(fmt::format(
            "no maps found but at least one is required in config file: {}",
            path.string()));

    if (auto loot_generator_config =
            jv.as_object().if_contains(Fields::lootGeneratorConfig);
        !loot_generator_config)
        throw std::out_of_range(
            fmt::format("config: field: {} not found in file: {} but required",
                        Fields::lootGeneratorConfig, path.string()));
    else {
        if (auto period = loot_generator_config->as_object().if_contains(
                Fields::LootGenerator::period);
            !period)
            throw std::out_of_range(fmt::format(
                "config: field: {} not found in {} but required",
                Fields::LootGenerator::period, Fields::lootGeneratorConfig));
        else
            loot_generator_attributes_.period =
                static_cast<float>(period->as_double());

        if (auto probability = loot_generator_config->as_object().if_contains(
                Fields::LootGenerator::probability);
            !probability)
            throw std::out_of_range(
                fmt::format("config: field: {} not found in {} but required",
                            Fields::LootGenerator::probability,
                            Fields::lootGeneratorConfig));
        else
            loot_generator_attributes_.probability =
                static_cast<float>(probability->as_double());

        LOG(debug) << Fields::lootGeneratorConfig << ": { "
                   << Fields::LootGenerator::period << " = "
                   << loot_generator_attributes_.period << ", "
                   << Fields::LootGenerator::probability << " = "
                   << loot_generator_attributes_.probability;
    }
}

Dimension Config::GetDogSpeed(const model::Map::Id& map_id) const {
    if (!dog_attributes_.speed_on_map.contains(*map_id))
        return dog_attributes_.default_speed;
    return dog_attributes_.speed_on_map.at(*map_id);
}

std::chrono::milliseconds Config::GetDogRetirementTime() const {
    return dog_attributes_.retirement_time;
}

float Config::GetLootGeneratorPeriod() const noexcept {
    return loot_generator_attributes_.period;
}
float Config::GetLootGeneratorProbability() const noexcept {
    return loot_generator_attributes_.probability;
}

size_t Config::GetBagCapacity(const model::Map::Id& map_id) const {
    if (!bag_attributes_.capacity_on_map.contains(*map_id))
        return bag_attributes_.default_capacity;
    return bag_attributes_.capacity_on_map.at(*map_id);
}

Config& Config::GetInstance() {
    static Config cfg;
    return cfg;
}
}  // namespace app
