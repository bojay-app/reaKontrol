#pragma once

#ifndef NIMIDISURFACE_H
#define NIMIDISURFACE_H

#include "TrackSelectionDebouncer.h"
#include "reaKontrol.h"
#include "MidiSender.h"

class CommandProcessor;
class MediaTrack;
enum CycleDirection;

class NiMidiSurface : public BaseSurface {
public:
    NiMidiSurface();
    virtual ~NiMidiSurface();

    MidiSender* GetMidiSender();

    virtual const char* GetTypeString() override;
    virtual const char* GetDescString() override;
    virtual void Run() override;
    virtual void SetPlayState(bool play, bool pause, bool rec) override;
    virtual void SetRepeatState(bool rep) override;
    virtual void SetTrackListChange() override;
    virtual void SetSurfaceSelected(MediaTrack* track, bool selected) override;
    virtual void SetSurfaceVolume(MediaTrack* track, double volume) override;
    virtual void SetSurfacePan(MediaTrack* track, double pan) override;
    virtual void SetSurfaceMute(MediaTrack* track, bool mute) override;
    virtual void SetSurfaceSolo(MediaTrack* track, bool solo) override;
    virtual void SetSurfaceRecArm(MediaTrack* track, bool armed) override;
    virtual int Extended(int call, void* parm1, void* parm2, void* parm3) override;

protected:
    void _onMidiEvent(MIDI_event_t* event) override;

private:
    MidiSender* midiSender;
    CommandProcessor* processor;
    TrackSelectionDebouncer trackDebouncer;
    void addEventToMap(unsigned char command, unsigned char value);
    void processClickEvent();
    void UpdateMixerScreenEncoder(int id, int numInBank);
    void updateTransportAndNavButtons();
    void cycleEncoderLEDs(int& cycleTimer, int& cyclePos, CycleDirection direction, MidiSender* midiSender);
};

#endif // NIMIDISURFACE_H