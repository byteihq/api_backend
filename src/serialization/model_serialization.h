#pragma once

#include <app/game_state.h>
#include <entities/dog.h>
#include <entities/player.h>

#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/vector.hpp>
#include <unordered_map>
#include <vector>

namespace geom {
template <typename Archive>
void serialize(Archive& ar, Point2D& point,
               [[maybe_unused]] const unsigned version) {
    ar & point.x;
    ar & point.y;
}

template <typename Archive>
void serialize(Archive& ar, Vec2D& vec,
               [[maybe_unused]] const unsigned version) {
    ar & vec.x;
    ar & vec.y;
}
}  // namespace geom

namespace serialization {
struct MovementRepr {
    app::entity::Dog::Movement movement;
};
struct ItemRepr {
    app::entity::Dog::Bag::Item item;
};
struct BagRepr {
    size_t capacity;
    std::vector<ItemRepr> items;
};

template <typename Archive>
void serialize(Archive& ar, ItemRepr& item_repr,
               [[maybe_unused]] const unsigned version) {
    ar & item_repr.item.id;
    ar & item_repr.item.type;
    ar & item_repr.item.value;
}

template <typename Archive>
void serialize(Archive& ar, BagRepr& bag_repr,
               [[maybe_unused]] const unsigned version) {
    ar & bag_repr.capacity;
    ar & bag_repr.items;
}

template <typename Archive>
void serialize(Archive& ar, MovementRepr& movement_repr,
               [[maybe_unused]] const unsigned version) {
    ar & movement_repr.movement.cur_position;
    ar & movement_repr.movement.prev_position;
    ar & movement_repr.movement.speed;
    ar & movement_repr.movement.direction;
    ar & movement_repr.movement.move_speed;
}

class DogRepr final {
   public:
    DogRepr() = default;
    explicit DogRepr(const app::entity::Dog& dog);

    [[nodiscard]] app::entity::Dog Restore() const;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & name_;
        ar & movement_repr_;
        ar & bag_repr_;
    }

   private:
    std::string name_;
    MovementRepr movement_repr_;
    BagRepr bag_repr_;
};

class PlayerRepr final {
   public:
    PlayerRepr() = default;
    explicit PlayerRepr(const app::entity::Player& player);

    [[nodiscard]] app::entity::Player Restore() const;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & dog_repr_;
        ar&(*map_id_);
        ar&(*player_id_);
        ar & hex_token_;
    }

   private:
    DogRepr dog_repr_;
    model::Map::Id map_id_{""};
    app::entity::Player::Id player_id_{0};
    std::string hex_token_;
};

struct LostObjectRepr {
    app::GameState::State::LostObject lost_object;
};

template <typename Archive>
void serialize(Archive& ar, LostObjectRepr& lost_object_repr,
               [[maybe_unused]] const unsigned version) {
    ar & lost_object_repr.lost_object.id;
    ar & lost_object_repr.lost_object.type;
    ar & lost_object_repr.lost_object.pos;
    ar & lost_object_repr.lost_object.value;
}

struct StateRepr {
    std::vector<LostObjectRepr> lost_objects_repr;
    size_t players_count{0};
    size_t id{0};
};

template <typename Archive>
void serialize(Archive& ar, StateRepr& state_repr,
               [[maybe_unused]] const unsigned version) {
    ar & state_repr.lost_objects_repr;
    ar & state_repr.players_count;
    ar & state_repr.id;
}

class ApplicationRepr final {
   public:
    ApplicationRepr() = default;
    explicit ApplicationRepr(const app::Players& players,
                             const app::GameState& game_state);

    [[nodiscard]] app::Players RestorePlayers() const;
    void RestoreGameState(app::GameState& game_state) const;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & players_repr_;
        ar & game_state_repr_;
    }

   private:
    std::vector<PlayerRepr> players_repr_;
    std::unordered_map<std::string, StateRepr> game_state_repr_;
};
};  // namespace serialization
