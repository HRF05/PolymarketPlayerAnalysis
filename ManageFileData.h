#pragma once
#include "Models.h"
#include <unordered_map>
#include <string>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <vector>

namespace ManageFileData{
    void usersFileAdd(const std::unordered_map<std::string, UserAnalysisResult> &user_credible, const std::string &user_filename);
    std::unordered_map<std::string, UserAnalysisResult> usersFileGet(const std::string &user_filename);
    std::vector<tradeEvent> marketFileGet(const std::string& asset_id);
    void marketFileAdd(const std::vector<tradeEvent>& trades, const std::string& filename);
};