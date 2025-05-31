#include <cstring>
#include <cmath>
#include <string>
#include <vector>

#include "Utils.h"
#include "reaKontrol.h"
#include "ActionList.h"
#include "Constants.h"
#include "Commands.h"
#include "MidiSender.h"
#include <sstream>
#include <reaper/reaper_plugin.h>
#include <reaper/reaper_plugin_functions.h>

static const char KK_VST_PREFIX[] = "VSTi: Kontakt";
static char KK_VST3_PREFIX[] = "VST3i: Kontakt";

static const char* kk_device_names[] = {
    "MIDIIN2 (KONTROL S61 MK3)",
    "MIDIOUT2 (KONTROL S61 MK3)"
};

static reaper_plugin_info_t* g_rec = nullptr;
static std::unordered_map<int, std::function<void()>> g_actionCallbacks;
static std::unordered_map<int, gaccel_register_t> g_registeredActions;
static std::vector<std::string> g_actionDescriptions; // To store descriptions and maintain their lifetime

void activateKkInstance(MediaTrack* track) {
    if (!track) return;

    const int fxCount = TrackFX_GetCount(track);
    char fxName[512];

    for (int fxIndex = 0; fxIndex < fxCount; ++fxIndex) {
        if (!TrackFX_GetFXName(track, fxIndex, fxName, sizeof(fxName))) {
            continue;
        }
        if (strstr(fxName, "Komplete Kontrol") || strstr(fxName, "Kontakt")) {
            // 🔄 Force plugin to re-register with NIHIA
            TrackFX_SetOffline(track, fxIndex, true);
            TrackFX_SetOffline(track, fxIndex, false);
            break;
        }
    }
}

// Hook function to handle actions
static bool HookCommandProc(int command, int flag) {
    auto it = g_actionCallbacks.find(command);
    if (it != g_actionCallbacks.end()) {
        it->second();
        return true;
    }
    return false;
}

void InitActionRegistry(reaper_plugin_info_t* rec) {
    g_rec = rec;
    g_rec->Register("hookcommand", (void*)HookCommandProc);
}

bool RegisterAction(const ReaKontrolAction& action) {
    if (!g_rec) return false;

    // Request a unique command ID
    int commandId = (int)(intptr_t)g_rec->Register("command_id", (void*)action.idstr.c_str());
    if (commandId == 0) return false;

    // Store the description to maintain its lifetime
    g_actionDescriptions.push_back(action.description);
    const char* desc_cstr = g_actionDescriptions.back().c_str();

    // Register the action with a description
    gaccel_register_t accel = {};
    accel.accel.cmd = commandId;
    accel.accel.fVirt = 0; // No default shortcut
    accel.accel.key = 0;
    accel.desc = desc_cstr;

    if (!g_rec->Register("gaccel", &accel)) return false;

    // Store the callback and accel for later use
    g_actionCallbacks[commandId] = action.callback;
    g_registeredActions[commandId] = accel;

    return true;
}

void UnregisterAllActions() {
    if (!g_rec) return;

    for (const auto& pair : g_registeredActions) {
        g_rec->Register("-gaccel", (void*)&pair.second);
    }
    g_rec->Register("-hookcommand", (void*)HookCommandProc);
    g_actionCallbacks.clear();
    g_registeredActions.clear();
    g_actionDescriptions.clear();
}

int getKkMidiInput() {
    int count = GetNumMIDIInputs();
    for (int dev = 0; dev < count; ++dev) {
        char name[60];
        bool present = GetMIDIInputName(dev, name, sizeof(name));
        if (!present) continue;

        for (int i = 0; kk_device_names[i] != nullptr; ++i) {
            if (strstr(name, kk_device_names[i])) {
                return dev;
            }
        }
    }
    return -1;
}

int getKkMidiOutput() {
    int count = GetNumMIDIOutputs();
    for (int dev = 0; dev < count; ++dev) {
        char name[60];
        bool present = GetMIDIOutputName(dev, name, sizeof(name));
        if (!present) continue;

        for (int i = 0; kk_device_names[i] != nullptr; ++i) {
            if (strstr(name, kk_device_names[i])) {
                return dev;
            }
        }
    }
    return -1;
}

