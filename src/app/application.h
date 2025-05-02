#pragma once

#include <entities/player.h>
#include <model/model.h>
#include <postgres/connection_pool.h>

#include <chrono>
#include <filesystem>
#include <optional>
#include <string_view>

#include "api.h"
#include "extra_data.h"
#include "game_state.h"
#include "move_handler.h"
#include "resource_handler.h"

namespace app {
class Application final {
   public:
    explicit Application(
        model::Game& game, ResourceHandler& resource_handler,
        extra::Data& extra_data, bool automatic_tick,
        bool randomize_spawn_position, const std::string& db_url,
        const std::optional<std::chrono::milliseconds>& save_period =
            std::nullopt,
        const std::optional<std::filesystem::path>& save_file = std::nullopt);

    API::Result<API::map::Info> GetMapUseCase(std::string_view map_id) const;
    API::Result<model::Game::Maps> ListMapsUseCase() const;
    API::Result<StaticResource> GetStaticResourceUseCase(
        const std::filesystem::path& path) const;
    API::Result<API::game::Authorization> JoinGameUseCase(
        std::string_view request);
    API::Result<app::Players::ConstPlayerList> GetPlayerList(
        std::string_view token) const;
    API::Result<API::game::State> GetGameState(std::string_view token) const;
    API::Result<app::EmptyJson> PlayerActionUseCase(std::string_view request,
                                                    std::string_view token);
    API::Result<app::EmptyJson> GameTickUseCase(std::string_view request);
    API::Result<app::EmptyJson> GameTick(std::chrono::milliseconds delta);

    API::Result<std::vector<API::game::PlayerRecord>> GetTopKPlayerRecords(
        const std::optional<size_t>& start = std::nullopt,
        const std::optional<size_t>& max_items = std::nullopt);

    void RestoreState(const std::filesystem::path& path);
    void SaveState(const std::filesystem::path& path) const;

   private:
    model::Game& game_;
    model::move::MoveHandler move_handler_;
    ResourceHandler& resource_handler_;
    extra::Data& extra_data_;
    Players players_;
    GameState game_state_;

    bool automatic_tick_;
    bool randomize_spawn_position_;

    struct AutoSave {
        bool enabled{false};
        std::filesystem::path file;
        std::chrono::milliseconds save_period;
        std::chrono::milliseconds time_since_last_save;
    };
    AutoSave auto_save_;

    static constexpr uint8_t max_db_connections = 8;
    postgres::ConnectionPool connection_pool_;
};
}  // namespace app
