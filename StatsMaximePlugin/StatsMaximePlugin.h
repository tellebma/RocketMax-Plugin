#pragma once

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include "version.h"

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);


class StatsMaximePlugin: public BakkesMod::Plugin::BakkesModPlugin
	,public SettingsWindowBase // Uncomment if you wanna render your own tab in the settings menu
	//,public PluginWindowBase // Uncomment if you want to render your own plugin window
{
	//Boilerplate
	void onLoad() override;
	void onUnload() override;
	
	// setup
	bool initAPI();

	// Trigger
	void onMatchEnded(std::string eventName);

	// Usefull fonctions
	bool getDataAndSendIt();
	int getMmrData(UniqueIDWrapper playerID, int gamemode);
	int getCurentPlaylist();
	bool isRankedGame();
	bool sendDataToAPI(UniqueIDWrapper playerID, long long timestamp, int mmr, int gameModeId);

public:
	void RenderSettings() override; // Uncomment if you wanna render your own tab in the settings menu
	//void RenderWindow() override; // Uncomment if you want to render your own plugin window
};
