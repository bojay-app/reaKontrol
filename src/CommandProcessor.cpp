#include "CommandProcessor.h"
#include "Utils.h"
#include "Commands.h"
#include "Constants.h"
#include "reaKontrol.h"
#include "CommandHandlerTable.h"
#include <string>
#include <sstream>
#include <functional>
#include <unordered_map>

// --- Transport Command Handlers ---

bool handlePlay(unsigned char command, unsigned char value) {
    CSurf_OnPlay();
    return true;
}

bool handleRestart(unsigned char command, unsigned char value) {
    CSurf_GoStart();
    if (!(GetPlayState() & 1)) {
        CSurf_OnPlay();
    }
    return true;
}

bool handleStop(unsigned char command, unsigned char value) {
    if (g_extEditMode != EXT_EDIT_OFF) {
        g_extEditMode = EXT_EDIT_OFF;
        ShowConsoleMsg("[Mode] Exiting extended mode\n");
        return true;
    }
    CSurf_OnStop();
    return true;
}

bool handleRec(unsigned char command, unsigned char value) {
    CSurf_OnRecord();
    return true;
}

bool handleLoop(unsigned char command, unsigned char value) {
    Main_OnCommand(1068, 0); // Toggle repeat
    return true;
}

bool handleMetro(unsigned char command, unsigned char value) {
    Main_OnCommand(40364, 0); // Toggle metronome
    return true;
}

bool handleTempo(unsigned char command, unsigned char value) {
    Main_OnCommand(1134, 0); // Tap tempo
    return true;
}

bool handleUndo(unsigned char command, unsigned char value) {
    Main_OnCommand(40029, 0);
    return true;
}

bool handleRedo(unsigned char command, unsigned char value) {
    Main_OnCommand(40030, 0);
    return true;
}

bool handleQuantize(unsigned char command, unsigned char value) {
    Main_OnCommand(42033, 0); // Toggle input quantize
    Main_OnCommand(40604, 0); // Open record settings
    return true;
}

bool handleAuto(unsigned char command, unsigned char value) {
    if (g_trackInFocus <= 0) {
        int mode = GetGlobalAutomationOverride();
        mode = (mode > 1) ? -1 : 4;
        SetGlobalAutomationOverride(mode);
    }
    else {
        MediaTrack* track = CSurf_TrackFromID(g_trackInFocus, false);
        if (!track) return false;
        int mode = *(int*)GetSetMediaTrackInfo(track, "I_AUTOMODE", nullptr);
        mode = (mode > 1) ? 0 : 4;
        GetSetMediaTrackInfo(track, "I_AUTOMODE", &mode);
    }
    return true;
}

bool handleEnterExtendedMode(unsigned char command, unsigned char value) {
    g_extEditMode = EXT_EDIT_ON;
    ShowConsoleMsg("[Mode] Entering extended mode\n");
    return true;
}

// --- MixerKnob Command Handlers ---

bool handleMixerKnob(unsigned char command, unsigned char value) {
    int trackIndex = -1;
    if (command >= CMD_KNOB_VOLUME0 && command <= CMD_KNOB_VOLUME7) {
        trackIndex = command - CMD_KNOB_VOLUME0;
        MediaTrack* track = CSurf_TrackFromID(trackIndex, false);
        if (track) {
            CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, value / 126.0, true), nullptr);
            return true;
        }
    }
    else if (command >= CMD_KNOB_PAN0 && command <= CMD_KNOB_PAN7) {
        trackIndex = command - CMD_KNOB_PAN0;
        MediaTrack* track = CSurf_TrackFromID(trackIndex, false);
        if (track) {
            CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, value * 0.00098425, true), nullptr);
            return true;
        }
    }
    return false;
}

// ---- Track Control Handlers ----

bool handleTrackSelected(unsigned char, unsigned char value) {
    MediaTrack* track = CSurf_TrackFromID(value, false);
    if (!track) return false;
    int sel = 0;
    for (int i = 0; i <= GetNumTracks(); ++i)
        GetSetMediaTrackInfo(CSurf_TrackFromID(i, false), "I_SELECTED", &sel);
    sel = 1;
    GetSetMediaTrackInfo(track, "I_SELECTED", &sel);
    return true;
}

bool handleTrackMuted(unsigned char, unsigned char value) {
    MediaTrack* track = CSurf_TrackFromID(value, false);
    if (!track) return false;
    bool muted = *(bool*)GetSetMediaTrackInfo(track, "B_MUTE", nullptr);
    CSurf_OnMuteChange(track, muted ? 0 : 1);
    return true;
}

bool handleTrackSoloed(unsigned char, unsigned char value) {
    MediaTrack* track = CSurf_TrackFromID(value, false);
    if (!track) return false;
    int solo = *(int*)GetSetMediaTrackInfo(track, "I_SOLO", nullptr);
    CSurf_OnSoloChange(track, solo == 0 ? 1 : 0);
    return true;
}

