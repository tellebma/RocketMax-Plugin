#include "pch.h"
#include "RocketMax.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <wincrypt.h>
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Advapi32.lib")

// Use constants namespace
using namespace RocketMaxConstants;


#define HOOK_MATCH_ENDED "Function TAGame.GameEvent_Soccar_TA.EventMatchEnded"
#define HOOK_MATCH_START "Function GameEvent_TA.Countdown.BeginState"

//#define HOOK_MATCH_ENDED "Function TAGame.GameEvent_Soccar_TA.OnMatchWinnerSet"
//#define HOOK_GAME_DESTORYED "Function TAGame.GameEvent_TA.Destroyed"



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

// Forward declaration - escapes curly braces for logging (std::format uses {} as placeholders)
std::string escapeForLog(const std::string& str);

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

    // Authentication & Privacy CVars (persistent)
    cvar_auth_secret = std::make_shared<std::string>("");
    cvarManager->registerCvar("rocketmax_auth_secret", "", "Authentication secret (do not share!)", true, false, 0, false, 0, false)
        .bindTo(cvar_auth_secret);

    cvar_hide_profile = std::make_shared<bool>(false);
    cvarManager->registerCvar("rocketmax_hide_profile", "0", "Hide profile from public listing")
        .bindTo(cvar_hide_profile);

    cvar_access_token = std::make_shared<std::string>("");
    cvarManager->registerCvar("rocketmax_access_token", "", "Access token for private profile link", true, false, 0, false, 0, false)
        .bindTo(cvar_access_token);

    cvar_profile_url = std::make_shared<std::string>("");
    cvarManager->registerCvar("rocketmax_profile_url", "", "Private profile URL", true, false, 0, false, 0, false)
        .bindTo(cvar_profile_url);

    // Process any offline queue from previous sessions
    processOfflineQueue();

    // Check for updates if enabled
    if (*cvar_enable_auto_update) {
        checkForUpdates();
    }

    // Initialize API connection (async)
    initAPI();

    // Hook game events regardless of API status (offline mode support)
    gameWrapper->HookEvent(HOOK_MATCH_START, std::bind(&RocketMax::gameStart, this, std::placeholders::_1));
    gameWrapper->HookEvent(HOOK_MATCH_ENDED, std::bind(&RocketMax::gameEnd, this, std::placeholders::_1));
    pluginLoaded = true;
    LOG("[RocketMax] Version " + std::string(plugin_version) + " loaded successfully");
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

    // Reset game state
    game_running = false;
    my_team_num = INVALID_TEAM_NUM;
    mmr_avant_match = 0;
    mmr_apres_match = 0;
    mmr_gagne = 0;
    playlistId = INVALID_PLAYLIST_ID;
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

int RocketMax::getCurrentPlaylist()
{
    ServerWrapper sw = gameWrapper->GetCurrentGameState();
    if (!sw) return -1;
    GameSettingPlaylistWrapper playlist = sw.GetPlaylist();
    if (!playlist) return -1;
    int currentPlaylistId = playlist.GetPlaylistId();
    LOG("[RocketMax] [getCurrentPlaylist] playlistId: " + std::to_string(currentPlaylistId) + " - Gamemode: " + gameModes[currentPlaylistId]);
    return currentPlaylistId;
}

bool RocketMax::isRankedGame()
{
    return gameWrapper->IsInOnlineGame() && !gameWrapper->IsInReplay() && !gameWrapper->IsInFreeplay();
}

void RocketMax::sendMmrUpdate(long long timestamp)
{
    LOG("[RocketMax] [sendMmrUpdate] -- SENDING DATA --");
    LOG("[RocketMax] [sendMmrUpdate]  Player ID     :" + playerId);
    LOG("[RocketMax] [sendMmrUpdate]  Gamemode ID   :" + std::to_string(playlistId));
    LOG("[RocketMax] [sendMmrUpdate]  Gamemode NAME :" + gameModes[playlistId]);
    LOG("[RocketMax] [sendMmrUpdate]  MMR           :" + std::to_string(mmr_apres_match));

    std::string body = R"({"player_id": ")" + playerId +
        R"(", "timestamp": ")" + std::to_string(timestamp) +
        R"(", "mmr": )" + std::to_string(mmr_apres_match) +
        R"(, "gamemode_id": )" + std::to_string(playlistId) +
        R"(})";

    sendAuthenticatedRequest("/updateMmr", body, [this](int code, std::string result)
    {
        LOG("Json result: " + escapeForLog(result));
        if (code == 200) {
            LOG("[RocketMax] [sendMmrUpdate] DATA SENT");
        }
        else if (code == 401) {
            LOG("[RocketMax] [sendMmrUpdate] AUTHENTICATION FAILED");
        }
        else {
            LOG("[RocketMax] [sendMmrUpdate] ERROR DATA NOT SENT");
        }
    });
}

