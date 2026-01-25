#pragma once

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include "version.h"

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);


class RocketMax: public BakkesMod::Plugin::BakkesModPlugin
	,public SettingsWindowBase // Uncomment if you wanna render your own tab in the settings menu
	//,public PluginWindowBase // Uncomment if you want to render your own plugin window
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



	// vars
	//#define API_ENDPOINT "http://localhost:8080"
	#define API_ENDPOINT "http://localhost:8080"

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

	std::unique_ptr<MMRNotifierToken> notifierToken;

public:
	void RenderSettings() override; // Uncomment if you wanna render your own tab in the settings menu
	//void RenderWindow() override; // Uncomment if you want to render your own plugin window
};
