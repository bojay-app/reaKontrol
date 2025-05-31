#pragma once

#include <string>
#include <unordered_map>
#include <functional>

class MidiSender;
class MediaTrack;
struct reaper_plugin_info_t;

struct KKPluginInfo {
    std::string renamedName;
    std::string fxIdent;
    std::string fxName;
};

// Structure to hold action information
struct ReaKontrolAction {
    std::string idstr;
    std::string description;
    std::function<void()> callback;
};

KKPluginInfo getKkInstanceInfo(MediaTrack* track);

// Initializes the action registry
void InitActionRegistry(reaper_plugin_info_t* rec);

// Registers a new action
bool RegisterAction(const ReaKontrolAction& action);

// Unregisters all actions
void UnregisterAllActions();

void toggleDAW(MidiSender* midiSender);

// Returns index of matching MIDI input device (or -1 if not found)
int getKkMidiInput();

// Returns index of matching MIDI output device (or -1 if not found)
int getKkMidiOutput();

// Convert 7-bit MIDI value to signed char (-64 to +63)
signed char convertSignedMidiValue(unsigned char value);

// Convert Reaper pan value (-1.0 to +1.0) to MIDI (0-127)
unsigned char panToChar(double pan);

// Convert Reaper volume to Komplete Kontrol Mk2 display value
unsigned char volToChar_KkMk2(double volume);

void showTempoInMixer(MidiSender* midiSender);
void metronomeUpdate(MidiSender* midiSender);
void allMixerUpdate(MidiSender* midiSender);
int getMetronomeState();
void enableRecCountIn();
void disableRecCountIn();

void* GetConfigVar(const char* cVar);

bool adjustTrackVolume(MediaTrack* track, signed char midiDelta);
bool adjustTrackPan(MediaTrack* track, signed char midiDelta);

bool toggleTrackMute(MediaTrack* track);
bool toggleTrackSolo(MediaTrack* track);
void peakMixerUpdate(MidiSender* midiSender);

void debugLog(const std::string& msg);
void debugLog(const std::ostringstream& msgStream);
