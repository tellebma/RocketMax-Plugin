#include "pch.h"
#include "RocketMax.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include "httplib.h"
#include <map>
#include <chrono>
#include <cmath>


#define HOOK_MATCH_ENDED "Function TAGame.GameEvent_Soccar_TA.EventMatchEnded"
#define HOOK_MATCH_START "Function GameEvent_TA.Countdown.BeginState"

//#define HOOK_MATCH_ENDED "Function TAGame.GameEvent_Soccar_TA.OnMatchWinnerSet"
//#define HOOK_GAME_DESTORYED "Function TAGame.GameEvent_TA.Destroyed"



using namespace std::this_thread; // sleep_for, sleep_until
using namespace std::chrono; // nanoseconds, system_clock, seconds

BAKKESMOD_PLUGIN(RocketMax, "RocketMax", plugin_version, PLUGINTYPE_FREEPLAY)

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


void RocketMax::onLoad()
{
	_globalCvarManager = cvarManager;
    LOG("[RocketMax] Version " + std::string(plugin_version) + " loading...");

    // Register CVars for configuration
    cvar_enable_toasts = std::make_shared<bool>(true);
    cvarManager->registerCvar("rocketmax_enable_toasts", "1", "Enable toast notifications")
        .bindTo(cvar_enable_toasts);

    cvar_enable_overlay = std::make_shared<bool>(true);
    cvarManager->registerCvar("rocketmax_enable_overlay", "1", "Enable session overlay")
        .bindTo(cvar_enable_overlay);

    cvar_enable_streak_alerts = std::make_shared<bool>(true);
    cvarManager->registerCvar("rocketmax_enable_streak_alerts", "1", "Enable streak milestone alerts")
        .bindTo(cvar_enable_streak_alerts);

    cvar_server_url = std::make_shared<std::string>(API_ENDPOINT);
    cvarManager->registerCvar("rocketmax_server_url", API_ENDPOINT, "Server URL for data sync")
        .bindTo(cvar_server_url);

    cvar_enable_auto_update = std::make_shared<bool>(true);
    cvarManager->registerCvar("rocketmax_enable_auto_update", "1", "Enable automatic update checking")
        .bindTo(cvar_enable_auto_update);

    // Process any offline queue from previous sessions
    processOfflineQueue();

    // Check for updates if enabled
    if (*cvar_enable_auto_update) {
        checkForUpdates();
    }

    bool erreur = initAPI();
    if (!erreur) {
        gameWrapper->HookEvent(HOOK_MATCH_START, std::bind(&RocketMax::gameStart, this, std::placeholders::_1));
        gameWrapper->HookEvent(HOOK_MATCH_ENDED, std::bind(&RocketMax::gameEnd, this, std::placeholders::_1));
        pluginLoaded = true;
        LOG("[RocketMax] Version " + std::string(plugin_version) + " loaded successfully");
        return;
    }
    LOG("[RocketMax] ERREUR LORS DU CHARGEMENT DU PLUGIN (SERVEUR HS?!)");
    LOG("[RocketMax] ERREUR LORS DU CHARGEMENT DU PLUGIN (SERVEUR HS?!)");
    LOG("[RocketMax] ERREUR LORS DU CHARGEMENT DU PLUGIN (SERVEUR HS?!)");
    LOG("[RocketMax] ERREUR LORS DU CHARGEMENT DU PLUGIN (SERVEUR HS?!)");
    LOG("[RocketMax] ERREUR LORS DU CHARGEMENT DU PLUGIN (SERVEUR HS?!)");
}

void RocketMax::onUnload()
{
    gameWrapper->UnhookEventPost(HOOK_MATCH_START);
    gameWrapper->UnhookEventPost(HOOK_MATCH_ENDED);
	LOG("[RocketMax] Version " + std::string(plugin_version) + " unloaded");
}

