#pragma once

#include <fmt/core.h>
#include <logger/logger.h>
#include <model/model.h>
#include <net/http_server.h>
#include <serialization/json_serialization.h>
#include <util/util.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <regex>

#include "application.h"

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace net = boost::asio;
using namespace std::literals;

struct ContentType {
    ContentType() = delete;

    constexpr static std::string_view TEXT_HTML = "text/html"sv;
    constexpr static std::string_view TEXT_PLAIN = "text/plain"sv;
    constexpr static std::string_view TEXT_CSS = "text/css"sv;
    constexpr static std::string_view TEXT_JS = "text/javascript"sv;
    constexpr static std::string_view APPLICATION_JSON = "application/json"sv;
    constexpr static std::string_view APPLICATION_XML = "application/xml"sv;
    constexpr static std::string_view IMAGE_PNG = "image/png"sv;
    constexpr static std::string_view IMAGE_JPEG = "image/jpeg"sv;
    constexpr static std::string_view IMAGE_GIF = "image/gif"sv;
    constexpr static std::string_view IMAGE_BMP = "image/bmp"sv;
    constexpr static std::string_view IMAGE_ICO = "image/vnd.microsoft.icon"sv;
    constexpr static std::string_view IMAGE_TIFF = "image/tiff"sv;
    constexpr static std::string_view IMAGE_SVG = "image/svg+xml"sv;
    constexpr static std::string_view AUDIO_MPEG = "audio/mpeg"sv;
    constexpr static std::string_view APPLICATION_OCTET_STREAM =
        "application/octet-stream"sv;

    constexpr static std::string_view FromExtension(
        app::IResource::Extension extension) {
        switch (extension) {
            case app::IResource::Extension::HTM:
            case app::IResource::Extension::HTML:
                return ContentType::TEXT_HTML;
            case app::IResource::Extension::CSS:
                return ContentType::TEXT_CSS;
            case app::IResource::Extension::TXT:
                return ContentType::TEXT_PLAIN;
            case app::IResource::Extension::JS:
                return ContentType::TEXT_JS;
            case app::IResource::Extension::JSON:
                return ContentType::APPLICATION_JSON;
            case app::IResource::Extension::XML:
                return ContentType::APPLICATION_XML;
            case app::IResource::Extension::PNG:
                return ContentType::IMAGE_PNG;
            case app::IResource::Extension::JPG:
            case app::IResource::Extension::JPE:
            case app::IResource::Extension::JPEG:
                return ContentType::IMAGE_JPEG;
            case app::IResource::Extension::GIF:
                return ContentType::IMAGE_GIF;
            case app::IResource::Extension::BMP:
                return ContentType::IMAGE_BMP;
            case app::IResource::Extension::ICO:
                return ContentType::IMAGE_ICO;
            case app::IResource::Extension::TIFF:
            case app::IResource::Extension::TIF:
                return ContentType::IMAGE_TIFF;
            case app::IResource::Extension::SVG:
            case app::IResource::Extension::SVGZ:
                return ContentType::IMAGE_SVG;
            case app::IResource::Extension::MP3:
                return ContentType::AUDIO_MPEG;
            case app::IResource::Extension::UNKNOWN:
            default:
                return ContentType::APPLICATION_OCTET_STREAM;
        }
    }
};

struct ErrorCode {
    ErrorCode() = delete;

    constexpr static std::string_view BAD_REQUEST = "badRequest"sv;
    constexpr static std::string_view NOT_FOUND = "notFound"sv;
    constexpr static std::string_view MAP_NOT_FOUND = "mapNotFound"sv;
    constexpr static std::string_view INAVLID_METHOD = "invalidMethod"sv;
    constexpr static std::string_view INAVLID_ARG = "invalidArgument"sv;
    constexpr static std::string_view INVALID_TOKEN = "invalidToken"sv;
    constexpr static std::string_view UNKNOWN_TOKEN = "unknownToken"sv;
    constexpr static std::string_view INTERNAL_SERVER_ERROR =
        "internalServerError"sv;
};

struct Uri {
    static constexpr size_t depth = 2;
    static constexpr std::string_view maps = "maps"sv;
    static constexpr std::string_view game = "game"sv;

    struct Maps {
        struct List {
            static constexpr size_t len = 3;
        };
        struct Get {
            static constexpr size_t len = 4;
            static constexpr size_t depth = 3;
        };
    };

    struct Game {
        static constexpr size_t len = 4;
        static constexpr size_t depth = 3;
        static constexpr std::string_view join = "join"sv;
        static constexpr std::string_view players = "players"sv;
        static constexpr std::string_view player = "player"sv;
        static constexpr std::string_view state = "state"sv;
        static constexpr std::string_view tick = "tick"sv;
        static constexpr std::string_view records = "records"sv;

