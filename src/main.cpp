#include <app/application.h>
#include <app/extra_data.h>
#include <app/request_handler.h>
#include <config/config.h>
#include <domain/retired_player.h>
#include <entities/player.h>
#include <logger/logger.h>
#include <model/json_loader.h>
#include <util/ticker.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <thread>

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;

namespace {
// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

struct Args {
    std::optional<std::chrono::milliseconds> tick_period;
    std::string config_file;
    std::string www_root;
    std::string db_url;
    bool randomize_spawn_position{false};
    std::optional<std::filesystem::path> state_file;
    std::optional<std::chrono::milliseconds> save_state_period;
};

std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    Args args{};
    int64_t ticks{0};
    std::string state_file;
    int64_t save_state_period{0};

    po::options_description desc{"All options"};

    desc.add_options()("help, h", "Show help")(
        "tick-period, t",
        po::value<int64_t>(&ticks)->value_name("milliseconds"s),
        "set tick period")(
        "config-file, c",
        po::value<std::string>(&args.config_file)->value_name("file"s),
        "set config file path")(
        "www-root, w",
        po::value<std::string>(&args.www_root)->value_name("dir"s),
        "set static files root")("randomize-spawn-points",
                                 po::bool_switch()->default_value(false),
                                 "spawn dogs at random positions")(
        "state-file", po::value<std::string>(&state_file)->value_name("file"s),
        "set state file path")(
        "save-state-period",
        po::value<int64_t>(&save_state_period)->value_name("milliseconds"s),
        "set save state period");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help"s)) {
        std::cout << desc;
        return std::nullopt;
    }

    if (!vm.contains("config-file"s))
        throw std::invalid_argument("Config file has not been specified");

    if (!vm.contains("www-root"s))
        throw std::invalid_argument(
            "Static files root dir has not been specified");

    if (vm.contains("tick-period"s))
        args.tick_period = std::chrono::milliseconds{ticks};

    args.randomize_spawn_position = vm["randomize-spawn-points"s].as<bool>();

    if (vm.contains("state-file"s)) args.state_file = state_file;

    if (vm.contains("save-state-period"s))
        args.save_state_period = std::chrono::milliseconds{save_state_period};

    if (const auto* db_url = std::getenv("GAME_DB_URL"); db_url)
        args.db_url = db_url;
    else
        throw std::invalid_argument(
            "GAME_DB_URL environment variable not found");
    return args;
}
}  // namespace

int main(int argc, const char* argv[]) {
    try {
        auto args = ParseCommandLine(argc, argv);
        if (!args) {
            LOG(info) << logging::add_value(app::log::additional_data,
                                            json::value{{"code", 0}})
                      << "server exited";
            return 0;
        }

        app::log::InitBoostLogFilter();

        // 1. Загружаем карту из файла и построить модель игры
        model::Game game = json_loader::LoadGame(args->config_file);
        app::Config::GetInstance().LoadFromFile(args->config_file);

        // 2. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec,
                                  [[maybe_unused]] int signal_number) {
            if (!ec) {
                LOG(info) << logging::add_value(
                                 app::log::additional_data,
                                 json::value{{"code", signal_number}})
                          << "signal received";
                ioc.stop();
            }
        });

        // 4. Создаём обработчик HTTP-запросов и связываем его с моделью игры
        app::extra::Data extra_data;
        json_loader::LoadMapLootTypes(args->config_file, extra_data);
        app::ResourceHandler resources{args->www_root};
        app::Application app{game,
                             resources,
                             extra_data,
                             args->tick_period.has_value(),
                             args->randomize_spawn_position,
                             args->db_url,
                             args->save_state_period,
                             args->state_file};
        if (args->state_file) app.RestoreState(args->state_file.value());

        auto api_strand = net::make_strand(ioc);

        if (args->tick_period) {
            auto ticker = std::make_shared<app::Ticker>(
                api_strand, *args->tick_period,
                [&app](std::chrono::milliseconds delta) {
                    (void)app.GameTick(delta);
                });
            ticker->Start();
        }

        auto handler =
            std::make_shared<http_handler::RequestHandler>(app, api_strand);
        http_handler::LoggingRequestHandler<http_handler::RequestHandler>
            logging_handler{*handler};

        // 5. Запустить обработчик HTTP-запросов, делегируя их обработчику
        // запросов
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        http_server::ServeHttp(
            ioc, {address, port},
            [&logging_handler](auto&& req, auto&& endpoint, auto&& send) {
                logging_handler(std::forward<decltype(req)>(req),
                                std::forward<decltype(endpoint)>(endpoint),
                                std::forward<decltype(send)>(send));
            });

        // Эта надпись сообщает тестам о том, что сервер запущен и готов
        // обрабатывать запросы
        LOG(info) << logging::add_value(
                         app::log::additional_data,
                         json::value{{"address", address.to_string()},
                                     {"port", port}})
                  << "server started";

        // 6. Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, num_threads), [&ioc] { ioc.run(); });

        if (args->state_file) app.SaveState(args->state_file.value());
    } catch (const std::exception& ex) {
        LOG(info) << logging::add_value(app::log::additional_data,
                                        json::value{{"code", EXIT_FAILURE},
                                                    {"exception", ex.what()}})
                  << "server exited";
        return EXIT_FAILURE;
    }

    LOG(info) << logging::add_value(app::log::additional_data,
                                    json::value{{"code", 0}})
              << "server exited";
    return 0;
}
