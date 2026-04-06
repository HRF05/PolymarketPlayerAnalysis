#include "PolymarketApiQueries.h"

PolymarketApiQueries::PolymarketApiQueries(const std::string& config_file){
    std::ifstream file(config_file);
    if(!file.is_open()){
        throw std::runtime_error("Error: could not open configuration file: " + config_file);
    }

    nlohmann::json config;
    try{
        file>>config;
        
        pnl_subgraph_url = config["goldsky"]["pnl_subgraph"].get<std::string>();
        orderbook_subgraph_url = config["goldsky"]["orderbook_subgraph"].get<std::string>();
        gamma_base_url = config["api"]["gamma"].get<std::string>();
        clob_base_url = config["api"]["clob"].get<std::string>();
        
    }
    catch(const nlohmann::json::exception& e){
        throw std::runtime_error(std::string("Error parsing config: ") + e.what());
    }
}



void PolymarketApiQueries::applyRateLimit(std::mutex& mtx, std::chrono::steady_clock::time_point& last_call, int backoff_ms) const{
    std::chrono::milliseconds sleep_duration(0);
    {
        std::lock_guard<std::mutex> lock(mtx);
        auto now = std::chrono::steady_clock::now();
        auto target_time = last_call + std::chrono::milliseconds(backoff_ms);
        
        if(now < target_time){
            sleep_duration = std::chrono::duration_cast<std::chrono::milliseconds>(target_time - now);
            last_call = target_time; 
        }
        else{
            last_call = now; 
        }
    }
    if(sleep_duration.count() > 0){
        std::this_thread::sleep_for(sleep_duration);
    }
}

