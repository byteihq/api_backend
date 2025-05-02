#include <config/config.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <filesystem>
#include <type_traits>

namespace fs = std::filesystem;

SCENARIO("Parsing config file") {
    GIVEN("a config parser") {
        app::Config& cfg{app::Config::GetInstance()};
        GIVEN("a non-exsitent config file") {
            fs::path cfg_file{"test.json"};
            CHECK_THROWS_AS(cfg.LoadFromFile(cfg_file), std::invalid_argument);
        }
        GIVEN("an existing config file with invalid json") {
            fs::path cfg_file{"invalid.json"};
            REQUIRE(fs::exists(cfg_file));
            CHECK_THROWS_AS(cfg.LoadFromFile(cfg_file), std::invalid_argument);
        }
        GIVEN("a specific valid config file") {
            fs::path cfg_file{"valid.json"};
            REQUIRE(fs::exists(cfg_file));
            REQUIRE_NOTHROW(cfg.LoadFromFile(cfg_file));

            using Catch::Matchers::WithinAbs;
            constexpr float margin{0.0001f};
            THEN("a loot generator has period = 5 and probability = 0.5") {
                CHECK_THAT(cfg.GetLootGeneratorPeriod(), WithinAbs(5, margin));
                CHECK_THAT(cfg.GetLootGeneratorProbability(),
                           WithinAbs(0.5, margin));
            }
            THEN("dog speed on map1 = 4") {
                model::Map::Id id{"map1"};
                if constexpr (std::is_floating_point_v<app::Dimension>)
                    CHECK_THAT(cfg.GetDogSpeed(id), WithinAbs(4, margin));
                else
                    CHECK(cfg.GetDogSpeed(id) == 4);
            }
            THEN("dog speed in town = default dog speed = 3") {
                model::Map::Id id{"town"};
                if constexpr (std::is_floating_point_v<app::Dimension>)
                    CHECK_THAT(cfg.GetDogSpeed(id), WithinAbs(3, margin));
                else
                    CHECK(cfg.GetDogSpeed(id) == 3);
            }
        }
    }
}
