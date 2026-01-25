#pragma once

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include "version.h"

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);


class RocketMax: public BakkesMod::Plugin::BakkesModPlugin
	,public SettingsWindowBase // Uncomment if you wanna render your own tab in the settings menu
	,public PluginWindowBase // Enabled for session overlay
{
	//Boilerplate
	void onLoad() override;
	void onUnload() override;

	// setup
	bool initAPI();

	//activate Trigger
	void gameHasEnded();

	// Trigger
	void gameStart(std::string eventName);
	void gameEnd(std::string eventName);

	// Usefull fonctions
	int getMmrData(int gamemode);
	int getCurentPlaylist();
	bool isRankedGame();
	bool sendMmrUpdate(long long timestamp);
	bool sendHistoriqueGame(long long timestamp);
	void collectMatchStats();

	// Streak & Session tracking
	void updateStreak(bool won);
	void checkStreakMilestone();
	void showStreakToast();

	// Offline queue
	void saveToOfflineQueue(const std::string& endpoint, const std::string& body);
	void processOfflineQueue();
	std::string getQueueFilePath();

	// Auto-update
	void checkForUpdates();
	void downloadUpdate();
	bool parseVersionString(const std::string& version, int& major, int& minor, int& patch);
	bool isNewerVersion(const std::string& remoteVersion);
	std::string getPluginPath();
	std::string getUpdateFilePath();



	// vars
	//#define API_ENDPOINT "http://localhost:8080"
	#define API_ENDPOINT "http://localhost:8080"
	#define GITHUB_API_RELEASES "https://api.github.com/repos/tellebma/RocketMax-Plugin/releases/latest"
	#define GITHUB_REPO_OWNER "tellebma"
	#define GITHUB_REPO_NAME "RocketMax-Plugin"

	// Vars used by prgm
	bool pluginLoaded = false;
	int game_running = 0;
	int my_team_num = -1;
	std::string playerId = "";
	std::string playerName = "";
	UniqueIDWrapper playerIdWrapper;
	bool mmr_player_updated = false;
	int mmr_avant_match = 0;
	int mmr_apres_match = 0;
	int mmr_gagne = 0;
	int playlistId = 100;
	bool victory = false;

	// Stats individuelles du match
	int match_goals = 0;
	int match_assists = 0;
	int match_saves = 0;
	int match_shots = 0;
	int match_score = 0;
	int match_demos = 0;
	bool match_mvp = false;

	// Stats de l'équipe
	int team_score = 0;
	int opponent_score = 0;
	bool overtime = false;

	// Session & Streak tracking
	int session_wins = 0;
	int session_losses = 0;
	int session_mmr_change = 0;
	int current_streak = 0;  // positif = wins, négatif = losses
	int best_win_streak = 0;
	int worst_loss_streak = 0;

	// Configuration CVars
	std::shared_ptr<bool> cvar_enable_toasts;
	std::shared_ptr<bool> cvar_enable_overlay;
	std::shared_ptr<bool> cvar_enable_streak_alerts;
	std::shared_ptr<bool> cvar_enable_auto_update;
	std::shared_ptr<std::string> cvar_server_url;

	// Auto-update state
	bool update_available = false;
	bool update_checking = false;
	bool update_downloading = false;
	bool update_ready = false;
	std::string latest_version = "";
	std::string update_download_url = "";
	std::string update_error = "";

	// Overlay state
	bool overlay_visible = true;

	std::unique_ptr<MMRNotifierToken> notifierToken;

public:
	void RenderSettings() override;
	void RenderWindow() override;
	std::string GetMenuName() override;
	std::string GetMenuTitle() override;
	void SetImGuiContext(uintptr_t ctx) override;
	bool ShouldBlockInput() override;
	bool IsActiveOverlay() override;
	void OnOpen() override;
	void OnClose() override;
};
