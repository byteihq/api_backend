#include "move_handler.h"

#include <fmt/core.h>
#include <logger/logger.h>
#include <util/util.h>

#include <algorithm>
#include <limits>
#include <random>

#include "assets.h"

namespace model {
namespace move {
geom::Point2D MoveHandler::Border::GetRandomPosition() const {
    geom::Point2D p;

    std::random_device rd;
    std::mt19937 gen(rd());
    if constexpr (std::is_floating_point_v<app::Dimension>) {
        std::uniform_real_distribution<> x_distr(start.x, end.x);
        std::uniform_real_distribution<> y_distr(start.y, end.y);

        p.x = x_distr(gen);
        p.y = y_distr(gen);
    } else {
        std::uniform_int_distribution<> x_distr(start.x, end.x);
        std::uniform_int_distribution<> y_distr(start.y, end.y);

        p.x = x_distr(gen);
        p.y = y_distr(gen);
    }

    return p;
}

std::pair<MoveHandler::Borders, MoveHandler::Borders>
MoveHandler::RoadBorders::GetRoadsByPoint(const geom::Point2D &p) const {
    std::pair<MoveHandler::Borders, MoveHandler::Borders> res;
    for (const auto &border : horizontal_borders) {
        if (border.start.x > p.x) break;
        if (border.end.x >= p.x && border.start.y <= p.y && border.end.y >= p.y)
            res.first.push_back(border);
    }

    for (const auto &border : vertical_borders) {
        if (border.start.y > p.y) break;
        if (border.end.y >= p.y && border.start.x <= p.x && border.end.x >= p.x)
            res.second.push_back(border);
    }

    return res;
}

MoveHandler::Border MoveHandler::RoadBorders::GetRandomBorder() const {
    auto size = horizontal_borders.size() + vertical_borders.size();
    if (size == 0) throw std::logic_error("no borders found");

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> distr(0, size - 1);

    auto idx = distr(gen);
    if (idx < horizontal_borders.size()) return horizontal_borders[idx];
    return vertical_borders[idx - horizontal_borders.size()];
}

MoveHandler::MoveHandler(const model::Game::Maps &maps) {
    std::for_each(maps.begin(), maps.end(), [this](const model::Map &map) {
        road_borders_on_map_[map.GetId()] = GetBordersOnMap(map);
    });
}

MoveHandler::RoadBorders MoveHandler::GetBordersOnMap(const model::Map &map) {
    MoveHandler::Borders horizontal_borders;
    MoveHandler::Borders vertical_borders;

    for (const auto &road : map.GetRoads()) {
        auto start = road.GetStart();
        auto end = road.GetEnd();

        if (road.IsVertical()) {
            if (start.y > end.y) std::swap(start, end);
            vertical_borders.emplace_back(
                geom::Point2D{start.x - model::RoadWidth / 2,
                              start.y - model::RoadWidth / 2},
                geom::Point2D{end.x + model::RoadWidth / 2,
                              end.y + model::RoadWidth / 2});
        } else {
            if (start.x > end.x) std::swap(start, end);
            horizontal_borders.emplace_back(
                geom::Point2D{start.x - model::RoadWidth / 2,
                              start.y - model::RoadWidth / 2},
                geom::Point2D{end.x + model::RoadWidth / 2,
                              end.y + model::RoadWidth / 2});
        }
    }

    std::sort(
        horizontal_borders.begin(), horizontal_borders.end(),
        [](const MoveHandler::Border &lhs, const MoveHandler::Border &rhs) {
            return lhs.start.x < rhs.start.x;
        });
    std::sort(
        vertical_borders.begin(), vertical_borders.end(),
        [](const MoveHandler::Border &lhs, const MoveHandler::Border &rhs) {
            return lhs.start.y < rhs.start.y;
        });

    return MoveHandler::RoadBorders{horizontal_borders, vertical_borders};
}

void MoveHandler::Tick(app::Players::PlayerRepr player,
                       std::chrono::milliseconds delta) const {
    static constexpr uint16_t ms_to_s = 1000;

    if (!road_borders_on_map_.contains(player->GetMapId()))
        throw std::invalid_argument(
            fmt::format("map {} not found", *player->GetMapId()));

    app::entity::Dog &dog = player->GetDog();
    auto direction = dog.GetDirection();
    if (direction == app::entity::Dog::Movement::Direction::STOP) return;

    auto [horizontal_borders, vertical_borders] =
        road_borders_on_map_.at(player->GetMapId())
            .GetRoadsByPoint(dog.GetCurPosition());
    if (horizontal_borders.empty() && vertical_borders.empty())
        throw std::invalid_argument(fmt::format(
            "no roads found on map {} in position {}:{}", *player->GetMapId(),
            dog.GetCurPosition().x, dog.GetCurPosition().y));

    geom::Point2D real_position{};
    geom::Point2D border_position{std::numeric_limits<app::Dimension>::max(),
                                  std::numeric_limits<app::Dimension>::max()};
    geom::Point2D dog_position{
        dog.GetCurPosition().x + dog.GetSpeed().x * delta.count() / ms_to_s,
        dog.GetCurPosition().y + dog.GetSpeed().y * delta.count() / ms_to_s};

    if (direction == app::entity::Dog::Movement::Direction::L) {
        real_position.y = dog_position.y;
        if (horizontal_borders.empty())
            border_position.x = vertical_borders[0].start.x;
        else
            border_position.x = horizontal_borders[0].start.x;
        real_position.x = std::max(border_position.x, dog_position.x);
    } else if (direction == app::entity::Dog::Movement::Direction::R) {
        real_position.y = dog_position.y;
        if (horizontal_borders.empty())
            border_position.x = vertical_borders[0].end.x;
        else
            border_position.x =
                std::max_element(horizontal_borders.begin(),
                                 horizontal_borders.end(),
                                 [](const Border &lhs, const Border &rhs) {
                                     return lhs.end.x < rhs.end.x;
                                 })
                    ->end.x;
        real_position.x = std::min(border_position.x, dog_position.x);
    } else if (direction == app::entity::Dog::Movement::Direction::U) {
        real_position.x = dog_position.x;
        if (vertical_borders.empty())
            border_position.y = horizontal_borders[0].start.y;
        else
            border_position.y = vertical_borders[0].start.y;
        real_position.y = std::max(border_position.y, dog_position.y);
    } else {
        real_position.x = dog_position.x;
        if (vertical_borders.empty())
            border_position.y = horizontal_borders[0].end.y;
        else
            border_position.y =
                std::max_element(vertical_borders.begin(),
                                 vertical_borders.end(),
                                 [](const Border &lhs, const Border &rhs) {
                                     return lhs.end.y < rhs.end.y;
                                 })
                    ->end.y;
        real_position.y = std::min(border_position.y, dog_position.y);
    }

    if (real_position.x == border_position.x ||
        real_position.y == border_position.y)
        dog.Stop();
    dog.SetPosition(real_position);
}

geom::Point2D MoveHandler::GetRandomPos(const model::Map::Id &map_id) const {
    if (!road_borders_on_map_.contains(map_id))
        throw std::invalid_argument(fmt::format("map {} not found", *map_id));
    auto pos =
        road_borders_on_map_.at(map_id).GetRandomBorder().GetRandomPosition();
    if constexpr (std::is_floating_point_v<app::Dimension>) {
        pos.x = util::SetPrecision(pos.x, app::Precision);
        pos.y = util::SetPrecision(pos.y, app::Precision);
    }
    return pos;
}

geom::Point2D MoveHandler::GetStartPos(const model::Map::Id &map_id) const {
    if (!road_borders_on_map_.contains(map_id))
        throw std::invalid_argument(fmt::format("map {} not found", *map_id));
    auto borders = road_borders_on_map_.at(map_id);
    geom::Point2D pos{};
    if (!borders.horizontal_borders.empty()) {
        pos.x = borders.horizontal_borders[0].start.x;
        pos.y = borders.horizontal_borders[0].start.y;
    } else if (!borders.vertical_borders.empty()) {
        pos.x = borders.vertical_borders[0].start.x;
        pos.y = borders.vertical_borders[0].start.y;
    } else
        throw std::invalid_argument(
            fmt::format("no borders found on map {}", *map_id));

    if constexpr (std::is_floating_point_v<app::Dimension>) {
        pos.x = util::SetPrecision(pos.x, app::Precision);
        pos.y = util::SetPrecision(pos.y, app::Precision);
    }
    return pos;
}
}  // namespace move
}  // namespace model
