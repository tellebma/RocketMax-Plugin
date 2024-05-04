#include "pch.h"
#include "RocketMax.h"

void RocketMax::RenderSettings() {
    ImGui::TextUnformatted("- RocketMax -");
    ImGui::TextUnformatted("This plugin dosen't require any settings");
    ImGui::TextUnformatted(("Configured serveur : " + std::string(API_ENDPOINT)).c_str());
    if (pluginLoaded){
        // Texte en vert
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Plugin chargé avec succès !");
    }
    else {
        // Texte en rouge
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "ERREUR lors du chargement du plugin.");
    }

    
}

