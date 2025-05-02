#pragma once

#include <algorithm>
#include <bitset>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

namespace detail {
template <size_t Size>
struct TokenImpl {
    using byte = uint8_t;

    static TokenImpl Generate() {
        static_assert((Size >= sizeof(byte) * 8) &&
                      (Size % (sizeof(byte) * 8)) == 0);

        std::random_device random_dev{};
        std::mt19937_64 gen{random_dev()};
        std::uniform_int_distribution<std::mt19937_64::result_type> dist{
            0, std::numeric_limits<byte>::max()};

        std::bitset<Size> bits;
        constexpr size_t iter_count = Size / (sizeof(byte) * 8);
        for (size_t i = 0; i < iter_count; ++i) {
            bits <<= sizeof(byte) * 8;
            bits |= std::bitset<Size>{dist(gen)};
        }

        return TokenImpl{std::move(bits)};
    }

    static TokenImpl FromHexStr(const std::string& str) {
        if (str.size() != Size / 8 * 2)
            throw std::invalid_argument("invalid string size");
        if (!std::ranges::all_of(str, [](char c) {
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                       (c >= 'A' && c <= 'F');
            }))
            throw std::invalid_argument("invalid characters found");
        std::bitset<Size> bits;
        uint16_t b;
        for (size_t i = 0; i <= str.size() - 2; i += 2) {
            std::stringstream ss;
            ss << std::hex << str.substr(i, 2);
            ss >> b;
            bits <<= sizeof(byte) * 8;
            bits |= std::bitset<Size>{b};
        }

        return TokenImpl{std::move(bits)};
    }

    std::bitset<Size> bits_;
};

template <size_t Size>
class Token {
   public:
    constexpr Token() : token_{TokenImpl<Size>::Generate()} {}
    explicit constexpr Token(const std::string& str)
        : token_{TokenImpl<Size>::FromHexStr(str)} {}

    std::string Hex() const {
        const auto tmp{token_.bits_.to_string()};
        std::stringstream ss;
        constexpr size_t iter_count =
            Size / (sizeof(typename TokenImpl<Size>::byte) * 8);
        for (size_t i = 0; i < iter_count; ++i)
            ss << std::setw(Size / iter_count / 4) << std::setfill('0')
               << std::hex
               << std::bitset<Size / iter_count>{tmp.substr(
                                                     i * Size / iter_count,
                                                     Size / iter_count)}
                      .to_ulong();
        return ss.str();
    }

    const TokenImpl<Size>& GetRawToken() const { return token_; }

    constexpr bool operator==(const Token<Size>& other) const noexcept {
        return token_.bits_ == other.token_.bits_;
    };

   private:
    TokenImpl<Size> token_;
};

template <typename Token, size_t Size>
struct TokenHasher {
    size_t operator()(const Token& value) const {
        return std::hash<std::bitset<Size>>{}(value.GetRawToken().bits_);
    }
};
}  // namespace detail