signed char convertSignedMidiValue(unsigned char value) {
    return (value <= 63) ? value : value - 128;
}

unsigned char panToChar(double pan) {
    pan = (pan + 1.0) * 63.5;
    if (pan < 0.0) pan = 0.0;
    else if (pan > 127.0) pan = 127.0;
    return static_cast<unsigned char>(pan + 0.5);
}

unsigned char volToChar_KkMk2(double volume) {
    constexpr double minus48dB = 0.00398107170553497250;
    constexpr double minus96dB = 1.5848931924611134E-05;
    constexpr double m = (16.0 - 2.0) / (minus48dB - minus96dB);
    constexpr double n = 16.0 - m * minus48dB;
    constexpr double a = -32.391538612390192;
    constexpr double b = 30.673618643561021;
    constexpr double c = 86.720798984917224;
    constexpr double d = 4.4920143012996103;

    double result = 0.0;
    if (volume > minus48dB)
        result = a + b * log(c * volume + d);
    else if (volume > minus96dB)
        result = m * volume + n;
    else
        result = 0.5;

    if (result > 126.5)
        result = 126.5;

    return static_cast<unsigned char>(result + 0.5);
}

void showTempoInMixer(MidiSender* midiSender) {
    double time = GetCursorPosition();
    int timesig_numOut = 0;
    int timesig_denomOut = 0;
    double tempoOut = 0;
    TimeMap_GetTimeSigAtTime(nullptr, time, &timesig_numOut, &timesig_denomOut, &tempoOut);

    std::string s = std::to_string((int)(tempoOut + 0.5)) + " BPM";
    std::vector<char> bpmText(s.begin(), s.end());
    bpmText.push_back('\0');
    char* name = &bpmText[0];
    if (midiSender) {
        midiSender->sendSysex(CMD_TRACK_VOLUME_TEXT, 0, 0, name);
    }
}