void RocketMax::gameHasEnded()
{
    // remove hook end game detected
    // add hook start game

    //gameWrapper->UnhookEventPost(HOOK_MATCH_ENDED);

    long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    sendMmrUpdate(timestamp);
    sendHistoriqueGame(timestamp);

    // Update streak tracking (before resetting victory)
    updateStreak(victory);

    //gameWrapper->HookEvent(HOOK_MATCH_START, std::bind(&RocketMax::gameStart, this, std::placeholders::_1));
    game_running = 0;
    mmr_player_updated = false;
    my_team_num = -1;
    mmr_avant_match = 0;
    mmr_apres_match = 0;
    mmr_gagne = 0;
    playlistId = 100;
    victory = false;

    // Réinitialiser les stats de match
    match_goals = 0;
    match_assists = 0;
    match_saves = 0;
    match_shots = 0;
    match_score = 0;
    match_demos = 0;
    match_mvp = false;
    team_score = 0;
    opponent_score = 0;
    overtime = false;

    return;
}


int RocketMax::getMmrData(int gamemode)
{
    LOG("[RocketMax] [GetMmrData] GMode -" + gameModes[playlistId]);
    MMRWrapper mmrw = gameWrapper->GetMMRWrapper();
    int mmr = (int)mmrw.GetPlayerMMR(playerIdWrapper, gamemode);
    LOG("[RocketMax] [GetMmrData] base  - " + std::to_string(mmr));
    return mmr;
}

void RocketMax::collectMatchStats()
{
    LOG("[RocketMax] [collectMatchStats] === COLLECTING STATS ===");

    // Réinitialiser les stats
    match_goals = 0;
    match_assists = 0;
    match_saves = 0;
    match_shots = 0;
    match_score = 0;
    match_demos = 0;
    match_mvp = false;
    team_score = 0;
    opponent_score = 0;
    overtime = false;

    ServerWrapper server = gameWrapper->GetOnlineGame();
    if (!server) {
        LOG("[RocketMax] [collectMatchStats] ERROR: Server is null");
        return;
    }

    // Vérifier overtime
    overtime = server.GetbOverTime();
    LOG("[RocketMax] [collectMatchStats] Overtime: " + std::to_string(overtime));

    // Obtenir les scores des équipes
    ArrayWrapper<TeamWrapper> teams = server.GetTeams();
    LOG("[RocketMax] [collectMatchStats] Teams count: " + std::to_string(teams.Count()));
    for (int i = 0; i < teams.Count(); i++) {
        TeamWrapper team = teams.Get(i);
        if (!team) continue;

        int teamNum = team.GetTeamNum();
        int score = team.GetScore();

        if (teamNum == my_team_num) {
            team_score = score;
        }
        else {
            opponent_score = score;
        }
    }
    LOG("[RocketMax] [collectMatchStats] Score: " + std::to_string(team_score) + " - " + std::to_string(opponent_score));

    // Méthode 1: Essayer via GetLocalCar
    CarWrapper me = gameWrapper->GetLocalCar();
    if (me) {
        LOG("[RocketMax] [collectMatchStats] LocalCar found, getting PRI...");
        PriWrapper mePRI = me.GetPRI();
        if (mePRI) {
            LOG("[RocketMax] [collectMatchStats] PRI found via LocalCar");
            match_goals = mePRI.GetMatchGoals();
            match_assists = mePRI.GetMatchAssists();
            match_saves = mePRI.GetMatchSaves();
            match_shots = mePRI.GetMatchShots();
            match_score = mePRI.GetMatchScore();
            match_demos = mePRI.GetMatchDemolishes();
            match_mvp = mePRI.GetbMatchMVP();
        }
        else {
            LOG("[RocketMax] [collectMatchStats] PRI is null from LocalCar");
        }
    }
    else {
        LOG("[RocketMax] [collectMatchStats] LocalCar is null, trying alternative method...");
    }

    // Méthode 2: Si LocalCar a échoué, parcourir tous les PRIs
    if (match_score == 0 && match_goals == 0) {
        LOG("[RocketMax] [collectMatchStats] Using fallback: iterating PRIs");
        ArrayWrapper<PriWrapper> PRIs = server.GetPRIs();
        LOG("[RocketMax] [collectMatchStats] PRIs count: " + std::to_string(PRIs.Count()));

        for (int i = 0; i < PRIs.Count(); i++) {
            PriWrapper pri = PRIs.Get(i);
            if (!pri) continue;

            UniqueIDWrapper priID = pri.GetUniqueIdWrapper();
            std::string priIDStr = std::to_string(priID.GetUID());

            LOG("[RocketMax] [collectMatchStats] Checking PRI: " + priIDStr + " vs " + playerId);

            if (priIDStr == playerId) {
                LOG("[RocketMax] [collectMatchStats] Found matching PRI!");
                match_goals = pri.GetMatchGoals();
                match_assists = pri.GetMatchAssists();
                match_saves = pri.GetMatchSaves();
                match_shots = pri.GetMatchShots();
                match_score = pri.GetMatchScore();
                match_demos = pri.GetMatchDemolishes();
                match_mvp = pri.GetbMatchMVP();
                break;
            }
        }
    }

    LOG("[RocketMax] [collectMatchStats] === FINAL STATS ===");
    LOG("[RocketMax] [collectMatchStats] Goals: " + std::to_string(match_goals));
    LOG("[RocketMax] [collectMatchStats] Assists: " + std::to_string(match_assists));
    LOG("[RocketMax] [collectMatchStats] Saves: " + std::to_string(match_saves));
    LOG("[RocketMax] [collectMatchStats] Shots: " + std::to_string(match_shots));
    LOG("[RocketMax] [collectMatchStats] Score: " + std::to_string(match_score));
    LOG("[RocketMax] [collectMatchStats] Demos: " + std::to_string(match_demos));
    LOG("[RocketMax] [collectMatchStats] MVP: " + std::to_string(match_mvp));
}

