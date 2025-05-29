#include "CommandProcessor.h"
#include "CommandProcessor.h"
#include "CommandProcessor.h"
#include "Utils.h"
#include "Commands.h"
#include "Constants.h"
#include "reaKontrol.h"
#include <string>
#include <sstream>

CommandProcessor::CommandProcessor(MidiSender& sender)
    : midiSender(sender) {}

void CommandProcessor::handle(unsigned char command, unsigned char value) {
    if (command >= CMD_PLAY && command <= CMD_TEMPO) {
        handleTransport(command, value);
    } 
    else if (command >= CMD_TRACK_SELECTED && command <= CMD_TRACK_MUTED_BY_SOLO) {
        handleTrackControl(command, value);
    } 
    else if (command >= CMD_KNOB_VOLUME0 && command <= CMD_KNOB_PAN7) {
        handleMixerKnob(command, convertSignedMidiValue(value));
    } 
    else if (command == CMD_NAV_TRACKS || command == CMD_NAV_BANKS || command == CMD_NAV_CLIPS) {
        handleNavigation(command, value);
    } 
    else if (command == CMD_CHANGE_SEL_TRACK_VOLUME || command == CMD_CHANGE_SEL_TRACK_PAN ||
        command == CMD_TOGGLE_SEL_TRACK_MUTE || command == CMD_TOGGLE_SEL_TRACK_SOLO) {
        handleSelectedTrack(command, value);
    } 
    else if (command == CMD_CLEAR) {
        handleClear(command, value);
    } 
    else if (command == CMD_COUNT) {
        handleCount(command, value);
    } 
    else {
        logCommand(command, value, "unhandled");
    }
}

void CommandProcessor::handleTransport(unsigned char command, unsigned char value) {
    logCommand(command, value, "transport");
    switch (command) {
        case CMD_PLAY:
            CSurf_OnPlay();
            break;
        case CMD_RESTART:
            CSurf_GoStart();
            if (!(GetPlayState() & 1)) {
                CSurf_OnPlay();
            }
            break;
        case CMD_STOP:
            CSurf_OnStop();
            break;
        case CMD_REC:
            CSurf_OnRecord();
            break;
        case CMD_LOOP:
            Main_OnCommand(1068, 0);
            break;
        case CMD_METRO:
            Main_OnCommand(40364, 0);
            break;
        case CMD_TEMPO:
            Main_OnCommand(1134, 0);
            break;
        case CMD_UNDO:
            Main_OnCommand(40029, 0);
            break;
        case CMD_REDO:
            Main_OnCommand(40030, 0);
            break;
        case CMD_QUANTIZE:
            Main_OnCommand(42033, 0);
            Main_OnCommand(40604, 0);
            break;
        case CMD_AUTO:
            toggleAutomationMode();
            break;
    }
}

void CommandProcessor::toggleAutomationMode() {
    if (g_trackInFocus == 0) {
        int mode = GetGlobalAutomationOverride();
        mode = (mode > 1) ? -1 : 4;
        SetGlobalAutomationOverride(mode);
    } else {
        MediaTrack* track = CSurf_TrackFromID(g_trackInFocus, false);
        if (!track) return;
        int mode = *(int*)GetSetMediaTrackInfo(track, "I_AUTOMODE", nullptr);
        mode = (mode > 1) ? 0 : 4;
        GetSetMediaTrackInfo(track, "I_AUTOMODE", &mode);
    }
}

void CommandProcessor::handleMixerKnob(unsigned char command, signed char value) {
    logCommand(command, value, "mixer knob");
    int trackIndex = -1;

    if (command >= CMD_KNOB_VOLUME0 && command <= CMD_KNOB_VOLUME7) {
        trackIndex = command - CMD_KNOB_VOLUME0;

        MediaTrack* track = CSurf_TrackFromID(trackIndex + 1, false); // Track IDs start at 1
        if (track) {
            CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, value / 126.0, true), nullptr);
        }

    } else if (command >= CMD_KNOB_PAN0 && command <= CMD_KNOB_PAN7) {
        trackIndex = command - CMD_KNOB_PAN0;

        MediaTrack* track = CSurf_TrackFromID(trackIndex + 1, false);
        if (track) {
            CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, value * 0.00098425, true), nullptr);
        }
    }
}

