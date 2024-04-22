#include "pch.h"
#include "StatsMaximePlugin.h"

void StatsMaximePlugin::RenderSettings() {
    ImGui::TextUnformatted("A really cool plugin");

    if (ImGui::Button("DEBUG")) {
        gameWrapper->Execute([this](GameWrapper* gw) {
            cvarManager->executeCommand("getDataAndSendIt");
            });
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("GetDataFor");
    }
}

