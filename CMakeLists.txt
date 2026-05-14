cmake_minimum_required(VERSION 3.14)
project(PolymarketDataGathering CXX)

cmake_policy(SET CMP0135 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(json)

set(CPR_CURL_USE_LIBPSL OFF CACHE BOOL "Disable libpsl in cpr" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Disable tests for dependencies" FORCE)

set(CURL_BROTLI OFF CACHE BOOL "Disable brotli in curl" FORCE)
set(CURL_ZSTD OFF CACHE BOOL "Disable zstd in curl" FORCE)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Force static libraries" FORCE)

FetchContent_Declare(cpr
  GIT_REPOSITORY https://github.com/libcpr/cpr.git
  GIT_TAG        1.10.5 
)
FetchContent_MakeAvailable(cpr)

find_package(Threads REQUIRED)


#
add_executable(${PROJECT_NAME}
  src/main.cpp
  src/PolymarketApiQueries.cpp
  src/UserStatsPipeline.cpp
  src/ManageFileData.cpp 
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${PROJECT_NAME} PRIVATE
    cpr::cpr
    nlohmann_json::nlohmann_json
    Threads::Threads
)