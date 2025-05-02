#pragma once

#include <model/model.h>
#include <util/tagged.h>

#include <boost/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace app {
namespace extra {
class Data final {
   public:
    struct Map {
        struct LootType {
            using ValueType = boost::json::object;
            ValueType obj;
        };

        using LootTypes = std::vector<LootType>;
        LootTypes loot_types;
    };

   public:
    Data() = default;

    void AddMapLootType(const model::Map::Id& id,
                        const Map::LootType& loot_type);
    std::optional<Map> GetMapExtraData(const model::Map::Id& id) const noexcept;

   private:
    std::unordered_map<model::Map::Id, Map, util::TaggedHasher<model::Map::Id>>
        map_extra_data_;
};
}  // namespace extra
}  // namespace app
