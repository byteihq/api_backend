#include "extra_data.h"

namespace app {
namespace extra {
void Data::AddMapLootType(const model::Map::Id &id,
                          const Map::LootType &loot_type) {
    map_extra_data_[id].loot_types.push_back(loot_type);
}

std::optional<Data::Map> Data::GetMapExtraData(
    const model::Map::Id &id) const noexcept {
    if (!map_extra_data_.contains(id)) return std::nullopt;
    return map_extra_data_.at(id);
}
}  // namespace extra
}  // namespace app
