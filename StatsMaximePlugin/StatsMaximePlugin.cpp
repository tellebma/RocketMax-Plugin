#include "pch.h"
#include "StatsMaximePlugin.h"
#include <iostream>
#include "httplib.h"
#include "calc.h"
#include <iostream>
#include <map>
#include <chrono>
#include <thread>

//#define HOOK_MATCH_ENDED "Function TAGame.GameEvent_Soccar_TA.EventMatchEnded"

#define HOOK_MATCH_START "Function GameEvent_TA.Countdown.BeginState"
#define HOOK_MATCH_ENDED "Function TAGame.GameEvent_Soccar_TA.OnMatchWinnerSet"
#define HOOK_GAME_DESTORYED "Function TAGame.GameEvent_TA.Destroyed"

//#define API_ENDPOINT "http://localhost:5000"
#define API_ENDPOINT "http://historl.tellebma.fr"

httplib::Client client(API_ENDPOINT);

using namespace std::this_thread; // sleep_for, sleep_until
using namespace std::chrono; // nanoseconds, system_clock, seconds

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
    bool erreur = initAPI();
    if (!erreur) {
        gameWrapper->HookEvent(HOOK_MATCH_START, std::bind(&StatsMaximePlugin::gameStart, this, std::placeholders::_1));
        LOG("[StatsMaximePlugin] Plugin loaded!")
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
    gameWrapper->UnhookEventPost(HOOK_MATCH_START);
	LOG("[StatsMaximePlugin] Plugin unloaded");
}

void StatsMaximePlugin::gameHasEnded()
{
    // remove hook end game detected
    // add hook start game
    gameWrapper->UnhookEventPost(HOOK_MATCH_ENDED);
    gameWrapper->UnhookEventPost(HOOK_GAME_DESTORYED);
    gameWrapper->HookEvent(HOOK_MATCH_START, std::bind(&StatsMaximePlugin::gameStart, this, std::placeholders::_1));
    my_team_num = -1;
    mmr_avant_match = 0;
    mmr_apres_match = 0;
    mmr_gagne = 0;
    playlistId = 100;
    victory = false;
    rage_quit = false;
    return;
}

void StatsMaximePlugin::gameHasBegun()
{
    // remove hook end game detected
    // add hook start game
    gameWrapper->HookEvent(HOOK_MATCH_ENDED, std::bind(&StatsMaximePlugin::gameEnd, this, std::placeholders::_1)); 
    gameWrapper->HookEvent(HOOK_GAME_DESTORYED, std::bind(&StatsMaximePlugin::gameDestroyed, this, std::placeholders::_1));
    gameWrapper->UnhookEventPost(HOOK_MATCH_START);
    return;
}

int StatsMaximePlugin::getMmrData(int gamemode)
{
    LOG("[StatsMaximePlugin] [GetMmrData] GMode -" + std::to_string(gamemode));
    MMRWrapper mmrw = gameWrapper->GetMMRWrapper();
    int mmr = (int)mmrw.GetPlayerMMR(playerIdWrapper, gamemode);
    LOG("[StatsMaximePlugin] [GetMmrData] base  - " + std::to_string(mmr));
    return mmr;
}

int StatsMaximePlugin::getCurentPlaylist()
{
    ServerWrapper sw = gameWrapper->GetCurrentGameState();
    if (!sw) return 100;
    GameSettingPlaylistWrapper playlist = sw.GetPlaylist();
    if (!playlist) return 100;
    int playlistId = playlist.GetPlaylistId();
    LOG("[StatsMaximePlugin] [getCurentPlaylist] playlistId: " + std::to_string(playlistId));
    return playlistId;
}

bool StatsMaximePlugin::isRankedGame()
{
    return gameWrapper->IsInOnlineGame() && !gameWrapper->IsInReplay() && !gameWrapper->IsInFreeplay();
}

bool StatsMaximePlugin::sendMmrUpdate(long long timestamp)
{
    LOG("[StatsMaximePlugin] [sendMmrUpdate] -- SENDING DATA --");
    LOG("[StatsMaximePlugin] [sendMmrUpdate]  Player ID     :" + playerId);
    LOG("[StatsMaximePlugin] [sendMmrUpdate]  Gamemode ID   :" + std::to_string(playlistId));
    LOG("[StatsMaximePlugin] [sendMmrUpdate]  Gamemode NAME :" + gameModes[playlistId]);
    LOG("[StatsMaximePlugin] [sendMmrUpdate]  MMR           :" + std::to_string(mmr_apres_match));

    
    std::string data = R"({"player_id": ")" + playerId +
        R"(", "timestamp": ")" + std::to_string(timestamp) +
        R"(", "elo": ")" + std::to_string(mmr_apres_match) +
        R"(", "gamemode": ")" + gameModes[playlistId] +
        R"("})";
    
    
    auto res = client.Post("/sendPlayerData", data, "application/json");
    // Vérifier si la requête a réussi
    if (res && res->status == 200) {
        LOG("[StatsMaximePlugin] [sendMmrUpdate] DATA SENT");
    }
    else {
        LOG("[StatsMaximePlugin] [sendMmrUpdate] ERROR DATA NOT SENT");
        return true;
    }
    return false;
}

