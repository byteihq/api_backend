#pragma once

#include <entities/player.h>
#include <model/model.h>
#include <util/tagged.h>

#include <chrono>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "assets.h"
#include "extra_data.h"
#include "geom.h"
#include "loot_generator.h"
#include "move_handler.h"

namespace app {
class GameState final {
   public:
    struct State {
        struct LostObject {
            size_t id;
            uint16_t type;
            uint16_t value;
            geom::Point2D pos;

            constexpr bool operator==(const LostObject& other) const noexcept {
                return id == other.id;
            }
        };
        struct LostObjectHasher {
            size_t operator()(const LostObject& obj) const {
                return std::hash<size_t>{}(obj.id);
            }
        };

        using LostObjects = std::unordered_set<LostObject, LostObjectHasher>;

        LostObjects lost_objects;
        size_t players_count{0};
        size_t loot_types_count{0};
        size_t id{0};
    };

   public:
    GameState();

    void Tick(std::chrono::milliseconds time_delta,
              const Players::PlayerList& players,
              const model::move::MoveHandler& move_handler);

    void SetLootTypes(const model::Map::Id& map_id,
                      const app::extra::Data::Map::LootTypes& loot_types);
    size_t AddLostObjectOnMap(const model::Map::Id& map_id,
                              const State::LostObject& lost_object);
    void SetPlayersCountOnMap(const model::Map::Id& map_id, size_t count);

    std::optional<State> GetState(const model::Map::Id& map_id) const;
    inline const std::unordered_map<model::Map::Id, State,
                                    util::TaggedHasher<model::Map::Id>>&
    GetState() const {
        return state_on_map_;
    }

   private:
    std::unordered_map<model::Map::Id, State,
                       util::TaggedHasher<model::Map::Id>>
        state_on_map_;
    std::unordered_map<model::Map::Id, std::vector<uint16_t>,
                       util::TaggedHasher<model::Map::Id>>
        loot_value_;
    loot_gen::LootGenerator loot_generator_;
};
}  // namespace app
