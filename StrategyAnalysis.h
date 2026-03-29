#pragma once
#include "Models.h"
#include <vector>
#include <iostream>
#include <cmath>
#include <numeric>

struct Prediction{
    std::string asset_id;
    double market_price;
    double model_price;
    double forward_price;
    double final_outcome;
};

class StrategyAnalysis{
private:
    int forward_lookahead_steps;

public:
    StrategyAnalysis(int lookahead_steps = 100) : forward_lookahead_steps(lookahead_steps) {}

    std::vector<Prediction> init(const std::vector<tradeEvent>& historical_trades, const std::vector<double>& model_projections, double final_outcome) const;
    
    void evaluatePerformance(const std::vector<Prediction>& predictions) const;

private:
    double calculateCorrelation(const std::vector<double>& x, const std::vector<double>& y) const;
};