void RocketMax::initAPI()
{
    // Obtenez l'identifiant unique du joueur sous forme de chaine
    playerIdWrapper = gameWrapper->GetUniqueID();
    playerId = std::to_string(playerIdWrapper.GetUID());
    // Obtenez le nom du joueur
    playerName = gameWrapper->GetPlayerName().ToString();

    LOG("[RocketMax] [InitAPI] " + playerId);
    LOG("[RocketMax] [InitAPI] " + playerName);

    // Check if we have a locally stored auth_secret
    std::string localSecret = *cvar_auth_secret;
    bool needsAuthSecret = localSecret.empty();
    LOG("[RocketMax] [InitAPI] Local auth_secret exists: " + std::string(needsAuthSecret ? "NO" : "YES"));

    CurlRequest req;
    req.url = std::string(API_ENDPOINT) + "/initPlayer";
    req.verb = "POST";
    req.body = R"({"player_id": ")" + playerId +
        R"(", "player_name": ")" + playerName +
        R"(", "needs_auth_secret": )" + (needsAuthSecret ? "true" : "false") + R"(})";
    req.headers["Content-Type"] = "application/json";

    LOG("[RocketMax] [InitAPI] URL: " + req.url);
    LOG("[RocketMax] [InitAPI] Body: " + escapeForLog(req.body));

    HttpWrapper::SendCurlRequest(req, [this](int code, std::string result)
        {
            LOG("[RocketMax] [InitAPI] Response code: " + std::to_string(code));
            LOG("[RocketMax] [InitAPI] Response: " + escapeForLog(result));

            if (code == 200) {
                LOG("[RocketMax] [InitAPI] DATA SENT");

                // Extract auth_secret from response (only returned for new players)
                std::string authSecret = extractJsonValue(result, "auth_secret");
                if (!authSecret.empty() && authSecret != "null") {
                    LOG("[RocketMax] [InitAPI] Received NEW auth_secret from server");
                    cvarManager->getCvar("rocketmax_auth_secret").setValue(authSecret);
                }
                else {
                    // Check if we have a locally stored auth_secret
                    std::string storedSecret = *cvar_auth_secret;
                    if (storedSecret.empty()) {
                        LOG("[RocketMax] [InitAPI] WARNING: No auth_secret from server and none stored locally!");
                    }
                    else {
                        LOG("[RocketMax] [InitAPI] Using locally stored auth_secret");
                    }
                }

                // Extract visibility state
                std::string isHiddenStr = extractJsonValue(result, "is_hidden");
                bool isHidden = (isHiddenStr == "true");
                cvarManager->getCvar("rocketmax_hide_profile").setValue(isHidden ? "1" : "0");

                // Extract access_token if profile is hidden
                std::string accessToken = extractJsonValue(result, "access_token");
                if (!accessToken.empty() && accessToken != "null") {
                    cvarManager->getCvar("rocketmax_access_token").setValue(accessToken);
                    std::string profileUrl = std::string(API_ENDPOINT) + "/p/" + accessToken;
                    cvarManager->getCvar("rocketmax_profile_url").setValue(profileUrl);
                    LOG("[RocketMax] [InitAPI] Profile is hidden, URL: " + profileUrl);
                }

                gameWrapper->Execute([this](GameWrapper* gw) {
                    std::string toastMsg = "Plugin v" + std::string(plugin_version) + " connecte et pret !";
                    gw->Toast("RocketMax", toastMsg, "default", TOAST_DURATION_DEFAULT);
                });
            }
            else {
                LOG("[RocketMax] [InitAPI] ERROR - HTTP " + std::to_string(code));
                gameWrapper->Execute([this](GameWrapper* gw) {
                    gw->Toast("RocketMax", "Erreur de connexion au serveur", "default", TOAST_DURATION_DEFAULT);
                });
            }
        });
}


void RocketMax::gameStart(std::string eventName)
{
    if (game_running) return;
    if (!isRankedGame()) return;
    playlistId = getCurrentPlaylist();
    if (playlistId == -1) return;
    LOG("===== GameStart =====");
    LOG("MODE DE JEU :" + gameModes[playlistId]);

    CarWrapper me = gameWrapper->GetLocalCar();
    if (me.IsNull()) return;

    PriWrapper mePRI = me.GetPRI();
    if (mePRI.IsNull()) return;

    TeamInfoWrapper myTeam = mePRI.GetTeam();
    if (myTeam.IsNull()) return;

    // Get TeamNum
    my_team_num = myTeam.GetTeamNum();

    mmr_avant_match = getMmrData(playlistId);

    game_running = true;
    LOG("===== !GameStart =====");
}



void RocketMax::gameEnd(std::string eventName)
{
    if (!game_running) return;

    game_running = false;
    LOG("GameEnd => is_online_game: yes my_team_num:" + std::to_string(my_team_num));

    if (my_team_num != INVALID_TEAM_NUM)
    {
        LOG("===== GameEnd =====");
        ServerWrapper server = gameWrapper->GetOnlineGame();
        TeamWrapper winningTeam = server.GetGameWinner();
        if (winningTeam.IsNull()) return;
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

        // IMPORTANT: Capture by value to avoid dangling references
        // The timeout callback runs 5 seconds later, so captured references could be invalid
        int captured_playlistId = playlistId;
        int captured_mmr_avant = mmr_avant_match;

        gameWrapper->SetTimeout([this, captured_playlistId, captured_mmr_avant](GameWrapper* gw) {
            mmr_apres_match = getMmrData(captured_playlistId);
            mmr_gagne = mmr_apres_match - captured_mmr_avant;
            LOG("MMR AVANT :" + std::to_string(captured_mmr_avant));
            LOG("MMR APRES :" + std::to_string(mmr_apres_match));
            LOG("MMR WON :" + std::to_string(mmr_gagne));
            gameHasEnded();
        }, MMR_UPDATE_DELAY_SECONDS);
        LOG("===== !GameEnd =====");
    }
}

