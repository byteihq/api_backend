#include "util.h"

#include <cmath>
#include <iomanip>
#include <regex>
#include <sstream>
#include <unordered_set>

namespace util {
std::vector<std::string> Split(std::string_view s, char delim) {
    std::vector<std::string> result;
    std::stringstream ss(std::string(s.data(), s.size()));
    std::string item;

    while (getline(ss, item, delim)) {
        if (item.empty()) continue;
        result.push_back(std::move(item));
    }

    return result;
}

std::string DecodeURL(const std::string& url) {
    std::string res;
    res.reserve(url.size());
    for (auto it = url.begin(); it != url.end();) {
        if (*it == '%') {
            if (it + 2 >= url.end()) {
                throw std::logic_error(
                    "DecodeURL: expected 2 symbols after '%'");
            }
            res.append(1, static_cast<char>(std::stoi(
                              std::string(it + 1, it + 3), nullptr, 16)));
            it += 3;
            continue;
        } else if (*it == '+')
            res.append(1, ' ');
        else
            res.append(1, *it);
        ++it;
    }

    return res;
}

std::string ScientificToFixed(const std::string& input) {
    const std::regex sci_regex(R"(([-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?))");
    std::string result;
    std::sregex_iterator iter(input.begin(), input.end(), sci_regex);
    std::sregex_iterator end;

    size_t last_pos = 0;
    for (; iter != end; ++iter) {
        result.append(input, last_pos, iter->position() - last_pos);
        std::string number = iter->str();

        if (number.find_first_of("eE") != std::string::npos) {
            double num = std::stod(number);
            std::ostringstream out;
            out << std::defaultfloat << num;
            auto tmp = out.str();
            if (tmp.find('.') == std::string::npos) tmp.append(".0");
            result.append(tmp);
        } else {
            result.append(number);
        }
        last_pos = iter->position() + iter->length();
    }
    result.append(input, last_pos, input.size() - last_pos);
    return result;
}

float SetPrecision(float input, uint8_t precision) {
    return std::round(input * std::pow(10, precision)) /
           std::pow(10, precision);
}

namespace postgres {
pqxx::zview ToZview(std::string_view str) { return {str.data(), str.size()}; }
}  // namespace postgres
}  // namespace util
