#pragma once

class MediaTrack;

// Compile-time constants
constexpr int BANK_NUM_TRACKS = 8;
constexpr unsigned char TRTYPE_UNSPEC = 1;
constexpr unsigned char TRTYPE_MIDI = 2;
constexpr unsigned char TRTYPE_AUDIO = 3;
constexpr unsigned char TRTYPE_GROUP = 4;
constexpr unsigned char TRTYPE_BUS = 5;
constexpr unsigned char TRTYPE_MASTER = 6;

constexpr int DOUBLE_CLICK_THRESHOLD = 20;
constexpr int CLICK_COOLDOWN = 20;
constexpr const char* EVENT_CLICK_SINGLE = "SINGLE";
constexpr const char* EVENT_CLICK_DOUBLE = "DOUBLE";

constexpr bool HIDE_MUTED_BY_SOLO = false;

constexpr int FLASH_T = 16;
constexpr int CYCLE_T = 6;
constexpr int SCAN_T = 90;
constexpr int CONNECT_N = 2;

constexpr int EXT_EDIT_OFF = 0; // no Extended Edit, Normal Mode. flashTimer = -1 
constexpr int EXT_EDIT_ON = 1; // Extended Edit 1st stage commands
constexpr int EXT_EDIT_LOOP = 2; // Extended: Set Loop Range
constexpr int EXT_EDIT_TEMPO = 3; // Extended Edit TEMPO

constexpr int KK_NOT_CONNECTED = 0; // not connected / scanning
constexpr int KK_MIDI_FOUND = 1; // KK MIDI device found / trying to connect to NIHIA
constexpr int KK_NIHIA_CONNECTED = 2; // NIHIA HELLO acknowledged / fully connected

#define CSURF_EXT_SETMETRONOME 0x00010002

// Global variables
extern bool g_debugLogging;
extern int nextOpenTimer; // Cooldown time for processing next click events

extern int protocolVersion;
extern int bankStart;
extern int bankEnd;
extern int scanTimer;
extern int connectCount;

extern int g_trackInFocus;
extern bool g_anySolo;
extern int g_soloStateBank[BANK_NUM_TRACKS];
extern bool g_muteStateBank[BANK_NUM_TRACKS];

extern bool g_KKcountInTriggered;
extern int g_KKcountInMetroState;

int getExtEditMode();
void setExtEditMode(int newMode);

extern int g_connectedState;

#ifdef CONNECTION_DIAGNOSTICS
extern int log_scanAttempts;
extern int log_connectAttempts;
#endif

inline const char* getConstantName(unsigned char cons) {
    switch (cons) {
    case EXT_EDIT_OFF: return "EXT_EDIT_OFF";
    case EXT_EDIT_ON: return "EXT_EDIT_ON";
    case EXT_EDIT_LOOP: return "EXT_EDIT_LOOP";
    case EXT_EDIT_TEMPO: return "EXT_EDIT_TEMPO";
    default: return "UNKNOWN";
    }
}