void RocketMax::sendHistoriqueGame(long long timestamp)
{
    LOG("[RocketMax] [sendHistoriqueGame]  MMR GAGNE     :" + std::to_string(mmr_gagne));
    LOG("[RocketMax] [sendHistoriqueGame]  victory ?     :" + std::to_string(victory));
    LOG("[RocketMax] [sendHistoriqueGame]  timestamp     :" + std::to_string(timestamp));

    std::string requestBody = R"({"player_id": ")" + playerId +
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

    sendAuthenticatedRequest("/updateHistorique", requestBody, [this, mmr_display, mmr_diff, requestBody](int code, std::string result)
        {
            LOG("Json result: " + escapeForLog(result));
            if (code == 200) {
                LOG("[RocketMax] [sendHistoriqueGame] DATA SENT");
                if (*cvar_enable_toasts) {
                    gameWrapper->Execute([this, mmr_display, mmr_diff](GameWrapper* gw) {
                        std::string toastMsg = "Donnees envoyees ! MMR: " + std::to_string(mmr_display) + " (" + (mmr_diff >= 0 ? "+" : "") + std::to_string(mmr_diff) + ")";
                        gw->Toast("RocketMax", toastMsg, "default", TOAST_DURATION_DEFAULT);
                    });
                }
            }
            else if (code == 401) {
                LOG("[RocketMax] [sendHistoriqueGame] AUTHENTICATION FAILED");
                if (*cvar_enable_toasts) {
                    gameWrapper->Execute([this](GameWrapper* gw) {
                        gw->Toast("RocketMax", "Erreur d'authentification", "default", TOAST_DURATION_DEFAULT);
                    });
                }
            }
            else {
                LOG("[RocketMax] [sendHistoriqueGame] ERROR DATA NOT SENT - Saving to offline queue");
                // Save to offline queue for later retry
                saveToOfflineQueue("/updateHistorique", requestBody);
                if (*cvar_enable_toasts) {
                    gameWrapper->Execute([this](GameWrapper* gw) {
                        gw->Toast("RocketMax", "Hors ligne - donnees sauvegardees", "default", TOAST_DURATION_DEFAULT);
                    });
                }
            }
        });
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
            gw->Toast("RocketMax", msg, "default", TOAST_DURATION_DEFAULT);
        } else {
            msg = "Lose Streak: " + std::to_string(std::abs(current_streak)) + " defaites...";
            gw->Toast("RocketMax", msg, "default", TOAST_DURATION_DEFAULT);
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
        if (infile.fail() && !infile.eof()) {
            LOG("[RocketMax] [OfflineQueue] WARNING: Error reading existing queue file");
        }
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

    // Write back with proper error handling
    std::ofstream outfile(filepath);
    if (outfile.is_open()) {
        outfile << content;
        outfile.flush();
        bool write_success = outfile.good();
        outfile.close();

        if (write_success) {
            LOG("[RocketMax] [OfflineQueue] Saved successfully");
        } else {
            LOG("[RocketMax] [OfflineQueue] ERROR: Write operation failed");
        }
    } else {
        LOG("[RocketMax] [OfflineQueue] ERROR: Could not open file for writing");
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

    // Clear the file first (we'll re-add failed ones via saveToOfflineQueue callback)
    std::ofstream clearFile(filepath);
    if (clearFile.is_open()) {
        clearFile << "[]";
        clearFile.close();
    }

    // Parse and process each queue entry
    // Format: [{"endpoint":"...", "body":{...}}, ...]
    size_t searchPos = 0;
    int processedCount = 0;

    while (searchPos < content.length()) {
        // Find the start of an entry
        size_t entryStart = content.find(R"({"endpoint":")", searchPos);
        if (entryStart == std::string::npos) break;

        // Extract endpoint
        size_t endpointStart = entryStart + 13; // Length of {"endpoint":"
        size_t endpointEnd = content.find('"', endpointStart);
        if (endpointEnd == std::string::npos) break;

        std::string endpoint = content.substr(endpointStart, endpointEnd - endpointStart);

        // Find the body
        size_t bodyStart = content.find(R"("body":)", endpointEnd);
        if (bodyStart == std::string::npos) break;
        bodyStart += 7; // Length of "body":

        // Find the end of the body (matching closing brace)
        int braceCount = 0;
        size_t bodyEnd = bodyStart;
        bool inString = false;

        for (size_t i = bodyStart; i < content.length(); i++) {
            char c = content[i];

            // Handle string escaping
            if (c == '"' && (i == 0 || content[i-1] != '\\')) {
                inString = !inString;
            }

            if (!inString) {
                if (c == '{') braceCount++;
                else if (c == '}') {
                    braceCount--;
                    if (braceCount == 0) {
                        bodyEnd = i + 1;
                        break;
                    }
                }
            }
        }

        if (bodyEnd > bodyStart) {
            std::string body = content.substr(bodyStart, bodyEnd - bodyStart);
            LOG("[RocketMax] [OfflineQueue] Retrying: " + endpoint);

            // Retry the request (failures will be re-queued by the callback)
            sendAuthenticatedRequest(endpoint, body, [this, endpoint](int code, std::string result) {
                if (code == 200) {
                    LOG("[RocketMax] [OfflineQueue] Successfully retried: " + endpoint);
                } else {
                    LOG("[RocketMax] [OfflineQueue] Retry failed for: " + endpoint + " (code " + std::to_string(code) + ")");
                    // Note: The original sendAuthenticatedRequest callbacks will handle re-queueing
                }
            });

            processedCount++;
        }

        searchPos = bodyEnd;
    }

    LOG("[RocketMax] [OfflineQueue] Processed " + std::to_string(processedCount) + " queued items");
}

// ============ AUTO-UPDATE ============

std::filesystem::path RocketMax::getPluginsFolder()
{
    // Use std::filesystem to safely navigate from data folder to plugins folder
    // Data folder: %APPDATA%/bakkesmod/bakkesmod/data
    // Plugins folder: %APPDATA%/bakkesmod/bakkesmod/plugins
    std::filesystem::path dataFolder = gameWrapper->GetDataFolder();
    std::filesystem::path bakkesmodFolder = dataFolder.parent_path();  // Go up from "data"
    return bakkesmodFolder / "plugins";
}

std::filesystem::path RocketMax::getUpdateScriptPath()
{
    return gameWrapper->GetDataFolder() / "rocketmax_update.ps1";
}

// Helper function to escape curly braces for logging (std::format uses {} as placeholders)
std::string escapeForLog(const std::string& str)
{
    std::string result;
    result.reserve(str.size() * 2);
    for (char c : str) {
        if (c == '{') result += "{{";
        else if (c == '}') result += "}}";
        else result += c;
    }
    return result;
}

std::string RocketMax::extractJsonValue(const std::string& json, const std::string& key)
{
    // JSON value extractor that handles strings, booleans, null, and numbers
    // Looks for: "key": "value" or "key": value or "key":value
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) {
        return "";
    }

    // Find the colon after the key
    size_t colonPos = json.find(":", keyPos + searchKey.length());
    if (colonPos == std::string::npos) {
        return "";
    }

    // Skip whitespace after colon
    size_t valueStart = colonPos + 1;
    while (valueStart < json.length() && (json[valueStart] == ' ' || json[valueStart] == '\t' || json[valueStart] == '\n' || json[valueStart] == '\r')) {
        valueStart++;
    }

    if (valueStart >= json.length()) {
        return "";
    }

    // Check if it's a string value (starts with quote)
    if (json[valueStart] == '\"') {
        // Find closing quote (handle escaped quotes)
        size_t quoteEnd = valueStart + 1;
        while (quoteEnd < json.length()) {
            if (json[quoteEnd] == '\"' && json[quoteEnd - 1] != '\\') {
                break;
            }
            quoteEnd++;
        }

        if (quoteEnd >= json.length()) {
            return "";
        }

        return json.substr(valueStart + 1, quoteEnd - valueStart - 1);
    }
    else {
        // Non-string value (boolean, null, number)
        // Find the end of the value (comma, closing brace/bracket, or whitespace)
        size_t valueEnd = valueStart;
        while (valueEnd < json.length()) {
            char c = json[valueEnd];
            if (c == ',' || c == '}' || c == ']' || c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                break;
            }
            valueEnd++;
        }

        return json.substr(valueStart, valueEnd - valueStart);
    }
}

