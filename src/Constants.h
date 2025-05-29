const int BANK_NUM_TRACKS = 8;
const unsigned char TRTYPE_UNSPEC = 1;
const unsigned char TRTYPE_MIDI = 2;
const unsigned char TRTYPE_AUDIO = 3;
const unsigned char TRTYPE_GROUP = 4;
const unsigned char TRTYPE_BUS = 5;
const unsigned char TRTYPE_MASTER = 6;

const bool HIDE_MUTED_BY_SOLO = false; // Meter Setting: If TRUE peak levels will not be shown in Mixer view of muted by solo tracks. If FALSE they will be shown but greyed out.
const int FLASH_T = 16; // value devided by 30 -> button light flash interval time in Extended Edit Mode
const int CYCLE_T = 8; // value devided by 30 -> 4D encoder LED cycle interval time in Extended Edit Mode
const int SCAN_T = 90; // value devided by 30 -> Scan interval time to check for Komplete Kontrol MIDI device plugged in (hot plugging)
const int CONNECT_N = 2; // number of connection attempts to switch detected keyboard to NiMidi Mode

#define CSURF_EXT_SETMETRONOME 0x00010002

// State Information (Cache)
// ToDo: Rather than using glocal variables consider moving these into the BaseSurface class declaration
static int g_trackInFocus = -1;
static bool g_anySolo = false;
static int g_soloStateBank[BANK_NUM_TRACKS] = { 0 };
static bool g_muteStateBank[BANK_NUM_TRACKS] = { false };

static bool g_KKcountInTriggered = false; // to discriminate if COUNT IN was requested by keyboard or from Reaper
static int g_KKcountInMetroState = 0; // to store metronome state when COUNT IN was requested by keyboard

// Connection Status State Variables
const int KK_NOT_CONNECTED = 0; // not connected / scanning
const int KK_MIDI_FOUND = 1; // KK MIDI device found / trying to connect to NIHIA
const int KK_NIHIA_CONNECTED = 2; // NIHIA HELLO acknowledged / fully connected
static int g_connectedState = KK_NOT_CONNECTED; 

# ifdef CONNECTION_DIAGNOSTICS
static int log_scanAttempts = 0;
static int log_connectAttempts = 0;
# endif

// Global action list structure for ReaKontrol
const int A_NAME_MAX = 128;