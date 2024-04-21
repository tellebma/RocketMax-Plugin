#include "pch.h"
#include "StatsMaximePlugin.h"
#include <iostream>
#include "httplib.h"
#include "calc.h"
#include <iostream>
#include <map>


#define HOOK_MATCH_ENDED "Function TAGame.GameEvent_Soccar_TA.EventMatchEnded"
#define API_ENDPOINT "http://localhost:5000"
httplib::Client client(API_ENDPOINT);



BAKKESMOD_PLUGIN(StatsMaximePlugin, "StatMaxime", plugin_version, PERMISSION_ALL)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;
enum GameMode {
    Unknown = -1337,
    Casual = 0,
    Duel = 1,
    Doubles = 2,
    Standard = 3,
    Chaos = 4,
    PrivateMatch = 6,
    Season = 7,
    OfflineSplitscreen = 8,
    Training = 9,
    RankedSoloDuel = 10,
    RankedTeamDoubles = 11,
    RankedStandard = 13,
    SnowDayPromotion = 15,
    Experimental = 16,
    BasketballDoubles = 17,
    Rumble = 18,
    Workshop = 19,
    UGCTrainingEditor = 20,
    UGCTraining = 21,
    Tournament = 22,
    Breakout = 23,
    TenthAnniversary = 25,
    FaceIt = 26,
    RankedBasketballDoubles = 27,
    RankedRumble = 28,
    RankedBreakout = 29,
    RankedSnowDay = 30,
    HauntedBall = 31,
    BeachBall = 32,
    Rugby = 33,
    AutoTournament = 34,
    RocketLabs = 35,
    RumShot = 37,
    GodBall = 38,
    CoopVsAI = 40,
    BoomerBall = 41,
    GodBallDoubles = 43,
    SpecialSnowDay = 44,
    Football = 46,
    Cubic = 47,
    TacticalRumble = 48,
    SpringLoaded = 49,
    SpeedDemon = 50,
    RumbleBM = 52,
    Knockout = 54,
    Thirdwheel = 55,
    MagnusFutball = 62
};

std::map<int, std::string> gameModes = {
    {Unknown, "Unknown"},
    {Casual, "Casual"},
    {Duel, "Duel"},
    {Doubles, "Doubles"},
    {Standard, "Standard"},
    {Chaos, "Chaos"},
    {PrivateMatch, "PrivateMatch"},
    {Season, "Season"},
    {OfflineSplitscreen, "OfflineSplitscreen"},
    {Training, "Training"},
    {RankedSoloDuel, "RankedSoloDuel"},
    {RankedTeamDoubles, "RankedTeamDoubles"},
    {RankedStandard, "RankedStandard"},
    {SnowDayPromotion, "SnowDayPromotion"},
    {Experimental, "Experimental"},
    {BasketballDoubles, "BasketballDoubles"},
    {Rumble, "Rumble"},
    {Workshop, "Workshop"},
    {UGCTrainingEditor, "UGCTrainingEditor"},
    {UGCTraining, "UGCTraining"},
    {Tournament, "Tournament"},
    {Breakout, "Breakout"},
    {TenthAnniversary, "TenthAnniversary"},
    {FaceIt, "FaceIt"},
    {RankedBasketballDoubles, "RankedBasketballDoubles"},
    {RankedRumble, "RankedRumble"},
    {RankedBreakout, "RankedBreakout"},
    {RankedSnowDay, "RankedSnowDay"},
    {HauntedBall, "HauntedBall"},
    {BeachBall, "BeachBall"},
    {Rugby, "Rugby"},
    {AutoTournament, "AutoTournament"},
    {RocketLabs, "RocketLabs"},
    {RumShot, "RumShot"},
    {GodBall, "GodBall"},
    {CoopVsAI, "CoopVsAI"},
    {BoomerBall, "BoomerBall"},
    {GodBallDoubles, "GodBallDoubles"},
    {SpecialSnowDay, "SpecialSnowDay"},
    {Football, "Football"},
    {Cubic, "Cubic"},
    {TacticalRumble, "TacticalRumble"},
    {SpringLoaded, "SpringLoaded"},
    {SpeedDemon, "SpeedDemon"},
    {RumbleBM, "RumbleBM"},
    {Knockout, "Knockout"},
    {Thirdwheel, "Thirdwheel"},
    {MagnusFutball, "MagnusFutball"}
};


void StatsMaximePlugin::onLoad()
{
	_globalCvarManager = cvarManager;
    std::string uniquePlayerId = std::to_string(gameWrapper->GetUniqueID().GetUID());
    bool erreur = InitAPI();
    if (!erreur) {
        gameWrapper->HookEvent(HOOK_MATCH_ENDED, std::bind(&StatsMaximePlugin::OnMatchEnded, this, std::placeholders::_1));
        LOG("[StatsMaximePlugin] Plugin loaded!");
        return;
    }
    LOG("[StatsMaximePlugin] ERREUR LORS DU CHARGEMENT DU PLUGIN (SERVEUR HS?!)");
    LOG("[StatsMaximePlugin] ERREUR LORS DU CHARGEMENT DU PLUGIN (SERVEUR HS?!)");
    LOG("[StatsMaximePlugin] ERREUR LORS DU CHARGEMENT DU PLUGIN (SERVEUR HS?!)");
    LOG("[StatsMaximePlugin] ERREUR LORS DU CHARGEMENT DU PLUGIN (SERVEUR HS?!)");
    LOG("[StatsMaximePlugin] ERREUR LORS DU CHARGEMENT DU PLUGIN (SERVEUR HS?!)");
}

