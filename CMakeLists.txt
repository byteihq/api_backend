cmake_minimum_required(VERSION 3.11)

cmake_policy(SET CMP0091 NEW)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTS "Build game server unit tests" ON)

find_package(Boost 1.83.0 COMPONENTS log program_options REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()
find_package(Catch2 3.6.0 REQUIRED)
find_package(fmt 11.0.1 REQUIRED)
find_package(libpqxx 7.9.2 REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(${PROJECT_NAME}_core
	STATIC
	src/config/config.cpp
	src/app/loot_generator.cpp
	src/app/collision_detector.cpp
	src/serialization/model_serialization.cpp
	src/entities/player.cpp
	src/app/game_state.cpp
	src/app/move_handler.cpp
    src/util/util.cpp
	src/util/tagged_uuid.cpp
	src/postgres/connection_pool.cpp
	src/postgres/retired_player_impl.cpp
	src/postgres/unit_of_work_impl.cpp
	src/domain/retired_player.cpp
)
target_include_directories(${PROJECT_NAME}_core PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
        "$<INSTALL_INTERFACE:include>"
        )
target_include_directories(${PROJECT_NAME}_core PUBLIC boost::boost)
target_link_libraries(${PROJECT_NAME}_core PUBLIC Threads::Threads boost::boost fmt::fmt libpqxx::pqxx)

add_executable(${PROJECT_NAME}
	src/main.cpp
	src/net/http_server.cpp
	src/model/model.cpp
	src/app/request_handler.cpp
	src/app/application.cpp
	src/app/resource_handler.cpp
	src/logger/logger.cpp
	src/util/ticker.cpp
	src/serialization/boost_json.cpp
	src/model/json_loader.cpp
	src/serialization/json_serialization.cpp
	src/app/api.cpp
	src/app/extra_data.cpp
	)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_core)

if (BUILD_TESTS)
	include(CTest)
	include(Catch)
	add_executable(${PROJECT_NAME}_tests
		tests/loot_generator_tests.cpp
		tests/config_loader_tests.cpp
		tests/token_generator_tests.cpp
		tests/collision_detector_tests.cpp
		tests/state_serialization_tests.cpp
	)
	target_link_libraries(${PROJECT_NAME}_tests PRIVATE Catch2::Catch2WithMain ${PROJECT_NAME}_core)
	catch_discover_tests(${PROJECT_NAME}_tests)

	file(COPY ${CMAKE_SOURCE_DIR}/tests/invalid.json ${CMAKE_SOURCE_DIR}/tests/valid.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

if (NOT WIN32)
	add_compile_options(
		-Werror
		-pedantic-errors
		-Wall
		-Wextra
		-Wnarrowing
		-Wpedantic
		-Wcast-align
		-Wcast-qual
		-Wconversion
		-Wctor-dtor-privacy
		-Wduplicated-branches
		-Wduplicated-cond
		-Wextra-semi
		-Wfloat-equal
		-Wlogical-op
		-Wnon-virtual-dtor
		-Wold-style-cast
		-Woverloaded-virtual
		-Wredundant-decls
		-Wsign-conversion
		-Wsign-promo
	)
endif()
