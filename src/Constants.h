// Compile-time constants
constexpr int BANK_NUM_TRACKS = 8;
constexpr unsigned char TRTYPE_UNSPEC = 1;
constexpr unsigned char TRTYPE_MIDI = 2;
constexpr unsigned char TRTYPE_AUDIO = 3;
constexpr unsigned char TRTYPE_GROUP = 4;
constexpr unsigned char TRTYPE_BUS = 5;
constexpr unsigned char TRTYPE_MASTER = 6;

constexpr bool HIDE_MUTED_BY_SOLO = false;

constexpr int FLASH_T = 16;
constexpr int CYCLE_T = 8;
constexpr int SCAN_T = 90;
constexpr int CONNECT_N = 2;

constexpr int EXT_EDIT_OFF = 0; // no Extended Edit, Normal Mode. flashTimer = -1 
constexpr int EXT_EDIT_ON = 1; // Extended Edit 1st stage commands
constexpr int EXT_EDIT_LOOP = 2; // Extended Edit LOOP
constexpr int EXT_EDIT_TEMPO = 3; // Extended Edit TEMPO
constexpr int EXT_EDIT_ACTIONS = 4; // Extended Edit ACTIONS. flashTimer = -4

constexpr int KK_NOT_CONNECTED = 0; // not connected / scanning
constexpr int KK_MIDI_FOUND = 1; // KK MIDI device found / trying to connect to NIHIA
constexpr int KK_NIHIA_CONNECTED = 2; // NIHIA HELLO acknowledged / fully connected

#define CSURF_EXT_SETMETRONOME 0x00010002

// Global variables
extern int g_trackInFocus;
extern bool g_anySolo;
extern int g_soloStateBank[BANK_NUM_TRACKS];
extern bool g_muteStateBank[BANK_NUM_TRACKS];

extern bool g_KKcountInTriggered;
extern int g_KKcountInMetroState;

extern int g_extEditMode;

extern int g_connectedState;

#ifdef CONNECTION_DIAGNOSTICS
extern int log_scanAttempts;
extern int log_connectAttempts;
#endif