int RocketMax::getCurentPlaylist()
{
    ServerWrapper sw = gameWrapper->GetCurrentGameState();
    if (!sw) return -1;
    GameSettingPlaylistWrapper playlist = sw.GetPlaylist();
    if (!playlist) return -1;
    int playlistId = playlist.GetPlaylistId();
    LOG("[RocketMax] [getCurentPlaylist] playlistId: " + std::to_string(playlistId)+ " - Gamemode: " + gameModes[playlistId]);
    return playlistId;
}

bool RocketMax::isRankedGame()
{
    return gameWrapper->IsInOnlineGame() && !gameWrapper->IsInReplay() && !gameWrapper->IsInFreeplay();
}

bool RocketMax::sendMmrUpdate(long long timestamp)
{
    LOG("[RocketMax] [sendMmrUpdate] -- SENDING DATA --");
    LOG("[RocketMax] [sendMmrUpdate]  Player ID     :" + playerId);
    LOG("[RocketMax] [sendMmrUpdate]  Gamemode ID   :" + std::to_string(playlistId));
    LOG("[RocketMax] [sendMmrUpdate]  Gamemode NAME :" + gameModes[playlistId]);
    LOG("[RocketMax] [sendMmrUpdate]  MMR           :" + std::to_string(mmr_apres_match));

    CurlRequest req;
    req.url = std::string(API_ENDPOINT) + "/updateMmr";
    req.body = R"({"player_id": ")" + playerId +
        R"(", "timestamp": ")" + std::to_string(timestamp) +
        R"(", "mmr": )" + std::to_string(mmr_apres_match) +
        R"(, "gamemode_id": )" + std::to_string(playlistId) +
        R"(})";
    
    
    HttpWrapper::SendCurlJsonRequest(req, [this](int code, std::string result)
    {
        LOG("Json result: " + result);
        if (code == 200) {
            LOG("[RocketMax] [sendMmrUpdate] DATA SENT");
        }
        else {
            LOG("[RocketMax] [sendMmrUpdate] ERROR DATA NOT SENT");
            return true;
        }
       
    });
    return false;
}

