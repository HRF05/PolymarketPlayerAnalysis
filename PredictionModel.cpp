#include "PredictionModel.h"

bool PredictionModel::IsCredible(UserAnalysisResult pnl){
    if(pnl.realized + pnl.unrealized > 1000) return true;
    if(pnl.is_bot == true) return true;
    if(pnl.num_pos == 1) return true; // if this is their only position not enough data.. Maybe could remove this
    return false;
}

std::vector<double> PredictionModel::ImpliedOddsFromNonCredibleUsers(const std::vector<tradeEvent>& market_history, const std::unordered_map<std::string, UserAnalysisResult>& user_stats){
    std::vector<double> res;
    res.reserve(market_history.size());
    double yes_weighted_volume = 0.0;
    double no_weighted_volume = 0.0;
    for(auto& trade : market_history){
        UserAnalysisResult maker_res = user_stats.at(trade.maker_id), taker_res = user_stats.at(trade.taker_id);

        bool cmaker = IsCredible(maker_res);
        bool ctaker = IsCredible(taker_res);

        double yes_cost = trade.size * trade.price;
        double no_cost = trade.size * (1.0 - trade.price);

        if(!cmaker){
            if(trade.is_buyer_maker){
                yes_weighted_volume += yes_cost;
            }
            else{
                no_weighted_volume += no_cost;
            }
        }

        if(!ctaker){
            if(!trade.is_buyer_maker){
                yes_weighted_volume += yes_cost;
            }
            else{
                no_weighted_volume += no_cost;
            }
        }
        if(yes_weighted_volume + no_weighted_volume == 0.0){
            res.push_back(0);
        }
        else{
            res.push_back(yes_weighted_volume / (yes_weighted_volume + no_weighted_volume));
        }
    }
    
    return res;
}

std::vector<double> PredictionModel::GeneratePredictedOdds(const std::vector<tradeEvent>& market_history, const std::unordered_map<std::string, UserAnalysisResult>& user_stats, double WEIGHT){

    std::vector<double> impliedOddsNC = ImpliedOddsFromNonCredibleUsers(market_history, user_stats);
    std::vector<double> res;
    res.reserve(market_history.size());
    for(int i = 0; i < market_history.size(); i++){ // using last sale as market price may be suboptimal
        double market_price = market_history.at(i).price;

        if(impliedOddsNC[i] == 0){
            res.push_back(market_price);
            continue;
        }

        double predicted_odds = market_price + (WEIGHT * (market_price - impliedOddsNC[i]));
        
        res.push_back(predicted_odds);
    }
    return res;
}