bool StatsMaximePlugin::initAPI()
{
    // Obtenez l'identifiant unique du joueur sous forme de chaîne
    playerIdWrapper = gameWrapper->GetUniqueID();
    playerId = std::to_string(playerIdWrapper.GetUID());
    // Obtenez le nom du joueur
    playerName = gameWrapper->GetPlayerName().ToString();

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


void StatsMaximePlugin::gameStart(std::string eventName)
{
    if (!isRankedGame())
        return;

    LOG("===== GameStart =====");

    CarWrapper me = gameWrapper->GetLocalCar();
    if (me.IsNull())
        return;

    PriWrapper mePRI = me.GetPRI();
    if (mePRI.IsNull())
        return;

    TeamInfoWrapper myTeam = mePRI.GetTeam();
    if (myTeam.IsNull())
        return;

    // Get TeamNum
    my_team_num = myTeam.GetTeamNum();

    playlistId = getCurentPlaylist();
    if (playlistId == 100) {
        LOG("MODE DE JEU INCONNU");
        return;
        // playlistId = 11;
    }
    mmr_avant_match = getMmrData(playlistId);
    
    // set new hooks
    gameHasBegun();

    LOG("===== !GameStart =====");
}

void StatsMaximePlugin::gameEnd(std::string eventName)
{
    if (!isRankedGame() || my_team_num == -1)
        return;
    LOG("GameEnd => is_online_game: yes my_team_num:" + std::to_string(my_team_num));
    
    if (my_team_num != -1)
    {
        LOG("===== GameEnd =====");
        ServerWrapper server = gameWrapper->GetOnlineGame();
        TeamWrapper winningTeam = server.GetGameWinner();
        if (winningTeam.IsNull())
            return;

        mmr_apres_match = getMmrData(playlistId);
        mmr_gagne = mmr_avant_match - mmr_apres_match;
        int win_team_num = winningTeam.GetTeamNum();

        LOG("GameEnd => my_team_num:" + std::to_string(my_team_num) + " GetTeamNum:" + std::to_string(win_team_num));
        if (my_team_num == win_team_num)
        {
            LOG("===== Game Won =====");
            victory = true;
        }
        else
        {
            LOG("===== Game Lost =====");
            victory = false;
        }

        long long timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        sendMmrUpdate(timestamp);
        rage_quit = false;
        sendHistoriqueGame(timestamp);
        gameHasEnded();
        LOG("===== !GameEnd =====");
    }
}

void StatsMaximePlugin::gameDestroyed(std::string eventName)
{
    LOG("===== GameDestroyed =====");
    // compter comme perdu
    // ??? Jsp du tout si ca fonctionne ...

    long long timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    mmr_apres_match = getMmrData(playlistId);
    mmr_gagne = mmr_avant_match - mmr_apres_match;
    sendMmrUpdate(timestamp);
    victory = false;
    rage_quit = true;
    sendHistoriqueGame(timestamp);
    gameHasEnded();
    
    LOG("===== !GameDestroyed =====");
}


bool StatsMaximePlugin::sendHistoriqueGame(long long timestamp)
{
    
    LOG("[StatsMaximePlugin] [sendHistoriqueGame]  MMR GAGNE     :" + std::to_string(mmr_gagne));
    LOG("[StatsMaximePlugin] [sendHistoriqueGame]  victory ?     :" + std::to_string(victory));
    LOG("[StatsMaximePlugin] [sendHistoriqueGame]  timestamp     :" + std::to_string(timestamp));

    std::string data = R"({"player_id": ")" + playerId +
        R"(", "timestamp": ")" + std::to_string(timestamp) +
        R"(", "victory": )" + std::to_string(victory) +
        R"(", "rage_quit": )" + std::to_string(rage_quit) +
        R"(", "elo_won": ")" + std::to_string(mmr_gagne) +
        R"(", "gamemode": ")" + gameModes[playlistId] +
        R"("})";


    auto res = client.Post("/historique", data, "application/json");
    // Vérifier si la requête a réussi
    if (res && res->status == 200) {
        LOG("[StatsMaximePlugin] [sendHistoriqueGame] DATA SENT");
    }
    else {
        LOG("[StatsMaximePlugin] [sendHistoriqueGame] ERROR DATA NOT SENT");
        return true;
    }
    return false;
}