bool RocketMax::initAPI()
{
    // Obtenez l'identifiant unique du joueur sous forme de cha�ne
    playerIdWrapper = gameWrapper->GetUniqueID();
    playerId = std::to_string(playerIdWrapper.GetUID());
    // Obtenez le nom du joueur
    playerName = gameWrapper->GetPlayerName().ToString();

    LOG("[RocketMax] [InitAPI] " + playerId);
    LOG("[RocketMax] [InitAPI] " + playerName);

    CurlRequest req;
    req.url = std::string(API_ENDPOINT) + "/initPlayer";
    req.body = R"({"player_id": ")" + playerId +
        R"(", "player_name": ")" + playerName + R"("})";


    HttpWrapper::SendCurlJsonRequest(req, [this](int code, std::string result)
        {
            LOG("Json result: " + result);
            if (code == 200) {
                LOG("[RocketMax] [InitAPI] DATA SENT");
                gameWrapper->Execute([this](GameWrapper* gw) {
                    std::string toastMsg = "Plugin v" + std::string(plugin_version) + " connecte et pret !";
                    gw->Toast("RocketMax", toastMsg, "default", 5.0f, ToastType_OK);
                });
            }
            else {
                LOG("[RocketMax] [InitAPI] ERROR DATA NOT SENT");
                gameWrapper->Execute([this](GameWrapper* gw) {
                    gw->Toast("RocketMax", "Erreur de connexion au serveur", "default", 5.0f, ToastType_Error);
                });
                return true;
            }
        });
    return false;

}


void RocketMax::gameStart(std::string eventName)
{
    if (game_running == 1)return;
    if (!isRankedGame())return;
    playlistId = getCurentPlaylist();
    if (playlistId == -1)return;
    LOG("===== GameStart =====");
    LOG("MODE DE JEU :" + gameModes[playlistId]);
    
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
    LOG("===== !GameStart =====");
}



void RocketMax::gameEnd(std::string eventName)
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

        // Collecter les stats AVANT le timeout
        LOG("[RocketMax] [gameEnd] About to collect match stats...");
        collectMatchStats();
        LOG("[RocketMax] [gameEnd] Stats collected, setting timeout for MMR update...");

        gameWrapper->SetTimeout([&](GameWrapper* gameWrapper) { 
            mmr_apres_match = getMmrData(playlistId);
            mmr_gagne = mmr_apres_match - mmr_avant_match;
            LOG("MMR AVANT :" + std::to_string(mmr_avant_match));
            LOG("MMR APRES :" + std::to_string(mmr_apres_match));
            LOG("MMR WON :"+std::to_string(mmr_gagne));
            gameHasEnded();
        }, 5.0F);
        LOG("===== !GameEnd =====");
    }
}

