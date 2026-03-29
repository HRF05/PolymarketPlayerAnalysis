#include "UserStatsPipeline.h"

UserStatsPipeline::UserStatsPipeline(PolymarketApiQueries& api_ref, int goldsky_threads, int clob_threads, int gamma_threads)
    : api(api_ref), num_goldsky(goldsky_threads), num_clob(clob_threads), num_gamma(gamma_threads) {}

void UserStatsPipeline::startWorkers() {

    // 1. goldsky (graphql user positions)
    for(int i = 0; i < num_goldsky; i++){
        goldsky_workers.emplace_back([this](){
            UserQueryTask task;
            while(goldsky_queue.wait_and_pop(task)){
                try{
                    if(api.isBotAccount(task.user_id)){
                        finalizeUser(task.user_id, 0.0, 0.0, 0, false, true);
                        continue;
                    }

                    auto positions = api.getUserPositions(task.user_id);
                    size_t total_positions_count = positions.size();
                    double realized = 0.0;
                    std::vector<UserPosition> open_positions;
                    
                    for(const auto& pos : positions){
                        realized += pos.realized_pnl;
                        if (pos.amount > 0.0) open_positions.push_back(pos);
                    }

                    if(!open_positions.empty()){
                        gamma_queue.push({task.user_id, realized, total_positions_count, open_positions});
                    }
                    else{
                        finalizeUser(task.user_id, realized, 0, total_positions_count);
                    }
                }
                catch(const std::exception& e){
                    std::cerr << "[goldsky error] user " << task.user_id << ": " << e.what() << "\n";
                    finalizeUser(task.user_id, 0.0, 0.0, 0, true);
                }
            }
        });
    }

    // 2. gamma (market resolution data)
    for(int i = 0; i < num_gamma; i++){
        gamma_workers.emplace_back([this](){
            GammaTask task;
            while(gamma_queue.wait_and_pop(task)){
                try{
                    std::vector<std::string> token_ids;
                    for (const auto& p : task.positions) token_ids.push_back(p.token_id);
                    
                    auto gamma_data = api.getGammaTokenData(token_ids);
                    
                    double resolved_unrealized = 0.0;
                    std::vector<UserPosition> unresolved_positions;

                    for(const auto& pos : task.positions){
                        if(gamma_data.count(pos.token_id) && gamma_data[pos.token_id].is_resolved){
                            // market is finished.. final worth immediately
                            double token_val = gamma_data[pos.token_id].is_winner ? 1.0 : 0.0;
                            resolved_unrealized += (token_val - pos.avg_price) * pos.amount;
                        }
                        else{
                            // market is still active
                            unresolved_positions.push_back(pos);
                        }
                    }


                    if(!unresolved_positions.empty()){
                        clob_queue.push({task.user_id, task.realized_pnl, resolved_unrealized,task.total_positions, unresolved_positions});
                    }
                    else{
                        finalizeUser(task.user_id, task.realized_pnl, resolved_unrealized, task.total_positions);
                    }
                }
                catch(const std::exception& e){
                    std::cerr << "[gamma error] user " << task.user_id << ": " << e.what() << "\n";
                    finalizeUser(task.user_id, 0.0, 0.0, 0, true);
                }
            }
        });
    }

    // 3. clob (orderbook prices)
    for(int i = 0; i < num_clob; ++i){
        clob_workers.emplace_back([this](){
            ClobTask task;
            while(clob_queue.wait_and_pop(task)){
                try{
                    std::vector<std::string> token_ids;
                    for(const auto& p : task.unresolved_positions) token_ids.push_back(p.token_id);
                    
                    auto prices = api.getTokenPrices(token_ids);
                    
                    double active_unrealized = 0.0;
                    for(const auto& pos : task.unresolved_positions){
                        if (prices.count(pos.token_id)) {
                            active_unrealized += (prices[pos.token_id] - pos.avg_price) * pos.amount;
                        }
                    }
                    
                    double final_unrealized_pnl = task.unrealized_pnl + active_unrealized;

                    finalizeUser(task.user_id, task.realized_pnl, final_unrealized_pnl, task.total_positions);
                }
                catch(const std::exception& e){
                    std::cerr << "[clob error] user " << task.user_id << ": " << e.what() << "\n";
                    finalizeUser(task.user_id, 0.0, 0.0, 0, true);
                }
            }
        });
    }
}
UserStatsPipeline::~UserStatsPipeline() {
    goldsky_queue.set_done();
    

    for(auto& w : goldsky_workers){
        if(w.joinable()) w.join();
    }
    gamma_queue.set_done();
    
    for(auto& w : gamma_workers){
        if (w.joinable()) w.join();
    }
    clob_queue.set_done();
    for(auto& w : clob_workers){
        if (w.joinable()) w.join();
    }
}
void UserStatsPipeline::finalizeUser(const std::string& user_id, double realized, double unrealized, size_t total_positions, bool is_error, bool is_bot){
    {
        std::lock_guard<std::mutex> lock(results_mtx);
        if(!is_error) final_results[user_id] = {realized, unrealized, total_positions, is_bot}; 
    }

    int remaining = --active_tasks;
    
    if(remaining == 0){
        std::lock_guard<std::mutex> lock(completion_mtx);
        completion_cv.notify_all();
    }
}
void UserStatsPipeline::processUsers(const std::vector<std::string>& users){
    if (users.empty()) return;

    active_tasks += static_cast<int>(users.size());

    for(const auto& u : users){
        goldsky_queue.push({u});
    }


    std::unique_lock<std::mutex> lock(completion_mtx);
    completion_cv.wait(lock, [this]() { return active_tasks == 0; });
    
    std::cout << "batch finished computing" << "\n";
}