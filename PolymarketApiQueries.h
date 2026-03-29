#pragma once
#include "Models.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <iostream>
#include <cpr/cpr.h>
#include <thread>
#include <chrono>
#include <random>
#include <mutex>

class BotDetectedException : public std::runtime_error {
public:
    BotDetectedException(const std::string& msg) : std::runtime_error(msg) {}
};

class PolymarketApiQueries {
public:
    PolymarketApiQueries(const std::string& config_file = "config.json");

    std::vector<UserPosition> getUserPositions(const std::string& user_id) const;
    std::unordered_map<std::string, GammaTokenData> getGammaTokenData(const std::vector<std::string>& token_ids) const;
    std::unordered_map<std::string, double> getTokenPrices(const std::vector<std::string>& token_ids) const;
    std::vector<tradeEvent> getMarketTradeHistory(const std::string& asset_id) const;
    bool isBotAccount(const std::string& user_id) const;

private:
    template <typename Func, typename RateLimitFunc>

    /// https://aws.amazon.com/blogs/architecture/exponential-backoff-and-jitter/#:~:text=Adding%20Backoff,delay%20has%20introduced%20some%20spreading.
    cpr::Response executeWithRetry(Func request_func, RateLimitFunc rate_limit_func, double initial_backoff, const std::string& api_name) const{
        cpr::Response r;
        double temp_backoff = initial_backoff;
        thread_local std::mt19937 generator(std::random_device{}());

        for (int i = 0; i < max_retries; i++){
            rate_limit_func();
            r = request_func();

            if(r.status_code == 200) return r;
            if(r.status_code == 429 || r.status_code == 0 || r.status_code >= 500){
                std::uniform_real_distribution<double> distribution(0.8, 1.2);
                double jittered_backoff = temp_backoff * distribution(generator); // randomlize +-20%


                std::cerr<<"rate limit hit .. retrying in "<<jittered_backoff<<"ms\n";

                std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(jittered_backoff));
                temp_backoff *= backoff_rate;
            } 
            else{
                throw std::runtime_error(api_name + " api error: " + std::to_string(r.status_code) + " - " + r.text);
            }
        }
        throw std::runtime_error("Max retries exceeded for " + api_name);
    }

    void applyRateLimit(std::mutex& mtx, std::chrono::steady_clock::time_point& last_call, int backoff_ms) const;

    std::string pnl_subgraph_url;
    std::string orderbook_subgraph_url;
    std::string gamma_base_url;
    std::string clob_base_url;

    int max_retries = 5;
    int backoff_ms_goldsky = 200; // backoff timings based on the specific applications not generally applicable for api
    int backoff_ms_clob = 10;
    int backoff_ms_gamma = 35;
    double backoff_rate = 1.25;

    mutable std::mutex gamma_rate_mtx;
    mutable std::chrono::steady_clock::time_point last_gamma_call = std::chrono::steady_clock::now();

    mutable std::mutex clob_rate_mtx;
    mutable std::chrono::steady_clock::time_point last_clob_call = std::chrono::steady_clock::now();

    mutable std::mutex goldsky_rate_mtx;
    mutable std::chrono::steady_clock::time_point last_goldsky_call = std::chrono::steady_clock::now();

    const double TOKEN_CONVERSION__DIVIDE = 1e6;
};