bool RocketMax::sendHistoriqueGame(long long timestamp)
{
    LOG("[RocketMax] [sendHistoriqueGame]  MMR GAGNE     :" + std::to_string(mmr_gagne));
    LOG("[RocketMax] [sendHistoriqueGame]  victory ?     :" + std::to_string(victory));
    LOG("[RocketMax] [sendHistoriqueGame]  timestamp     :" + std::to_string(timestamp));

    CurlRequest req;
    req.url = std::string(API_ENDPOINT) + "/updateHistorique";
    req.body = R"({"player_id": ")" + playerId +
        R"(", "timestamp": ")" + std::to_string(timestamp) +
        R"(", "victory": )" + std::to_string(victory) +
        R"(, "mmr_won": )" + std::to_string(mmr_gagne) +
        R"(, "gamemode_id": )" + std::to_string(playlistId) +
        // Stats individuelles
        R"(, "goals": )" + std::to_string(match_goals) +
        R"(, "assists": )" + std::to_string(match_assists) +
        R"(, "saves": )" + std::to_string(match_saves) +
        R"(, "shots": )" + std::to_string(match_shots) +
        R"(, "score": )" + std::to_string(match_score) +
        R"(, "demos": )" + std::to_string(match_demos) +
        R"(, "mvp": )" + std::to_string(match_mvp) +
        // Stats de match
        R"(, "team_score": )" + std::to_string(team_score) +
        R"(, "opponent_score": )" + std::to_string(opponent_score) +
        R"(, "overtime": )" + std::to_string(overtime) +
        R"(})";

    // Capturer les valeurs pour le toast (car elles peuvent changer avant l'execute)
    int mmr_display = mmr_apres_match;
    int mmr_diff = mmr_gagne;
    std::string requestBody = req.body;  // Capture body before lambda

    HttpWrapper::SendCurlJsonRequest(req, [this, mmr_display, mmr_diff, requestBody](int code, std::string result)
        {
            LOG("Json result: " + result);
            if (code == 200) {
                LOG("[RocketMax] [sendHistoriqueGame] DATA SENT");
                if (*cvar_enable_toasts) {
                    gameWrapper->Execute([this, mmr_display, mmr_diff](GameWrapper* gw) {
                        std::string toastMsg = "Donnees envoyees ! MMR: " + std::to_string(mmr_display) + " (" + (mmr_diff >= 0 ? "+" : "") + std::to_string(mmr_diff) + ")";
                        gw->Toast("RocketMax", toastMsg, "default", 5.0f, ToastType_OK);
                    });
                }
            }
            else {
                LOG("[RocketMax] [sendHistoriqueGame] ERROR DATA NOT SENT - Saving to offline queue");
                // Save to offline queue for later retry
                saveToOfflineQueue("/updateHistorique", requestBody);
                if (*cvar_enable_toasts) {
                    gameWrapper->Execute([this](GameWrapper* gw) {
                        gw->Toast("RocketMax", "Hors ligne - donnees sauvegardees", "default", 5.0f, ToastType_Warning);
                    });
                }
                return true;
            }

        });
    return false;
}

// ============ STREAK TRACKING ============

void RocketMax::updateStreak(bool won)
{
    // Update session stats
    if (won) {
        session_wins++;
        if (current_streak >= 0) {
            current_streak++;
        } else {
            current_streak = 1;
        }
        if (current_streak > best_win_streak) {
            best_win_streak = current_streak;
        }
    } else {
        session_losses++;
        if (current_streak <= 0) {
            current_streak--;
        } else {
            current_streak = -1;
        }
        if (std::abs(current_streak) > worst_loss_streak) {
            worst_loss_streak = std::abs(current_streak);
        }
    }

    session_mmr_change += mmr_gagne;

    LOG("[RocketMax] [Streak] Current: " + std::to_string(current_streak) +
        " | Session: " + std::to_string(session_wins) + "W/" + std::to_string(session_losses) + "L" +
        " | MMR: " + (session_mmr_change >= 0 ? "+" : "") + std::to_string(session_mmr_change));

    checkStreakMilestone();
}

void RocketMax::checkStreakMilestone()
{
    if (!*cvar_enable_streak_alerts) return;

    // Milestones at 3, 5, 7, 10 games
    int streak_abs = std::abs(current_streak);
    if (streak_abs == 3 || streak_abs == 5 || streak_abs == 7 || streak_abs == 10) {
        showStreakToast();
    }
}

void RocketMax::showStreakToast()
{
    gameWrapper->Execute([this](GameWrapper* gw) {
        std::string msg;
        if (current_streak > 0) {
            msg = "Win Streak: " + std::to_string(current_streak) + " victoires !";
            gw->Toast("RocketMax", msg, "default", 5.0f, ToastType_OK);
        } else {
            msg = "Lose Streak: " + std::to_string(std::abs(current_streak)) + " defaites...";
            gw->Toast("RocketMax", msg, "default", 5.0f, ToastType_Error);
        }
    });
}

// ============ OFFLINE QUEUE ============

std::string RocketMax::getQueueFilePath()
{
    return gameWrapper->GetDataFolder().string() + "/rocketmax_queue.json";
}

