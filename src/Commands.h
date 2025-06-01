#pragma once

const unsigned char MIDI_CC = 0xBF;
const unsigned char MIDI_SYSEX_BEGIN[] = {
	0xF0, 0x00, 0x21, 0x09, 0x00, 0x00, 0x44, 0x43, 0x01, 0x00};
const unsigned char MIDI_SYSEX_END = 0xF7;

const unsigned char CMD_HELLO = 0x01;
const unsigned char CMD_GOODBYE = 0x02;
const unsigned char CMD_PLAY = 0x10;
const unsigned char CMD_RESTART = 0x11;
const unsigned char CMD_REC = 0x12; // ExtEdit: Toggle record arm for selected track
const unsigned char CMD_COUNT = 0x13;
const unsigned char CMD_STOP = 0x14;
const unsigned char CMD_CLEAR = 0x15; // ExtEdit: Remove Selected Track
const unsigned char CMD_LOOP = 0x16; // ExtEdit: Change right edge of time selection +/- 1 beat length
const unsigned char CMD_METRO = 0x17; // ExtEdit: Change project tempo in 1 bpm steps decrease/increase
const unsigned char CMD_TEMPO = 0x18;
const unsigned char CMD_UNDO = 0x20;
const unsigned char CMD_REDO = 0x21;
const unsigned char CMD_QUANTIZE = 0x22;
const unsigned char CMD_AUTO = 0x23;
const unsigned char CMD_NAV_TRACKS = 0x30;
const unsigned char CMD_NAV_BANKS = 0x31;
const unsigned char CMD_NAV_CLIPS = 0x32;
const unsigned char CMD_NAV_SCENES = 0x33; // not used in NIHIA?
const unsigned char CMD_MOVE_TRANSPORT = 0x34;
const unsigned char CMD_MOVE_LOOP = 0x35;
const unsigned char CMD_TRACK_AVAIL = 0x40;
const unsigned char CMD_SET_KK_INSTANCE = 0x41;
const unsigned char CMD_TRACK_SELECTED = 0x42;
const unsigned char CMD_TRACK_MUTED = 0x43;
const unsigned char CMD_TRACK_SOLOED = 0x44;
const unsigned char CMD_TRACK_ARMED = 0x45;
const unsigned char CMD_TRACK_VOLUME_TEXT = 0x46;
const unsigned char CMD_TRACK_PAN_TEXT = 0x47;
const unsigned char CMD_TRACK_NAME = 0x48;
const unsigned char CMD_TRACK_VU = 0x49;
const unsigned char CMD_TRACK_MUTED_BY_SOLO = 0x4A;
const unsigned char CMD_KNOB_VOLUME0 = 0x50;
const unsigned char CMD_KNOB_VOLUME1 = 0x51;
const unsigned char CMD_KNOB_VOLUME2 = 0x52;
const unsigned char CMD_KNOB_VOLUME3 = 0x53;
const unsigned char CMD_KNOB_VOLUME4 = 0x54;
const unsigned char CMD_KNOB_VOLUME5 = 0x55;
const unsigned char CMD_KNOB_VOLUME6 = 0x56;
const unsigned char CMD_KNOB_VOLUME7 = 0x57;
const unsigned char CMD_KNOB_PAN0 = 0x58;
const unsigned char CMD_KNOB_PAN1 = 0x59;
const unsigned char CMD_KNOB_PAN2 = 0x5a;
const unsigned char CMD_KNOB_PAN3 = 0x5b;
const unsigned char CMD_KNOB_PAN4 = 0x5c;
const unsigned char CMD_KNOB_PAN5 = 0x5d;
const unsigned char CMD_KNOB_PAN6 = 0x5e;
const unsigned char CMD_KNOB_PAN7 = 0x5f;
const unsigned char CMD_PLAY_CLIP = 0x60; // ExtEdit: Insert track
const unsigned char CMD_STOP_CLIP = 0x61; // Enter extEditMode
const unsigned char CMD_PLAY_SCENE = 0x62; // not used in NIHIA?
const unsigned char CMD_RECORD_SESSION = 0x63; // not used in NIHIA?
const unsigned char CMD_CHANGE_SEL_TRACK_VOLUME = 0x64;
const unsigned char CMD_CHANGE_SEL_TRACK_PAN = 0x65;
const unsigned char CMD_TOGGLE_SEL_TRACK_MUTE = 0x66; // Attention(!): NIHIA 1.8.7 used SysEx, NIHIA 1.8.8 uses Cc
const unsigned char CMD_TOGGLE_SEL_TRACK_SOLO = 0x67; // Attention(!): NIHIA 1.8.7 used SysEx, NIHIA 1.8.8 uses Cc
const unsigned char CMD_SEL_TRACK_AVAILABLE = 0x68; // Attention(!): NIHIA 1.8.7 used SysEx, NIHIA 1.8.8 uses Cc
const unsigned char CMD_SEL_TRACK_MUTED_BY_SOLO = 0x69; // Attention(!): NIHIA 1.8.7 used SysEx, NIHIA 1.8.8 uses Cc

