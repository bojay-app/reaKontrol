#include "Constants.h"
#include "Commands.h"
#include <reaper/reaper_plugin_functions.h>
#include <sstream>

namespace {
    int extEditMode = EXT_EDIT_OFF; // private to this translation unit
}

int protocolVersion = 0;
int bankStart = 0;
int bankEnd = 0;

int g_trackInFocus = -1;
bool g_anySolo = false;
int g_soloStateBank[BANK_NUM_TRACKS] = { 0 };
bool g_muteStateBank[BANK_NUM_TRACKS] = { false };

bool g_KKcountInTriggered = false;
int g_KKcountInMetroState = 0;

int getExtEditMode() {
    return extEditMode;
}

void setExtEditMode(int newMode) {
    if (extEditMode != newMode) {
        std::ostringstream msg;
        msg << "[ExtEditMode] '" << getConstantName(extEditMode) << "' -> '" << getConstantName(newMode) << "'\n";
        ShowConsoleMsg(msg.str().c_str());
        extEditMode = newMode;
    }
}

int g_connectedState = KK_NOT_CONNECTED;

#ifdef CONNECTION_DIAGNOSTICS
int log_scanAttempts = 0;
int log_connectAttempts = 0;
#endif
