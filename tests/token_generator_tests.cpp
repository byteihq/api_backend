#include <app/assets.h>
#include <fmt/core.h>

#include <catch2/catch_test_macros.hpp>
#include <string_view>
#include <unordered_set>

SCENARIO("Token generation") {
    GIVEN(fmt::format("a {} bit token", app::TokenLen)) {
        app::Token token;

        GIVEN("its hex representation") {
            auto hex_token = token.Hex();
            THEN(fmt::format("hex representation len = token len / 8 * 2 = {}",
                             app::TokenLen / 4)) {
                CHECK(hex_token.size() == app::TokenLen / 4);
            }
            THEN("hex representation contains only valid hex characters") {
                constexpr std::string_view hex_symbols =
                    "0123456789abcedfABCDEF";
                for (char c : hex_token)
                    CHECK(hex_symbols.find(c) != std::string_view::npos);
            }
        }
    }
    GIVEN("a token generator") {
        std::unordered_set<app::Token,
                           detail::TokenHasher<app::Token, app::TokenLen>>
            tokens;
        constexpr size_t token_count{10'000};
        AND_WHEN(
            fmt::format("it generates {} tokens, all of them will be unique",
                        token_count)) {
            for (size_t i = 0; i < token_count; ++i) {
                app::Token token;
                REQUIRE(!tokens.contains(token));
                tokens.insert(std::move(token));
            }
        }
    }
}
