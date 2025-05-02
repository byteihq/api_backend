#pragma once

#include <util/token_generator.h>

#include <cstdint>
#include <string>

namespace app {
static constexpr uint8_t TokenLen = 128;  // 128 bit
using Token = detail::Token<app::TokenLen>;

using Dimension = double;
static constexpr uint8_t Precision = 2;

struct EmptyJson {};
}  // namespace app

namespace model {
static constexpr double RoadWidth = 0.8f;
static constexpr double PlayerWidth = 0.6f;
static constexpr double ItemWidth = 0.f;
static constexpr double BaseWidth = 0.5f;
}  // namespace model
