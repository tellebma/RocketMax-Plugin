#include "pch.h"
#include "StatsMaximePlugin.h"
#include <iostream>
#include "httplib.h"
#include "calc.h"
#include <iostream>
#include <map>
#include <chrono>


#define HOOK_MATCH_ENDED "Function TAGame.GameEvent_Soccar_TA.EventMatchEnded"
#define HOOK_MATCH_START "Function GameEvent_TA.Countdown.BeginState"

//#define HOOK_MATCH_ENDED "Function TAGame.GameEvent_Soccar_TA.OnMatchWinnerSet"
//#define HOOK_GAME_DESTORYED "Function TAGame.GameEvent_TA.Destroyed"

#define API_ENDPOINT "http://localhost:5000"
// #define API_ENDPOINT "http://historl.tellebma.fr"

httplib::Client client(API_ENDPOINT);

using namespace std::this_thread; // sleep_for, sleep_until
using namespace std::chrono; // nanoseconds, system_clock, seconds

BAKKESMOD_PLUGIN(StatsMaximePlugin, "StatMaxime", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;
enum GameMode {
    RankedSoloDuel = 10,
    RankedTeamDoubles = 11,
    RankedStandard = 13,
    Tournament = 22,
    RankedBasketballDoubles = 27,
    RankedRumble = 28,
    RankedBreakout = 29,
    RankedSnowDay = 30,
    AutoTournament = 34,
};

std::map<int, std::string> gameModes = {
    {RankedSoloDuel, "Ranked Solo"},
    {RankedTeamDoubles, "Ranked Duo"},
    {RankedStandard, "Ranked Standard"},
    {Tournament, "Tournament"},
    {RankedBasketballDoubles, "Ranked Basketball Doubles"},
    {RankedRumble, "Ranked Rumble"},
    {RankedBreakout, "Ranked Breakout"},
    {RankedSnowDay, "Ranked SnowDay"},
    {AutoTournament, "Auto Tournament"}
};


void StatsMaximePlugin::onLoad()
{
	_globalCvarManager = cvarManager;
    bool erreur = initAPI();
    if (!erreur) {
        gameWrapper->HookEvent(HOOK_MATCH_START, std::bind(&StatsMaximePlugin::gameStart, this, std::placeholders::_1));
        gameWrapper->HookEvent(HOOK_MATCH_ENDED, std::bind(&StatsMaximePlugin::gameEnd, this, std::placeholders::_1));
        notifierToken = gameWrapper->GetMMRWrapper().RegisterMMRNotifier(
            [this](UniqueIDWrapper id) {
                if (!game_running)return;
                if (id != playerIdWrapper)return;
                mmr_player_updated = true;
            }
        );
        
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
    gameWrapper->UnhookEventPost(HOOK_MATCH_ENDED);
	LOG("[StatsMaximePlugin] Plugin unloaded");
}

void StatsMaximePlugin::gameHasEnded()
{
    // remove hook end game detected
    // add hook start game
    
    //gameWrapper->UnhookEventPost(HOOK_MATCH_ENDED);

    long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    sendMmrUpdate(timestamp);
    sendHistoriqueGame(timestamp);

    //gameWrapper->HookEvent(HOOK_MATCH_START, std::bind(&StatsMaximePlugin::gameStart, this, std::placeholders::_1));
    game_running = 0;
    mmr_player_updated = false;
    my_team_num = -1;
    mmr_avant_match = 0;
    mmr_apres_match = 0;
    mmr_gagne = 0;
    playlistId = 100;
    victory = false;
    return;
}

void StatsMaximePlugin::gameHasBegun()
{
    // remove hook end game detected
    // add hook start game
    //gameWrapper->UnhookEventPost(HOOK_MATCH_START);
    //gameWrapper->HookEvent(HOOK_MATCH_ENDED, std::bind(&StatsMaximePlugin::gameEnd, this, std::placeholders::_1)); 
    
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
    if (!sw) return -1;
    GameSettingPlaylistWrapper playlist = sw.GetPlaylist();
    if (!playlist) return -1;
    int playlistId = playlist.GetPlaylistId();
    LOG("[StatsMaximePlugin] [getCurentPlaylist] playlistId: " + std::to_string(playlistId)+ " - Gamemode: " + gameModes[playlistId]);
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

    CurlRequest req;
    req.url = std::string(API_ENDPOINT) + "/updateMmr";
    req.body = R"({"player_id": ")" + playerId +
        R"(", "timestamp": ")" + std::to_string(timestamp) +
        R"(", "mmr": )" + std::to_string(mmr_apres_match) +
        R"(, "gamemode_id": )" + std::to_string(playlistId) +
        R"(})";
    
    
    HttpWrapper::SendCurlJsonRequest(req, [this](int code, std::string result)
    {
        LOG("Json result{}", result);
        if (code == 200) {
            LOG("[StatsMaximePlugin] [sendMmrUpdate] DATA SENT");
        }
        else {
            LOG("[StatsMaximePlugin] [sendMmrUpdate] ERROR DATA NOT SENT");
            return true;
        }
       
    });
    return false;
}

