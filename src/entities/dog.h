#pragma once

#include <app/assets.h>
#include <app/geom.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace app {
namespace entity {
namespace detail {
template <typename Dimension>
class DogImpl final {
   public:
    struct Bag {
        struct Item {
            size_t id{0};
            uint16_t type{0};
            uint16_t value{0};
        };

        std::vector<Item> items;
        size_t capacity{0};
        size_t score{0};
    };

    struct Movement {
        geom::Point2D cur_position;
        geom::Point2D prev_position;
        geom::Vec2D speed;
        enum class Direction : uint8_t {
            L,  // Left
            R,  // Right
            U,  // Up
            D,  // Down
            STOP
        } direction{Direction::U};
        Dimension move_speed{0};
    };

   public:
    DogImpl() = default;
    DogImpl(std::string name, Dimension move_speed) noexcept
        : name_{std::move(name)}, time_stats_{[]() {
              return TimeStats{std::chrono::system_clock::now(),
                               std::chrono::system_clock::now(),
                               std::chrono::milliseconds::zero()};
          }()} {
        movement_.move_speed = move_speed;
    }

    const std::string& GetName() const { return name_; }

    constexpr void SetPosition(const geom::Point2D& new_position) noexcept {
        if (movement_.cur_position == new_position) return;
        movement_.prev_position =
            std::exchange(movement_.cur_position, new_position);
    }

    constexpr geom::Point2D GetCurPosition() const noexcept {
        return movement_.cur_position;
    }
    constexpr geom::Point2D GetPrevPosition() const noexcept {
        return movement_.prev_position;
    }
    constexpr geom::Vec2D GetSpeed() const noexcept { return movement_.speed; }
    constexpr Movement::Direction GetDirection() const noexcept {
        return movement_.direction;
    }

    constexpr const Bag& GetBag() const noexcept { return bag_; }

    constexpr void IncreaseInactiveTime(
        std::chrono::milliseconds delta) noexcept {
        time_stats_.inactive_ += delta;
    }
    constexpr std::chrono::milliseconds GetInactiveTime() const noexcept {
        return time_stats_.inactive_;
    }
    constexpr std::chrono::system_clock::time_point GetLastActiveTimePoint()
        const noexcept {
        return time_stats_.last_active_time_point_;
    }
    constexpr std::chrono::system_clock::time_point GetJoinGameTimePoint()
        const noexcept {
        return time_stats_.join_game_time_point_;
    }

    // Добавление нового предмета в рюкзак, если достаточно места. Возвращает
    // количество добавленных предметов
    size_t AddItemToBag(const Bag::Item& item) {
        if (bag_.items.size() >= bag_.capacity) return 0;
        bag_.items.push_back(item);
        bag_.score += item.value;
        return 1;
    }

    inline void SetBagCapacity(size_t capacity) noexcept {
        bag_.capacity = capacity;
    }

    void MoveTo(Movement::Direction dir) noexcept {
        Dimension speed_x{0};
        Dimension speed_y{0};
        switch (dir) {
            case Movement::Direction::L:
                speed_x = -movement_.move_speed;
                movement_.direction = Movement::Direction::L;
                break;
            case Movement::Direction::R:
                speed_x = movement_.move_speed;
                movement_.direction = Movement::Direction::R;
                break;
            case Movement::Direction::U:
                speed_y = -movement_.move_speed;
                movement_.direction = Movement::Direction::U;
                break;
            case Movement::Direction::D:
                speed_y = movement_.move_speed;
                movement_.direction = Movement::Direction::D;
                break;
            default:
                movement_.direction = Movement::Direction::STOP;
                break;
        }

        movement_.speed.x = speed_x;
        movement_.speed.y = speed_y;
        if (speed_x != 0 || speed_y != 0) {
            time_stats_.inactive_ = std::chrono::milliseconds::zero();
            time_stats_.last_active_time_point_ =
                std::chrono::system_clock::now();
        }
    }

    constexpr void Stop() noexcept {
        movement_.speed.x = 0;
        movement_.speed.y = 0;
    }

    constexpr const Movement& GetMovement() const { return movement_; }
    constexpr void SetMovement(const Movement& movement) {
        movement_ = movement;
    }

   private:
    std::string name_;
    Movement movement_;
    Bag bag_;
    struct TimeStats {
        const std::chrono::system_clock::time_point join_game_time_point_;
        std::chrono::system_clock::time_point last_active_time_point_;
        std::chrono::milliseconds inactive_;
    } time_stats_;
};
}  // namespace detail

using Dog = detail::DogImpl<app::Dimension>;

constexpr std::optional<Dog::Movement::Direction> StrToDogDirection(
    std::string_view dir) {
    using enum Dog::Movement::Direction;
    if (dir.empty()) return STOP;
    if (dir == "L") return L;
    if (dir == "R") return R;
    if (dir == "U") return U;
    if (dir == "D") return D;

    return std::nullopt;
}
}  // namespace entity
}  // namespace app
