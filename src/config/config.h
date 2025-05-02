#pragma once

#include <app/assets.h>
#include <model/model.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace app {
using namespace std::literals;
namespace fs = std::filesystem;

class Config final {
   private:
    Config() = default;

   public:
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    void LoadFromFile(const fs::path& path);
    Dimension GetDogSpeed(const model::Map::Id& map_id) const;
    std::chrono::milliseconds GetDogRetirementTime() const;

    float GetLootGeneratorPeriod() const noexcept;
    float GetLootGeneratorProbability() const noexcept;

    size_t GetBagCapacity(const model::Map::Id& map_id) const;

    static Config& GetInstance();

   private:
    struct DefaultValues {
        static constexpr Dimension dogSpeed = 1;
        static constexpr size_t bagCapacity = 3;
        static constexpr std::chrono::milliseconds dogRetirementTime{1000};
    };

    struct DogAttributes {
        Dimension default_speed{DefaultValues::dogSpeed};
        std::unordered_map<std::string, Dimension> speed_on_map;
        std::chrono::milliseconds retirement_time{
            DefaultValues::dogRetirementTime};
    } dog_attributes_;

    struct LootGeneratorAttributes {
        float period;
        float probability;
    } loot_generator_attributes_;

    struct BagAttributes {
        size_t default_capacity{DefaultValues::bagCapacity};
        std::unordered_map<std::string, size_t> capacity_on_map;
    } bag_attributes_;

    struct Fields {
        static constexpr std::string_view defaultDogSpeed = "defaultDogSpeed"sv;
        static constexpr std::string_view defaultBagCapacity =
            "defaultBagCapacity"sv;
        static constexpr std::string_view dogRetirementTime =
            "dogRetirementTime"sv;
        static constexpr std::string_view maps = "maps"sv;
        struct Map {
            static constexpr std::string_view id = "id"sv;
            static constexpr std::string_view dogSpeed = "dogSpeed"sv;
            static constexpr std::string_view bagCapacity = "bagCapacity"sv;
        };

        static constexpr std::string_view lootGeneratorConfig =
            "lootGeneratorConfig"sv;
        struct LootGenerator {
            static constexpr std::string_view period = "period"sv;
            static constexpr std::string_view probability = "probability"sv;
        };
    };
};
}  // namespace app
