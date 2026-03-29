#include "PolymarketApiQueries.h"
#include "UserStatsPipeline.h"
#include "ManageFileData.h"
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
        std::string asset_id = "89308404461300170915063446315611587250054643852836517235026678818761584555534";
        std::string filename = "asset-trades-" + asset_id + ".csv";
        PolymarketApiQueries api("config.json");

        ManageFileData::marketFileAdd(api.getMarketTradeHistory(asset_id), filename);
        std::vector<tradeEvent> trades = ManageFileData::marketFileGet(filename);

        UserStatsPipeline pipeline(api);
        
        std::cout<<"startWorkers\n";
        pipeline.startWorkers();
        
        std::vector<std::string> test_users = {"", ""};
        pipeline.processUsers(test_users);
        
    }
    catch(const std::exception& e){
        std::cerr << "Fatal error: " << e.what() << "\n";
    }
    return 0;
}