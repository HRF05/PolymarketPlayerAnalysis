#include "PolymarketApiQueries.h"
#include "UserStatsPipeline.h"
#include "ManageFileData.h"
#include <unordered_map>
#include <iostream>


int main(){
    std::string series_id = "10345";
    std::string tag_id = "100639";
    ////

    PolymarketApiQueries api;
    std::vector<std::string> asset_ids = api.getTokenIds(series_id, tag_id);
    std::ofstream of("./data/asset_ids-" + (tag_id) + ".csv");
    for(std::string asset_id : asset_ids){
        of<<asset_id<<",\n";
    }


    ////

    std::string token_id = "89308404461300170915063446315611587250054643852836517235026678818761584555534";


    std::vector<TradeEvent> marketTrades = api.getMarketTradeHistory(token_id); 


    ManageFileData::marketFileAdd(marketTrades, token_id);

    std::unordered_map<std::string, UserAnalysisResult> users = ManageFileData::usersFileGet("./data/users-cache.csv");


    std::unordered_set<std::string> current_users_seen;

    std::vector<std::string> users_to_calculate;

    for(auto trade : marketTrades){
        if(!current_users_seen.count(trade.maker_id) && !users.count(trade.maker_id)){
            current_users_seen.emplace(trade.maker_id);
            users_to_calculate.push_back(trade.maker_id);
        }
        if(!current_users_seen.count(trade.taker_id) && !users.count(trade.taker_id)){
            current_users_seen.emplace(trade.taker_id);
            users_to_calculate.push_back(trade.taker_id);
        }
    }



    UserStatsPipeline getPnl(api);


    getPnl.startWorkers();
    getPnl.processUsers(users_to_calculate);
    

    std::unordered_map<std::string, UserAnalysisResult> users_t = getPnl.getPnlData();

    users.merge(users_t);

    ManageFileData::usersFileAdd(users, "./data/users-cache.csv");
}