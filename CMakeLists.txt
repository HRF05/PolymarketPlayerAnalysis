cmake_minimum_required(VERSION 3.14)
project(PolymarketAnalyzer CXX)

cmake_policy(SET CMP0135 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(json)


FetchContent_Declare(cpr
  GIT_REPOSITORY https://github.com/libcpr/cpr.git
  GIT_TAG        master 
)
FetchContent_MakeAvailable(cpr)

find_package(Threads REQUIRED)

add_executable(${PROJECT_NAME}
    main.cpp
    PolymarketApiQueries.cpp
    StrategyAnalysis.cpp
    UserStatsPipeline.cpp
    ManageFileData.cpp 
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    cpr::cpr
    nlohmann_json::nlohmann_json
    Threads::Threads
)