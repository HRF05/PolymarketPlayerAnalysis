#include "StrategyAnalysis.h"


std::vector<Prediction> StrategyAnalysis::init(const std::vector<TradeEvent>& historical_trades, const std::vector<double>& model_predictions, double final_outcome) const {
    std::vector<Prediction> predictions;
    if(historical_trades.size() != model_predictions.size() || historical_trades.empty()){
        std::cerr << "StrategyAnalysis::init\n";
        return predictions;
    }
    if(historical_trades.size() < forward_lookahead_steps){
        std::cerr<<"historical_trades < forward_lookahead_steps in StrategyAnalysis::init\n";
        return predictions;
    }
    for(size_t i = 0; i < historical_trades.size() - forward_lookahead_steps; i++){
        Prediction obs;
        obs.asset_id = historical_trades[i].asset_id;
        obs.market_price = historical_trades[i].price;
        obs.model_price = model_predictions[i];
        obs.final_outcome = final_outcome;
        obs.forward_price = historical_trades[i + forward_lookahead_steps].price;
        
        predictions.push_back(obs);
    }
    return predictions;
}

double StrategyAnalysis::calculateCorrelation(const std::vector<double>& x, const std::vector<double>& y) const{
    if(x.empty() || x.size() != y.size()) return 0.0;

    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_sq_x = 0, sum_sq_y = 0;
    size_t n = x.size();

    for(size_t i = 0; i < n; i++){
        sum_x += x[i];
        sum_y += y[i];
        sum_xy += x[i] * y[i];
        sum_sq_x += x[i] * x[i];
        sum_sq_y += y[i] * y[i];
    }

    double numerator = (n * sum_xy) - (sum_x * sum_y);
    double denominator = std::sqrt(std::max((n * sum_sq_x) - (sum_x * sum_x), 0.0) * std::max((n * sum_sq_y) - (sum_y * sum_y), 0.0));

    if(denominator == 0) return 0.0;
    return numerator / denominator;
}

void StrategyAnalysis::evaluatePerformance(const std::vector<Prediction>& predictions) const{
    if(predictions.empty()){
        std::cerr << "No predictions to evaluate.\n";
        return;
    }

    double brier_score_model_sum = 0.0;
    double brier_score_market_sum = 0.0;
    double log_loss_sum = 0.0;
    std::vector<double> edges;
    std::vector<double> forward_returns;

    struct Bucket{int count = 0; double sum_model = 0.0; double sum_actual = 0.0; };
    std::vector<Bucket> calibration_buckets(10);

    int hits = 0;
    int correct_direction_calls = 0;

    for(const auto& obs : predictions){
        // model brier score
        double model_error = obs.model_price - obs.final_outcome;
        brier_score_model_sum += (model_error * model_error);

        // market brier score
        double market_error = obs.market_price - obs.final_outcome;
        brier_score_market_sum += (market_error * market_error);

        // log-loss component
        double p = std::max(1e-15, std::min(1.0 - 1e-15, obs.model_price));
        log_loss_sum += -(obs.final_outcome * std::log(p) + (1.0 - obs.final_outcome) * std::log(1.0 - p));

        // edge and return tracking for correlation
        double edge = obs.model_price - obs.market_price;
        double fwd_return = obs.forward_price - obs.market_price;
        edges.push_back(edge);
        forward_returns.push_back(fwd_return);

        // directional hit rate
        if(std::abs(edge) > 0.01){
            correct_direction_calls++;
            if((edge > 0 && fwd_return > 0) || (edge < 0 && fwd_return < 0)){
                hits++;
            }
        }

        // calibration mapping
        int bin = std::max(0, std::min(9, static_cast<int>(obs.model_price * 10)));
        calibration_buckets[bin].count++;
        calibration_buckets[bin].sum_model += obs.model_price;
        calibration_buckets[bin].sum_actual += obs.final_outcome;
    }

    size_t n = predictions.size();
    
    double bs_model = brier_score_model_sum / n;
    double bs_market = brier_score_market_sum / n;
    

    double bss = 0.0;
    if(bs_market != 0.0){
        bss = 1.0 - (bs_model / bs_market);
    }

    double log_loss = log_loss_sum / n;
    double edge_return_correlation = calculateCorrelation(edges, forward_returns);
    double hit_rate = correct_direction_calls > 0 ? (static_cast<double>(hits) / correct_direction_calls) * 100.0 : 0.0;



   std::ofstream file("data/strategy_performance.txt");
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file to write performance report.\n";
        return;
    }

    file << "Total Trades: " << n << "\n";
    file << "Model Brier Score: " << bs_model << "\n";
    file << "Market Brier Score: " << bs_market << "\n";
    file << "Brier Skill Score: " << bss << "\n";
    file << "Global Log-Loss: " << log_loss << "\n";
    file << "Edge/Return Pearsonr: " << edge_return_correlation << "\n";
    file << "Directional Hit Rate: " << hit_rate << "%\n";

    file << "\nPROBABILITY CALIBRATION\n";
    file << "Bucket, N trades, Avg Model Prob, Actual Resolution Rate\n";
    
    for (int i = 0; i < 10; ++i) {
        if (calibration_buckets[i].count > 0) {
            double avg_model = calibration_buckets[i].sum_model / calibration_buckets[i].count;
            double actual_rate = calibration_buckets[i].sum_actual / calibration_buckets[i].count;     
            file << (i * 10) << "-" << ((i + 1) * 10) << "%, "
                 << calibration_buckets[i].count << ", "
                 << avg_model << ", "
                 << actual_rate << "\n";
        }
    }
    
    file.close();
}