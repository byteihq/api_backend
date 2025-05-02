#pragma once

#include <entities/player.h>
#include <model/model.h>

#include <chrono>
#include <unordered_map>
#include <vector>

#include "assets.h"
#include "geom.h"

namespace model {
namespace move {
class MoveHandler {
   public:
    explicit MoveHandler(const model::Game::Maps& maps);

    void Tick(app::Players::PlayerRepr player,
              std::chrono::milliseconds delta) const;

    geom::Point2D GetRandomPos(const model::Map::Id& map_id) const;
    geom::Point2D GetStartPos(const model::Map::Id& map_id) const;

   public:
    struct Border {
        geom::Point2D start;
        geom::Point2D end;

        geom::Point2D GetRandomPosition() const;
    };

    using Borders = std::vector<Border>;

    struct RoadBorders {
        Borders horizontal_borders;
        Borders vertical_borders;

        std::pair<Borders, Borders> GetRoadsByPoint(
            const geom::Point2D& p) const;
        Border GetRandomBorder() const;
    };

   private:
    RoadBorders GetBordersOnMap(const model::Map& map);

   private:
    std::unordered_map<model::Map::Id, RoadBorders,
                       util::TaggedHasher<model::Map::Id>>
        road_borders_on_map_;
};
}  // namespace move
}  // namespace model
