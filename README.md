Prerequisites:
-To compile this C++17 compatible compiler is needed
-Cmake is needed
-To build run: cmake -S . -B build
-To compile run: cmake --build build --config Release
-The exe will be located in either the build directory or build\Release



Overview
This project is a C++ pipeline designed to analyze decentralized prediction market data from Polymarket. It reconstructs market order books, evaluates the historical profitability of market participants, and generates an alternative probability model, using a simple demo strategy of creating some negative weight proportional to the difference in opinion of unprofitable market participants and market odds.

Core Concepts:
- System Architecture & Concurrency Multi-threaded Data Ingestion: Uses a ThreadSafeQueue to manage a thread pool across three data fetching pipelines (Goldsky subgraphs, Gamma API, and CLOB API). Asynchronous Processing: The UserStatsPipeline handles concurrent task processing, ensuring thread safety with mutexes and condition variables while gathering various data needed to accurately calculate user positions.

- Efficient and Safe API Queries: Implements exponential backoff algorithms with randomized jitter to handle API rate limits. The system manages thread-safe request  across multiple concurrent API endpoints to ensure reliable, data ingestion without triggering server-side blocks.

- Probability Modeling: PredictionModel isolates trades from "non-credible" actors (users who do not meet specific profitability or volume thresholds). It calculates implied odds by weighting the volume of these actors, creating a contrarion indicator based on the probability that is logically implied by their actions.

- Algorithmic Bot Detection: The system employs a heuristic analysis to identify bots, analyzing trade density for bursts and macro time horizons.

- Predictive Evaluation Metrics: StrategyAnalysis module evaluates the generated probability models against actual market resolutions using standard scoring rules: Brier Score & Brier Skill Score (BSS): Measures the mean squared difference between predicted probabilities and actual outcomes. Logarithmic Loss: Punishes extreme confidence in incorrect predictions. Pearson Correlation: Calculates the correlation between the model's perceived edge and forward market returns. Probability Calibration: Segments predictions into decile buckets to measure how well the model's confidence aligns with empirical resolution rates.