void StatsMaximePlugin::onUnload()
{
    gameWrapper->UnhookEventPost(HOOK_MATCH_ENDED);
	LOG("[StatsMaximePlugin] Plugin unloaded");
}
float StatsMaximePlugin::GetMmrData(int gamemode) 
{
    LOG("[StatsMaximePlugin] [GetMmrData] GMode -" + std::to_string(gamemode));
    MMRWrapper mmrw = gameWrapper->GetMMRWrapper();
    float mmr = mmrw.GetPlayerMMR(uniquePlayerId, gamemode);
    LOG("[StatsMaximePlugin] [GetMmrData] base  - " + std::to_string(mmr));
    SkillRating mmr2 = mmrw.GetPlayerSkillRating(uniquePlayerId, gamemode);
    LOG("[StatsMaximePlugin] [GetMmrData] Mu    - " + std::to_string(mmr2.Mu));
    LOG("[StatsMaximePlugin] [GetMmrData] Sigma - " + std::to_string(mmr2.Sigma));
    SkillRank mmr3 = mmrw.GetPlayerRank(uniquePlayerId, gamemode);
    LOG("[StatsMaximePlugin] [GetMmrData] Tier  - " + std::to_string(mmr3.Tier));
    LOG("[StatsMaximePlugin] [GetMmrData] Div   - " + std::to_string(mmr3.Division));
    LOG("[StatsMaximePlugin] [GetMmrData] Mplay - " + std::to_string(mmr3.MatchesPlayed));
    bool mmr4 = mmrw.IsRanked(gamemode);
    LOG("[StatsMaximePlugin] [GetMmrData] rank? - " + std::to_string(mmr4));
    return mmr;
}

int StatsMaximePlugin::getCurentPlaylist()
{
    ServerWrapper sw = gameWrapper->GetCurrentGameState();
    if (!sw) return 100;
    GameSettingPlaylistWrapper playlist = sw.GetPlaylist();
    if (!playlist) return 100;
    int playlistID = playlist.GetPlaylistId();
    LOG("[StatsMaximePlugin] [getCurentPlaylist] PlaylistId: " + std::to_string(playlistID));
    return playlistID;
}


void StatsMaximePlugin::OnMatchEnded(std::string eventName)
{
    if (!IsRankedGame())
        return;

    LOG("[StatsMaximePlugin] [OnMatchEnded] Triggered");
    LOG("[StatsMaximePlugin] [OnMatchEnded] eventName " + eventName);
    
    int playlistID = getCurentPlaylist();
    if (playlistID == 100) {
        LOG("[StatsMaximePlugin] [OnMatchEnded] REcuperation du gamemode erreur ("+ std::to_string(playlistID)+")");
        return;
    }
    float mmr = GetMmrData(playlistID);
    
    
    MMRWrapper mmrw = gameWrapper->GetMMRWrapper();
    int gamemode = mmrw.GetCurrentPlaylist();
    GetMmrData(gamemode);

    long long timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    
    SendDataToAPI(timestamp, mmr, gamemode);
}

bool StatsMaximePlugin::IsRankedGame()
{
    return gameWrapper->IsInOnlineGame() && !gameWrapper->IsInReplay() && !gameWrapper->IsInFreeplay();
}

void StatsMaximePlugin::SendDataToAPI(long long timestamp, float mmr, int gameModeId)
{
    std::string playerid = std::to_string(gameWrapper->GetUniqueID().GetUID()); 

    LOG("[StatsMaximePlugin] [SendDataToAPI] -- SENDING DATA --");
    LOG("[StatsMaximePlugin] [SendDataToAPI]  Player ID     :" + playerid);
    LOG("[StatsMaximePlugin] [SendDataToAPI]  Gamemode ID   :" + std::to_string(gameModeId)); 
    LOG("[StatsMaximePlugin] [SendDataToAPI]  Gamemode NAME :" + gameModes[gameModeId]);
    LOG("[StatsMaximePlugin] [SendDataToAPI]  MMR           :" + std::to_string(mmr));
    
    std::string data = R"({"player_id": ")" + playerid +
        R"(", "timestamp": ")" + std::to_string(timestamp) +
        R"(", "elo": ")" + std::to_string(mmr) +
        R"(", "gamemode": ")" + gameModes[gameModeId] +
        R"("})";
    
    
    auto res = client.Post("/sendPlayerData", data, "application/json");
    // Vérifier si la requête a réussi
    if (res && res->status == 200) {
        LOG("[StatsMaximePlugin] [SendDataToAPI] DATA SENT");
    }
    else {
        LOG("[StatsMaximePlugin] [SendDataToAPI] ERROR DATA NOT SENT");
    }

}

bool StatsMaximePlugin::InitAPI()
{
    // Obtenez l'identifiant unique du joueur sous forme de chaîne
    std::string playerId = std::to_string(gameWrapper->GetUniqueID().GetUID());
    // Obtenez le nom du joueur
    std::string playerName = gameWrapper->GetPlayerName().ToString();

    // Construisez la chaîne JSON avec l'identifiant du joueur
    std::string data = R"({"player_id": ")" + playerId +
        R"(", "player_name": ")" + playerName + R"("})";
    LOG("[StatsMaximePlugin] [InitAPI] " + playerId);
    LOG("[StatsMaximePlugin] [InitAPI] " + playerName);
    auto res = client.Post("/initPlayer", data, "application/json");
    // Vérifier si la requête a réussi
    if (res && res->status == 200) {
        LOG("[StatsMaximePlugin] [InitAPI] DATA SENT");
    }
    else {
        LOG("[StatsMaximePlugin] [InitAPI] ERROR DATA NOT SENT");
        return true;
    }
    return false;
}
