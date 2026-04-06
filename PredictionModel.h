#include <vector>
#include "Models.h"
#include <string>
#include <unordered_map>

namespace PredictionModel{
    bool isCredible(UserAnalysisResult pnl);
    std::vector<double> impliedOddsFromNonCredibleUsers(const std::vector<TradeEvent>& market_history, const std::unordered_map<std::string, UserAnalysisResult>& user_stats); 
    std::vector<double> generatePredictedOdds(const std::vector<TradeEvent>& market_history, const std::unordered_map<std::string, UserAnalysisResult>& user_stats, double WEIGHT = 0.05);// both return odds of "yes" outcome
}