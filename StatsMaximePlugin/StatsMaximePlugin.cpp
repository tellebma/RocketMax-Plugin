#include "pch.h"
#include "StatsMaximePlugin.h"

#include <curl\curl.h>

#define HOOK_MATCH_ENDED "Function TAGame.GameEvent_Soccar_TA.EventMatchEnded"
#define API_ENDPOINT_SEND_DATA "http://127.0.0.1:5000/sendMMRPlayer"
#define API_ENDPOINT_INIT "http://127.0.0.1:5000/initPlayer"

BAKKESMOD_PLUGIN(StatsMaximePlugin, "StatMaxime", plugin_version, PERMISSION_ALL)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

void StatsMaximePlugin::onLoad()
{
	_globalCvarManager = cvarManager;
	LOG("Plugin loaded!");
    InitAPI();
    gameWrapper->HookEvent(HOOK_MATCH_ENDED, std::bind(&StatsMaximePlugin::OnMatchEnded, this, std::placeholders::_1));
}

void StatsMaximePlugin::onUnload()
{
	gameWrapper->UnhookEventPost(HOOK_MATCH_ENDED);
	LOG("Plugin unloaded");
}

void StatsMaximePlugin::OnMatchEnded(std::string eventName)
{
    LOG("Triggered");
    if (!IsRankedGame())
        return;

    float playerMMR = gameWrapper->GetMMRWrapper().GetPlayerMMR(gameWrapper->GetUniqueID(), 0); // Assuming player 1's MMR
    long long timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    LOG("playerMMR :" + std::to_string(playerMMR));
    SendMMRToAPI(playerMMR, timestamp);
}

bool StatsMaximePlugin::IsRankedGame()
{
    return gameWrapper->IsInOnlineGame() && !gameWrapper->IsInReplay() && !gameWrapper->IsInFreeplay();
}

void StatsMaximePlugin::SendMMRToAPI(float mmr, long long timestamp)
{
    LOG("SEND DATA");
    CURL* curl;
    CURLcode res;
    curl = curl_easy_init();
    if (curl)
    {
        std::string postData = "player_id=" + std::to_string(gameWrapper->GetUniqueID().GetUID()) +
            "&timestamp=" + std::to_string(timestamp) +
            "&elo=" + std::to_string(mmr);

        curl_easy_setopt(curl, CURLOPT_URL, API_ENDPOINT_SEND_DATA);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
        res = curl_easy_perform(curl);

        curl_easy_cleanup(curl);
    }
}

void StatsMaximePlugin::InitAPI()
{
    CURL* curl;
    CURLcode res;
    curl = curl_easy_init();
    if (curl)
    {
        std::string postData = "player_id=" + std::to_string(gameWrapper->GetUniqueID().GetUID());

        curl_easy_setopt(curl, CURLOPT_URL, API_ENDPOINT_INIT);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
        res = curl_easy_perform(curl);

        curl_easy_cleanup(curl);
    }
}