#include "game_state.h"

#include <config/config.h>
#include <fmt/core.h>

#include <boost/json.hpp>
#include <random>
#include <stdexcept>
#include <string_view>

#include "collision_detector.h"

namespace app {
struct Fields {
    struct LostObject {
        static constexpr std::string_view value = "value"sv;
    };
};

class DogGathererProvider : public collision_detector::ItemGathererProvider {
   public:
    inline size_t ItemsCount() const override { return items_.size(); }
    inline collision_detector::Item GetItem(size_t idx) const override {
        auto lost_obj = items_.at(idx);
        return collision_detector::Item{lost_obj.pos, model::ItemWidth};
    }
    inline size_t GatherersCount() const override { return gatherers_.size(); }
    inline collision_detector::Gatherer GetGatherer(size_t idx) const override {
        auto& dog = gatherers_.at(idx);
        return collision_detector::Gatherer{
            dog->GetPrevPosition(), dog->GetCurPosition(), model::PlayerWidth};
    }

    app::entity::Dog& GetGathererAsDog(size_t idx) {
        return *gatherers_.at(idx);
    }

    void AddDogAsGatherer(app::entity::Dog& dog) { gatherers_.push_back(&dog); }

    GameState::State::LostObject GetLostObject(size_t idx) {
        return items_.at(idx);
    }

    void AddItem(const GameState::State::LostObject& lost_obj) {
        items_.push_back(lost_obj);
    }

   private:
    std::vector<GameState::State::LostObject> items_;
    std::vector<app::entity::Dog*> gatherers_;
};

GameState::GameState()
    : loot_generator_{
          loot_gen::LootGenerator::TimeInterval(static_cast<int64_t>(
              Config::GetInstance().GetLootGeneratorPeriod())),
          Config::GetInstance().GetLootGeneratorProbability()} {}

void GameState::Tick(std::chrono::milliseconds time_delta,
                     const Players::PlayerList& players,
                     const model::move::MoveHandler& move_handler) {
    // Собираем информацию о предметах и игроках по всем картам
    std::unordered_map<model::Map::Id, DogGathererProvider,
                       util::TaggedHasher<model::Map::Id>>
        gatherers_on_map;
    for (const auto& player : players) {
        auto& dog = player->GetDog();
        const auto& map_id = player->GetMapId();

        if (!gatherers_on_map.contains(map_id)) {
            if (!state_on_map_.contains(map_id))
                throw std::out_of_range(
                    fmt::format("unknown state on {}", *map_id));
            for (const auto& lost_obj : state_on_map_[map_id].lost_objects)
                gatherers_on_map[map_id].AddItem(lost_obj);
        }
        gatherers_on_map[map_id].AddDogAsGatherer(dog);
    }

    // Обрабатываем подбор предмета игроком на карте
    for (auto& [map, gatherers] : gatherers_on_map) {
        for (auto events = collision_detector::FindGatherEvents(gatherers);
             const auto& event : events) {
            auto& dog = gatherers.GetGathererAsDog(event.gatherer_id);
            auto lost_object = gatherers.GetLostObject(event.item_id);

            // Если предмета с таким id нет на карте, то его уже зарбрал другой
            // игрок
            if (!state_on_map_.at(map).lost_objects.contains(lost_object))
                continue;
            // Если игрок прошел рядом с предметом, но его рюкзак полон, то
            // предмет остается на карте
            if (dog.AddItemToBag(app::entity::Dog::Bag::Item{
                    lost_object.id, lost_object.type, lost_object.value}) == 0)
                continue;

            state_on_map_.at(map).lost_objects.erase(lost_object);
        }
        state_on_map_.at(map).players_count = gatherers.GatherersCount();
    }

    // Генерируем новые предметы
    std::random_device rd;
    std::mt19937 gen(rd());
    for (auto& [map_id, state] : state_on_map_) {
        auto loot_count = loot_generator_.Generate(
            time_delta, state.lost_objects.size(), state.players_count);
        std::uniform_int_distribution<> distrib(0, state.loot_types_count - 1);
        for (size_t i = 0; i < loot_count; ++i) {
            const auto type = distrib(gen);
            state.lost_objects.emplace(state.id++, type,
                                       loot_value_.at(map_id).at(type),
                                       move_handler.GetRandomPos(map_id));
        }
    }
}

void GameState::SetLootTypes(
    const model::Map::Id& map_id,
    const app::extra::Data::Map::LootTypes& loot_types) {
    state_on_map_[map_id].loot_types_count = loot_types.size();
    for (const auto& loot_type : loot_types) {
        if (auto value = loot_type.obj.if_contains(Fields::LostObject::value);
            !value)
            throw std::invalid_argument(fmt::format(
                "field {} not found in lost object json but required",
                Fields::LostObject::value));
        else
            loot_value_[map_id].push_back(
                static_cast<uint16_t>(value->as_int64()));
    }
}

size_t GameState::AddLostObjectOnMap(const model::Map::Id& map_id,
                                     const State::LostObject& lost_object) {
    if (!state_on_map_.contains(map_id))
        throw std::out_of_range(
            fmt::format("map {} has unknown state", *map_id));
    auto state = state_on_map_.at(map_id);
    if (state.lost_objects.size() == state.players_count)
        throw std::out_of_range(fmt::format(
            "maximum number of lost objects reached on map {}", *map_id));
    if (state.lost_objects.contains(lost_object))
        throw std::invalid_argument(fmt::format(
            "lost object with id {} already exists", lost_object.id));

    auto [_, res] = state_on_map_.at(map_id).lost_objects.insert(lost_object);
    if (res) ++state_on_map_.at(map_id).id;
    return res;
}

void GameState::SetPlayersCountOnMap(const model::Map::Id& map_id,
                                     size_t count) {
    if (!state_on_map_.contains(map_id))
        throw std::out_of_range(
            fmt::format("map {} has unknown state", *map_id));
    state_on_map_.at(map_id).players_count = count;
}

std::optional<GameState::State> GameState::GetState(
    const model::Map::Id& map_id) const {
    if (!state_on_map_.contains(map_id)) return std::nullopt;
    return state_on_map_.at(map_id);
}
}  // namespace app
