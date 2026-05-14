#include "PolymarketApiQueries.h"

PolymarketApiQueries::PolymarketApiQueries(const std::string& config_file){
    std::ifstream file(config_file);
    if(!file.is_open()){
        std::cerr<<"constructor \n";
        throw std::runtime_error("Error: could not open configuration file: " + config_file);
    }
    
    nlohmann::json config;
    try{
        file>>config;
        

        gamma_base_url = config["api"]["gamma"].get<std::string>();
        clob_base_url = config["api"]["clob"].get<std::string>();


        pnl_subgraph_url = config["goldsky"]["pnl_subgraph"].get<std::string>();
        orderbook_subgraph_url = config["goldsky"]["orderbook_subgraph"].get<std::string>();

    }
    catch(const nlohmann::json::exception& e){
        std::cerr<<"constructor \n";
        throw std::runtime_error(std::string("Error parsing config: ") + e.what());
    }
}
bool PolymarketApiQueries::isTimeout(const cpr::Response &r) const{
    if(r.status_code == 504) return true;

    if(r.status_code == 200){
        auto response = nlohmann::json::parse(r.text, nullptr, false);
        if(!response.is_discarded() && response.contains("errors")){
            std::string err = response["errors"].dump();
            if(err.find("statement timeout") != std::string::npos){
                return true;
            }
        }
    }
    return false;
}
void PolymarketApiQueries::applyRateLimit(std::atomic<int64_t>& last_call, int backoff_ms) const{
    using namespace std::chrono;
    int64_t expected = last_call.load(std::memory_order_relaxed);

    while(true){
        int64_t current_time_ms = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        
        int64_t time_to_wait = (expected + backoff_ms) - current_time_ms;

        if(time_to_wait > 0){
            std::this_thread::sleep_for(milliseconds(time_to_wait));
            
            expected = last_call.load(std::memory_order_relaxed);
        }
        else{
            if(last_call.compare_exchange_weak(expected, current_time_ms, std::memory_order_acquire, std::memory_order_relaxed)){
                break;
            }
        }
    }
}
std::optional<nlohmann::json> PolymarketApiQueries::parseAndValidateJson(const cpr::Response& r, const std::string& context) const{
    auto response = nlohmann::json::parse(r.text, nullptr, false);
    if(response.is_discarded()){
        std::cerr<<"received invalid json from api "<<context<<"\n";
        return std::nullopt;
    }
    if(response.contains("errors")){
        std::cerr<<context<<" error: "<<response["errors"].dump()<<"\n";
        return std::nullopt;
    }
    return response;
}
std::vector<std::string> PolymarketApiQueries::getTokenIds(const std::string &series_id, const std::string &tag_id) const{
    std::vector<std::string> token_ids;

    std::string url = gamma_base_url + "/events";


    thread_local cpr::Session session;

    session.SetUrl(url);
    session.SetParameters(cpr::Parameters{{"active", "true"}, {"closed", "true"}, {"series_id", series_id}, {"tag_id", tag_id}});

    cpr::Response r = executeWithRetry([](){return session.Get();}, [&](){ applyRateLimit(last_gamma_call, backoff_ms_gamma);}, backoff_ms_gamma, "gamma");

    auto response_opt = parseAndValidateJson(r, "getTokenIds");

    if(!response_opt) return token_ids;

    auto& response = *response_opt;

    for(const auto& market : response){
        if(!market.contains("markets")) continue;

        for(const auto& markett : market["markets"]){
            if(!markett.contains("clobTokenIds")) continue;

            std::string clob_str = markett["clobTokenIds"].get<std::string>();
            
            auto clob_array = nlohmann::json::parse(clob_str, nullptr, false);
            
            if(!clob_array.is_discarded() && clob_array.is_array()){
                for(const auto& token_id : clob_array){
                    token_ids.push_back(token_id.get<std::string>());
                }
            }
            else{
                std::cerr<<"getTokenIds .?\n";
            }
        }
    }
    return token_ids;
}
std::unordered_map<std::string, GammaTokenData> PolymarketApiQueries::getGammaTokenData(const std::vector<std::string>& token_ids) const{
    std::unordered_map<std::string, GammaTokenData> results;
    if(token_ids.empty()) return results;

    const size_t BATCH_SIZE = 45;
    thread_local cpr::Session session;

    for(size_t i = 0; i < token_ids.size(); i += BATCH_SIZE){
        std::string gamma_url = gamma_base_url + "/markets" +  "?limit=100";
        
        for(size_t j = i; j < (std::min)(i + BATCH_SIZE, token_ids.size()); ++j){
            gamma_url += "&clob_token_ids=" + token_ids[j];
        }

        session.SetUrl(cpr::Url(gamma_url));

        
        cpr::Response r = executeWithRetry([](){return session.Get();}, [&](){ applyRateLimit(last_gamma_call, backoff_ms_gamma);}, backoff_ms_gamma, "gamma");

        auto response_opt = parseAndValidateJson(r, "getGammaTokenData");

        if(!response_opt) break;

        auto& response = *response_opt;

        for(const auto& market : response){
            if(!market.contains("tokens")) continue;

            for(const auto& token_info : market["tokens"]){
                std::string token_id = token_info.value("token_id", "");
                GammaTokenData data;
                
                data.is_resolved = market.value("resolved", false);
                if(data.is_resolved){
                    data.is_winner = token_info.value("winner", false);
                }
                else if(token_info.contains("price")){
                    auto price_val = token_info["price"];
                    data.price = price_val.is_number() ? price_val.get<double>() : std::stod(price_val.get<std::string>());
                }
                results[token_id] = data;
            }
        }
    }
    
    return results;
}
std::unordered_map<std::string, double> PolymarketApiQueries::getTokenPrices(const std::vector<std::string>& token_ids) const{
    std::unordered_map<std::string, double> prices;
    if(token_ids.empty()) return prices;

    const size_t BATCH_SIZE = 15;

    thread_local cpr::Session session;
    session.SetUrl(cpr::Url{clob_base_url + "/prices"});
    session.SetHeader(cpr::Header{{"Content-Type", "application/json"}});


    for(size_t i = 0; i < token_ids.size(); i += BATCH_SIZE){
        
    
        nlohmann::json request_body = nlohmann::json::array();
        for(size_t j = i; j < (std::min)(i + BATCH_SIZE, token_ids.size()); j++){
            request_body.push_back({{"token_id", token_ids[j]}, {"side", "BUY"}});
        }


        session.SetBody(cpr::Body{request_body.dump()});


        cpr::Response r = executeWithRetry([](){ return session.Post();}, [&](){ applyRateLimit(last_clob_call, backoff_ms_clob); }, backoff_ms_clob, "clob");

        auto response_opt = parseAndValidateJson(r, "getTokenPrices");

        if(!response_opt) break;

        auto& response = *response_opt;
        
        for(const auto& item : response){
            if(item.contains("token_id") && item.contains("price") && !item["price"].is_null()){
                std::string id = item["token_id"];
                auto val = item["price"];
                prices[id] = val.is_string() ? std::stod(val.get<std::string>()) : val.get<double>();
            }
        }
    }
    return prices;
}