void allMixerUpdate(MidiSender* midiSender) {
    int numInBank = 0;
    bankEnd = bankStart + BANK_NUM_TRACKS - 1; // avoid ambiguity: track counting always zero based
    int numTracks = CSurf_NumTracks(false);
    // Update bank select button lights
    // ToDo: Consider optimizing this piece of code
    int bankLights = 3; // left and right on
    if (numTracks < BANK_NUM_TRACKS) {
        bankLights = 0; // left and right off
    }
    else if (bankStart == 0) {
        bankLights = 2; // left off, right on
    }
    else if (bankEnd >= numTracks) {
        bankLights = 1; // left on, right off
    }
    midiSender->sendCc(CMD_NAV_BANKS, bankLights);
    if (bankEnd > numTracks) {
        bankEnd = numTracks;
        // Mark additional bank tracks as not available
        int lastInLastBank = numTracks % BANK_NUM_TRACKS;
        for (int i = 7; i > lastInLastBank; --i) {
            midiSender->sendSysex(CMD_TRACK_AVAIL, 0, i);
        }
    }
    // Update 4D Encoder track navigation LEDs
    int trackNavLights = 3; // left and right on
    if (g_trackInFocus < 2) {
        trackNavLights &= 2; // left off
    }
    if (g_trackInFocus >= numTracks) {
        trackNavLights &= 1; // right off
    }
    midiSender->sendCc(CMD_NAV_TRACKS, trackNavLights);
    // Update current bank
    for (int id = bankStart; id <= bankEnd; ++id, ++numInBank) {
        MediaTrack* track = CSurf_TrackFromID(id, false);
        if (!track) {
            break;
        }
        // Master track needs special consideration: no soloing, no record arm
        if (id == 0) {
            midiSender->sendSysex(CMD_TRACK_AVAIL, TRTYPE_MASTER, 0);
            midiSender->sendSysex(CMD_TRACK_NAME, 0, 0, "MASTER");
            midiSender->sendSysex(CMD_TRACK_SOLOED, 0, 0);
            midiSender->sendSysex(CMD_TRACK_MUTED_BY_SOLO, 0, 0);
            midiSender->sendSysex(CMD_TRACK_ARMED, 0, 0);
        }
        // Ordinary tracks can be soloed and record armed
        else {
            midiSender->sendSysex(CMD_TRACK_AVAIL, TRTYPE_UNSPEC, numInBank);
            int soloState = *(int*)GetSetMediaTrackInfo(track, "I_SOLO", nullptr);
            if (soloState == 0) {
                g_soloStateBank[numInBank] = 0;
                midiSender->sendSysex(CMD_TRACK_SOLOED, 0, numInBank);
                midiSender->sendSysex(CMD_TRACK_MUTED_BY_SOLO, g_anySolo ? 1 : 0, numInBank);
            }
            else {
                g_soloStateBank[numInBank] = 1;
                midiSender->sendSysex(CMD_TRACK_SOLOED, 1, numInBank);
                midiSender->sendSysex(CMD_TRACK_MUTED_BY_SOLO, 0, numInBank);
            }
            int armed = *(int*)GetSetMediaTrackInfo(track, "I_RECARM", nullptr);
            midiSender->sendSysex(CMD_TRACK_ARMED, armed, numInBank);
            char* name = (char*)GetSetMediaTrackInfo(track, "P_NAME", nullptr);
            if ((!name) || (*name == '\0')) {
                std::string s = "TRACK " + std::to_string(id);
                std::vector<char> nameGeneric(s.begin(), s.end()); // memory safe conversion to C style char
                nameGeneric.push_back('\0');
                name = &nameGeneric[0];
            }
            midiSender->sendSysex(CMD_TRACK_NAME, 0, numInBank, name);
        }
        midiSender->sendSysex(CMD_TRACK_SELECTED, id == g_trackInFocus ? 1 : 0, numInBank);
        bool muted = *(bool*)GetSetMediaTrackInfo(track, "B_MUTE", nullptr);
        g_muteStateBank[numInBank] = muted;
        midiSender->sendSysex(CMD_TRACK_MUTED, muted ? 1 : 0, numInBank);
        double volume = *(double*)GetSetMediaTrackInfo(track, "D_VOL", nullptr);
        char volText[64];
        mkvolstr(volText, volume);
        midiSender->sendSysex(CMD_TRACK_VOLUME_TEXT, 0, numInBank, volText);
        midiSender->sendCc((CMD_KNOB_VOLUME0 + numInBank), volToChar_KkMk2(volume * 1.05925));
        double pan = *(double*)GetSetMediaTrackInfo(track, "D_PAN", nullptr);
        char panText[64];
        mkpanstr(panText, pan);
        midiSender->sendSysex(CMD_TRACK_PAN_TEXT, 0, numInBank, panText); // NIHIA v1.8.7.135 uses internal text
        midiSender->sendCc((CMD_KNOB_PAN0 + numInBank), panToChar(pan));
    }
}

void metronomeUpdate(MidiSender* midiSender) {
	// Actively poll the metronome status on project tab changes
	midiSender->sendCc(CMD_METRO, getMetronomeState());
}

int getMetronomeState() {
    return (*(int*)GetConfigVar("projmetroen") & 1);
}

void enableRecCountIn() {
	*(int*)GetConfigVar("projmetroen") |= 16; 
}

void disableRecCountIn() {
    *(int*)GetConfigVar("projmetroen") &= ~16;
    g_KKcountInTriggered = false;
}

void* GetConfigVar(const char* cVar) { // Copyright (c) 2010 and later Tim Payne (SWS), Jeffos
    int sztmp;
    void* p = NULL;
    if (int iOffset = projectconfig_var_getoffs(cVar, &sztmp))
    {
        p = projectconfig_var_addr(EnumProjects(-1, NULL, 0), iOffset);
    }
    else
    {
        p = get_config_var(cVar, &sztmp);
    }
    return p;
}

bool adjustTrackVolume(MediaTrack* track, signed char midiDelta) {
    if (!track) return false;
    double step = (abs(midiDelta) > 38 ? 1.0 : 0.1) * (midiDelta >= 0 ? 1 : -1);
    CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, step, true), nullptr);
    return true;
}

bool adjustTrackPan(MediaTrack* track, signed char midiDelta) {
    if (!track) return false;
    double step = midiDelta * 0.00098425;
    CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, step, true), nullptr);
    return true;
}

