#include "CommandProcessor.h"
#include "Utils.h"
#include "Commands.h"
#include "Constants.h"
#include "reaKontrol.h"
#include "CommandHandlerTable.h"
#include "ActionList.h"
#include <string>
#include <sstream>
#include <functional>
#include <unordered_map>

// ---- Init & Registration & Handle ----

CommandProcessor::CommandProcessor(MidiSender& sender, BaseSurface* surface)
    : midiSender(sender), surface(surface) {

    // Transport Command Handlers
    registerHandler(CMD_PLAY, &CommandProcessor::handlePlay);
    registerHandler(CMD_RESTART, &CommandProcessor::handleRestart);
    registerHandler(CMD_STOP, &CommandProcessor::handleStop);
    registerHandler(CMD_REC, &CommandProcessor::handleRec);
    registerHandler(CMD_LOOP, &CommandProcessor::handleLoop);
    registerHandler(CMD_METRO, &CommandProcessor::handleMetro);
    registerHandler(CMD_TEMPO, &CommandProcessor::handleTempo);
    registerHandler(CMD_UNDO, &CommandProcessor::handleUndo);
    registerHandler(CMD_REDO, &CommandProcessor::handleRedo);
    registerHandler(CMD_QUANTIZE, &CommandProcessor::handleQuantize);
    registerHandler(CMD_AUTO, &CommandProcessor::handleAuto);
    registerHandler(CMD_STOP_CLIP, &CommandProcessor::toggleExtendedMode);

    // 8 Mixer knobs registration
    for (unsigned char cmd = CMD_KNOB_VOLUME0; cmd <= CMD_KNOB_VOLUME7; ++cmd) {
        registerHandler(cmd, &CommandProcessor::handleMixerKnob);
    }
    for (unsigned char cmd = CMD_KNOB_PAN0; cmd <= CMD_KNOB_PAN7; ++cmd) {
        registerHandler(cmd, &CommandProcessor::handleMixerKnob);
    }

    // Track
    registerHandler(CMD_TRACK_SELECTED, &CommandProcessor::handleTrackSelected);
    registerHandler(CMD_TRACK_MUTED, &CommandProcessor::handleTrackMuted);
    registerHandler(CMD_TRACK_SOLOED, &CommandProcessor::handleTrackSoloed);

    // Encoder
    registerHandler(CMD_NAV_TRACKS, &CommandProcessor::handleNavTracks);
    registerHandler(CMD_NAV_BANKS, &CommandProcessor::handleNavBanks);
    registerHandler(CMD_NAV_CLIPS, &CommandProcessor::handleNavClips);
    registerHandler(CMD_PLAY_CLIP, &CommandProcessor::handlePlayClip);
    registerHandler(CMD_MOVE_LOOP, &CommandProcessor::handleLoopMove);

    registerHandler(CMD_MOVE_TRANSPORT, &CommandProcessor::handleSelectedTrackVolume);
    registerHandler(CMD_CHANGE_SEL_TRACK_VOLUME, &CommandProcessor::handleSelectedTrackVolume);
    registerHandler(CMD_CHANGE_SEL_TRACK_PAN, &CommandProcessor::handleSelectedTrackPan);
    registerHandler(CMD_TOGGLE_SEL_TRACK_MUTE, &CommandProcessor::handleSelectedTrackMute);
    registerHandler(CMD_TOGGLE_SEL_TRACK_SOLO, &CommandProcessor::handleSelectedTrackSolo);

    // Others
    registerHandler(CMD_CLEAR, &CommandProcessor::handleClear);
    registerHandler(CMD_COUNT, &CommandProcessor::handleCount);
}

void CommandProcessor::Handle(unsigned char command, unsigned char value) {
    if (CommandHandlerTable::get().dispatch(command, value)) {
        LogCommand(command, value, "handled");
    }
    else {
        LogCommand(command, value, "***unhandled***");
    }
}

// --- Transpose Handlers ---

bool CommandProcessor::handlePlay(unsigned char, unsigned char) {
    CSurf_OnPlay();
    return true;
}

bool CommandProcessor::handleRestart(unsigned char, unsigned char) {
    CSurf_GoStart();
    if (!(GetPlayState() & 1)) {
        CSurf_OnPlay();
    }
    return true;
}

bool CommandProcessor::handleStop(unsigned char, unsigned char) {
    int playState = GetPlayState();

    // Check if REAPER is playing or recording
    if ((playState & 1) == 1 || (playState & 4) == 4) {
        // Stop playback or recording
        CSurf_OnStop();
        return true;
    }
    else {
        if (getExtEditMode() == EXT_EDIT_ON) {
            Main_OnCommand(40184, 0); // Remove items / tracks / envelope points(depending on focus) - no prompting
            return true;
        }
        else {
            Main_OnCommand(9, 0); // Toggle record arm for selected track
            return true;
        }
    }

    return false;
}

