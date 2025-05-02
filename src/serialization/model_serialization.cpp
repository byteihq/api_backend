#include "model_serialization.h"

#include <fmt/core.h>

namespace serialization {
DogRepr::DogRepr(const app::entity::Dog& dog)
    : name_{dog.GetName()},
      movement_repr_{dog.GetMovement()},
      bag_repr_{[&dog]() {
          auto bag = dog.GetBag();
          BagRepr bag_repr;
          bag_repr.capacity = bag.capacity;
          std::ranges::transform(bag.items, std::back_inserter(bag_repr.items),
                                 [](const app::entity::Dog::Bag::Item& item) {
                                     return ItemRepr{item};
                                 });
          return bag_repr;
      }()} {}

app::entity::Dog DogRepr::Restore() const {
    app::entity::Dog dog{name_, movement_repr_.movement.move_speed};
    dog.SetMovement(movement_repr_.movement);
    dog.SetBagCapacity(bag_repr_.capacity);
    std::ranges::for_each(bag_repr_.items, [&dog](const ItemRepr& item_repr) {
        if (!dog.AddItemToBag(item_repr.item))
            throw std::invalid_argument(fmt::format(
                "failed to add item for dog {} while restoring state",
                dog.GetName()));
    });
    return dog;
}

PlayerRepr::PlayerRepr(const app::entity::Player& player)
    : dog_repr_{player.GetDog()},
      map_id_{player.GetMapId()},
      player_id_{player.GetId()},
      hex_token_{player.GetToken().Hex()} {}

app::entity::Player PlayerRepr::Restore() const {
    return app::entity::Player{dog_repr_.Restore(), map_id_, player_id_,
                               app::Token{hex_token_}};
}

ApplicationRepr::ApplicationRepr(const app::Players& players,
                                 const app::GameState& game_state)
    : players_repr_{[&players]() {
          std::vector<PlayerRepr> players_repr;

          auto players_list = players.GetPlayerList();
          std::ranges::transform(
              players_list, std::back_inserter(players_repr),
              [](const app::Players::ConstPlayerRepr& player) {
                  return PlayerRepr{*player};
              });

          return players_repr;
      }()},
      game_state_repr_{[&game_state]() {
          std::unordered_map<std::string, StateRepr> game_state_repr;
          for (const auto& state_on_all_maps = game_state.GetState();
               const auto& [map, state] : state_on_all_maps) {
              game_state_repr[*map].id = state.id;
              game_state_repr[*map].players_count = state.players_count;
              std::ranges::transform(
                  state.lost_objects,
                  std::back_inserter(game_state_repr[*map].lost_objects_repr),
                  [](const app::GameState::State::LostObject& lost_object) {
                      return LostObjectRepr{lost_object};
                  });
          }

          return game_state_repr;
      }()} {}
app::Players ApplicationRepr::RestorePlayers() const {
    app::Players players;
    std::ranges::for_each(players_repr_,
                          [&players](const PlayerRepr& player_repr) {
                              players.AddPlayer(player_repr.Restore());
                          });
    return players;
}
void ApplicationRepr::RestoreGameState(app::GameState& game_state) const {
    for (const auto& [map, state] : game_state_repr_) {
        game_state.SetPlayersCountOnMap(model::Map::Id{map},
                                        state.players_count);
        std::ranges::for_each(
            state.lost_objects_repr,
            [&map, &game_state](const LostObjectRepr& lost_object_repr) {
                game_state.AddLostObjectOnMap(model::Map::Id{map},
                                              lost_object_repr.lost_object);
            });
    }
}
}  // namespace serialization