// ---- Navigation Handlers ----

bool handleNavTracks(unsigned char, unsigned char value) {
    int step = convertSignedMidiValue(value);
    int newFocus = g_trackInFocus + step;
    int numTracks = CSurf_NumTracks(false);
    if (newFocus < 1 || newFocus > numTracks) return false;

    MediaTrack* track = CSurf_TrackFromID(newFocus, false);
    if (!track) return false;
    int sel = 0;
    for (int i = 0; i <= numTracks; ++i)
        GetSetMediaTrackInfo(CSurf_TrackFromID(i, false), "I_SELECTED", &sel);
    sel = 1;
    GetSetMediaTrackInfo(track, "I_SELECTED", &sel);
    g_trackInFocus = newFocus;
    return true;
}

bool handleNavBanks(unsigned char, unsigned char value) {
    int step = convertSignedMidiValue(value);
    int numTracks = CSurf_NumTracks(false);
    int newBankStart = g_trackInFocus + step * BANK_NUM_TRACKS;
    if (newBankStart < 1 || newBankStart > numTracks) return false;
    g_trackInFocus = newBankStart;
    MediaTrack* track = CSurf_TrackFromID(g_trackInFocus, false);
    if (!track) return false;
    int sel = 1;
    GetSetMediaTrackInfo(track, "I_SELECTED", &sel);
    return true;
}

bool handleNavClips(unsigned char, unsigned char value) {
    int step = convertSignedMidiValue(value);
    Main_OnCommand(step > 0 ? 40173 : 40172, 0);
    return true;
}

bool handlePlayClip(unsigned char, unsigned char) {
    int numTracks = CSurf_NumTracks(false);
    if (g_trackInFocus < 1 || g_trackInFocus > numTracks) return false;
    MediaTrack* track = CSurf_TrackFromID(g_trackInFocus, false);
    if (!track) return false;

    int sel = 0;
    for (int i = 0; i <= numTracks; ++i)
        GetSetMediaTrackInfo(CSurf_TrackFromID(i, false), "I_SELECTED", &sel);
    sel = 1;
    GetSetMediaTrackInfo(track, "I_SELECTED", &sel);
    Main_OnCommand(40913, 0);
    SetMixerScroll(track);
    return true;
}

// ---- Selected Track Knob Handlers ----

bool handleSelectedTrackVolume(unsigned char, unsigned char value) {
    if (g_trackInFocus < 1) return false;
    MediaTrack* track = CSurf_TrackFromID(g_trackInFocus, false);
    if (!track) return false;
    signed char diff = convertSignedMidiValue(value);
    double step = (abs(diff) > 38 ? 1.0 : 0.1) * (diff >= 0 ? 1 : -1);
    CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, step, true), nullptr);
    return true;
}

bool handleSelectedTrackPan(unsigned char, unsigned char value) {
    if (g_trackInFocus < 1) return false;
    MediaTrack* track = CSurf_TrackFromID(g_trackInFocus, false);
    if (!track) return false;
    signed char diff = convertSignedMidiValue(value);
    double step = diff * 0.00098425;
    CSurf_SetSurfacePan(track, CSurf_OnPanChange(track, step, true), nullptr);
    return true;
}

bool handleSelectedTrackMute(unsigned char, unsigned char) {
    if (g_trackInFocus < 1) return false;
    MediaTrack* track = CSurf_TrackFromID(g_trackInFocus, false);
    if (!track) return false;
    bool muted = *(bool*)GetSetMediaTrackInfo(track, "B_MUTE", nullptr);
    CSurf_OnMuteChange(track, muted ? 0 : 1);
    return true;
}

bool handleSelectedTrackSolo(unsigned char, unsigned char) {
    if (g_trackInFocus < 1) return false;
    MediaTrack* track = CSurf_TrackFromID(g_trackInFocus, false);
    if (!track) return false;
    int solo = *(int*)GetSetMediaTrackInfo(track, "I_SOLO", nullptr);
    CSurf_OnSoloChange(track, solo == 0 ? 1 : 0);
    return true;
}

// ---- Misc Handlers ----

bool handleClear(unsigned char, unsigned char) {
    Main_OnCommand(40129, 0);
    Main_OnCommand(41349, 0);
    return true;
}

bool handleCount(unsigned char, unsigned char) {
    g_KKcountInTriggered = true;
    g_KKcountInMetroState = (*(int*)GetConfigVar("projmetroen") & 1);
    Main_OnCommand(41745, 0);
    *(int*)GetConfigVar("projmetroen") |= 16;
    CSurf_OnRecord();
    return true;
}

// ---- Init & Registration & Handle ----