bool CommandProcessor::handleRec(unsigned char, unsigned char) {
    if (getExtEditMode() == EXT_EDIT_ON) {
        Main_OnCommand(9, 0); // Toggle record arm for selected track
    }
    else {
        CSurf_OnRecord();
    }
    return true;
}

bool CommandProcessor::handleLoop(unsigned char, unsigned char) {
    if (getExtEditMode() == EXT_EDIT_ON) {
        Main_OnCommand(40020, 0); // Time selection: Remove (unselect) time selection and loop points
        setExtEditMode(EXT_EDIT_LOOP);
    }
    else if (getExtEditMode() == EXT_EDIT_LOOP) {
        setExtEditMode(EXT_EDIT_OFF);
    }
    else {
        Main_OnCommand(1068, 0); // Toggle Repeat
    }
    return true;
}

bool CommandProcessor::handleMetro(unsigned char, unsigned char) {
    Main_OnCommand(40364, 0); // Options: Toggle metronome
    return true;
}

bool CommandProcessor::handleTempo(unsigned char, unsigned char) {
    Main_OnCommand(1134, 0);
    return true;
}

bool CommandProcessor::handleUndo(unsigned char, unsigned char) {
    Main_OnCommand(40029, 0);
    return true;
}

bool CommandProcessor::handleRedo(unsigned char, unsigned char) {
    Main_OnCommand(40030, 0);
    return true;
}

bool CommandProcessor::handleQuantize(unsigned char, unsigned char) {
    Main_OnCommand(42033, 0);
    return true;
}

