#include "Utils.h"
#include "reaKontrol.h"
#include <cstring>
#include <cmath>
#include "ActionList.h"
#include "Constants.h"
#include "Commands.h"
#include <string>
#include <vector>
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