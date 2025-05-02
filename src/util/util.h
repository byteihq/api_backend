#pragma once

#include <chrono>
#include <pqxx/pqxx>
#include <string>
#include <string_view>
#include <vector>

namespace util {
std::vector<std::string> Split(std::string_view s, char delim);

std::string DecodeURL(const std::string& url);

std::string ScientificToFixed(const std::string& input);

float SetPrecision(float input, uint8_t precision);

class DurationMeasure final {
   public:
    std::chrono::system_clock::duration GetTime() const {
        return std::chrono::system_clock::now() - begin_;
    }

   private:
    std::chrono::system_clock::time_point begin_{
        std::chrono::system_clock::now()};
};

namespace postgres {
pqxx::zview ToZview(std::string_view str);
}
}  // namespace util
