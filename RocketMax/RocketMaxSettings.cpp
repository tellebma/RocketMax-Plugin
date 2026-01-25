#include "pch.h"
#include "RocketMax.h"

void RocketMax::RenderSettings() {
    // Header
    ImGui::TextUnformatted("- RocketMax -");
    ImGui::TextUnformatted(("Version: " + std::string(plugin_version)).c_str());
    ImGui::Separator();

    // Status
    if (pluginLoaded) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Plugin charge avec succes !");
    }
    else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "ERREUR lors du chargement du plugin.");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Configuration Section
    ImGui::TextUnformatted("Configuration");
    ImGui::Spacing();

    // Toast notifications toggle
    CVarWrapper toastCvar = cvarManager->getCvar("rocketmax_enable_toasts");
    if (toastCvar) {
        bool toastEnabled = toastCvar.getBoolValue();
        if (ImGui::Checkbox("Activer les notifications toast", &toastEnabled)) {
            toastCvar.setValue(toastEnabled);
        }
    }

    // Overlay toggle
    CVarWrapper overlayCvar = cvarManager->getCvar("rocketmax_enable_overlay");
    if (overlayCvar) {
        bool overlayEnabled = overlayCvar.getBoolValue();
        if (ImGui::Checkbox("Activer l'overlay de session", &overlayEnabled)) {
            overlayCvar.setValue(overlayEnabled);
        }
    }

    // Streak alerts toggle
    CVarWrapper streakCvar = cvarManager->getCvar("rocketmax_enable_streak_alerts");
    if (streakCvar) {
        bool streakEnabled = streakCvar.getBoolValue();
        if (ImGui::Checkbox("Activer les alertes de serie", &streakEnabled)) {
            streakCvar.setValue(streakEnabled);
        }
    }

    ImGui::Spacing();

    // Server URL (read-only display)
    ImGui::TextUnformatted(("Serveur: " + std::string(API_ENDPOINT)).c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Session Stats Section
    ImGui::TextUnformatted("Session actuelle");
    ImGui::Spacing();

    // Win/Loss display
    ImGui::Text("Parties: ");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "%d V", session_wins);
    ImGui::SameLine();
    ImGui::Text(" / ");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1.0f), "%d D", session_losses);

    // Win rate
    float win_rate = (session_wins + session_losses) > 0
        ? (float)session_wins / (session_wins + session_losses) * 100.0f
        : 0.0f;
    ImGui::Text("Win Rate: %.1f%%", win_rate);

    // MMR change
    ImGui::Text("MMR: ");
    ImGui::SameLine();
    if (session_mmr_change >= 0) {
        ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "+%d", session_mmr_change);
    }
    else {
        ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1.0f), "%d", session_mmr_change);
    }

    // Current streak
    ImGui::Text("Serie: ");
    ImGui::SameLine();
    if (current_streak > 0) {
        ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "%d victoires", current_streak);
    }
    else if (current_streak < 0) {
        ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1.0f), "%d defaites", std::abs(current_streak));
    }
    else {
        ImGui::Text("-");
    }

    // Best/Worst
    ImGui::Text("Record: %d V | %d D", best_win_streak, worst_loss_streak);

    ImGui::Spacing();

    // Reset session button
    if (ImGui::Button("Reinitialiser la session")) {
        session_wins = 0;
        session_losses = 0;
        session_mmr_change = 0;
        current_streak = 0;
        best_win_streak = 0;
        worst_loss_streak = 0;
    }
}
