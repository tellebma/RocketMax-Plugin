# CLAUDE.md - RocketMax Plugin

Guide de référence pour les assistants IA travaillant sur ce projet BakkesMod plugin.

## Aperçu du projet

RocketMax est un plugin BakkesMod pour Rocket League qui :
- Collecte les statistiques de fin de match (MMR, victoires/défaites, stats individuelles)
- Envoie les données à un serveur externe (`https://rocketmax.tellebma.fr`)
- Affiche un overlay de session en temps réel
- Offre un système d'authentification HMAC-SHA256
- Propose une mise à jour automatique via GitHub releases

**Technologies principales** : C++20, BakkesMod SDK, ImGui, Windows CryptAPI, MSBuild

## Structure du projet

```
RocketMax-Plugin/
├── RocketMax/                    # Code source principal
│   ├── RocketMax.h               # Classe principale et définitions
│   ├── RocketMax.cpp             # Implémentation (hooks, API, logique)
│   ├── RocketMaxSettings.cpp     # Interface ImGui des paramètres
│   ├── GuiBase.h/cpp            # Classes de base pour ImGui
│   ├── logging.h                 # Système de log (C++20 std::format)
│   ├── version.h                 # Version sémantique (auto-mise à jour par CI)
│   ├── pch.h/cpp                # Headers précompilés
│   ├── httplib.h                 # Librairie HTTP (non utilisée - utilise HttpWrapper)
│   ├── IMGUI/                    # Bibliothèque ImGui + widgets personnalisés
│   ├── BakkesMod.props           # Configuration SDK BakkesMod (local)
│   └── RocketMax.vcxproj         # Projet Visual Studio
├── plugins/                      # Sortie de build (DLL compilée)
├── .github/workflows/build.yml   # Pipeline CI/CD
├── build-local.ps1               # Script build dev local
├── build-prod.ps1                # Script build production
├── install-rocketmax.ps1         # Script d'installation utilisateur
└── RocketMax.sln                 # Solution Visual Studio
```

## Fichiers principaux

| Fichier | Rôle | Points clés |
|---------|------|-------------|
| `RocketMax.cpp` | Cœur du plugin | Hooks de match, collecte stats, API, offline queue |
| `RocketMax.h` | Déclarations | CVars, variables d'état, constantes API |
| `RocketMaxSettings.cpp` | Interface utilisateur | Paramètres ImGui, overlay session |
| `logging.h` | Logging | Template variadic avec `std::format` |
| `version.h` | Versioning | Macros VERSION_MAJOR/MINOR/PATCH/BUILD |

## Conventions de code

### Nommage
- **Classes** : `PascalCase` (ex: `RocketMax`, `ServerWrapper`)
- **Méthodes** : `camelCase` (ex: `gameHasEnded`, `getMmrData`)
- **Variables membres** : `snake_case` (ex: `game_running`, `mmr_avant_match`)
- **Constantes/Macros** : `UPPER_SNAKE_CASE` (ex: `API_ENDPOINT`, `HOOK_MATCH_ENDED`)

### Langue
- **Variables et commentaires** : Français (majoritaire)
- **API/Logs** : Mix français/anglais
- **Interface utilisateur** : Français

### Patterns utilisés
- `std::atomic<>` pour les flags asynchrones (update_available, update_checking)
- `std::shared_ptr<bool/string>` pour les bindings CVars
- Lambdas avec `gameWrapper->Execute()` pour les opérations thread-safe UI
- `gameWrapper->SetTimeout()` pour les opérations différées (ex: attente MMR après match)

## Hooks de jeu

```cpp
#define HOOK_MATCH_START "Function GameEvent_TA.Countdown.BeginState"
#define HOOK_MATCH_ENDED "Function TAGame.GameEvent_Soccar_TA.EventMatchEnded"
```

**Flux d'événements** :
1. `gameStart` → Détecte début de match ranked, capture MMR initial
2. `gameEnd` → Collecte stats, attend 5s (timeout pour mise à jour MMR serveur)
3. `gameHasEnded` → Envoie données API, met à jour session

## Configuration (CVars)

