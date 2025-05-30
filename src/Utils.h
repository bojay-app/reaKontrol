#pragma once

#include <string>
class MidiSender;

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
