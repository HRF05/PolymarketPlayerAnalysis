#include <vector>
#include "Models.h"
#include <string>
#include <unordered_map>

namespace PredictionModel{
    bool IsCredible(UserAnalysisResult pnl);
    std::vector<double> ImpliedOddsFromNonCredibleUsers(const std::vector<tradeEvent>& market_history, const std::unordered_map<std::string, UserAnalysisResult>& user_stats); 
    std::vector<double> GeneratePredictedOdds(const std::vector<tradeEvent>& market_history, const std::unordered_map<std::string, UserAnalysisResult>& user_stats, double WEIGHT = 0.05);// both return odds of "yes" outcome
}