        struct Player {
            static constexpr size_t len = 5;
            static constexpr size_t depth = 4;
            static constexpr std::string_view action = "action"sv;
        };
    };
};

std::string MakeResponseBody(std::string_view ec, std::string_view msg);

class RequestHandler final
    : public std::enable_shared_from_this<RequestHandler> {
   public:
    using StringResponse = http::response<http::string_body>;
    using Strand = net::strand<net::io_context::executor_type>;

   public:
    explicit RequestHandler(app::Application& application, Strand strand)
        : application_{application}, api_strand_{strand} {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req,
                    Send&& send) {
        auto version = req.version();
        auto keep_alive = req.keep_alive();
        try {
            auto uri = util::Split(util::DecodeURL(req.target()), '/');

            if (uri.empty() || uri[0] != "api")
                return send(
                    HandleResourceRequest(std::move(req), std::move(uri)));

            auto handle = [self = shared_from_this(),
                           req = std::forward<decltype(req)>(req),
                           send = std::forward<decltype(send)>(send), uri,
                           version, keep_alive] {
                assert(self->api_strand_.running_in_this_thread());
                try {
                    if (uri.size() < 3 || !SupportedApiVersion(uri[1]))
                        return send(BadApiRequest(req));

                    return send(self->HandleApiRequest(req, uri));
                } catch (const std::exception& e) {
                    LOG(error) << logging::add_value(
                        app::log::additional_data,
                        json::value{{"exception", e.what()}});
                    return send(InternalServerError(version, keep_alive));
                } catch (...) {
                    return send(InternalServerError(version, keep_alive));
                }
            };
            return net::dispatch(api_strand_, handle);
        } catch (const std::exception& e) {
            LOG(error) << logging::add_value(
                app::log::additional_data,
                json::value{{"exception", e.what()}});
            return send(InternalServerError(version, keep_alive));
        } catch (...) {
            return send(InternalServerError(version, keep_alive));
        }
    }

   private:
    static bool SupportedApiVersion(std::string_view ver) {
        return ver == "v1"sv;
    }

    template <typename Body, typename Allocator>
    StringResponse HandleResourceRequest(
        const http::request<Body, http::basic_fields<Allocator>>& req,
        const std::vector<std::string>& params) {
        std::filesystem::path path;
        std::for_each(params.begin(), params.end(),
                      [&path](const std::string& param) { path /= param; });
        auto res = application_.GetStaticResourceUseCase(path);
        if (!res) {
            if (res.ec == API::Status::RESOURCE_NOT_FOUND) return NotFound(req);
            if (res.ec == API::Status::INVALID_RESOURCE_PATH)
                return BadRequest(req);
            throw std::logic_error(
                fmt::format("unexpected GetStaticResourceUseCase status: {}",
                            app::serialize(res.ec)));
        }
        auto resource_data = res().GetData();
        return MakeResponse(req, http::status::ok,
                            {resource_data.data(), resource_data.size()},
                            ContentType::FromExtension(res().GetExtension()));
    }

    template <typename Body, typename Allocator>
    StringResponse HandleApiRequest(
        const http::request<Body, http::basic_fields<Allocator>>& req,
        const std::vector<std::string>& params) {
        if (params[Uri::depth] == Uri::maps) {
            if (params.size() == Uri::Maps::List::len)
                return HandleListMapsRequest(req);
            if (params.size() == Uri::Maps::Get::len)
                return HandleGetMapRequest(req, params[Uri::Maps::Get::depth]);
            return BadApiRequest(req);
        }

        if (params[Uri::depth] == Uri::game) {
            if (params.size() >= Uri::Game::len &&
                params[Uri::Game::depth].size() >= Uri::Game::records.size() &&
                std::string_view(params[Uri::Game::depth])
                        .substr(0, Uri::Game::records.size()) ==
                    Uri::Game::records) {
                return HandleGameRecordsRequest(req, params[Uri::Game::depth]);
            }
            if (params.size() == Uri::Game::len) {
                if (params[Uri::Game::depth] == Uri::Game::join)
                    return HandleJoinGameRequest(req);
                if (params[Uri::Game::depth] == Uri::Game::players)
                    return HandlePlayerListRequest(req);
                if (params[Uri::Game::depth] == Uri::Game::state)
                    return HandleGameStateRequest(req);
                if (params[Uri::Game::depth] == Uri::Game::tick)
                    return HandleGameTickRequest(req);
            } else if (params.size() == Uri::Game::Player::len &&
                       params[Uri::Game::depth] == Uri::Game::player &&
                       params[Uri::Game::Player::depth] ==
                           Uri::Game::Player::action) {
                return HandlePlayerActionRequest(req);
            }

            return BadApiRequest(req);
        }

        return NotFound(req);
    }

    template <typename Body, typename Allocator>
    StringResponse HandleGameRecordsRequest(
        const http::request<Body, http::basic_fields<Allocator>>& req,
        const std::string& optional_params) {
        if (req.method() != http::verb::get && req.method() != http::verb::head)
            return MethodApiNotAllowed(req, "GET, HEAD"sv);

        std::optional<size_t> start{};
        std::optional<size_t> max_items{};
        const auto parse_optional_params = [&optional_params, &start,
                                            &max_items]() {
            std::vector<std::string> params;
            if (auto pos = optional_params.find('?'); pos == std::string::npos)
                return;
            else
                params = util::Split(
                    std::string_view(optional_params).substr(pos + 1), '&');
            static std::regex uri_regex{
                R"(^(start|maxItems)=([0-9]{1}|[1-9]{1}[0-9]*)$)"};
            std::smatch match;
            for (const auto& param : params) {
                if (std::regex_match(param, match, uri_regex)) {
                    if (match[1] == "start")
                        start = std::stoull(match[2]);
                    else
                        max_items = std::stoull(match[2]);
                }
            }
        };
        parse_optional_params();
        auto res = application_.GetTopKPlayerRecords(start, max_items);
        if (!res) {
            if (res.ec == API::Status::INVALID_QUERY_PARAM)
                return BadRequest(req);
            throw std::logic_error(
                fmt::format("unexpected HandleGameRecordsRequest status: {}",
                            app::serialize(res.ec)));
        }
        return MakeResponse(req, http::status::ok, app::serialize(res()));
    }

    template <typename Body, typename Allocator>
    StringResponse HandleGameTickRequest(
        const http::request<Body, http::basic_fields<Allocator>>& req) {
        if (req.method() != http::verb::post)
            return MethodApiNotAllowed(req, "POST"sv);
        if (auto it = req.find(http::field::content_type);
            it == req.end() || it->value() != ContentType::APPLICATION_JSON)
            return InvalidArgument(
                req, "Content-type field should be 'application/json'"sv);

        auto res = application_.GameTickUseCase(req.body());
        if (!res) {
            if (res.ec == API::Status::INVALID_JSON)
                return InvalidArgument(req, "Game tick request parse error"sv);
            if (res.ec == API::Status::BAD_API_REQUEST)
                return BadApiRequest(req);
            throw std::logic_error(
                fmt::format("unexpected HandleTickRequest status: {}",
                            app::serialize(res.ec)));
        }
        return MakeResponse(req, http::status::ok, app::serialize(res()));
    }

    template <typename Body, typename Allocator>
    StringResponse HandlePlayerActionRequest(
        const http::request<Body, http::basic_fields<Allocator>>& req) {
        if (req.method() != http::verb::post)
            return MethodApiNotAllowed(req, "POST"sv);
        if (auto it = req.find(http::field::content_type);
            it == req.end() || it->value() != ContentType::APPLICATION_JSON)
            return InvalidArgument(
                req, "Content-type field should be 'application/json'"sv);
        if (auto it = req.find(http::field::authorization); it == req.end())
            return InvalidToken(req);
        auto res = application_.PlayerActionUseCase(
            req.body(), req.find(http::field::authorization)->value());
        if (!res) {
            if (res.ec == API::Status::INVALID_TOKEN) return InvalidToken(req);
            if (res.ec == API::Status::TOKEN_NOT_FOUND)
                return UnknownToken(req);
            if (res.ec == API::Status::INVALID_JSON)
                return InvalidArgument(req,
                                       "Player action request parse error"sv);
            throw std::logic_error(
                fmt::format("unexpected JoinGameUseCase status: {}",
                            app::serialize(res.ec)));
        }
        return MakeResponse(req, http::status::ok, app::serialize(res()));
    }

    template <typename Body, typename Allocator>
    StringResponse HandleJoinGameRequest(
        const http::request<Body, http::basic_fields<Allocator>>& req) {
        if (req.method() != http::verb::post)
            return MethodApiNotAllowed(req, "POST"sv);
        if (auto it = req.find(http::field::content_type);
            it == req.end() || it->value() != ContentType::APPLICATION_JSON)
            return InvalidArgument(
                req, "Content-type field should be 'application/json'");
        auto res = application_.JoinGameUseCase(req.body());
        if (!res) {
            if (res.ec == API::Status::MAP_NOT_FOUND) return MapNotFound(req);
            if (res.ec == API::Status::INVALID_USER_NAME)
                return InvalidArgument(req, "Invalid name"sv);
            if (res.ec == API::Status::INVALID_JSON)
                return InvalidArgument(req, "Join game request parse error"sv);
            throw std::logic_error(
                fmt::format("unexpected JoinGameUseCase status: {}",
                            app::serialize(res.ec)));
        }
        return MakeResponse(req, http::status::ok, app::serialize(res()));
    }

    template <typename Body, typename Allocator>
    StringResponse HandlePlayerListRequest(
        const http::request<Body, http::basic_fields<Allocator>>& req) {
        if (req.method() != http::verb::get && req.method() != http::verb::head)
            return MethodApiNotAllowed(req, "GET, HEAD"sv);
        if (auto it = req.find(http::field::authorization); it == req.end())
            return InvalidToken(req);
        else {
            auto res = application_.GetPlayerList(it->value());
            if (!res) {
                if (res.ec == API::Status::INVALID_TOKEN)
                    return InvalidToken(req);
                if (res.ec == API::Status::TOKEN_NOT_FOUND)
                    return UnknownToken(req);
                throw std::logic_error(
                    fmt::format("unexpected GetPlayerList status: {}",
                                app::serialize(res.ec)));
            }
            return MakeResponse(
                req, http::status::ok,
                app::serialize(res(), app::detail::player_serializer::Minimal));
        }
    }

    template <typename Body, typename Allocator>
    StringResponse HandleGameStateRequest(
        const http::request<Body, http::basic_fields<Allocator>>& req) {
        if (req.method() != http::verb::get && req.method() != http::verb::head)
            return MethodApiNotAllowed(req, "GET, HEAD"sv);
        if (auto it = req.find(http::field::authorization); it == req.end())
            return InvalidToken(req);
        else {
            auto res = application_.GetGameState(it->value());
            if (!res) {
                if (res.ec == API::Status::INVALID_TOKEN)
                    return InvalidToken(req);
                if (res.ec == API::Status::TOKEN_NOT_FOUND)
                    return UnknownToken(req);
                throw std::logic_error(
                    fmt::format("unexpected GetGaveState status: {}",
                                app::serialize(res.ec)));
            }
            return MakeResponse(req, http::status::ok, app::serialize(res()));
        }
    }

    template <typename Body, typename Allocator>
    StringResponse HandleGetMapRequest(
        const http::request<Body, http::basic_fields<Allocator>>& req,
        std::string_view map_id) {
        if (req.method() != http::verb::get && req.method() != http::verb::head)
            return MethodApiNotAllowed(req, "GET, HEAD"sv);
        auto res = application_.GetMapUseCase(map_id);
        if (!res) {
            if (res.ec == API::Status::MAP_NOT_FOUND) return MapNotFound(req);
            throw std::logic_error("unexpected GetMap status: " +
                                   app::serialize(res.ec));
        }
        return MakeResponse(req, http::status::ok, app::serialize(res()));
    }

    template <typename Body, typename Allocator>
    StringResponse HandleListMapsRequest(
        const http::request<Body, http::basic_fields<Allocator>>& req) {
        auto res = application_.ListMapsUseCase();
        if (!res)
            throw std::logic_error("unexpected ListMap status: " +
                                   app::serialize(res.ec));
        return MakeResponse(req, http::status::ok, json::serialize(res()));
    }

    template <typename Body, typename Allocator>
    static StringResponse MakeResponse(
        const http::request<Body, http::basic_fields<Allocator>>& req,
        http::status status, std::string_view body,
        std::string_view content_type = ContentType::APPLICATION_JSON) {
        return MakeResponse(req.version(), req.keep_alive(), status, body,
                            content_type);
    }

    static StringResponse MakeResponse(
        unsigned version, bool keep_alive, http::status status,
        std::string_view body,
        std::string_view content_type = ContentType::APPLICATION_JSON) {
        StringResponse response(status, version);
        response.set(http::field::content_type, content_type);
        response.set(http::field::cache_control, "no-cache");
        response.body() = body;
        response.content_length(body.size());
        response.keep_alive(keep_alive);
        return response;
    }

    template <typename Body, typename Allocator>
    static StringResponse MethodNotAllowed(
        const http::request<Body, http::basic_fields<Allocator>>& req) {
        auto response =
            MakeResponse(req, http::status::method_not_allowed,
                         "Invalid method"sv, ContentType::TEXT_HTML);
        response.set(http::field::allow, "GET, HEAD, POST"sv);
        return response;
    }

    template <typename Body, typename Allocator>
    static StringResponse MethodApiNotAllowed(
        const http::request<Body, http::basic_fields<Allocator>>& req,
        std::string_view allowed_methods) {
        auto response =
            MakeResponse(req, http::status::method_not_allowed,
                         MakeResponseBody(ErrorCode::INAVLID_METHOD,
                                          "Only "s + allowed_methods.data() +
                                              " method(s) is expected"s));
        response.set(http::field::allow, allowed_methods);
        return response;
    }

    template <typename Body, typename Allocator>
    static StringResponse BadRequest(
        const http::request<Body, http::basic_fields<Allocator>>& req) {
        return MakeResponse(req, http::status::bad_request, "Bad request"sv,
                            ContentType::TEXT_PLAIN);
    }

    template <typename Body, typename Allocator>
    static StringResponse BadApiRequest(
        const http::request<Body, http::basic_fields<Allocator>>& req) {
        return MakeResponse(
            req, http::status::bad_request,
            MakeResponseBody(ErrorCode::BAD_REQUEST, "Bad request"sv));
    }

    template <typename Body, typename Allocator>
    static StringResponse InvalidArgument(
        const http::request<Body, http::basic_fields<Allocator>>& req,
        std::string_view message = "Bad request"sv) {
        return MakeResponse(req, http::status::bad_request,
                            MakeResponseBody(ErrorCode::INAVLID_ARG, message));
    }

    template <typename Body, typename Allocator>
    static StringResponse InvalidToken(
        const http::request<Body, http::basic_fields<Allocator>>& req) {
        return MakeResponse(
            req, http::status::unauthorized,
            MakeResponseBody(ErrorCode::INVALID_TOKEN,
                             "Authorization header is missing"sv));
    }

    template <typename Body, typename Allocator>
    static StringResponse UnknownToken(
        const http::request<Body, http::basic_fields<Allocator>>& req) {
        return MakeResponse(
            req, http::status::unauthorized,
            MakeResponseBody(ErrorCode::UNKNOWN_TOKEN,
                             "Player token has not been found"sv));
    }

    template <typename Body, typename Allocator>
    static StringResponse NotFound(
        const http::request<Body, http::basic_fields<Allocator>>& req) {
        return MakeResponse(req, http::status::not_found,
                            "The requested resource could not be found"sv,
                            ContentType::TEXT_PLAIN);
    }

    template <typename Body, typename Allocator>
    static StringResponse MapNotFound(
        const http::request<Body, http::basic_fields<Allocator>>& req) {
        return MakeResponse(
            req, http::status::not_found,
            MakeResponseBody(ErrorCode::MAP_NOT_FOUND, "Map not found"sv));
    }

    static StringResponse InternalServerError(unsigned version,
                                              bool keep_alive) {
        return MakeResponse(version, keep_alive,
                            http::status::internal_server_error,
                            "Something went wrong... Try again later."sv,
                            ContentType::TEXT_PLAIN);
    }

   private:
    app::Application& application_;
    Strand api_strand_;
};