void CommandProcessor::handleTrackControl(unsigned char command, unsigned char value) {
    logCommand(command, value, "track");
    int trackIndex = static_cast<int>(value);
    MediaTrack* track = CSurf_TrackFromID(trackIndex + 1, false); // tracks are 1-based

    if (!track) return;

    switch (command) {
        case CMD_TRACK_SELECTED:
            {
                // Clear all selection first
                for (int i = 0; i <= GetNumTracks(); i++) {
                    int sel = 0;
                    GetSetMediaTrackInfo(CSurf_TrackFromID(i, false), "I_SELECTED", &sel);
                }
                int sel = 1;
                GetSetMediaTrackInfo(track, "I_SELECTED", &sel);
            }
            break;

        case CMD_TRACK_MUTED:
            {
                bool muted = *(bool*)GetSetMediaTrackInfo(track, "B_MUTE", nullptr);
                CSurf_OnMuteChange(track, muted ? 0 : 1);
            }
            break;

        case CMD_TRACK_SOLOED:
            {
                int solo = *(int*)GetSetMediaTrackInfo(track, "I_SOLO", nullptr);
                CSurf_OnSoloChange(track, solo == 0 ? 1 : 0);
            }
            break;

        default:
            break;
    }
}

void CommandProcessor::handleNavigation(unsigned char command, unsigned char value) {
    logCommand(command, value, "navigation");
    const int step = convertSignedMidiValue(value); // -1 or 1

    switch (command) {
        case CMD_NAV_TRACKS:
            {
                int newFocus = g_trackInFocus + step;
                int numTracks = CSurf_NumTracks(false);

                // Clamp range
                if (newFocus < 1) newFocus = 1;
                if (newFocus > numTracks) newFocus = numTracks;

                MediaTrack* track = CSurf_TrackFromID(newFocus, false);
                if (track) {
                    // Deselect all
                    for (int i = 0; i <= numTracks; ++i) {
                        int sel = 0;
                        GetSetMediaTrackInfo(CSurf_TrackFromID(i, false), "I_SELECTED", &sel);
                    }
                    int sel = 1;
                    GetSetMediaTrackInfo(track, "I_SELECTED", &sel);
                    g_trackInFocus = newFocus;
                }
            }
            break;

        case CMD_NAV_BANKS:
            {
                int numTracks = CSurf_NumTracks(false);
                int newBankStart = g_trackInFocus + step * BANK_NUM_TRACKS;
                if (newBankStart >= 1 && newBankStart <= numTracks) {
                    g_trackInFocus = newBankStart;
                    MediaTrack* track = CSurf_TrackFromID(g_trackInFocus, false);
                    if (track) {
                        int sel = 1;
                        GetSetMediaTrackInfo(track, "I_SELECTED", &sel);
                    }
                }
            }
            break;

        case CMD_NAV_CLIPS:
            Main_OnCommand(step > 0 ? 40173 : 40172, 0);
            break;

        default:
            break;
    }
}

void CommandProcessor::handleSelectedTrack(unsigned char command, unsigned char value) {
    logCommand(command, value, "selected track");
    MediaTrack* track = CSurf_TrackFromID(g_trackInFocus, false);
    if (!track || g_trackInFocus < 1) return;

    switch (command) {
        case CMD_CHANGE_SEL_TRACK_VOLUME:
            {
                signed char diff = convertSignedMidiValue(value);
                double step = (abs(diff) > 38 ? 1.0 : 0.1) * (diff >= 0 ? 1 : -1);
                CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, step, true), nullptr);
            }
            break;

        case CMD_CHANGE_SEL_TRACK_PAN:
            {
                signed char diff = convertSignedMidiValue(value);
                double step = diff * 0.00098425;
                CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, step, true), nullptr);
            }
            break;

        case CMD_TOGGLE_SEL_TRACK_MUTE:
            {
                bool muted = *(bool*)GetSetMediaTrackInfo(track, "B_MUTE", nullptr);
                CSurf_OnMuteChange(track, muted ? 0 : 1);
            }
            break;

        case CMD_TOGGLE_SEL_TRACK_SOLO:
            {
                int solo = *(int*)GetSetMediaTrackInfo(track, "I_SOLO", nullptr);
                CSurf_OnSoloChange(track, solo == 0 ? 1 : 0);
            }
            break;

        default:
            break;
    }
}

void CommandProcessor::handleClear(unsigned char command, unsigned char value)
{
    logCommand(command, value, "clear");
    Main_OnCommand(40129, 0);
    Main_OnCommand(41349, 0);
}

void CommandProcessor::handleCount(unsigned char command, unsigned char value) {
    logCommand(command, value, "count");
    g_KKcountInTriggered = true;
    g_KKcountInMetroState = (*(int*)GetConfigVar("projmetroen") & 1);
    Main_OnCommand(41745, 0);
    *(int*)GetConfigVar("projmetroen") |= 16;
    CSurf_OnRecord();
}

void CommandProcessor::logCommand(unsigned char command, unsigned char value, const std::string& context)
{
    std::ostringstream msg;
    msg << "[Command/" << context << "] "
        << getCommandName(command)
        << " (" << static_cast<int>(command) << "), Value: "
        << static_cast<int>(value) << "\n";
    ShowConsoleMsg(msg.str().c_str());
}