| CVar | Type | Description |
|------|------|-------------|
| `rocketmax_enable_toasts` | bool | Notifications toast |
| `rocketmax_enable_overlay` | bool | Overlay de session |
| `rocketmax_enable_streak_alerts` | bool | Alertes de série |
| `rocketmax_enable_auto_update` | bool | Vérification MAJ auto |
| `rocketmax_server_url` | string | URL serveur API |
| `rocketmax_hide_profile` | bool | Profil privé |
| `rocketmax_auth_secret` | string | Secret HMAC (caché, persistant) |
| `rocketmax_access_token` | string | Token d'accès profil privé |
| `rocketmax_profile_url` | string | URL profil généré |

## API Endpoints

Base URL: `https://rocketmax.tellebma.fr` (configurable)

| Endpoint | Méthode | Description |
|----------|---------|-------------|
| `/initPlayer` | POST | Initialisation joueur, récupération auth_secret |
| `/updateMmr` | POST | Mise à jour MMR (authentifié) |
| `/updateHistorique` | POST | Historique de match (authentifié) |
| `/setVisibility` | POST | Visibilité profil (authentifié) |
| `/regenerateAccessToken` | POST | Nouveau token privé (authentifié) |

**Authentification** : Header `X-Signature` avec HMAC-SHA256(body, auth_secret)

## Build et développement

### Prérequis
- Visual Studio 2022 avec "Desktop development with C++"
- BakkesMod installé (registre Windows)
- PowerShell 5.0+

### Build local
```powershell
.\build-local.ps1  # Configure localhost:8080, compile, installe
```

### Build CI/CD
1. Push sur `main` → semantic-release analyse les commits
2. `feat:` → version minor, `fix:` → patch, `BREAKING CHANGE:` → major
3. Build Windows → GitHub Release avec DLL

### Conventions de commit
```
feat: nouvelle fonctionnalité       → 0.X.0
fix: correction de bug              → 0.0.X
perf: amélioration performances     → 0.0.X
docs: documentation seulement       → pas de release
chore: maintenance                  → pas de release
```

## Points d'attention pour modifications

### Thread Safety
- Les callbacks HTTP sont asynchrones - utiliser `gameWrapper->Execute()` pour le UI
- Les `std::atomic<bool>` sont utilisés pour les flags (`update_available`, etc.)
- Les strings partagées (`latest_version`, `update_download_url`, `update_error`) sont protégées par `update_mutex`

### Capture de variables dans lambdas
```cpp
// Pattern utilisé pour les timeouts (capture par valeur)
int captured_playlistId = playlistId;
int captured_mmr_avant = mmr_avant_match;
gameWrapper->SetTimeout([this, captured_playlistId, captured_mmr_avant](GameWrapper* gw) {
    // Utiliser les valeurs capturées, pas les membres
}, MMR_UPDATE_DELAY_SECONDS);
```

### Constants Namespace
Les constantes sont définies dans `RocketMaxConstants` :
```cpp
namespace RocketMaxConstants {
    constexpr float MMR_UPDATE_DELAY_SECONDS = 5.0f;
    constexpr float TOAST_DURATION_DEFAULT = 5.0f;
    constexpr int INVALID_PLAYLIST_ID = 100;
    constexpr int INVALID_TEAM_NUM = -1;
}
```

### JSON Manual Parsing
Le code utilise `extractJsonValue()` pour parser le JSON manuellement.
Fragile avec les cas edge (strings avec guillemets échappés, valeurs nested).

### Logging avec std::format
Les accolades `{}` sont des placeholders. Pour logger du JSON :
```cpp
LOG("Json: " + escapeForLog(jsonString));  // Escape {} → {{}}
```

## Tests et validation

**Actuellement non implémenté** - le plugin n'a pas de tests automatisés.

Pour tester manuellement :
1. Lancer le backend Flask local (`python app.py`)
2. Compiler avec `build-local.ps1`
3. Lancer Rocket League avec BakkesMod
4. Jouer un match ranked
5. Vérifier les logs BakkesMod (F6 → Console)

## Améliorations futures recommandées

1. Utiliser une vraie librairie JSON (nlohmann/json ou rapidjson) pour un parsing plus robuste
2. Encapsuler les CVars dans une structure dédiée
3. Ajouter des tests unitaires
4. Séparer la logique API dans une classe dédiée

## Ressources

- [BakkesMod SDK Documentation](https://wiki.bakkesplugins.com/)
- [ImGui Documentation](https://github.com/ocornut/imgui)
- [Frontend GitHub](https://github.com/tellebma/RocketMax-FrontEnd)
- [Site web](https://rocketmax.tellebma.fr)