bool RocketMax::parseVersionString(const std::string& version, int& major, int& minor, int& patch)
{
    // Parse version string like "v1.2.3" or "1.2.3"
    std::string ver = version;
    if (!ver.empty() && (ver[0] == 'v' || ver[0] == 'V')) {
        ver = ver.substr(1);
    }

    // Remove any suffix like "-beta", "-rc1", etc.
    size_t dashPos = ver.find('-');
    if (dashPos != std::string::npos) {
        ver = ver.substr(0, dashPos);
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
    if (update_checking.load()) {
        LOG("[RocketMax] [Update] Already checking for updates...");
        return;
    }

    update_checking.store(true);
    {
        std::lock_guard<std::mutex> lock(update_mutex);
        update_error = "";
    }
    LOG("[RocketMax] [Update] Checking for updates...");

    CurlRequest req;
    req.url = GITHUB_API_RELEASES;
    req.verb = "GET";

    HttpWrapper::SendCurlRequest(req, [this](int code, std::string result)
    {
        update_checking.store(false);

        if (code != 200) {
            LOG("[RocketMax] [Update] Failed to check for updates. HTTP code: " + std::to_string(code));
            std::lock_guard<std::mutex> lock(update_mutex);
            update_error = "Erreur de connexion (code " + std::to_string(code) + ")";
            return;
        }

        LOG("[RocketMax] [Update] Received response from GitHub API");

        // Extract tag_name using helper function
        std::string version = extractJsonValue(result, "tag_name");
        if (version.empty()) {
            LOG("[RocketMax] [Update] Could not find tag_name in response");
            std::lock_guard<std::mutex> lock(update_mutex);
            update_error = "Format de reponse invalide";
            return;
        }
        LOG("[RocketMax] [Update] Latest version: " + version);

        // Find download URL for RocketMax.dll in assets array
        std::string downloadUrl = "";
        std::string dllName = "RocketMax.dll";
        size_t dllPos = result.find(dllName);
        if (dllPos != std::string::npos) {
            std::string urlKey = "\"browser_download_url\"";
            size_t urlKeyPos = result.rfind(urlKey, dllPos);
            if (urlKeyPos != std::string::npos) {
                size_t colonPos = result.find(":", urlKeyPos + urlKey.length());
                size_t quoteStart = result.find("\"", colonPos + 1);
                size_t quoteEnd = result.find("\"", quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    downloadUrl = result.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                    LOG("[RocketMax] [Update] Download URL: " + downloadUrl);
                }
            }
        }

        // Update shared strings with mutex protection
        {
            std::lock_guard<std::mutex> lock(update_mutex);
            latest_version = version;
            update_download_url = downloadUrl;
        }

        // Check if this is a newer version
        if (isNewerVersion(version)) {
            update_available.store(true);
            LOG("[RocketMax] [Update] New version available: " + version);

            gameWrapper->Execute([this, version](GameWrapper* gw) {
                if (*cvar_enable_toasts) {
                    std::string msg = "Nouvelle version disponible: " + version;
                    gw->Toast("RocketMax Update", msg, "default", TOAST_DURATION_LONG);
                }
            });
        }
        else {
            update_available.store(false);
            LOG("[RocketMax] [Update] Already on latest version");
        }
    });
}

void RocketMax::launchUpdateScript()
{
    // Thread-safe access to shared strings
    std::string downloadUrl;
    std::string version;
    {
        std::lock_guard<std::mutex> lock(update_mutex);
        downloadUrl = update_download_url;
        version = latest_version;
    }

    if (downloadUrl.empty()) {
        std::lock_guard<std::mutex> lock(update_mutex);
        update_error = "URL de telechargement non disponible";
        LOG("[RocketMax] [Update] No download URL available");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(update_mutex);
        update_error = "";
    }
    LOG("[RocketMax] [Update] Creating update script...");

    // Get paths
    std::filesystem::path pluginsFolder = getPluginsFolder();
    std::filesystem::path pluginPath = pluginsFolder / "RocketMax.dll";
    std::filesystem::path scriptPath = getUpdateScriptPath();

    // Create PowerShell script that will:
    // 1. Download the DLL from GitHub
    // 2. Verify the download (check file size > 100KB for a valid DLL)
    // 3. Replace the old plugin
    // 4. Clean up
    std::ofstream scriptFile(scriptPath);
    if (!scriptFile.is_open()) {
        std::lock_guard<std::mutex> lock(update_mutex);
        update_error = "Impossible de creer le script";
        LOG("[RocketMax] [Update] Could not create update script: " + scriptPath.string());
        return;
    }

    scriptFile << "# RocketMax Auto-Update Script\n";
    scriptFile << "# Generated by RocketMax Plugin v" << plugin_version << "\n";
    scriptFile << "$ErrorActionPreference = 'Stop'\n";
    scriptFile << "$Host.UI.RawUI.WindowTitle = 'RocketMax Update'\n\n";

    scriptFile << "$downloadUrl = '" << downloadUrl << "'\n";
    scriptFile << "$pluginPath = '" << pluginPath.string() << "'\n";
    scriptFile << "$tempPath = '" << (pluginsFolder / ("RocketMax_" + version + ".dll")).string() << "'\n";
    scriptFile << "$backupPath = '" << (pluginsFolder / "RocketMax_backup.dll").string() << "'\n";
    scriptFile << "$updateSuccess = $false\n\n";

    scriptFile << "Write-Host ''\n";
    scriptFile << "Write-Host '========================================' -ForegroundColor Cyan\n";
    scriptFile << "Write-Host '       RocketMax Auto-Update Script' -ForegroundColor Cyan\n";
    scriptFile << "Write-Host '========================================' -ForegroundColor Cyan\n";
    scriptFile << "Write-Host ''\n";
    scriptFile << "Write-Host 'IMPORTANT: Fermez Rocket League avant de continuer!' -ForegroundColor Yellow\n";
    scriptFile << "Write-Host '           Le plugin ne peut pas etre mis a jour' -ForegroundColor Yellow\n";
    scriptFile << "Write-Host '           pendant que le jeu est en cours.' -ForegroundColor Yellow\n";
    scriptFile << "Write-Host ''\n";
    scriptFile << "Write-Host 'Appuyez sur Entree quand Rocket League est ferme...' -ForegroundColor White\n";
    scriptFile << "Read-Host\n\n";

    scriptFile << "try {\n";
    scriptFile << "    # Check if Rocket League is running\n";
    scriptFile << "    $rlProcess = Get-Process -Name 'RocketLeague' -ErrorAction SilentlyContinue\n";
    scriptFile << "    if ($rlProcess) {\n";
    scriptFile << "        Write-Host ''\n";
    scriptFile << "        Write-Host 'ERREUR: Rocket League est toujours en cours!' -ForegroundColor Red\n";
    scriptFile << "        Write-Host 'Fermez le jeu et relancez ce script.' -ForegroundColor Red\n";
    scriptFile << "        throw 'Rocket League is still running'\n";
    scriptFile << "    }\n\n";

    scriptFile << "    # Remove temp file if it exists from a previous attempt\n";
    scriptFile << "    if (Test-Path $tempPath) { Remove-Item $tempPath -Force }\n\n";

    scriptFile << "    Write-Host 'Telechargement de la nouvelle version...' -ForegroundColor Yellow\n";
    scriptFile << "    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12\n";
    scriptFile << "    $ProgressPreference = 'SilentlyContinue'\n";
    scriptFile << "    Invoke-WebRequest -Uri $downloadUrl -OutFile $tempPath -UseBasicParsing\n";
    scriptFile << "    $ProgressPreference = 'Continue'\n\n";

    scriptFile << "    # Verify download - DLL should be at least 100KB\n";
    scriptFile << "    $fileSize = (Get-Item $tempPath).Length\n";
    scriptFile << "    if ($fileSize -lt 102400) {\n";
    scriptFile << "        throw \"Le fichier telecharge est trop petit ($fileSize octets). Telechargement corrompu.\"\n";
    scriptFile << "    }\n";
    scriptFile << "    Write-Host \"Telecharge avec succes ($fileSize octets)\" -ForegroundColor Green\n\n";

    scriptFile << "    # Backup current version\n";
    scriptFile << "    if (Test-Path $pluginPath) {\n";
    scriptFile << "        Write-Host 'Sauvegarde de la version actuelle...' -ForegroundColor Yellow\n";
    scriptFile << "        Copy-Item $pluginPath $backupPath -Force -ErrorAction Stop\n";
    scriptFile << "    }\n\n";

    scriptFile << "    # Replace plugin\n";
    scriptFile << "    Write-Host 'Installation de la nouvelle version...' -ForegroundColor Yellow\n";
    scriptFile << "    Move-Item $tempPath $pluginPath -Force -ErrorAction Stop\n\n";

    scriptFile << "    $updateSuccess = $true\n";
    scriptFile << "    Write-Host ''\n";
    scriptFile << "    Write-Host '========================================' -ForegroundColor Green\n";
    scriptFile << "    Write-Host '       Mise a jour terminee!' -ForegroundColor Green\n";
    scriptFile << "    Write-Host '========================================' -ForegroundColor Green\n";
    scriptFile << "    Write-Host ''\n";
    scriptFile << "    Write-Host 'Vous pouvez maintenant relancer Rocket League.' -ForegroundColor Cyan\n";
    scriptFile << "    Write-Host ''\n";

    scriptFile << "    # Clean up backup on success\n";
    scriptFile << "    if (Test-Path $backupPath) { Remove-Item $backupPath -Force -ErrorAction SilentlyContinue }\n";
    scriptFile << "}\n";
    scriptFile << "catch {\n";
    scriptFile << "    Write-Host ''\n";
    scriptFile << "    Write-Host '========================================' -ForegroundColor Red\n";
    scriptFile << "    Write-Host '       ERREUR!' -ForegroundColor Red\n";
    scriptFile << "    Write-Host '========================================' -ForegroundColor Red\n";
    scriptFile << "    Write-Host ''\n";
    scriptFile << "    Write-Host \"Details: $_\" -ForegroundColor Red\n";
    scriptFile << "    Write-Host ''\n";

    scriptFile << "    # Restore backup if exists and update failed\n";
    scriptFile << "    if ((Test-Path $backupPath) -and -not $updateSuccess) {\n";
    scriptFile << "        Write-Host 'Restauration de la sauvegarde...' -ForegroundColor Yellow\n";
    scriptFile << "        try {\n";
    scriptFile << "            Move-Item $backupPath $pluginPath -Force -ErrorAction Stop\n";
    scriptFile << "            Write-Host 'Sauvegarde restauree.' -ForegroundColor Green\n";
    scriptFile << "        } catch {\n";
    scriptFile << "            Write-Host \"Echec de la restauration: $_\" -ForegroundColor Red\n";
    scriptFile << "        }\n";
    scriptFile << "    }\n";

    scriptFile << "    # Clean up temp file\n";
    scriptFile << "    if (Test-Path $tempPath) { Remove-Item $tempPath -Force -ErrorAction SilentlyContinue }\n";
    scriptFile << "}\n\n";

    scriptFile << "Write-Host ''\n";
    scriptFile << "Write-Host 'Appuyez sur Entree pour fermer cette fenetre...' -ForegroundColor Gray\n";
    scriptFile << "Read-Host\n";

    scriptFile.flush();
    bool write_success = scriptFile.good();
    scriptFile.close();

    if (!write_success) {
        std::lock_guard<std::mutex> lock(update_mutex);
        update_error = "Erreur lors de l'ecriture du script";
        LOG("[RocketMax] [Update] Failed to write update script");
        return;
    }

    LOG("[RocketMax] [Update] Created update script: " + scriptPath.string());

    // Launch the PowerShell script with -NoExit to keep window open on any error
    std::string psArgs = "-NoExit -ExecutionPolicy Bypass -File \"" + scriptPath.string() + "\"";
    LOG("[RocketMax] [Update] Launching PowerShell with args: " + psArgs);

    // Use ShellExecute to run the script (shows a window so user can see progress)
    HINSTANCE result = ShellExecuteA(NULL, "open", "powershell.exe",
        psArgs.c_str(),
        NULL, SW_SHOW);

    if ((intptr_t)result <= 32) {
        std::lock_guard<std::mutex> lock(update_mutex);
        update_error = "Impossible de lancer le script (erreur " + std::to_string((intptr_t)result) + ")";
        LOG("[RocketMax] [Update] Failed to launch script: " + std::to_string((intptr_t)result));
        return;
    }

    update_ready.store(true);
    update_available.store(false);

    gameWrapper->Execute([this](GameWrapper* gw) {
        gw->Toast("RocketMax Update", "Fermez Rocket League puis suivez les instructions!", "default", TOAST_DURATION_LONG);
    });
}

// ============ AUTHENTICATION & PRIVACY ============

std::string RocketMax::computeHmacSha256(const std::string& data, const std::string& key)
{
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    HCRYPTKEY hKey = 0;
    BYTE* pbHash = nullptr;
    DWORD dwHashLen = 32; // SHA256 = 32 bytes
    std::string result;

    LOG("[RocketMax] [HMAC] Computing HMAC-SHA256...");
    LOG("[RocketMax] [HMAC] Data length: " + std::to_string(data.length()));
    LOG("[RocketMax] [HMAC] Key length: " + std::to_string(key.length()));

    // Structure for importing the key
    struct {
        BLOBHEADER hdr;
        DWORD keySize;
        BYTE key[256];
    } keyBlob;

    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        LOG("[RocketMax] [HMAC] CryptAcquireContext failed - Error: " + std::to_string(GetLastError()));
        return "";
    }

    // Setup key blob
    keyBlob.hdr.bType = PLAINTEXTKEYBLOB;
    keyBlob.hdr.bVersion = CUR_BLOB_VERSION;
    keyBlob.hdr.reserved = 0;
    keyBlob.hdr.aiKeyAlg = CALG_RC2; // Placeholder, will be used for HMAC
    keyBlob.keySize = static_cast<DWORD>(key.length());
    memcpy(keyBlob.key, key.c_str(), key.length());

    // Import the key
    if (!CryptImportKey(hProv, (BYTE*)&keyBlob, sizeof(BLOBHEADER) + sizeof(DWORD) + key.length(), 0, CRYPT_IPSEC_HMAC_KEY, &hKey)) {
        LOG("[RocketMax] [HMAC] CryptImportKey failed");
        CryptReleaseContext(hProv, 0);
        return "";
    }

    // Create HMAC hash
    if (!CryptCreateHash(hProv, CALG_HMAC, hKey, 0, &hHash)) {
        LOG("[RocketMax] [HMAC] CryptCreateHash failed");
        CryptDestroyKey(hKey);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    // Set HMAC info to use SHA256
    HMAC_INFO hmacInfo;
    ZeroMemory(&hmacInfo, sizeof(hmacInfo));
    hmacInfo.HashAlgid = CALG_SHA_256;

    if (!CryptSetHashParam(hHash, HP_HMAC_INFO, (BYTE*)&hmacInfo, 0)) {
        LOG("[RocketMax] [HMAC] CryptSetHashParam failed");
        CryptDestroyHash(hHash);
        CryptDestroyKey(hKey);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    // Hash the data
    if (!CryptHashData(hHash, (BYTE*)data.c_str(), static_cast<DWORD>(data.length()), 0)) {
        LOG("[RocketMax] [HMAC] CryptHashData failed");
        CryptDestroyHash(hHash);
        CryptDestroyKey(hKey);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    // Get the hash value
    pbHash = new BYTE[dwHashLen];
    if (!CryptGetHashParam(hHash, HP_HASHVAL, pbHash, &dwHashLen, 0)) {
        LOG("[RocketMax] [HMAC] CryptGetHashParam failed");
        delete[] pbHash;
        CryptDestroyHash(hHash);
        CryptDestroyKey(hKey);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    // Convert to hex string
    std::stringstream ss;
    for (DWORD i = 0; i < dwHashLen; i++) {
        ss << std::hex << std::setfill('0') << std::setw(2) << (int)pbHash[i];
    }
    result = ss.str();

    LOG("[RocketMax] [HMAC] Successfully computed HMAC, result length: " + std::to_string(result.length()));

    // Cleanup
    delete[] pbHash;
    CryptDestroyHash(hHash);
    CryptDestroyKey(hKey);
    CryptReleaseContext(hProv, 0);

    return result;
}

void RocketMax::sendAuthenticatedRequest(const std::string& endpoint, const std::string& body,
    std::function<void(int, std::string)> callback)
{
    std::string authSecret = *cvar_auth_secret;
    LOG("[RocketMax] [Auth] === Sending authenticated request ===");
    LOG("[RocketMax] [Auth] Endpoint: " + endpoint);
    LOG("[RocketMax] [Auth] Body length: " + std::to_string(body.length()));
    LOG("[RocketMax] [Auth] Body: " + escapeForLog(body.substr(0, 200)) + (body.length() > 200 ? "..." : ""));

    std::string signature = "";

    if (!authSecret.empty()) {
        LOG("[RocketMax] [Auth] auth_secret length: " + std::to_string(authSecret.length()));
        LOG("[RocketMax] [Auth] auth_secret (first 8 chars): " + authSecret.substr(0, 8) + "...");

        // Compute HMAC-SHA256 signature
        signature = computeHmacSha256(body, authSecret);
        LOG("[RocketMax] [Auth] Computed signature: " + signature.substr(0, 16) + "...");
        LOG("[RocketMax] [Auth] Player ID: " + playerId);
    }
    else {
        LOG("[RocketMax] [Auth] WARNING: No auth secret available, sending unauthenticated request");
    }

    // Build the request using HttpWrapper
    CurlRequest req;
    req.url = std::string(API_ENDPOINT) + endpoint;
    req.verb = "POST";
    req.body = body;
    req.headers["Content-Type"] = "application/json";

    if (!signature.empty()) {
        req.headers["X-Player-Id"] = playerId;
        req.headers["X-Signature"] = signature;
    }

    LOG("[RocketMax] [Auth] URL: " + req.url);
    LOG("[RocketMax] [Auth] Headers set: Content-Type, X-Player-Id, X-Signature");

    HttpWrapper::SendCurlRequest(req, [this, callback](int code, std::string result) {
        LOG("[RocketMax] [Auth] Response code: " + std::to_string(code));
        LOG("[RocketMax] [Auth] Response body: " + escapeForLog(result));
        callback(code, result);
    });
}

void RocketMax::setProfileVisibility(bool hidden)
{
    LOG("[RocketMax] [Privacy] Setting profile visibility: " + std::string(hidden ? "hidden" : "public"));

    std::string body = R"({"is_hidden": )" + std::string(hidden ? "true" : "false") + "}";

    sendAuthenticatedRequest("/setVisibility", body, [this, hidden](int code, std::string result) {
        LOG("[RocketMax] [Privacy] setVisibility response: " + escapeForLog(result));

        if (code == 200) {
            // Update local CVars
            cvarManager->getCvar("rocketmax_hide_profile").setValue(hidden ? "1" : "0");

            // Extract access_token from response if profile is hidden
            if (hidden) {
                std::string token = extractJsonValue(result, "access_token");
                if (!token.empty() && token != "null") {
                    cvarManager->getCvar("rocketmax_access_token").setValue(token);
                    std::string profileUrl = std::string(API_ENDPOINT) + "/p/" + token;
                    cvarManager->getCvar("rocketmax_profile_url").setValue(profileUrl);
                    LOG("[RocketMax] [Privacy] Profile URL: " + profileUrl);
                }
            }

            gameWrapper->Execute([this, hidden](GameWrapper* gw) {
                if (hidden) {
                    gw->Toast("RocketMax", "Profil masque ! Utilisez le lien prive pour y acceder.", "default", TOAST_DURATION_DEFAULT);
                } else {
                    gw->Toast("RocketMax", "Profil rendu public.", "default", TOAST_DURATION_DEFAULT);
                }
            });
        }
        else if (code == 401) {
            LOG("[RocketMax] [Privacy] Authentication failed");
            gameWrapper->Execute([this](GameWrapper* gw) {
                gw->Toast("RocketMax", "Erreur d'authentification. Essayez de recharger le plugin.", "default", TOAST_DURATION_DEFAULT);
            });
        }
        else {
            LOG("[RocketMax] [Privacy] setVisibility failed with code: " + std::to_string(code));
            gameWrapper->Execute([this](GameWrapper* gw) {
                gw->Toast("RocketMax", "Erreur lors du changement de visibilite.", "default", TOAST_DURATION_DEFAULT);
            });
        }
    });
}

void RocketMax::regenerateAccessToken()
{
    LOG("[RocketMax] [Privacy] Regenerating access token...");

    sendAuthenticatedRequest("/regenerateAccessToken", "{}", [this](int code, std::string result) {
        LOG("[RocketMax] [Privacy] regenerateAccessToken response: " + escapeForLog(result));

        if (code == 200) {
            std::string token = extractJsonValue(result, "access_token");
            if (!token.empty() && token != "null") {
                cvarManager->getCvar("rocketmax_access_token").setValue(token);
                std::string profileUrl = std::string(API_ENDPOINT) + "/p/" + token;
                cvarManager->getCvar("rocketmax_profile_url").setValue(profileUrl);
                LOG("[RocketMax] [Privacy] New profile URL: " + profileUrl);
            }

            gameWrapper->Execute([this](GameWrapper* gw) {
                gw->Toast("RocketMax", "Nouveau lien prive genere ! L'ancien lien ne fonctionne plus.", "default", TOAST_DURATION_DEFAULT);
            });
        }
        else {
            LOG("[RocketMax] [Privacy] regenerateAccessToken failed with code: " + std::to_string(code));
            gameWrapper->Execute([this](GameWrapper* gw) {
                gw->Toast("RocketMax", "Erreur lors de la regeneration du lien.", "default", TOAST_DURATION_DEFAULT);
            });
        }
    });
}

void RocketMax::copyProfileLinkToClipboard()
{
    std::string url = *cvar_profile_url;
    if (url.empty()) {
        // If no private URL, use public URL
        url = std::string(API_ENDPOINT) + "/user?id=" + playerId;
    }

    // Copy to clipboard using Windows API
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, url.size() + 1);
        if (hg) {
            memcpy(GlobalLock(hg), url.c_str(), url.size() + 1);
            GlobalUnlock(hg);
            SetClipboardData(CF_TEXT, hg);
        }
        CloseClipboard();
        LOG("[RocketMax] [Privacy] Copied profile URL to clipboard: " + url);

        gameWrapper->Execute([this](GameWrapper* gw) {
            gw->Toast("RocketMax", "Lien copie dans le presse-papiers !", "default", TOAST_DURATION_SHORT);
        });
    }
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
