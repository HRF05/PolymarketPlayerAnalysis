#include "ManageFileData.h"

void ManageFileData::usersFileAdd(const std::unordered_map<std::string, UserAnalysisResult> &user_result, const std::string &user_filename){
    std::ofstream file(user_filename);
    if(!file.is_open()){
        std::cerr<<"usersFileAdd, file open failed\n";
        return;
    }
    file<<"user_id,num_pos,realzied,unrealized,is_bot\n";
    for(const std::pair<std::string, UserAnalysisResult>& res : user_result){
        file<<res.first<<","
            <<res.second.num_pos<<","
            <<res.second.realized<<","
            <<res.second.unrealized<<","
            <<res.second.is_bot<<"\n";
    }
}

std::unordered_map<std::string, UserAnalysisResult> ManageFileData::usersFileGet(const std::string &user_filename){
    std::ifstream file(user_filename);
    std::string line;
    std::getline(file, line);
    std::unordered_map<std::string, UserAnalysisResult> ret;
    while(std::getline(file, line)){
        if (line.empty()) continue;
        size_t start = 0;
        size_t end = 0;

        auto next_token = [&](){
            end = line.find(',', start);
            std::string token = line.substr(start, end - start);
            start = end + 1;
            return token;
        };
        std::string user_id = next_token();
        size_t num_pos = static_cast<size_t>(std::stoull(next_token()));
        double realized = std::stod(next_token());
        double unrealized = std::stod(line.substr(start)); 
        ret[user_id] = {realized, unrealized, num_pos};
    }
    return ret;
}

void ManageFileData::marketFileAdd(const std::vector<TradeEvent>& trades, const std::string& asset_id){
    std::filesystem::create_directories("./data");
    std::string filename = "./data/market-" + asset_id + ".csv";
    std::ofstream outFile(filename);
    if (!outFile.is_open()) {
        std::cerr<<"marketFileAdd: could not create file at "<<filename<<"\n";
        return; 
    }
    outFile<<"timestamp,price,size,is_buyer_maker,asset_id,maker_id,taker_id\n";
    for(const auto& t : trades){
        outFile<<t.timestamp<<"," 
               <<t.price<<"," 
               <<t.size<<"," 
               <<t.is_buyer_maker<<","
               <<t.token_id<<","
               <<t.maker_id<<","
               <<t.taker_id<<"\n";
    }
}

std::vector<TradeEvent> ManageFileData::marketFileGet(const std::string& asset_id){
    std::vector<TradeEvent> trades;
    std::string filename = "./data/market-" + asset_id + ".csv";
    
    if(!std::filesystem::exists(filename)){
        return trades;
    }
    
    std::ifstream file(filename);
    if(!file.is_open()){
        std::cerr<<"marketFileGet: could not open file"<<filename<<"\n";
        return trades;
    }

    trades.reserve(50000);
    std::string line;
    std::getline(file, line);

    while(std::getline(file, line)){
        if(line.empty()) continue;

        TradeEvent row;
        size_t start = 0;
        size_t end = 0;

        auto next_token = [&](){
            end = line.find(',', start);
            std::string token = line.substr(start, end - start);
            start = end + 1;
            return token;
        };

        row.timestamp = std::stoull(next_token());
        row.price = std::stod(next_token());
        row.size = std::stod(next_token());
        row.is_buyer_maker = (next_token() == "1");
        row.token_id = next_token();
        row.maker_id = next_token();
        row.taker_id = line.substr(start); 
        trades.push_back(std::move(row));
    }
    return trades;
}