inline const char* getCommandName(unsigned char cmd) {
    switch (cmd) {
        case CMD_HELLO: return "CMD_HELLO";
        case CMD_GOODBYE: return "CMD_GOODBYE";
        case CMD_PLAY: return "CMD_PLAY";
        case CMD_RESTART: return "CMD_RESTART";
        case CMD_REC: return "CMD_REC";
        case CMD_COUNT: return "CMD_COUNT";
        case CMD_STOP: return "CMD_STOP";
        case CMD_CLEAR: return "CMD_CLEAR";
        case CMD_LOOP: return "CMD_LOOP";
        case CMD_METRO: return "CMD_METRO";
        case CMD_TEMPO: return "CMD_TEMPO";
        case CMD_UNDO: return "CMD_UNDO";
        case CMD_REDO: return "CMD_REDO";
        case CMD_QUANTIZE: return "CMD_QUANTIZE";
        case CMD_AUTO: return "CMD_AUTO";
        case CMD_NAV_TRACKS: return "CMD_NAV_TRACKS";
        case CMD_NAV_BANKS: return "CMD_NAV_BANKS";
        case CMD_NAV_CLIPS: return "CMD_NAV_CLIPS";
        case CMD_NAV_SCENES: return "CMD_NAV_SCENES";
        case CMD_MOVE_TRANSPORT: return "CMD_MOVE_TRANSPORT";
        case CMD_MOVE_LOOP: return "CMD_MOVE_LOOP";
        case CMD_TRACK_AVAIL: return "CMD_TRACK_AVAIL";
        case CMD_SET_KK_INSTANCE: return "CMD_SET_KK_INSTANCE";
        case CMD_TRACK_SELECTED: return "CMD_TRACK_SELECTED";
        case CMD_TRACK_MUTED: return "CMD_TRACK_MUTED";
        case CMD_TRACK_SOLOED: return "CMD_TRACK_SOLOED";
        case CMD_TRACK_ARMED: return "CMD_TRACK_ARMED";
        case CMD_TRACK_VOLUME_TEXT: return "CMD_TRACK_VOLUME_TEXT";
        case CMD_TRACK_PAN_TEXT: return "CMD_TRACK_PAN_TEXT";
        case CMD_TRACK_NAME: return "CMD_TRACK_NAME";
        case CMD_TRACK_VU: return "CMD_TRACK_VU";
        case CMD_TRACK_MUTED_BY_SOLO: return "CMD_TRACK_MUTED_BY_SOLO";
        case CMD_KNOB_VOLUME0: return "CMD_KNOB_VOLUME0";
        case CMD_KNOB_VOLUME1: return "CMD_KNOB_VOLUME1";
        case CMD_KNOB_VOLUME2: return "CMD_KNOB_VOLUME2";
        case CMD_KNOB_VOLUME3: return "CMD_KNOB_VOLUME3";
        case CMD_KNOB_VOLUME4: return "CMD_KNOB_VOLUME4";
        case CMD_KNOB_VOLUME5: return "CMD_KNOB_VOLUME5";
        case CMD_KNOB_VOLUME6: return "CMD_KNOB_VOLUME6";
        case CMD_KNOB_VOLUME7: return "CMD_KNOB_VOLUME7";
        case CMD_KNOB_PAN0: return "CMD_KNOB_PAN0";
        case CMD_KNOB_PAN1: return "CMD_KNOB_PAN1";
        case CMD_KNOB_PAN2: return "CMD_KNOB_PAN2";
        case CMD_KNOB_PAN3: return "CMD_KNOB_PAN3";
        case CMD_KNOB_PAN4: return "CMD_KNOB_PAN4";
        case CMD_KNOB_PAN5: return "CMD_KNOB_PAN5";
        case CMD_KNOB_PAN6: return "CMD_KNOB_PAN6";
        case CMD_KNOB_PAN7: return "CMD_KNOB_PAN7";
        case CMD_PLAY_CLIP: return "CMD_PLAY_CLIP";
        case CMD_STOP_CLIP: return "CMD_STOP_CLIP";
        case CMD_PLAY_SCENE: return "CMD_PLAY_SCENE";
        case CMD_RECORD_SESSION: return "CMD_RECORD_SESSION";
        case CMD_CHANGE_SEL_TRACK_VOLUME: return "CMD_CHANGE_SEL_TRACK_VOLUME";
        case CMD_CHANGE_SEL_TRACK_PAN: return "CMD_CHANGE_SEL_TRACK_PAN";
        case CMD_TOGGLE_SEL_TRACK_MUTE: return "CMD_TOGGLE_SEL_TRACK_MUTE";
        case CMD_TOGGLE_SEL_TRACK_SOLO: return "CMD_TOGGLE_SEL_TRACK_SOLO";
        case CMD_SEL_TRACK_AVAILABLE: return "CMD_SEL_TRACK_AVAILABLE";
        case CMD_SEL_TRACK_MUTED_BY_SOLO: return "CMD_SEL_TRACK_MUTED_BY_SOLO";
        default: return "UNKNOWN";
    }
}