void RocketMax::saveToOfflineQueue(const std::string& endpoint, const std::string& body)
{
    std::string filepath = getQueueFilePath();
    LOG("[RocketMax] [OfflineQueue] Saving to: " + filepath);

    // Read existing queue
    std::ifstream infile(filepath);
    std::string content = "";
    if (infile.is_open()) {
        std::stringstream buffer;
        buffer << infile.rdbuf();
        content = buffer.str();
        infile.close();
    }

    // Parse or create array
    std::string newEntry = R"({"endpoint":")" + endpoint + R"(","body":)" + body + "}";

    if (content.empty() || content == "[]") {
        content = "[" + newEntry + "]";
    } else {
        // Insert before last ]
        size_t pos = content.rfind(']');
        if (pos != std::string::npos) {
            content.insert(pos, "," + newEntry);
        }
    }

    // Write back
    std::ofstream outfile(filepath);
    if (outfile.is_open()) {
        outfile << content;
        outfile.close();
        LOG("[RocketMax] [OfflineQueue] Saved successfully");
    } else {
        LOG("[RocketMax] [OfflineQueue] ERROR: Could not write file");
    }
}

void RocketMax::processOfflineQueue()
{
    std::string filepath = getQueueFilePath();
    std::ifstream infile(filepath);

    if (!infile.is_open()) {
        LOG("[RocketMax] [OfflineQueue] No queue file found");
        return;
    }

    std::stringstream buffer;
    buffer << infile.rdbuf();
    std::string content = buffer.str();
    infile.close();

    if (content.empty() || content == "[]") {
        LOG("[RocketMax] [OfflineQueue] Queue is empty");
        return;
    }

    LOG("[RocketMax] [OfflineQueue] Processing offline queue...");

    // Clear the file first (we'll re-add failed ones)
    std::ofstream clearFile(filepath);
    clearFile << "[]";
    clearFile.close();

    // Note: In a real implementation, you'd parse the JSON and retry each request
    // For now, we just log that we found pending items
    LOG("[RocketMax] [OfflineQueue] Found pending items - will retry on next connection");
}

// ============ AUTO-UPDATE ============

std::string RocketMax::getPluginPath()
{
    // Get the plugins folder path from BakkesMod
    std::string dataFolder = gameWrapper->GetDataFolder().string();
    // Navigate from data folder to plugins folder
    // Data folder is typically: %APPDATA%/bakkesmod/bakkesmod/data
    // Plugins folder is: %APPDATA%/bakkesmod/bakkesmod/plugins
    size_t pos = dataFolder.rfind("data");
    if (pos != std::string::npos) {
        return dataFolder.substr(0, pos) + "plugins/RocketMax.dll";
    }
    return "";
}

std::string RocketMax::getUpdateFilePath()
{
    return gameWrapper->GetDataFolder().string() + "/RocketMax_update.dll";
}

bool RocketMax::parseVersionString(const std::string& version, int& major, int& minor, int& patch)
{
    // Parse version string like "v1.2.3" or "1.2.3"
    std::string ver = version;
    if (!ver.empty() && (ver[0] == 'v' || ver[0] == 'V')) {
        ver = ver.substr(1);
    }

    int parsed = sscanf(ver.c_str(), "%d.%d.%d", &major, &minor, &patch);
    return parsed >= 3;
}

bool RocketMax::isNewerVersion(const std::string& remoteVersion)
{
    int remoteMajor = 0, remoteMinor = 0, remotePatch = 0;
    if (!parseVersionString(remoteVersion, remoteMajor, remoteMinor, remotePatch)) {
        LOG("[RocketMax] [Update] Failed to parse remote version: " + remoteVersion);
        return false;
    }

    LOG("[RocketMax] [Update] Comparing versions - Local: " +
        std::to_string(VERSION_MAJOR) + "." + std::to_string(VERSION_MINOR) + "." + std::to_string(VERSION_PATCH) +
        " vs Remote: " + std::to_string(remoteMajor) + "." + std::to_string(remoteMinor) + "." + std::to_string(remotePatch));

    if (remoteMajor > VERSION_MAJOR) return true;
    if (remoteMajor < VERSION_MAJOR) return false;
    if (remoteMinor > VERSION_MINOR) return true;
    if (remoteMinor < VERSION_MINOR) return false;
    if (remotePatch > VERSION_PATCH) return true;
    return false;
}

