#pragma once
#include <string>
#include <cstdint>
#include <vector>

struct UserPosition {
    std::string id;
    std::string token_id;
    double amount = 0.0;
    double avg_price = 0.0;
    double realized_pnl = 0.0;
    double gamma_price = 0.0;
};
struct GammaTokenData {
    bool is_resolved = false;
    bool is_winner = false;
    double price = 0.0;
};
struct PnlResult {
    double realized = 0.0;
    double unrealized = 0.0;
    
    double total() const { return realized + unrealized; }
};
struct tradeEvent {
    uint64_t timestamp = 0;
    double price = 0.0;
    double size = 0;
    std::string maker_id, taker_id, asset_id;
    bool is_buyer_maker = false;
};

struct UserQueryTask {
    std::string user_id;
};
struct GammaTask {
    std::string user_id;
    double realized_pnl;
    size_t total_positions;
    std::vector<UserPosition> positions;
};
struct ClobTask {
    std::string user_id;
    double realized_pnl;
    double unrealized_pnl;
    size_t total_positions;
    std::vector<UserPosition> unresolved_positions;
};


struct UserAnalysisResult {
    double realized = 0.0;
    double unrealized = 0.0;
    size_t num_pos = 0;
    bool is_bot = false;
};