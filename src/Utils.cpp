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


static const char* kk_device_names[] = {
    "MIDIIN2 (KONTROL S61 MK3)",
    "MIDIOUT2 (KONTROL S61 MK3)",
    nullptr
};

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