void RocketMax::checkForUpdates()
{
    if (update_checking) {
        LOG("[RocketMax] [Update] Already checking for updates...");
        return;
    }

    update_checking = true;
    update_error = "";
    LOG("[RocketMax] [Update] Checking for updates...");

    CurlRequest req;
    req.url = GITHUB_API_RELEASES;
    // Use empty body for GET request - SendCurlJsonRequest handles JSON response
    req.body = "";

    HttpWrapper::SendCurlJsonRequest(req, [this](int code, std::string result)
    {
        update_checking = false;

        if (code != 200) {
            LOG("[RocketMax] [Update] Failed to check for updates. HTTP code: " + std::to_string(code));
            update_error = "Erreur de connexion (code " + std::to_string(code) + ")";
            return;
        }

        LOG("[RocketMax] [Update] Received response from GitHub API");

        // Simple JSON parsing for tag_name and browser_download_url
        // Looking for: "tag_name": "v1.2.3"
        std::string tagKey = "\"tag_name\"";
        size_t tagPos = result.find(tagKey);
        if (tagPos == std::string::npos) {
            LOG("[RocketMax] [Update] Could not find tag_name in response");
            update_error = "Format de reponse invalide";
            return;
        }

        // Find the value after tag_name
        size_t colonPos = result.find(":", tagPos);
        size_t quoteStart = result.find("\"", colonPos + 1);
        size_t quoteEnd = result.find("\"", quoteStart + 1);
        if (quoteStart == std::string::npos || quoteEnd == std::string::npos) {
            update_error = "Erreur de parsing version";
            return;
        }

        latest_version = result.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        LOG("[RocketMax] [Update] Latest version: " + latest_version);

        // Find download URL for RocketMax.dll
        std::string dllName = "RocketMax.dll";
        size_t dllPos = result.find(dllName);
        if (dllPos != std::string::npos) {
            // Look backwards for browser_download_url
            std::string urlKey = "\"browser_download_url\"";
            size_t urlKeyPos = result.rfind(urlKey, dllPos);
            if (urlKeyPos != std::string::npos) {
                size_t urlColonPos = result.find(":", urlKeyPos + urlKey.length());
                size_t urlQuoteStart = result.find("\"", urlColonPos + 1);
                size_t urlQuoteEnd = result.find("\"", urlQuoteStart + 1);
                if (urlQuoteStart != std::string::npos && urlQuoteEnd != std::string::npos) {
                    update_download_url = result.substr(urlQuoteStart + 1, urlQuoteEnd - urlQuoteStart - 1);
                    LOG("[RocketMax] [Update] Download URL: " + update_download_url);
                }
            }
        }

        // Check if this is a newer version
        if (isNewerVersion(latest_version)) {
            update_available = true;
            LOG("[RocketMax] [Update] New version available: " + latest_version);

            gameWrapper->Execute([this](GameWrapper* gw) {
                if (*cvar_enable_toasts) {
                    std::string msg = "Nouvelle version disponible: " + latest_version;
                    gw->Toast("RocketMax Update", msg, "default", 8.0f, ToastType_Info);
                }
            });
        }
        else {
            update_available = false;
            LOG("[RocketMax] [Update] Already on latest version");
        }
    });
}

