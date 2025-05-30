#pragma once

#include "MidiSender.h"

class CommandProcessor {
public:
    explicit CommandProcessor(MidiSender& sender);

    // Main entry point to handle MIDI CC events
    void Handle(unsigned char command, unsigned char value);

private:
    MidiSender& midiSender;
    void RefocusBank();
    void ClearAllSelectedTracks();
    void LogCommand(unsigned char command, unsigned char value, const std::string& context);
};
