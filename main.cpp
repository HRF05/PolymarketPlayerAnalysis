#include "PolymarketApiQueries.h"
#include "UserStatsPipeline.h"
#include "ManageFileData.h"
#include <unordered_map>
#include <iostream>

/*

for testing:
clob token id s are asset ids

mike tyson: https://gamma-api.polymarket.com/markets?condition_ids=0xb4961db4b70b4ebeebce6dee9816eda7a18443ae2d25240a70a0614a01f44ed2/
"105698626207188362137224559516875335051389535882173153619615964622033134253951"
jake paul:
draw:

trump # of tweets is between 0-19 from feb 27 to march 6: https://gamma-api.polymarket.com/markets?condition_ids=0x7b93af81c68efe4892bcfbc4bac8091dc2ffe0c85b63e2e955277778f6d813ac
"99186634638554651998743980431533134312247631653178164081175829567227353524421"
"89308404461300170915063446315611587250054643852836517235026678818761584555534"
*/
int main() {
    try{
        std::string asset_id = "99186634638554651998743980431533134312247631653178164081175829567227353524421";

        std::string user_filename = "data/users-cache.csv";

        PolymarketApiQueries api("config.json");

        ManageFileData::marketFileAdd(api.getMarketTradeHistory(asset_id), asset_id);
        std::vector<tradeEvent> trades = ManageFileData::marketFileGet(asset_id);

        UserStatsPipeline pipeline(api);
        
        std::cout<<"startWorkers\n";
        pipeline.startWorkers();
        std::vector<std::string> test_users;
        std::unordered_set<std::string> seen;
        for(const auto &trade : trades){
            if(!seen.count(trade.maker_id)){
                test_users.push_back(trade.maker_id);
                seen.insert(trade.maker_id);
            }
            if(!seen.count(trade.taker_id)){
                test_users.push_back(trade.taker_id);
                seen.insert(trade.taker_id);
            }
        }
        {
            pipeline.processUsers(test_users);
        }
        
        

        std::unordered_map<std::string, UserAnalysisResult> final_results = pipeline.getPnlData();

        ManageFileData::usersFileAdd(final_results, user_filename);

    }
    catch(const std::exception& e){
        std::cerr << "Fatal error: " << e.what() << "\n";
    }
    return 0;
}