std::vector<UserPosition> PolymarketApiQueries::getUserPositions(const std::string& user_id) const{
    std::vector<UserPosition> ret;
    std::string last_id = "";
    int batch_size = 1000;
    const double USD_CONVERSION = 1e6;


    thread_local cpr::Session session;
    session.SetUrl(cpr::Url{pnl_subgraph_url});
    session.SetHeader(cpr::Header{{"Content-Type", "application/json"}});
    std::string query = R"(
        query getPL($user: String!, $lastId: String!, $positionsSize: Int!) { 
            userPositions(first: $positionsSize, orderBy: id, orderDirection: asc, where: { id_gt: $lastId, user: $user }) {
                id realizedPnl tokenId amount avgPrice
            }
        }
    )";

    while(true){
        nlohmann::json request_body = {
            {"query", query},
            {"variables", {
            {"user", user_id},
            {"lastId", last_id},
            {"positionsSize", batch_size}
            }}
        };
        session.SetBody(cpr::Body{request_body.dump()});

        

        cpr::Response r = executeWithRetry([](){ return session.Post(); }, [&](){applyRateLimit(last_goldsky_call, backoff_ms_goldsky);}, backoff_ms_goldsky, "goldsky-pnl");
        

        while(isTimeout(r) && batch_size > 20){
            batch_size /= 2;
            std::cerr<<"got timeout in getUserPositions, reducing batch size to: "<<batch_size<<"\n";
            request_body["variables"]["positionsSize"] = batch_size;
            session.SetBody(cpr::Body{request_body.dump()});
            r = executeWithRetry([](){ return session.Post(); }, [&](){applyRateLimit(last_goldsky_call, backoff_ms_goldsky);}, backoff_ms_goldsky, "goldsky-pnl");
        }
        if (r.status_code != 200) {
            std::cerr<<"API error in getUserPositions, Code: "<<r.status_code<<"\n";
            break;
        }
        auto response_opt = parseAndValidateJson(r, "getUserPositions");

        if(!response_opt) break;

        auto& response = *response_opt;

        if(!response.contains("data") || !response["data"].contains("userPositions") || response["data"]["userPositions"].is_null()){
            std::cerr<<"unexpected graphql structure: "<<user_id<<"\n";
            break;
        }

        auto positions = response["data"]["userPositions"];
        for(const auto& pos : positions){
            UserPosition ps;
            ps.token_id = pos.value("tokenId", "");
            ps.amount = std::stod(pos.value("amount", "0")) / USD_CONVERSION; 
            ps.avg_price = std::stod(pos.value("avgPrice", "0")) / USD_CONVERSION;
            ps.realized_pnl = std::stod(pos.value("realizedPnl", "0")) / USD_CONVERSION;
            ret.push_back(ps);
        }
        if(positions.size() < batch_size) break;
        last_id = positions.back()["id"].get<std::string>();

    }
    return ret;
}