bool toggleTrackMute(MediaTrack* track) {
    if (!track) return false;
    bool muted = *(bool*)GetSetMediaTrackInfo(track, "B_MUTE", nullptr);
    CSurf_OnMuteChange(track, muted ? 0 : 1);
    return true;
}

bool toggleTrackSolo(MediaTrack* track) {
    if (!track) return false;
    int solo = *(int*)GetSetMediaTrackInfo(track, "I_SOLO", nullptr);
    CSurf_OnSoloChange(track, solo == 0 ? 1 : 0);
    return true;
}

void peakMixerUpdate(MidiSender* midiSender) {
    // Peak meters. Note: Reaper reports peak, NOT VU	

    // ToDo: Peak Hold in KK display shall be erased immediately when changing bank
    // ToDo: Peak Hold in KK display shall be erased after decay time t when track muted or no signal.
    // ToDo: Explore the effect of sending CMD_SEL_TRACK_PARAMS_CHANGED after sending CMD_TRACK_VU
    // ToDo: Consider caching and not sending anything via SysEx if no values have changed.

    // Meter information is sent to KK as array (string of chars) for all 16 channels (8 x stereo) of one bank.
    // A value of 0 will result in stopping to refresh meters further to right as it is interpretated as "end of string".
    // peakBank[0]..peakBank[31] are used for data. The array needs one additional last char peakBank[32] set as "end of string" marker.
    static char peakBank[(BANK_NUM_TRACKS * 2) + 1];
    int j = 0;
    double peakValue = 0;
    int numInBank = 0;
    for (int id = bankStart; id <= bankEnd; ++id, ++numInBank) {
        MediaTrack* track = CSurf_TrackFromID(id, false);
        if (!track) {
            break;
        }
        j = 2 * numInBank;
        if (HIDE_MUTED_BY_SOLO) {
            // If any track is soloed then only soloed tracks and the master show peaks (irrespective of their mute state)
            if (g_anySolo) {
                if ((g_soloStateBank[numInBank] == 0) && (((numInBank != 0) && (bankStart == 0)) || (bankStart != 0))) {
                    peakBank[j] = 1;
                    peakBank[j + 1] = 1;
                }
                else {
                    peakValue = Track_GetPeakInfo(track, 0); // left channel
                    peakBank[j] = volToChar_KkMk2(peakValue); // returns value between 1 and 127
                    peakValue = Track_GetPeakInfo(track, 1); // right channel
                    peakBank[j + 1] = volToChar_KkMk2(peakValue); // returns value between 1 and 127
                }
            }
            // If no tracks are soloed then muted tracks shall show no peaks
            else {
                if (g_muteStateBank[numInBank]) {
                    peakBank[j] = 1;
                    peakBank[j + 1] = 1;
                }
                else {
                    peakValue = Track_GetPeakInfo(track, 0); // left channel
                    peakBank[j] = volToChar_KkMk2(peakValue); // returns value between 1 and 127
                    peakValue = Track_GetPeakInfo(track, 1); // right channel
                    peakBank[j + 1] = volToChar_KkMk2(peakValue); // returns value between 1 and 127					
                }
            }
        }
        else {
            // Muted tracks that are NOT soloed shall show no peaks. Tracks muted by solo show peaks but they appear greyed out.
            if ((g_soloStateBank[numInBank] == 0) && (g_muteStateBank[numInBank])) {
                peakBank[j] = 1;
                peakBank[j + 1] = 1;
            }
            else {
                peakValue = Track_GetPeakInfo(track, 0); // left channel
                peakBank[j] = volToChar_KkMk2(peakValue); // returns value between 1 and 127
                peakValue = Track_GetPeakInfo(track, 1); // right channel
                peakBank[j + 1] = volToChar_KkMk2(peakValue); // returns value between 1 and 127					
            }
        }
    }
    peakBank[j + 2] = '\0'; // end of string (no tracks available further to the right)
    midiSender->sendSysex(CMD_TRACK_VU, 2, 0, peakBank);
}

void debugLog(const std::string& msg)
{
    if (!g_debugLogging) return;
    ShowConsoleMsg((msg + "\n").c_str());
}

void debugLog(const std::ostringstream& msgStream) {
    debugLog(msgStream.str());
}
