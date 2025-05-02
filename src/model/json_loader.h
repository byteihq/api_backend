#pragma once

#include <app/extra_data.h>
#include <model/model.h>

#include <filesystem>

namespace json_loader {

model::Game LoadGame(const std::filesystem::path& json_path);
void LoadMapLootTypes(const std::filesystem::path& json_path,
                      app::extra::Data& extra);

}  // namespace json_loader
