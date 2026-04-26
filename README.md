Prerequisites:\
-To compile this C++17 compatible compiler is needed\
-Cmake is needed\
-To build run: cmake -S . -B build\
-To compile run: cmake --build build --config Release\
-The exe will be located in either the build directory or build\Release\




This project is a C++ pipeline designed to analyze prediction market data from Polymarket. It valuates the historical profitability of market participants, and generates an alternative probability model, using a simple demo strategy of creating some negative weight proportional to the difference in opinion of unprofitable market participants and market odds.

Core Concepts:
- Multi-threaded Data Gathering: Uses a ThreadSafeQueue to manage a thread pool across three data fetching pipelines (Goldsky subgraphs, Gamma API, and CLOB API). Asynchronous Processing: The UserStatsPipeline handles concurrent task processing, ensuring thread safety with mutexes and condition variables while gathering various data needed to accurately calculate user positions.

- Efficient and Safe API Queries: Implements exponential backoff algorithms with randomized jitter to handle API rate limits. Queries, and json data parsing for different api endpoints, with differing structue

- Probability Modeling: PredictionModel takes trades from "non-credible" actors (users who do not meet specific profitability thresholds, and have a human amount of volume). It calculates implied odds by weighting the volume of these actors, creating an implied probability based on their actions, and inverting it.

- Algorithmic Bot Detection: The system employs a heuristic analysis to identify bots, analyzing trade density for bursts and macro time frames.

- Predictive Evaluation Metrics: StrategyAnalysis component evaluates the generated probability models against actual market resolutions using standard scoring rules: Brier Score & Brier Skill Score (BSS): Measures the mean squared difference between predicted probabilities and actual outcomes. Logarithmic Loss: Punishes confidence in incorrect predictions. Pearson Correlation: Calculates the correlation between the model's perceived edge and forward market returns. Probability Calibration: Measures difference between confidence and accuracy.