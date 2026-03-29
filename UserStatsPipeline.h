#pragma once
#include "ThreadSafeQueue.h"
#include "PolymarketApiQueries.h"
#include <thread>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <atomic>
#include <condition_variable>

class UserStatsPipeline {
private:
    PolymarketApiQueries& api;
    ThreadSafeQueue<UserQueryTask> goldsky_queue;
    ThreadSafeQueue<GammaTask> gamma_queue;
    ThreadSafeQueue<ClobTask> clob_queue;
    
    
    std::vector<std::thread> goldsky_workers;
    std::vector<std::thread> gamma_workers;
    std::vector<std::thread> clob_workers;

    std::mutex results_mtx;
    std::unordered_map<std::string, UserAnalysisResult> final_results;


    std::atomic<int> active_tasks{0};
    std::mutex completion_mtx;
    std::condition_variable completion_cv;


    int num_goldsky, num_clob, num_gamma;

public:
    UserStatsPipeline(PolymarketApiQueries& api_ref, int goldsky_threads = 5, int clob_threads = 20, int gamma_threads = 20);
    ~UserStatsPipeline();

    void startWorkers();

    void processUsers(const std::vector<std::string>& users);
    void finalizeUser(const std::string& user_id, double realized, double unrealized, int total_positions, bool is_error = false);
    std::unordered_map<std::string, UserAnalysisResult> getPnlData() { std::lock_guard<std::mutex> lock(results_mtx); return final_results;}
    void clearResults() { std::lock_guard<std::mutex> lock(results_mtx); final_results.clear(); }
};