bool StatsMaximePlugin::initAPI()
{
    // Obtenez l'identifiant unique du joueur sous forme de chaîne
    playerIdWrapper = gameWrapper->GetUniqueID();
    playerId = std::to_string(playerIdWrapper.GetUID());
    // Obtenez le nom du joueur
    playerName = gameWrapper->GetPlayerName().ToString();

    LOG("[StatsMaximePlugin] [InitAPI] " + playerId);
    LOG("[StatsMaximePlugin] [InitAPI] " + playerName);

    CurlRequest req;
    req.url = std::string(API_ENDPOINT) + "/initPlayer";
    req.body = R"({"player_id": ")" + playerId +
        R"(", "player_name": ")" + playerName + R"("})";


    HttpWrapper::SendCurlJsonRequest(req, [this](int code, std::string result)
        {
            LOG("Json result{}", result);
            if (code == 200) {
                LOG("[StatsMaximePlugin] [InitAPI] DATA SENT");
            }
            else {
                LOG("[StatsMaximePlugin] [InitAPI] ERROR DATA NOT SENT");
                return true;
            }
        });
    return false;

}


void StatsMaximePlugin::gameStart(std::string eventName)
{
    if (game_running == 1)return;
    if (!isRankedGame())return;

    playlistId = getCurentPlaylist();
    if (playlistId == -1)return;
    
    LOG("MODE DE JEU :" + std::to_string(playlistId));

    LOG("===== GameStart =====");

    CarWrapper me = gameWrapper->GetLocalCar();
    if (me.IsNull())return;

    PriWrapper mePRI = me.GetPRI();
    if (mePRI.IsNull())return;

    TeamInfoWrapper myTeam = mePRI.GetTeam();
    if (myTeam.IsNull())return;

    // Get TeamNum
    my_team_num = myTeam.GetTeamNum();

    mmr_avant_match = getMmrData(playlistId);

    game_running = 1;
    // set new hooks
    gameHasBegun();

    LOG("===== !GameStart =====");
}

void StatsMaximePlugin::gameEnd(std::string eventName)
{
    if (game_running != 1)return;
    
    game_running = 0;
    LOG("GameEnd => is_online_game: yes my_team_num:" + std::to_string(my_team_num));
    
    if (my_team_num != -1)
    {
        LOG("===== GameEnd =====");
        ServerWrapper server = gameWrapper->GetOnlineGame();
        TeamWrapper winningTeam = server.GetGameWinner();
        if (winningTeam.IsNull())return;
        int millisecondes_waited = 0;
        while (mmr_player_updated || millisecondes_waited >= 30000)
        {
            Sleep(10); // 10 milisecondes 
            millisecondes_waited += 10;
        }

        mmr_apres_match = getMmrData(playlistId);
        mmr_gagne = mmr_apres_match - mmr_avant_match;
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

        gameHasEnded();
        LOG("===== !GameEnd =====");
    }
}

void StatsMaximePlugin::gameDestroyed(std::string eventName)
{
    if (game_running != 1)return;
    
    game_running = 0;

    LOG("===== GameDestroyed =====");
    // compter comme perdu ??
    // peut arrviver si le joueur rage quit ou si le serveur va jusqu'a sa fin de vie
    mmr_apres_match = getMmrData(playlistId);
    mmr_gagne = mmr_avant_match - mmr_apres_match;
    victory = false;
    gameHasEnded();
    LOG("===== !GameDestroyed =====");
}


bool StatsMaximePlugin::sendHistoriqueGame(long long timestamp)
{
    // TODO https://bakkesmodwiki.github.io/code_snippets/using_http_wrapper/#perform-an-https-json-request
    LOG("[StatsMaximePlugin] [sendHistoriqueGame]  MMR GAGNE     :" + std::to_string(mmr_gagne));
    LOG("[StatsMaximePlugin] [sendHistoriqueGame]  victory ?     :" + std::to_string(victory));
    LOG("[StatsMaximePlugin] [sendHistoriqueGame]  timestamp     :" + std::to_string(timestamp));

    CurlRequest req;
    req.url = std::string(API_ENDPOINT) + "/updateHistorique";
    req.body = R"({"player_id": ")" + playerId +
        R"(", "timestamp": ")" + std::to_string(timestamp) +
        R"(", "victory": )" + std::to_string(victory) +
        R"(, "mmr_won": )" + std::to_string(mmr_gagne) +
        R"(, "gamemode_id": )" + std::to_string(playlistId) +
        R"(})";


    HttpWrapper::SendCurlJsonRequest(req, [this](int code, std::string result)
        {
            LOG("Json result{}", result);
            if (code == 200) {
                LOG("[StatsMaximePlugin] [sendHistoriqueGame] DATA SENT");
            }
            else {
                LOG("[StatsMaximePlugin] [sendHistoriqueGame] ERROR DATA NOT SENT");
                return true;
            }

        });
    return false;
}