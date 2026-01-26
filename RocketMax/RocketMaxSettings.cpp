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

    // Auto-update toggle
    CVarWrapper autoUpdateCvar = cvarManager->getCvar("rocketmax_enable_auto_update");
    if (autoUpdateCvar) {
        bool autoUpdateEnabled = autoUpdateCvar.getBoolValue();
        if (ImGui::Checkbox("Verifier les mises a jour automatiquement", &autoUpdateEnabled)) {
            autoUpdateCvar.setValue(autoUpdateEnabled);
        }
    }

    ImGui::Spacing();

    // Server URL (read-only display)
    ImGui::TextUnformatted(("Serveur: " + std::string(API_ENDPOINT)).c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Privacy Section
    ImGui::TextUnformatted("Confidentialite du profil");
    ImGui::Spacing();

    // Hide profile toggle
    CVarWrapper hideCvar = cvarManager->getCvar("rocketmax_hide_profile");
    if (hideCvar) {
        bool hideEnabled = hideCvar.getBoolValue();
        if (ImGui::Checkbox("Masquer mon profil public", &hideEnabled)) {
            setProfileVisibility(hideEnabled);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Si active, votre profil ne sera plus visible\ndans la liste publique. Utilisez le lien prive\npour partager votre profil.");
        }
    }

    // Show profile link and actions only if profile is hidden
    CVarWrapper profileUrlCvar = cvarManager->getCvar("rocketmax_profile_url");
    CVarWrapper accessTokenCvar = cvarManager->getCvar("rocketmax_access_token");
    bool isHidden = hideCvar && hideCvar.getBoolValue();

    if (isHidden && profileUrlCvar && !profileUrlCvar.getStringValue().empty()) {
        ImGui::Spacing();
        ImGui::TextUnformatted("Lien prive:");

        // Display truncated URL
        std::string fullUrl = profileUrlCvar.getStringValue();
        std::string displayUrl = fullUrl;
        if (displayUrl.length() > 50) {
            displayUrl = displayUrl.substr(0, 47) + "...";
        }
        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", displayUrl.c_str());

        ImGui::Spacing();

        // Copy link button
        if (ImGui::Button("Copier le lien")) {
            copyProfileLinkToClipboard();
        }

        ImGui::SameLine();

        // Regenerate token button
        if (ImGui::Button("Regenerer le lien")) {
            regenerateAccessToken();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Genere un nouveau lien prive.\nL'ancien lien ne fonctionnera plus.");
        }
    }
    else if (!isHidden) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Votre profil est public");

        // Copy public link button
        if (ImGui::Button("Copier le lien public")) {
            copyProfileLinkToClipboard();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Update Section
    ImGui::TextUnformatted("Mises a jour");
    ImGui::Spacing();

    if (update_checking.load()) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Verification en cours...");
    }
    else if (update_ready.load()) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Mise a jour en cours !");
        ImGui::TextUnformatted("Le script de mise a jour a ete lance.");
        ImGui::TextUnformatted("Redemarrez Rocket League une fois termine.");
        ImGui::Spacing();
        ImGui::TextUnformatted(("Nouvelle version: " + latest_version).c_str());
    }
    else if (update_available.load()) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Nouvelle version disponible !");
        ImGui::TextUnformatted(("Version actuelle: " + std::string(plugin_version)).c_str());
        ImGui::TextUnformatted(("Nouvelle version: " + latest_version).c_str());
        ImGui::Spacing();

        if (ImGui::Button("Installer la mise a jour")) {
            launchUpdateScript();
        }
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(lance un script PowerShell)");
    }
    else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Vous avez la derniere version");
        ImGui::Spacing();
        if (ImGui::Button("Verifier les mises a jour")) {
            checkForUpdates();
        }
    }

    if (!update_error.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), ("Erreur: " + update_error).c_str());
    }

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
