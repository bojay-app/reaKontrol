#pragma once

#include <string>

class midi_Output;

class MidiSender {
public:
    explicit MidiSender(midi_Output* output);

    void sendCc(unsigned char command, unsigned char value);

    void sendSysex(unsigned char command,
                   unsigned char value,
                   unsigned char track,
                   const std::string& info = "");

private:
    midi_Output* _output;
};
