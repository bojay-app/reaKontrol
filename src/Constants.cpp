#include "Constants.h"
#include "Commands.h"
#include <reaper/reaper_plugin_functions.h>
#include <sstream>
#include "Utils.h"
#include "ActionList.h"

namespace {
    int extEditMode = EXT_EDIT_OFF; // private to this translation unit
}

bool g_debugLogging = false;

int protocolVersion = 0;
int bankStart = 0;
int bankEnd = 0;
int scanTimer = SCAN_T - 1;
int connectCount = 0;


MediaTrack* activeKkInstance = nullptr;
int g_trackInFocus = 0;
bool g_anySolo = false;
int g_soloStateBank[BANK_NUM_TRACKS] = { 0 };
bool g_muteStateBank[BANK_NUM_TRACKS] = { false };

bool g_KKcountInTriggered = false;
int g_KKcountInMetroState = 0;

int g_connectedState = KK_NOT_CONNECTED;

int getExtEditMode() {
    return extEditMode;
}

void setExtEditMode(int newMode) {
    if (extEditMode != newMode) {
        std::ostringstream msg;
        msg << "[ExtEditMode] '" << getConstantName(extEditMode) << "' -> '" << getConstantName(newMode) << "'\n";
        debugLog(msg);
        extEditMode = newMode;
    }

    if (newMode == EXT_EDIT_OFF) {
        // We refresh config for each edit mode switch
        debugLog("Refreshed Configs & Cleared activeKkInstance");
        activeKkInstance = nullptr;
        loadConfigFile();
    }
}

#ifdef CONNECTION_DIAGNOSTICS
int log_scanAttempts = 0;
int log_connectAttempts = 0;
#endif
