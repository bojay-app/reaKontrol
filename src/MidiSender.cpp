#include "MidiSender.h"
#include "Commands.h"
#include "reaKontrol.h"
#include <cstring>
#include <sstream>
#include <reaper/reaper_plugin_functions.h>

MidiSender::MidiSender(midi_Output* output) : _output(output) {}

void MidiSender::sendCc(unsigned char command, unsigned char value) {
    if (_output) {
        _output->Send(MIDI_CC, command, value, -1);
    }
}

void MidiSender::sendSysex(unsigned char command,
    unsigned char value,
    unsigned char track,
    const std::string& info) {
    if (!_output) return;

    size_t sysexBeginSize = sizeof(MIDI_SYSEX_BEGIN);
    size_t infoLength = info.length();
    size_t length = sysexBeginSize + 3 + infoLength + 1; // 3 for command, value, track; 1 for MIDI_SYSEX_END

    // Allocate memory for MIDI_event_t with the appropriate size
    MIDI_event_t* event = reinterpret_cast<MIDI_event_t*>(
        new unsigned char[sizeof(MIDI_event_t) - 4 + length]);

    event->frame_offset = 0;
    event->size = static_cast<int>(length); // Explicit cast to suppress warning

    // Copy the SysEx header
    memcpy(event->midi_message, MIDI_SYSEX_BEGIN, sysexBeginSize);
    size_t pos = sysexBeginSize;

    // Add command, value, and track
    event->midi_message[pos++] = command;
    event->midi_message[pos++] = value;
    event->midi_message[pos++] = track;

    // Copy additional info if present
    if (!info.empty()) {
        memcpy(event->midi_message + pos, info.c_str(), infoLength);
        pos += infoLength;
    }

    // Append SysEx end byte
    event->midi_message[pos++] = MIDI_SYSEX_END;

    // Send the MIDI message
    _output->SendMsg(event, -1);

    // Clean up allocated memory
    delete[] reinterpret_cast<unsigned char*>(event);
}

