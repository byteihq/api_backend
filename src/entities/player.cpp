#include "player.h"

#include <config/config.h>
#include <fmt/core.h>

namespace app {
namespace entity {
Player::Player(const Dog& dog, const model::Map::Id& map_id, const Id& id,
               const std::optional<Token>& token)
    : dog_{dog}, map_id_{map_id}, id_{id}, token_{[&token]() {
          if (!token.has_value()) return Token{};
          return *token;
      }()} {}

const Token& Player::GetToken() const noexcept { return token_; }

const model::Map::Id& Player::GetMapId() const noexcept { return map_id_; }

const Player::Id& Player::GetId() const noexcept { return id_; }

const std::string& Player::GetName() const noexcept(noexcept(Dog{}.GetName())) {
    return dog_.GetName();
}

const Dog& Player::GetDog() const { return dog_; }

Dog& Player::GetDog() { return dog_; }
}  // namespace entity

Players::PlayerRepr Players::AddPlayer(std::string_view name,
                                       const model::Map::Id& map_id,
                                       const geom::Point2D& dog_position) {
    entity::Dog dog{std::string(name.begin(), name.end()),
                    Config::GetInstance().GetDogSpeed(map_id)};
    dog.SetPosition(dog_position);
    dog.SetBagCapacity(Config::GetInstance().GetBagCapacity(map_id));
    entity::Player::Id player_id{player_id_++};
    for (uint8_t i = 0; i < GenerateTokenTries; ++i) {
        entity::Player player{dog, map_id, player_id};
        if (auto player_repr = TryAddPlayer(player); player_repr)
            return player_repr;
    }

    return nullptr;
}

void Players::AddPlayer(const entity::Player& player) {
    if (!TryAddPlayer(player))
        throw std::invalid_argument(
            fmt::format("failed to add player: id: {}, name: {}",
                        *player.GetId(), player.GetName()));
    ++player_id_;
}

Players::PlayerRepr Players::TryAddPlayer(const entity::Player& player) {
    auto player_repr = std::make_shared<entity::Player>(player);

    if (players_.contains(player_repr->GetToken())) return nullptr;

    try {
        players_[player_repr->GetToken()] = player_repr;
    } catch (...) {
        return nullptr;
    }
    return player_repr;
}

Players::ConstPlayerList Players::GetPlayerList(
    const model::Map::Id& map_id) const {
    return GetPlayerList<ConstPlayerList>([&map_id](ConstPlayerRepr player) {
        return player->GetMapId() == map_id;
    });
}

Players::ConstPlayerList Players::GetPlayerList() const {
    return GetPlayerList<ConstPlayerList>(
        []([[maybe_unused]] ConstPlayerRepr player) constexpr { return true; });
}

Players::PlayerList Players::GetPlayerList(const model::Map::Id& map_id) {
    return GetPlayerList<PlayerList>(
        [&map_id](PlayerRepr player) { return player->GetMapId() == map_id; });
}

Players::PlayerList Players::GetPlayerList() {
    return GetPlayerList<PlayerList>(
        []([[maybe_unused]] PlayerRepr player) constexpr { return true; });
}

bool Players::TokenExists(const Token& token) const {
    return players_.contains(token) == 1;
}

Players::ConstPlayerRepr Players::GetPlayer(const Token& token) const {
    if (!players_.contains(token)) return nullptr;
    return ConstPlayerRepr{players_.at(token)};
}

Players::PlayerRepr Players::GetPlayer(const Token& token) {
    if (!players_.contains(token)) return nullptr;
    return players_.at(token);
}

size_t Players::RemovePlayer(const Token& token) {
    return players_.erase(token);
}
}  // namespace app