std::unordered_map<std::string, GammaTokenData> PolymarketApiQueries::getGammaTokenData(const std::vector<std::string>& token_ids) const{
    std::unordered_map<std::string, GammaTokenData> results;
    if(token_ids.empty()) return results;

    const size_t BATCH_SIZE = 45;
    thread_local cpr::Session session;

    for(size_t i = 0; i < token_ids.size(); i += BATCH_SIZE){
        std::string gamma_url = gamma_base_url + "?limit=100";
        
        for(size_t j = i; j < (std::min)(i + BATCH_SIZE, token_ids.size()); ++j){
            gamma_url += "&clob_token_ids=" + token_ids[j];
        }

        session.SetUrl(cpr::Url(gamma_url));

        
        cpr::Response r = executeWithRetry([](){return session.Get();}, [&](){ applyRateLimit(gamma_rate_mtx, last_gamma_call, backoff_ms_gamma);}, backoff_ms_gamma, "gamma");

        auto response = nlohmann::json::parse(r.text, nullptr, false);
        if(response.is_discarded()){
            std::cerr<<"received invalid json from api getGammaTokenData"<<"\n";
            continue;
        }
        if(response.contains("errors")){
            throw std::runtime_error("clob error in getGammaTokenData: " + response["errors"].dump());
        }
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


        cpr::Response r = executeWithRetry([](){ return session.Post();}, [&](){ applyRateLimit(clob_rate_mtx, last_clob_call, backoff_ms_clob); }, backoff_ms_clob, "clob");

        auto response = nlohmann::json::parse(r.text, nullptr, false);
        if(response.is_discarded()){
            std::cerr<<"received invalid json from api getTokenPrices"<<"\n";
            continue;
        }

        if(response.contains("errors")){
            throw std::runtime_error("clob error in getTokenPrices: " + response["errors"].dump());
        }
        for(size_t j = i; j < (std::min)(i + BATCH_SIZE, token_ids.size()); j++){
            const auto& id = token_ids[j];
            if(response.contains(id) && response[id].contains("BUY") && !response[id]["BUY"].is_null()){
                auto val = response[id]["BUY"];
                prices[id] = val.is_string() ? std::stod(val.get<std::string>()) : val.get<double>();
            }
        }
    }
    return prices;
}
std::vector<UserPosition> PolymarketApiQueries::getUserPositions(const std::string& user_id) const{
    std::vector<UserPosition> ret;
    std::string last_id = "";
    int positions_size = 200;
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
        std::string id_filter = last_id.empty() ? "" : "id_gt: \"" + last_id + "\", ";
        nlohmann::json request_body = {
            {"query", query},
            {"variables", {
            {"user", user_id},
            {"lastId", last_id},
            {"positionsSize", positions_size}
            }}
        };
        session.SetBody(cpr::Body{request_body.dump()});

        

        cpr::Response r = executeWithRetry([](){ return session.Post(); }, [&](){applyRateLimit(goldsky_rate_mtx, last_goldsky_call, backoff_ms_goldsky);}, backoff_ms_goldsky, "goldsky-pnl");

        auto response = nlohmann::json::parse(r.text, nullptr, false);
        if(response.is_discarded()){
            std::cerr<<"received invalid json from api getGammaTokenData"<<"\n";
            continue;
        }

        if(response.contains("errors")){
            throw std::runtime_error("Graph error in getUserPositions: " + response["errors"].dump());
        }

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
        if(positions.size() < positions_size) break;
        last_id = positions.back()["id"].get<std::string>();

    }
    return ret;
}
bool PolymarketApiQueries::isBotAccount(const std::string& user_id) const {
    const size_t BURST_POSITION_COUNT = 20;
    const uint64_t BURST_TIME_WINDOW_SEC = 2;
    const size_t MACRO_POSITION_COUNT = 1000;
    const uint64_t MACRO_TIME_WINDOW_SEC = 60*60*24*5;
    
    thread_local cpr::Session session;
    session.SetUrl(cpr::Url{pnl_subgraph_url});
    session.SetHeader(cpr::Header{{"Content-Type", "application/json"}});

    std::string query = R"(query getBotSample($user: String!){
        userPositions(first: 1000, orderBy: timestamp, orderDirection: desc, where: { user: $user }){timestamp}
    })";
    
    nlohmann::json request_body = {
        {"query", query},
        {"variables", {{"user", user_id}}}
    };
    session.SetBody(cpr::Body{request_body.dump()});

    cpr::Response r = executeWithRetry([](){ return session.Post(); }, [&](){applyRateLimit(goldsky_rate_mtx, last_goldsky_call, backoff_ms_goldsky);}, backoff_ms_goldsky, "goldsky-pnl");
    
    auto response = nlohmann::json::parse(r.text, nullptr, false);
    if(response.is_discarded() || response.contains("errors") || !response.contains("data")) return false; // Default to human on network error

    auto positions = response["data"]["userPositions"];
    if(positions.empty()) return false;

    std::vector<uint64_t> timestamps;
    timestamps.reserve(positions.size());
    for(const auto& pos : positions){
        if(!pos["timestamp"].is_null()){
            timestamps.push_back(std::stoull(pos["timestamp"].get<std::string>()));
        }
    }
    if(timestamps.size() == 1000){
        uint64_t newest_ts = timestamps.front();
        uint64_t oldest_ts = timestamps.back();
        
        if((newest_ts - oldest_ts) <= MACRO_TIME_WINDOW_SEC){
            return true;
        }
    }
    if(timestamps.size() >= BURST_POSITION_COUNT){
        for(size_t i = 0; i <= timestamps.size() - BURST_POSITION_COUNT; ++i){
            uint64_t newest_ts = timestamps[i];
            uint64_t oldest_ts = timestamps[i + BURST_POSITION_COUNT - 1];
            
            if((newest_ts - oldest_ts) <= BURST_TIME_WINDOW_SEC){
                return true;
            }
        }
    }
    
    return false;
}
std::vector<std::string> PolymarketApiQueries::getAssetIds(const std::string &tag_id) const{
    std::vector<std::string> res;

    cpr::Session session;
    session.SetUrl(cpr::Url{orderbook_subgraph_url});
    session.SetHeader(cpr::Header{{"Content-Type", "application/json"}});

    std::string last_id = "";
    std::string query = R"(query get($lastId: String!) {conditions(first: 1000, orderBy: id, orderDirection: asc, where: { id_gt: $lastId, resolved: true }) {id}})";


    int limit = 100;
    int offset = 0;
    int totalEventsFetched = 0;
    bool hasMoreData = true;


    while(true){
        nlohmann::json request_body = {
            {"query", query},
            {"variables", {
                {"lastId", last_id}
            }}
        };
        session.SetBody(cpr::Body{request_body.dump()});

        cpr::Response r = executeWithRetry([&session](){return session.Post();}, [&](){applyRateLimit(goldsky_rate_mtx, last_goldsky_call, backoff_ms_goldsky);}, backoff_ms_goldsky, "goldsky-market_ids");

        auto json_data = nlohmann::json::parse(r.text, nullptr, false);
        if(json_data.is_discarded()){
            std::cerr<<"received invalid json from api getGammaTokenData"<<"\n";
            continue;
        }
        
        if(json_data.contains("errors")){
            std::cerr<<"graphql error in getMarketTradeHistory: "<<json_data["errors"].dump()<<"\n";
            break;
        }
        auto conditions = json_data["data"]["conditions"];

        if(conditions.empty()){
            break; 
        }

        for(const auto& condition : conditions){
            res.push_back(condition["id"].get<std::string>());
        }

        last_id = conditions.back()["id"].get<std::string>();
    }
    return res;
}
std::vector<TradeEvent> PolymarketApiQueries::getMarketTradeHistory(const std::string& asset_id) const{
    std::vector<TradeEvent> trades;
    const double USD_CONVERSION = 1e6;
    cpr::Session session;
    session.SetUrl(cpr::Url{orderbook_subgraph_url});
    session.SetHeader(cpr::Header{{"Content-Type", "application/json"}});

    auto fetchTrades = [&](const std::string& side){ // querying only one side at a time may improve performance
        std::string last_id = "";
        std::string asset_field = side + "AssetId";
        
        while(true){
            std::string id_filter = last_id.empty() ? "" : "id_gt: \"" + last_id + "\", ";

            std::string graphql_query = R"(
                query GetTrades($assetId: String!, $lastId: String!) {
                    orderFilledEvents(
                        first: 1000, 
                        orderBy: id, 
                        orderDirection: asc, 
                        where: { )" + asset_field + R"(: $assetId, id_gt: $lastId }
                    ) {
                        id timestamp maker taker makerAssetId makerAmountFilled takerAssetId takerAmountFilled
                    }
                }
            )";
            
            nlohmann::json request_body = {
                {"query", graphql_query},
                {"variables", {
                    {"assetId", asset_id},
                    {"lastId", last_id}
                }}
            };
            session.SetBody(cpr::Body{request_body.dump()});
            cpr::Response r = executeWithRetry([&session](){return session.Post();}, [&](){applyRateLimit(goldsky_rate_mtx, last_goldsky_call, backoff_ms_goldsky);}, backoff_ms_goldsky, "goldsky-market_history");

            auto json_data = nlohmann::json::parse(r.text, nullptr, false);
            if(json_data.is_discarded()){
                std::cerr<<"received invalid json from api getGammaTokenData"<<"\n";
                continue;
            }
            
            if(json_data.contains("errors")){
                std::cerr<<"graphql error in getMarketTradeHistory: "<<json_data["errors"].dump()<<"\n";
                break;
            }
            
            auto trades_array = json_data["data"]["orderFilledEvents"]; 
            for(const auto& item : trades_array){
                TradeEvent event;
                event.timestamp = std::stoull(item["timestamp"].get<std::string>());
                event.asset_id = asset_id;
                event.maker_id = item["maker"].get<std::string>();
                event.taker_id = item["taker"].get<std::string>();
                
                last_id = item["id"].get<std::string>();
                std::string maker_asset = item["makerAssetId"].get<std::string>();
                double maker_amount = std::stod(item["makerAmountFilled"].get<std::string>());
                double taker_amount = std::stod(item["takerAmountFilled"].get<std::string>());
                double usdc_amount = 0;
                double token_amount = 0;
                if(maker_asset == "0"){ // usdc
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
            
            if(trades_array.size() < 1000) break;
            
        }
    };
    fetchTrades("maker");
    fetchTrades("taker");
    std::sort(trades.begin(), trades.end(), [](const TradeEvent& a, const TradeEvent& b){
        return a.timestamp < b.timestamp;
    });
    return trades;
}