template <typename RequestHandler>
class LoggingRequestHandler final {
   public:
    explicit LoggingRequestHandler(RequestHandler& handler)
        : handler_{handler} {}

    template <typename Request, typename Endpoint, typename Send>
    void operator()(Request&& req, Endpoint&& endpoint, Send&& send) {
        LogRequest(req, endpoint.address().to_string());
        auto dm = std::make_shared<util::DurationMeasure>();
        return handler_(
            std::move(req),
            [dm, send = std::forward<decltype(send)>(send)](auto&& response) {
                LogResponse(response, dm->GetTime());
                return send(std::forward<decltype(response)>(response));
            });
    }

   private:
    template <typename Request>
    static void LogRequest(const Request& request, std::string_view address) {
        LOG(info) << logging::add_value(
                         app::log::additional_data,
                         json::value{
                             {"ip", address},
                             {"URI", request.target()},
                             {"method", http::to_string(request.method())}})
                  << "request received";
    }

    template <typename Response, typename Duration = std::chrono::milliseconds>
    static void LogResponse(
        const Response& response,
        const std::chrono::system_clock::duration& response_time) {
        json::object obj;
        obj["code"] = response.result_int();
        if (auto it = response.find(http::field::content_type);
            it != response.end())
            obj["content_type"] = it->value();
        else
            obj["content_type"] = nullptr;
        obj["response_time"] =
            std::chrono::duration_cast<Duration>(response_time).count();
        LOG(info) << logging::add_value(app::log::additional_data, obj)
                  << "response sent";
    }

   private:
    RequestHandler& handler_;
};

}  // namespace http_handler
