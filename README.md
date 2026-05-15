Prerequisites:\
-To compile this C++17 compatible compiler is needed\
-Cmake is needed\
-To build run: cmake -S . -B build\
-To compile run: cmake --build build --config Release\
-The exe will be located in either the build directory or build\Release\




This project is a C++ pipeline designed to efficiently get prediction market data from Polymarket. As of 2026-04-28 the goldsky subgraph queries are outdated, the gamma and clob api queries still work though, as well as the goldsky market history query (for now). The architecture should demonstrate ability to use C++ concurrency features. The main feature of this code is the full PNL queries, to get accurate PNL data pre 2026-04-28 requires goldsky subgraph queries, polymarket clob, and polymarket gamma api queries. These each have seperate rate limits so the code uses concurrency to maximally hit all simultaneously.

Core Concepts:
- Multi-threaded Data Gathering: Uses a ThreadSafeQueue to manage a thread pool across three data fetching pipelines (Goldsky subgraphs, Gamma API, and CLOB API). Asynchronous Processing: The UserStatsPipeline handles concurrent task processing, ensuring thread safety with mutexes and condition variables while gathering various data needed to accurately calculate user positions.

- Efficient and Safe API Queries: Implements exponential backoff algorithms with randomized jitter to handle API rate limits. Queries, and json data parsing for different api endpoints, with differing structue.