std::vector<TradeEvent> PolymarketApiQueries::getMarketTradeHistory(const std::string& token_id) const{
    std::vector<TradeEvent> trades;
    const double USD_CONVERSION = 1e6;
    cpr::Session session;
    session.SetUrl(cpr::Url{orderbook_subgraph_url});
    session.SetHeader(cpr::Header{{"Content-Type", "application/json"}});

    auto fetchTrades = [&](const std::string& side){ // querying only one side at a time may improve performance
        std::string last_id = "";
        std::string asset_field = side + "AssetId";
        int batch_size = 1000;

        std::string graphql_query = R"(
                query GetTrades($assetId: String!, $lastId: String!, $batchSize: Int!) {
                    orderFilledEvents(
                        first: $batchSize, 
                        orderBy: id, 
                        orderDirection: asc, 
                        where: { )" + asset_field + R"(: $assetId, id_gt: $lastId }
                    ) {
                        id timestamp maker taker makerAssetId makerAmountFilled takerAssetId takerAmountFilled
                    }
                }
            )";

        while(true){
            
            nlohmann::json request_body = {
                {"query", graphql_query},
                {"variables", {
                    {"assetId", token_id},
                    {"lastId", last_id},
                    {"batchSize", batch_size}
                }}
            };
            session.SetBody(cpr::Body{request_body.dump()});
            cpr::Response r = executeWithRetry([&session](){return session.Post();}, [&](){applyRateLimit(last_goldsky_call, backoff_ms_goldsky);}, backoff_ms_goldsky, "goldsky-market_history");

            while(isTimeout(r) && batch_size > 20){
                batch_size /= 2;
                std::cerr<<"timeout in getMarketTradeHistory, reducing batch size to "<<batch_size<<"\n";
                request_body["variables"]["batchSize"] = batch_size;
                session.SetBody(cpr::Body{request_body.dump()});
                r = executeWithRetry([&session](){return session.Post();}, [&](){applyRateLimit(last_goldsky_call, backoff_ms_goldsky);}, backoff_ms_goldsky, "goldsky-market_history");
            }

            if(r.status_code != 200){
                std::cerr<<"API error in getMarketTradeHistory, Code: "<<r.status_code<<"\n";
                break;
            }

            auto response_opt = parseAndValidateJson(r, "getMarketTradeHistory");

            if(!response_opt) break;

            auto& response = *response_opt;
            
            auto trades_array = response["data"]["orderFilledEvents"]; 
            for(const auto& item : trades_array){
                TradeEvent event;
                event.timestamp = std::stoull(item["timestamp"].get<std::string>());
                event.token_id = token_id;
                event.maker_id = item["maker"].get<std::string>();
                event.taker_id = item["taker"].get<std::string>();
                
                last_id = item["id"].get<std::string>();
                std::string maker_token_id = item["makerAssetId"].get<std::string>();
                double maker_amount = std::stod(item["makerAmountFilled"].get<std::string>());
                double taker_amount = std::stod(item["takerAmountFilled"].get<std::string>());
                double usdc_amount = 0;
                double token_amount = 0;
                if(maker_token_id == "0"){ // usdc
                    usdc_amount = maker_amount;
                    token_amount = taker_amount;
                    event.is_buyer_maker = true; 
                }
                else{
                    usdc_amount = taker_amount;
                    token_amount = maker_amount;
                    event.is_buyer_maker = false; 
                }
                if(token_amount > 0) event.price = usdc_amount / token_amount;
                else std::cerr<<"token_amount is 0 in get_trade_history"<<"\n";
                event.size = token_amount / USD_CONVERSION; 
                trades.push_back(event);
            }
            
            std::cout<<side<<" batches fetched, current total: "<<trades.size()<<"\n";
            
            if(trades_array.size() < batch_size) break;
            
        }
    };
    fetchTrades("maker");
    fetchTrades("taker");
    std::sort(trades.begin(), trades.end(), [](const TradeEvent& a, const TradeEvent& b){
        return a.timestamp < b.timestamp;
    });
    return trades;
}