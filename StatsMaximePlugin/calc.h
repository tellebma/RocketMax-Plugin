#pragma once

#include <string>
#include <vector>
#include "bakkesmod/plugin/bakkesmodplugin.h"

struct PlayerInfo {
    UniqueIDWrapper playerId;
    std::string playerName;
    float playerMMR;
};

class Calc {
public:
    static std::string PlayerInfoToString(std::vector<PlayerInfo> playerInfo);
};
