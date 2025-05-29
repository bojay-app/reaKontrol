#include "Constants.h"
#include "Commands.h"

int g_trackInFocus = -1;
bool g_anySolo = false;
int g_soloStateBank[BANK_NUM_TRACKS] = { 0 };
bool g_muteStateBank[BANK_NUM_TRACKS] = { false };

bool g_KKcountInTriggered = false;
int g_KKcountInMetroState = 0;

int g_extEditMode = EXT_EDIT_OFF;

int g_connectedState = KK_NOT_CONNECTED;

#ifdef CONNECTION_DIAGNOSTICS
int log_scanAttempts = 0;
int log_connectAttempts = 0;
#endif
