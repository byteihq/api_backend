#include "application.h"

#include <fmt/core.h>
#include <logger/logger.h>
#include <postgres/connection_pool.h>
#include <postgres/retired_player_impl.h>
#include <postgres/unit_of_work_impl.h>
#include <serialization/model_serialization.h>
#include <util/util.h>

#include <algorithm>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <fstream>
#include <stdexcept>
#include <string>

namespace app {
Application::Application(
    model::Game& game, app::ResourceHandler& resource_handler,
    extra::Data& extra_data, bool automatic_tick, bool randomize_spawn_position,
    const std::string& db_url,
    const std::optional<std::chrono::milliseconds>& save_period,
    const std::optional<std::filesystem::path>& save_file)
    : game_{game},
      move_handler_{game_.GetMaps()},
      resource_handler_{resource_handler},
      extra_data_{extra_data},
      automatic_tick_{automatic_tick},
      randomize_spawn_position_{randomize_spawn_position},
      auto_save_{[&save_period, &save_file]() {
          if (!save_period) return AutoSave{};
          if (!save_file)
              throw std::invalid_argument("save file path not specified");
          return AutoSave{true, *save_file, *save_period,
                          std::chrono::milliseconds::zero()};
      }()},
      connection_pool_{
          Application::max_db_connections, [&db_url]() {
              auto conn = std::make_unique<pqxx::connection>(db_url);
              postgres::UnitOfWorkImpl{*conn}.InitPlayerDB();
              conn->prepare(
                  util::postgres::ToZview(domain::request::add_new_player),
                  util::postgres::ToZview(postgres::request::add_new_player));
              conn->prepare(util::postgres::ToZview(
                                domain::request::increase_player_scores),
                            util::postgres::ToZview(
                                postgres::request::increase_player_scores));
              conn->prepare(util::postgres::ToZview(
                                domain::request::increase_player_play_time),
                            util::postgres::ToZview(
                                postgres::request::increase_player_play_time));
              return conn;
          }} {
    auto maps = game_.GetMaps();
    for (const auto& map : maps) {
        auto extra_map_data = extra_data_.GetMapExtraData(map.GetId());
        if (!extra_map_data)
            throw std::out_of_range(fmt::format(
                "extra data about {} not found but required", *map.GetId()));
        game_state_.SetLootTypes(map.GetId(), extra_map_data->loot_types);
    }
}

API::Result<API::map::Info> Application::GetMapUseCase(
    std::string_view map_id) const {
    return API::map::Get(game_, extra_data_, map_id);
}

API::Result<model::Game::Maps> Application::ListMapsUseCase() const {
    return API::map::List(game_);
}

API::Result<StaticResource> Application::GetStaticResourceUseCase(
    const std::filesystem::path& path) const {
    return API::resource::Get(resource_handler_, path);
}

API::Result<API::game::Authorization> Application::JoinGameUseCase(
    std::string_view request) {
    return API::game::player::Join(game_, players_, request,
                                   randomize_spawn_position_, move_handler_);
}

API::Result<app::Players::ConstPlayerList> Application::GetPlayerList(
    std::string_view token) const {
    return API::game::player::List(players_, token);
}

API::Result<API::game::State> Application::GetGameState(
    std::string_view token) const {
    return API::game::GetState(game_state_, players_, token);
}

API::Result<app::EmptyJson> Application::PlayerActionUseCase(
    std::string_view request, std::string_view token) {
    return API::game::player::Action(players_, request, token);
}

API::Result<app::EmptyJson> Application::GameTickUseCase(
    std::string_view request) {
    if (automatic_tick_) return {std::nullopt, API::Status::BAD_API_REQUEST};
    auto time_delta = API::game::util::TryExtractTimeDelta(request);
    if (!time_delta) return {std::nullopt, API::Status::INVALID_JSON};
    return GameTick(*time_delta);
}

API::Result<app::EmptyJson> Application::GameTick(
    std::chrono::milliseconds delta) {
    auto res = API::game::Tick(game_state_, move_handler_, players_,
                               connection_pool_, delta);

    if (auto_save_.enabled) {
        auto_save_.time_since_last_save += delta;
        if (auto_save_.time_since_last_save >= auto_save_.save_period) {
            SaveState(auto_save_.file);
            auto_save_.time_since_last_save = std::chrono::milliseconds::zero();
        }
    }
    return res;
}

API::Result<std::vector<API::game::PlayerRecord>>
Application::GetTopKPlayerRecords(const std::optional<size_t>& start,
                                  const std::optional<size_t>& max_items) {
    return API::game::GetRecords(connection_pool_, start, max_items);
}

void Application::RestoreState(const std::filesystem::path& path) {
    namespace fs = std::filesystem;
    using namespace boost::archive;

    if (!fs::exists(path)) return;

    if (!fs::is_regular_file(path))
        throw std::invalid_argument(
            fmt::format("file {} has invalid type", path.string()));

    std::ifstream in{path, std::ios::binary};
    if (!in)
        throw std::invalid_argument(
            fmt::format("failed to open file {}", path.string()));

    binary_iarchive ia{in};
    serialization::ApplicationRepr application_repr;
    ia >> application_repr;
    players_ = application_repr.RestorePlayers();
    application_repr.RestoreGameState(game_state_);
}

void Application::SaveState(const std::filesystem::path& path) const {
    namespace fs = std::filesystem;
    using namespace boost::archive;
    using namespace std::literals;

    auto tmp_file = path;
    tmp_file.replace_filename(tmp_file.filename().string() + ".tmp"s);
    std::ofstream out{tmp_file, std::ios::binary};
    if (!out)
        throw std::invalid_argument(
            fmt::format("failed to open file: {}", tmp_file.string()));

    binary_oarchive oa{out};
    serialization::ApplicationRepr application_repr{players_, game_state_};
    oa << application_repr;
    out.close();

    fs::rename(tmp_file, path);
}
}  // namespace app
