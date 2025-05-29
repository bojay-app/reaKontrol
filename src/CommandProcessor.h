#pragma once

#include "MidiSender.h"

class CommandProcessor {
public:
    explicit CommandProcessor(MidiSender& sender);

    // Main entry point to handle MIDI CC events
    void handle(unsigned char command, unsigned char value);

private:
    MidiSender& midiSender;
    bool handleTransport(unsigned char command, unsigned char value);
    bool handleMixerKnob(unsigned char command, signed char value);
    bool handleTrackControl(unsigned char command, unsigned char value);
    bool handleNavigation(unsigned char command, unsigned char value);
    bool handleSelectedTrack(unsigned char command, unsigned char value);
    bool handleClear(unsigned char command, unsigned char value);
    bool handleCount(unsigned char command, unsigned char value);
    void toggleAutomationMode();
    void logCommand(unsigned char command, unsigned char value, const std::string& context);
};
