#include "ManageFileData.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <string>
#include <sstream>
#include <vector>
#include <thread>

void ManageFileData::usersFileAdd(const std::unordered_map<std::string, bool> &user_credible, const std::string &user_filename){
    std::filesystem::create_directories("./data");
    std::string filename = "./data/users_temp_" + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + ".csv";
    {
        std::ofstream file(filename);
        if (!file.is_open()) return; 
        
        file << "user_id,credibility\n";
        for (const auto& user : user_credible) {
            file << user.first << "," << user.second << "\n";
        }
    }
    std::error_code ec;
    std::filesystem::rename(filename, user_filename, ec); // atomic
    if(ec){
        std::cerr << "error renaming file: " << ec.message() << "\n";
        std::filesystem::remove(filename); 
    }
    return;
}

std::unordered_map<std::string, bool> ManageFileData::usersFileGet(const std::string &user_filename){
    std::ifstream file(user_filename);
    std::string line;
    std::getline(file, line);
    std::unordered_map<std::string, bool> ret;
    while(std::getline(file, line)){
        if (line.empty()) continue;
        size_t comma_pos = line.find(',');
        if (comma_pos != std::string::npos) {
            std::string user_id = line.substr(0, comma_pos);
            bool credible = (line.substr(comma_pos + 1) == "1");
            ret[user_id] = credible;
        }
    }
    return ret;
}

void ManageFileData::marketFileAdd(const std::vector<tradeEvent>& trades, const std::string& filename){
    std::filesystem::create_directories("./data");
    std::ofstream outFile(filename);
    if (!outFile.is_open()) {
        std::cerr << "\ncould not create file at " << filename << "\n";
        return; 
    }
    outFile << "timestamp,price,size,is_buyer_maker,asset_id,maker_id,taker_id\n";
    for(const auto& t : trades){
        outFile << t.timestamp << "," 
                << t.price << "," 
                << t.size << "," 
                << t.is_buyer_maker << ","
                << t.asset_id << ","
                << t.maker_id << ","
                << t.taker_id << "\n";
    }
}

std::vector<tradeEvent> ManageFileData::marketFileGet(const std::string& asset_id){
    std::vector<tradeEvent> trades;
    std::string filename = "./data/market-" + asset_id + ".csv";
    
    if(!std::filesystem::exists(filename)){
        return trades;
    }
    
    std::ifstream file(filename);
    if(!file.is_open()){
        std::cerr << "error: could not open file  ... ManageFileData::marketFileGet" << filename << "\n";
        return trades;
    }

    trades.reserve(50000);
    std::string line;
    std::getline(file, line);

    while(std::getline(file, line)){
        if(line.empty()) continue;

        tradeEvent row;
        size_t start = 0;
        size_t end = 0;

        auto next_token = [&](){
            std::string token = line.substr(start, end - start);
            start = end + 1;
            return token;
        };

        row.timestamp = std::stoull(next_token());
        row.price = std::stod(next_token());
        row.size = std::stod(next_token());
        row.is_buyer_maker = (next_token() == "1");
        row.asset_id = next_token();
        row.maker_id = next_token();
        row.taker_id = line.substr(start); 
        trades.push_back(std::move(row));
    }
    return trades;
}