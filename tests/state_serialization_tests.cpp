#include <app/geom.h>
#include <entities/dog.h>
#include <serialization/model_serialization.h>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sstream>

using namespace std::literals;
namespace {

using InputArchive = boost::archive::text_iarchive;
using OutputArchive = boost::archive::text_oarchive;

struct Fixture {
    std::stringstream strm;
    OutputArchive output_archive{strm};
};

}  // namespace

SCENARIO_METHOD(Fixture, "Point serialization") {
    GIVEN("A point") {
        const geom::Point2D p{10, 20};
        WHEN("point is serialized") {
            output_archive << p;

            THEN("it is equal to point after serialization") {
                InputArchive input_archive{strm};
                geom::Point2D restored_point;
                input_archive >> restored_point;
                CHECK(p == restored_point);
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "Dog Serialization") {
    GIVEN("a dog") {
        const auto dog = [] {
            app::entity::Dog dog{"Pluto"s, 3};
            app::entity::Dog::Bag::Item item{1, 2, 3};
            dog.SetPosition({42.2, 12.5});
            dog.MoveTo(app::entity::Dog::Movement::Direction::R);
            dog.AddItemToBag(item);
            return dog;
        }();

        WHEN("dog is serialized") {
            {
                serialization::DogRepr repr{dog};
                output_archive << repr;
            }

            THEN("it can be deserialized") {
                InputArchive input_archive{strm};
                serialization::DogRepr repr;
                input_archive >> repr;
                const auto restored = repr.Restore();

                CHECK(dog.GetName() == restored.GetName());
                CHECK(dog.GetCurPosition() == restored.GetCurPosition());
                CHECK(dog.GetSpeed() == restored.GetSpeed());

                const auto& dog_bag = dog.GetBag();
                const auto& restored_bag = restored.GetBag();
                CHECK(dog_bag.capacity == restored_bag.capacity);
                CHECK(dog_bag.score == restored_bag.score);
                CHECK(dog_bag.items.size() == restored_bag.items.size());
            }
        }
    }
}