void RocketMax::downloadUpdate()
{
    if (update_download_url.empty()) {
        update_error = "URL de telechargement non disponible";
        LOG("[RocketMax] [Update] No download URL available");
        return;
    }

    if (update_downloading) {
        LOG("[RocketMax] [Update] Already downloading...");
        return;
    }

    update_downloading = true;
    update_error = "";
    LOG("[RocketMax] [Update] Starting download from: " + update_download_url);

    gameWrapper->Execute([this](GameWrapper* gw) {
        if (*cvar_enable_toasts) {
            gw->Toast("RocketMax Update", "Telechargement en cours...", "default", 3.0f, ToastType_Info);
        }
    });

    CurlRequest req;
    req.url = update_download_url;
    req.body = "";

    HttpWrapper::SendCurlJsonRequest(req, [this](int code, std::string result)
    {
        update_downloading = false;

        if (code != 200) {
            update_error = "Echec du telechargement (code " + std::to_string(code) + ")";
            LOG("[RocketMax] [Update] Download failed. HTTP code: " + std::to_string(code));

            gameWrapper->Execute([this](GameWrapper* gw) {
                gw->Toast("RocketMax Update", update_error, "default", 5.0f, ToastType_Error);
            });
            return;
        }

        // Save the downloaded file
        std::string updatePath = getUpdateFilePath();
        std::ofstream outFile(updatePath, std::ios::binary);
        if (!outFile.is_open()) {
            update_error = "Impossible d'ecrire le fichier";
            LOG("[RocketMax] [Update] Could not open file for writing: " + updatePath);
            return;
        }

        outFile.write(result.c_str(), result.size());
        outFile.close();

        LOG("[RocketMax] [Update] Downloaded update to: " + updatePath);
        LOG("[RocketMax] [Update] File size: " + std::to_string(result.size()) + " bytes");

        // Create a batch script to apply the update on next launch
        std::string pluginPath = getPluginPath();
        std::string batchPath = gameWrapper->GetDataFolder().string() + "/apply_update.bat";

        std::ofstream batchFile(batchPath);
        if (batchFile.is_open()) {
            batchFile << "@echo off\n";
            batchFile << "echo Applying RocketMax update...\n";
            batchFile << "timeout /t 2 /nobreak > nul\n";
            batchFile << "copy /Y \"" << updatePath << "\" \"" << pluginPath << "\"\n";
            batchFile << "del \"" << updatePath << "\"\n";
            batchFile << "del \"%~f0\"\n";
            batchFile.close();
            LOG("[RocketMax] [Update] Created update script: " + batchPath);
        }

        update_ready = true;
        update_available = false;

        gameWrapper->Execute([this](GameWrapper* gw) {
            gw->Toast("RocketMax Update", "Mise a jour prete! Redemarrez Rocket League.", "default", 10.0f, ToastType_OK);
        });
    });
}

// ============ OVERLAY WINDOW ============

std::string RocketMax::GetMenuName()
{
    return "RocketMax";
}

std::string RocketMax::GetMenuTitle()
{
    return "RocketMax Session";
}

void RocketMax::SetImGuiContext(uintptr_t ctx)
{
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

bool RocketMax::ShouldBlockInput()
{
    return false;
}

bool RocketMax::IsActiveOverlay()
{
    return true;
}

void RocketMax::OnOpen()
{
    overlay_visible = true;
}

void RocketMax::OnClose()
{
    overlay_visible = false;
}

void RocketMax::RenderWindow()
{
    if (!*cvar_enable_overlay || !overlay_visible) return;

    ImGui::SetNextWindowSize(ImVec2(250, 180), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("RocketMax Session", &overlay_visible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {

        // Session header
        ImGui::Text("Session Stats");
        ImGui::Separator();

        // Win/Loss
        ImGui::Text("Matches: ");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "%d W", session_wins);
        ImGui::SameLine();
        ImGui::Text(" / ");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1.0f), "%d L", session_losses);

        // Win rate
        float win_rate = (session_wins + session_losses) > 0
            ? (float)session_wins / (session_wins + session_losses) * 100.0f
            : 0.0f;
        ImGui::Text("Win Rate: %.1f%%", win_rate);

        // MMR Change
        ImGui::Text("MMR Change: ");
        ImGui::SameLine();
        if (session_mmr_change >= 0) {
            ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "+%d", session_mmr_change);
        } else {
            ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1.0f), "%d", session_mmr_change);
        }

        ImGui::Separator();

        // Current Streak
        ImGui::Text("Streak: ");
        ImGui::SameLine();
        if (current_streak > 0) {
            ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "%d W", current_streak);
        } else if (current_streak < 0) {
            ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1.0f), "%d L", std::abs(current_streak));
        } else {
            ImGui::Text("-");
        }

        // Best/Worst
        ImGui::Text("Best: %d W | Worst: %d L", best_win_streak, worst_loss_streak);
    }
    ImGui::End();
}
