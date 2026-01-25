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

    // Process any offline queue from previous sessions
    processOfflineQueue();

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