CommandProcessor::CommandProcessor(MidiSender& sender)
    : midiSender(sender) {
    CommandHandlerTable& table = CommandHandlerTable::get();

    // Transport Command Handlers
    table.registerHandler(CMD_PLAY, handlePlay);
    table.registerHandler(CMD_RESTART, handleRestart);
    table.registerHandler(CMD_STOP, handleStop);
    table.registerHandler(CMD_REC, handleRec);
    table.registerHandler(CMD_LOOP, handleLoop);
    table.registerHandler(CMD_METRO, handleMetro);
    table.registerHandler(CMD_TEMPO, handleTempo);
    table.registerHandler(CMD_UNDO, handleUndo);
    table.registerHandler(CMD_REDO, handleRedo);
    table.registerHandler(CMD_QUANTIZE, handleQuantize);
    table.registerHandler(CMD_AUTO, handleAuto);
    table.registerHandler(CMD_STOP_CLIP, handleEnterExtendedMode);

    // Mixer knobs registration
    for (unsigned char cmd = CMD_KNOB_VOLUME0; cmd <= CMD_KNOB_VOLUME7; ++cmd) {
        table.registerHandler(cmd, handleMixerKnob);
    }
    for (unsigned char cmd = CMD_KNOB_PAN0; cmd <= CMD_KNOB_PAN7; ++cmd) {
        table.registerHandler(cmd, handleMixerKnob);
    }

    // Track
    table.registerHandler(CMD_TRACK_SELECTED, handleTrackSelected);
    table.registerHandler(CMD_TRACK_MUTED, handleTrackMuted);
    table.registerHandler(CMD_TRACK_SOLOED, handleTrackSoloed);

    // Encoder
    table.registerHandler(CMD_NAV_TRACKS, handleNavTracks);
    table.registerHandler(CMD_NAV_BANKS, handleNavBanks);
    table.registerHandler(CMD_NAV_CLIPS, handleNavClips);
    table.registerHandler(CMD_PLAY_CLIP, handlePlayClip);
    table.registerHandler(CMD_CHANGE_SEL_TRACK_VOLUME, handleSelectedTrackVolume);
    table.registerHandler(CMD_CHANGE_SEL_TRACK_PAN, handleSelectedTrackPan);
    table.registerHandler(CMD_TOGGLE_SEL_TRACK_MUTE, handleSelectedTrackMute);
    table.registerHandler(CMD_TOGGLE_SEL_TRACK_SOLO, handleSelectedTrackSolo);

    // Others
    table.registerHandler(CMD_CLEAR, handleClear);
    table.registerHandler(CMD_COUNT, handleCount);
}

void CommandProcessor::Handle(unsigned char command, unsigned char value) {
    if (CommandHandlerTable::get().dispatch(command, value)) {
        LogCommand(command, value, "handled");
    }
    else {
        LogCommand(command, value, "***unhandled***");
    }
}

// ---- Helpers ----

void CommandProcessor::RefocusBank()
{
    // Switch Mixer view to the bank containing the currently focused (= selected) track and also focus Reaper's TCP and MCP
    if (g_trackInFocus < 1) {
        return;
    }
    int numTracks = CSurf_NumTracks(false);
    // Backstop measure to protect against unreported track removal that was not captured in SetTrackListChange callback due to race condition
    if (g_trackInFocus > numTracks) {
        g_trackInFocus = numTracks;
    }
    MediaTrack* track = CSurf_TrackFromID(g_trackInFocus, false);
    int iSel = 1; // "Select"
    ClearAllSelectedTracks();
    GetSetMediaTrackInfo(track, "I_SELECTED", &iSel);
    Main_OnCommand(40913, 0); // Vertical scroll selected track into view (TCP)
    SetMixerScroll(track); // Horizontal scroll making the selected track the leftmost track if possible (MCP)
    bankStart = (int)(g_trackInFocus / BANK_NUM_TRACKS) * BANK_NUM_TRACKS;
    allMixerUpdate(&midiSender);
}

void CommandProcessor::ClearAllSelectedTracks() {
    // Clear all selected tracks. Copyright (c) 2010 and later Tim Payne (SWS)
    int iSel = 0;
    for (int i = 0; i <= GetNumTracks(); i++) // really ALL tracks, hence no use of CSurf_NumTracks
        GetSetMediaTrackInfo(CSurf_TrackFromID(i, false), "I_SELECTED", &iSel);
}

void CommandProcessor::LogCommand(unsigned char command, unsigned char value, const std::string& context)
{
    std::ostringstream msg;
    msg << "[" << context << "] "
        << getCommandName(command)
        << " (" << static_cast<int>(command) << "), Value: "
        << static_cast<int>(value) << "\n";
    ShowConsoleMsg(msg.str().c_str());
}


