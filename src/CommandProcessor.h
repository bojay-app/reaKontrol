#pragma once

#include "MidiSender.h"

class CommandProcessor {
public:
    explicit CommandProcessor(MidiSender& sender);

    // Main entry point to handle MIDI CC events
    void handle(unsigned char command, unsigned char value);

private:
    MidiSender& midiSender;
    void handleTransport(unsigned char command, unsigned char value);
    void handleMixerKnob(unsigned char command, signed char value);
    void handleTrackControl(unsigned char command, unsigned char value);
    void handleNavigation(unsigned char command, unsigned char value);
    void handleSelectedTrack(unsigned char command, unsigned char value);
    void handleClear(unsigned char command, unsigned char value);
    void handleCount(unsigned char command, unsigned char value);
    void toggleAutomationMode();
    void logCommand(unsigned char command, unsigned char value, const std::string& context);
};