bool CommandProcessor::handleAuto(unsigned char, unsigned char) {
    if (g_trackInFocus < 1) {
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

bool CommandProcessor::toggleExtendedMode(unsigned char, unsigned char) {
    if (getExtEditMode() == EXT_EDIT_ON) {
        allMixerUpdate(&midiSender);
        peakMixerUpdate(&midiSender);
        setExtEditMode(EXT_EDIT_OFF);
    } else {
        setExtEditMode(EXT_EDIT_ON);
        showActionList(&midiSender);
    }
    return true;
}

// ---- Mixer Knob Hanlders ----

bool CommandProcessor::handleMixerKnob(unsigned char command, unsigned char value) {
    signed char delta = convertSignedMidiValue(value);
    MediaTrack* track = nullptr;

    if (command >= CMD_KNOB_VOLUME0 && command <= CMD_KNOB_VOLUME7) {
        int trackIndex = command - CMD_KNOB_VOLUME0;
        track = CSurf_TrackFromID(trackIndex, false);
        return adjustTrackVolume(track, delta);
    }
    else if (command >= CMD_KNOB_PAN0 && command <= CMD_KNOB_PAN7) {
        int trackIndex = command - CMD_KNOB_PAN0;
        track = CSurf_TrackFromID(trackIndex, false);
        return adjustTrackPan(track, delta);
    }
    return false;
}

// ---- Track Control Handlers ----

bool CommandProcessor::handleTrackSelected(unsigned char, unsigned char value) {
    MediaTrack* track = CSurf_TrackFromID(value, false);
    if (!track) return false;

    int sel = 0;
    for (int i = 0; i <= GetNumTracks(); ++i)
        GetSetMediaTrackInfo(CSurf_TrackFromID(i, false), "I_SELECTED", &sel);

    sel = 1;
    GetSetMediaTrackInfo(track, "I_SELECTED", &sel);
    return true;
}

bool CommandProcessor::handleTrackMuted(unsigned char, unsigned char value) {
    if (getExtEditMode() != EXT_EDIT_OFF) {
        callAction(value);
        showActionList(&midiSender);
        return true;
    }
    else {
        MediaTrack* track = CSurf_TrackFromID(value, false);
        return toggleTrackMute(track);
    }
}

bool CommandProcessor::handleTrackSoloed(unsigned char, unsigned char value) {
    if (getExtEditMode() != EXT_EDIT_OFF) {
        callAction(value);
        return true;
    }
    else {
        MediaTrack* track = CSurf_TrackFromID(value, false);
        return toggleTrackSolo(track);
    }
}

// ---- Navigation Handlers ----

bool CommandProcessor::handleNavTracks(unsigned char, unsigned char value) {
    if (getExtEditMode() == EXT_EDIT_LOOP) {
        if (value == 127) {
            Main_OnCommand(40222, 0); // Loop points: Set start point
        }
        else {
            Main_OnCommand(40223, 0); // Loop points: Set end point
        }
        return true;
    }
    else {
        int step = convertSignedMidiValue(value);
        int newFocus = g_trackInFocus + step;
        int numTracks = CSurf_NumTracks(false);

        if (newFocus < 1 || newFocus > numTracks) newFocus = 1;

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
}

bool CommandProcessor::handleNavBanks(unsigned char, unsigned char value) {
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

bool CommandProcessor::handleNavClips(unsigned char, unsigned char value) {
    int step = convertSignedMidiValue(value);
    Main_OnCommand(step > 0 ? 40173 : 40172, 0);
    return true;
}

bool CommandProcessor::handlePlayClip(unsigned char, unsigned char) {
    if (getExtEditMode() == EXT_EDIT_ON) {
        Main_OnCommand(40012, 0); // Item: Split items at edit or play cursor (select right)
        return true;
    }
    else if (getExtEditMode() == EXT_EDIT_OFF) {
        RefocusBank();

        // Enable kkInstance
        bool tryTargetFirstTrack = g_trackInFocus == 0;
        MediaTrack* track = CSurf_TrackFromID(tryTargetFirstTrack ? 1 : g_trackInFocus, false);
        if (!track) return false;
        g_trackInFocus = tryTargetFirstTrack ? 1 : g_trackInFocus;
        activateKkInstance(track);
        return true;
    }

    return false;
}

bool CommandProcessor::handleLoopMove(unsigned char, unsigned char value) {
    if (value <= 63) {
        Main_OnCommand(40038, 0); // Shift time selection right (by its own length)
    }
    else {
        Main_OnCommand(40037, 0); // Shift time selection left (by its own length)
    }
    return true;
}

// ---- Selected Track Knob Handlers ----

bool CommandProcessor::handleSelectedTrackVolume(unsigned char cmd, unsigned char value) {
    if (getExtEditMode() == EXT_EDIT_ON || getExtEditMode() == EXT_EDIT_LOOP) {
        // Scroll playhead to next/previous grid division
        if (value <= 63) {
            Main_OnCommand(40647, 0); // move cursor right 1 grid division (no seek)
        }
        else {
            Main_OnCommand(40646, 0); // move cursor left 1 grid division (no seek)
        }
        return true;
    }
    else {
        // Adjust selected track vol (default 0 master track)
        MediaTrack* track = CSurf_TrackFromID(g_trackInFocus, false);
        signed char vol = convertSignedMidiValue(value);
        if (cmd == CMD_MOVE_TRANSPORT) {
            // The signal is 1 : 127 => 1 : -1, which is too small for volume. So we make it the same value as the track volume cmd
            vol *= 63; 
        }
        return adjustTrackVolume(track, vol);
    }
}

bool CommandProcessor::handleSelectedTrackPan(unsigned char, unsigned char value) {
    if (g_trackInFocus < 1) return false;
    MediaTrack* track = CSurf_TrackFromID(g_trackInFocus, false);
    return adjustTrackPan(track, convertSignedMidiValue(value));
}

bool CommandProcessor::handleSelectedTrackMute(unsigned char, unsigned char) {
    if (getExtEditMode() == EXT_EDIT_ON) { return true; }
    if (g_trackInFocus < 1) return false;
    MediaTrack* track = CSurf_TrackFromID(g_trackInFocus, false);
    return toggleTrackMute(track);
}

bool CommandProcessor::handleSelectedTrackSolo(unsigned char, unsigned char) {
    if (getExtEditMode() == EXT_EDIT_ON) { return true; }

    if (g_trackInFocus < 1) return false;
    MediaTrack* track = CSurf_TrackFromID(g_trackInFocus, false);
    return toggleTrackSolo(track);
}

// ---- Miscellaneous Handlers ----

bool CommandProcessor::handleClear(unsigned char, unsigned char) {
    if (getExtEditMode() == EXT_EDIT_ON) {
        Main_OnCommand(40005, 0); // Remove selected track
        if (surface) {
            surface->SetTrackListChange();
        }
    }
    else {
        // Delete active takes. Typically, when recording an item in loop mode this allows to remove take by take until the entire item is removed.
        Main_OnCommand(40129, 0); // Delete active take
        Main_OnCommand(41349, 0); // Remove empty take lane
    }
    return true;
}

bool CommandProcessor::handleCount(unsigned char, unsigned char) {
    g_KKcountInTriggered = true;
    g_KKcountInMetroState = (*(int*)GetConfigVar("projmetroen") & 1);
    Main_OnCommand(41745, 0);        // Enable metronome
    *(int*)GetConfigVar("projmetroen") |= 16; // Enable count-in
    CSurf_OnRecord();
    return true;
}


// ---- Helpers ----

template <typename Method>
void CommandProcessor::registerHandler(unsigned char cmd, Method method) {
    CommandHandlerTable::get().registerHandler(cmd, std::bind(method, this, std::placeholders::_1, std::placeholders::_2));
}

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
    debugLog(msg);
}


