#pragma once

#include "MidiSender.h"
class BaseSurface;

class CommandProcessor {
public:
    CommandProcessor(MidiSender& sender, BaseSurface* surface = nullptr);

    // Main entry point to handle MIDI CC events
    void Handle(unsigned char command, unsigned char value);

private:
    MidiSender& midiSender;
    BaseSurface* surface;

    template <typename Method>
    void registerHandler(unsigned char cmd, Method method);
    
    void RefocusBank();
    void ClearAllSelectedTracks();
    void LogCommand(unsigned char command, unsigned char value, const std::string& context);

    // Handler methods
    bool handlePlay(unsigned char, unsigned char);
    bool handleRestart(unsigned char, unsigned char);
    bool handleStop(unsigned char, unsigned char);
    bool handleRec(unsigned char, unsigned char);
    bool handleLoop(unsigned char, unsigned char);
    bool handleMetro(unsigned char, unsigned char);
    bool handleTempo(unsigned char, unsigned char);
    bool handleUndo(unsigned char, unsigned char);
    bool handleRedo(unsigned char, unsigned char);
    bool handleQuantize(unsigned char, unsigned char);
    bool handleAuto(unsigned char, unsigned char);
    bool toggleExtendedMode(unsigned char, unsigned char);

    bool handleMixerKnob(unsigned char, unsigned char);

    bool handleTrackSelected(unsigned char, unsigned char);
    bool handleTrackMuted(unsigned char, unsigned char);
    bool handleTrackSoloed(unsigned char, unsigned char);

    bool handleNavTracks(unsigned char, unsigned char);
    bool handleNavBanks(unsigned char, unsigned char);
    bool handleNavClips(unsigned char, unsigned char);
    bool handlePlayClip(unsigned char, unsigned char);
    bool handleLoopMove(unsigned char, unsigned char);

    bool handleSelectedTrackVolume(unsigned char, unsigned char);
    bool handleSelectedTrackPan(unsigned char, unsigned char);
    bool handleSelectedTrackMute(unsigned char, unsigned char);
    bool handleSelectedTrackSolo(unsigned char, unsigned char);

    bool handleClear(unsigned char, unsigned char);
    bool handleCount(unsigned char, unsigned char);
};
