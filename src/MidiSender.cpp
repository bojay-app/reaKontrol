#include "MidiSender.h"
#include "Commands.h"
#include "reaKontrol.h"
#include <cstring>
#include <sstream>
#include <reaper/reaper_plugin_functions.h>

MidiSender::MidiSender(midi_Output* output) : _output(output) {}

void MidiSender::sendCc(unsigned char command, unsigned char value) {
    if (_output) {
        std::ostringstream msg;
        msg << "[MidiSender] CC Command: " << (int)command << ", Value: " << (int)value << "\n";
        ShowConsoleMsg(msg.str().c_str());
        _output->Send(MIDI_CC, command, value, -1);
    }
}

void MidiSender::sendSysex(unsigned char command,
    unsigned char value,
    unsigned char track,
    const std::string& info) {
    if (!_output) return;

    int length = sizeof(MIDI_SYSEX_BEGIN) + 3 + info.length() + 1;
    MIDI_event_t* event = (MIDI_event_t*)new unsigned char[
        sizeof(MIDI_event_t) - 4 + length];

    event->frame_offset = 0;
    event->size = length;

    memcpy(event->midi_message, MIDI_SYSEX_BEGIN, sizeof(MIDI_SYSEX_BEGIN));
    int pos = sizeof(MIDI_SYSEX_BEGIN);
    event->midi_message[pos++] = command;
    event->midi_message[pos++] = value;
    event->midi_message[pos++] = track;

    if (!info.empty()) {
        memcpy(event->midi_message + pos, info.c_str(), info.length());
        pos += info.length();
    }

    event->midi_message[pos++] = MIDI_SYSEX_END;
    _output->SendMsg(event, -1);
    delete[](unsigned char*)event;
}
