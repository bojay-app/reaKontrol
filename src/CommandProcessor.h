#pragma once

#include "MidiSender.h"
class BaseSurface;

class CommandProcessor {
public:
    CommandProcessor(MidiSender& sender, BaseSurface* surface = nullptr);

    // Main entry point to handle MIDI CC events
    void Handle(unsigned char command, unsigned char value, const char* info);

private:
    MidiSender& midiSender;
    BaseSurface* surface;

    template <typename Method>
    void registerHandler(unsigned char cmd, Method method);
    
    void RefocusBank();
    void ClearAllSelectedTracks();
    void LogCommand(unsigned char command, unsigned char value, const std::string& context);

    // Handler methods
    bool handlePlay(unsigned char command, unsigned char value, const char* info);
    bool handleRestart(unsigned char command, unsigned char value, const char* info);
    bool handleStop(unsigned char command, unsigned char value, const char* info);
    bool handleRec(unsigned char command, unsigned char value, const char* info);
    bool handleLoop(unsigned char command, unsigned char value, const char* info);
    bool handleMetro(unsigned char command, unsigned char value, const char* info);
    bool handleTempo(unsigned char command, unsigned char value, const char* info);
    bool handleUndo(unsigned char command, unsigned char value, const char* info);
    bool handleRedo(unsigned char command, unsigned char value, const char* info);
    bool handleQuantize(unsigned char command, unsigned char value, const char* info);
    bool handleAuto(unsigned char command, unsigned char value, const char* info);
    bool toggleExtendedMode(unsigned char command, unsigned char value, const char* info);

    bool handleMixerKnob(unsigned char command, unsigned char value, const char* info);

    bool handleTrackSelected(unsigned char command, unsigned char value, const char* info);
    bool handleTrackMuted(unsigned char command, unsigned char value, const char* info);
    bool handleTrackSoloed(unsigned char command, unsigned char value, const char* info);

    bool handleNavTracks(unsigned char command, unsigned char value, const char* info);
    bool handleNavBanks(unsigned char command, unsigned char value, const char* info);
    bool handleNavClips(unsigned char command, unsigned char value, const char* info);
    bool handlePlayClip(unsigned char command, unsigned char value, const char* info);
    bool handleLoopMove(unsigned char command, unsigned char value, const char* info);

    bool handleSelectedTrackVolume(unsigned char command, unsigned char value, const char* info);
    bool handleSelectedTrackPan(unsigned char command, unsigned char value, const char* info);
    bool handleSelectedTrackMute(unsigned char command, unsigned char value, const char* info);
    bool handleSelectedTrackSolo(unsigned char command, unsigned char value, const char* info);

    bool handleClear(unsigned char command, unsigned char value, const char* info);
    bool handleCount(unsigned char command, unsigned char value, const char* info);
};
