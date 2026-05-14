#pragma once
#include "Models.h"
#include <atomic>
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <stdexcept>
#include <iostream>
#include <cpr/cpr.h>
#include <thread>
#include <chrono>
#include <optional>
#include <random>
#include <mutex>
#include <algorithm>
#include <nlohmann/json.hpp>
/*
Note: std::stod, std::stoull, and nlohman::json::get<std::string()> throw exceptions, not currently handled
*/
class PolymarketApiQueries {
public:
    PolymarketApiQueries(const std::string& config_file = "config.json");


    std::vector<std::string> getTokenIds(const std::string &series_id, const std::string &tag_id) const;
    std::unordered_map<std::string, double> getTokenPrices(const std::vector<std::string>& token_ids) const;
    std::unordered_map<std::string, GammaTokenData> getGammaTokenData(const std::vector<std::string>& token_ids) const;

    std::vector<UserPosition> getUserPositions(const std::string& user_id) const;
    std::vector<TradeEvent> getMarketTradeHistory(const std::string& token_id) const;


private:
    // https://aws.amazon.com/blogs/architecture/exponential-backoff-and-jitter/#:~:text=Adding%20Backoff,delay%20has%20introduced%20some%20spreading.
    template <typename Func, typename RateLimitFunc>
    cpr::Response executeWithRetry(Func request_func, RateLimitFunc rate_limit_func, double initial_backoff, const std::string& api_name) const{
        cpr::Response r;
        double temp_backoff = initial_backoff;
        thread_local std::mt19937 generator(std::random_device{}());
        bool is_goldsky = api_name.find("goldsky") != std::string::npos;
        for(int i = 0; i < max_retries; i++){
            rate_limit_func();
            r = request_func();

            if(r.status_code == 200) return r;
            
            if(r.status_code == 504 && is_goldsky){
                return r; 
            }

            if(r.status_code == 429 || r.status_code == 0 || r.status_code >= 500){
                std::uniform_real_distribution<double> distribution(0.8, 1.2);
                double jittered_backoff = temp_backoff * distribution(generator); // randomlize +-20%


                std::cerr<<"rate limit hit .. retrying in "<<jittered_backoff<<"ms\n";

                std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(jittered_backoff));
                temp_backoff *= backoff_rate;
            } 
            else{
                return r;
            }
        }
        return r;
    }


    bool isTimeout(const cpr::Response &r) const;



    std::optional<nlohmann::json> parseAndValidateJson(const cpr::Response& r, const std::string& context) const;
    void applyRateLimit(std::atomic<int64_t>& last_call, int backoff_ms) const;

    std::string gamma_base_url;
    std::string clob_base_url;
    std::string pnl_subgraph_url;
    std::string orderbook_subgraph_url;

    int max_retries = 5;
    int backoff_ms_clob = 20;
    int backoff_ms_gamma = 35;
    int backoff_ms_goldsky = 200;
    double backoff_rate = 1.25;

    mutable std::atomic<int64_t> last_gamma_call;

    mutable std::atomic<int64_t> last_clob_call;

    mutable std::atomic<int64_t> last_goldsky_call;
};