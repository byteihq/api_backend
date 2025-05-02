#pragma once

#include <app/assets.h>
#include <app/geom.h>
#include <model/model.h>
#include <util/tagged.h>

#include <optional>
#include <vector>

#include "dog.h"

namespace app {
namespace entity {
class Player final {
   public:
    using Id = util::Tagged<uint64_t, Player>;

    explicit Player(const Dog& dog, const model::Map::Id& map_id, const Id& id,
                    const std::optional<Token>& token = std::nullopt);

    const Token& GetToken() const noexcept;
    const model::Map::Id& GetMapId() const noexcept;
    const Id& GetId() const noexcept;
    const std::string& GetName() const noexcept(noexcept(Dog{}.GetName()));
    const Dog& GetDog() const;
    Dog& GetDog();

   private:
    Dog dog_;
    model::Map::Id map_id_;
    Id id_;
    Token token_;
};
}  // namespace entity

class Players final {
   public:
    using PlayerRepr = std::shared_ptr<entity::Player>;
    using ConstPlayerRepr = std::shared_ptr<const entity::Player>;
    using ConstPlayerList = std::vector<ConstPlayerRepr>;
    using PlayerList = std::vector<PlayerRepr>;

    PlayerRepr AddPlayer(std::string_view name, const model::Map::Id& map_id,
                         const geom::Point2D& dog_position);
    void AddPlayer(const entity::Player& player);

    ConstPlayerList GetPlayerList(const model::Map::Id& map_id) const;
    ConstPlayerList GetPlayerList() const;

    PlayerList GetPlayerList(const model::Map::Id& map_id);
    PlayerList GetPlayerList();

    bool TokenExists(const Token& token) const;

    ConstPlayerRepr GetPlayer(const Token& token) const;
    PlayerRepr GetPlayer(const Token& token);

    size_t RemovePlayer(const Token& token);

   private:
    template <typename T, typename Pred>
    T GetPlayerList(Pred&& pred) const {
        T list;
        list.reserve(players_.size());

        std::ranges::for_each(players_, [&list, &pred](auto&& it) {
            if (pred(it.second)) list.push_back(it.second);
        });

        return list;
    }

    [[nodiscard]] PlayerRepr TryAddPlayer(const entity::Player& player);

   private:
    static constexpr uint8_t GenerateTokenTries = 10;
    using PlayersContainer =
        std::unordered_map<Token, PlayerRepr,
                           detail::TokenHasher<Token, TokenLen>>;

    PlayersContainer players_;
    typename entity::Player::Id::ValueType player_id_{0};
};
}  // namespace app
