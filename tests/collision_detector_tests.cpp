#define _USE_MATH_DEFINES

#include <app/collision_detector.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_templated.hpp>
#include <cmath>
#include <functional>
#include <sstream>

namespace Catch {
template <>
struct StringMaker<collision_detector::GatheringEvent> {
    static std::string convert(
        collision_detector::GatheringEvent const& value) {
        std::ostringstream tmp;
        tmp << "(" << value.gatherer_id << value.item_id << value.sq_distance
            << value.time << ")";

        return tmp.str();
    }
};
}  // namespace Catch

template <typename Range, typename Predicate>
struct RangeMatcher : public Catch::Matchers::MatcherGenericBase {
    RangeMatcher(const Range& range, Predicate predicate)
        : range_{range}, predicate_{predicate} {}

    inline bool match(const Range& other) const {
        return std::equal(std::begin(range_), std::end(range_),
                          std::begin(other), std::end(other), predicate_);
    }

    std::string describe() const override {
        return "Equals: " + Catch::rangeToString(range_);
    }

   private:
    const Range& range_;
    Predicate predicate_;
};

template <typename Range, typename Predicate>
auto MatchRanges(const Range& range, Predicate prediate) {
    return RangeMatcher{range, prediate};
}

class TestItemGathererProvider
    : public collision_detector::ItemGathererProvider {
   public:
    TestItemGathererProvider(
        const std::vector<collision_detector::Item>& items,
        const std::vector<collision_detector::Gatherer>& gatherers)
        : items_(items), gatherers_(gatherers) {}

    inline size_t ItemsCount() const override { return items_.size(); }
    inline collision_detector::Item GetItem(size_t idx) const override {
        return items_.at(idx);
    }
    inline size_t GatherersCount() const override { return gatherers_.size(); }
    inline collision_detector::Gatherer GetGatherer(size_t idx) const override {
        return gatherers_.at(idx);
    }

   private:
    std::vector<collision_detector::Item> items_;
    std::vector<collision_detector::Gatherer> gatherers_;
};

SCENARIO("Collision detection") {
    GIVEN("events comprartor") {
        const auto events_cmp =
            [](const collision_detector::GatheringEvent& lhs,
               const collision_detector::GatheringEvent& rhs) constexpr
            -> bool {
            constexpr auto eps = 1e-10;

            return !((lhs.gatherer_id != rhs.gatherer_id ||
                      lhs.item_id != rhs.item_id) ||
                     (std::abs(lhs.sq_distance - rhs.sq_distance) > eps) ||
                     (std::abs(lhs.time - rhs.time) > eps));
        };

        WHEN("no items") {
            TestItemGathererProvider provider{{}, {{{-3, 2}, {5, 7}, 10.}}};
            THEN("no events") {
                CHECK(collision_detector::FindGatherEvents(provider).empty());
            }
        }
        WHEN("no gatherers") {
            TestItemGathererProvider provider{{{{0, 0}, 3.}}, {}};
            THEN("no events") {
                CHECK(collision_detector::FindGatherEvents(provider).empty());
            }
        }
        WHEN("no items and no gatherers") {
            TestItemGathererProvider provider{{}, {}};
            THEN("no events") {
                CHECK(collision_detector::FindGatherEvents(provider).empty());
            }
        }

        WHEN("multiple items on a way of gatherer") {
            TestItemGathererProvider provider{{
                                                  {{9, 0.27}, .1},
                                                  {{8, 0.24}, .1},
                                                  {{7, 0.21}, .1},
                                                  {{6, 0.18}, .1},
                                                  {{5, 0.15}, .1},
                                                  {{4, 0.12}, .1},
                                                  {{3, 0.09}, .1},
                                                  {{2, 0.06}, .1},
                                                  {{1, 0.03}, .1},
                                                  {{0, 0.0}, .1},
                                                  {{-1, 0}, .1},
                                              },
                                              {
                                                  {{0, 0}, {10, 0}, 0.1},
                                              }};
            THEN("gathered items in right order") {
                CHECK_THAT(collision_detector::FindGatherEvents(provider),
                           MatchRanges(
                               std::vector{
                                   collision_detector::GatheringEvent{
                                       9, 0, 0. * 0., 0.0},
                                   collision_detector::GatheringEvent{
                                       8, 0, 0.03 * 0.03, 0.1},
                                   collision_detector::GatheringEvent{
                                       7, 0, 0.06 * 0.06, 0.2},
                                   collision_detector::GatheringEvent{
                                       6, 0, 0.09 * 0.09, 0.3},
                                   collision_detector::GatheringEvent{
                                       5, 0, 0.12 * 0.12, 0.4},
                                   collision_detector::GatheringEvent{
                                       4, 0, 0.15 * 0.15, 0.5},
                                   collision_detector::GatheringEvent{
                                       3, 0, 0.18 * 0.18, 0.6},
                               },
                               events_cmp));
            }
        }
        WHEN("multiple gatherers and one item") {
            TestItemGathererProvider provider{
                {
                    {{0, 0}, 0.},
                },
                {
                    {{-5, 0}, {5, 0}, 1.},
                    {{0, 1}, {0, -1}, 1.},
                    {{-10, 10}, {101, -100}, 0.5},  // <-- that one
                    {{-100, 100}, {10, -10}, 0.5},
                }};
            THEN("item gathered by faster gatherer") {
                CHECK(collision_detector::FindGatherEvents(provider)
                          .front()
                          .gatherer_id == 2);
            }
        }
        WHEN("gatherers stay put") {
            TestItemGathererProvider provider{{
                                                  {{0, 0}, 10.},
                                              },
                                              {{{-5, 0}, {-5, 0}, 1.},
                                               {{0, 0}, {0, 0}, 1.},
                                               {{-10, 10}, {-10, 10}, 100}}};
            THEN("no events detected") {
                CHECK(collision_detector::FindGatherEvents(provider).empty());
            }
        }
    }
}
