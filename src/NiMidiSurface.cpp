#include <unordered_set>
#include "NiMidiSurface.h"
#include "reaKontrol.h"
#include "Constants.h"
#include "Commands.h"
#include "Utils.h"
#include "ActionList.h"
#include "MidiSender.h"
#include "CommandProcessor.h"

enum CycleDirection {
    CLOCKWISE,
    COUNTER_CLOCKWISE
};

struct MidiEventData {
    unsigned char value;
    int timer;
};

static int timer = 0;
static std::unordered_map<unsigned char, std::vector<MidiEventData>> eventMap;

std::unordered_set<unsigned char> doubleClickCommands = {
    CMD_PLAY_CLIP,
    CMD_REC
};

NiMidiSurface::NiMidiSurface()
    : midiSender(nullptr), processor(nullptr) {
    g_connectedState = KK_NOT_CONNECTED;
}

NiMidiSurface::~NiMidiSurface() {
    for (int i = 0; i < 8; ++i) {
        if (midiSender) {
            midiSender->sendSysex(CMD_TRACK_AVAIL, 0, i);
        }
    }
    if (midiSender) {
        midiSender->sendCc(CMD_GOODBYE, 0);
    }
    delete processor;
    delete midiSender;
    processor = nullptr;
    midiSender = nullptr;
    protocolVersion = 0;
    g_connectedState = KK_NOT_CONNECTED;
}

MidiSender* NiMidiSurface::GetMidiSender() {
    return midiSender;
}

const char* NiMidiSurface::GetTypeString() {
    return "KompleteKontrolNiMidi";
}

const char* NiMidiSurface::GetDescString() {
    return "Komplete Kontrol S-series Mk3";
}

void NiMidiSurface::Run() {
    static int inDev = -1;
    static int outDev = -1;

    static bool lightOn = false;
    static int flashTimer = -1; // EXT_EDIT_OFF: flashTimer = -1
    static int cycleTimer = -1;
    static int cyclePos = 0;

    if (g_connectedState == KK_NOT_CONNECTED) {
        scanTimer++;
        if (scanTimer >= SCAN_T) {
            scanTimer = 0;
            inDev = getKkMidiInput();
            if (inDev != -1) {
                outDev = getKkMidiOutput();
                if (outDev != -1) {
                    this->_midiIn = CreateMIDIInput(inDev);
                    this->_midiOut = CreateMIDIOutput(outDev, false, nullptr);
                    if (this->_midiOut) {
                        this->_midiIn->start();
                        midiSender = new MidiSender(this->_midiOut);
                        if (!processor) {
                            processor = new CommandProcessor(*midiSender);
                        }
                        g_connectedState = KK_MIDI_FOUND;
                        scanTimer = SCAN_T;
                    }
                }
            }
        }
    }
    else if (g_connectedState == KK_MIDI_FOUND) {
        BaseSurface::Run();
        scanTimer++;
        if (scanTimer >= SCAN_T) {
            scanTimer = 0;
            if (connectCount < CONNECT_N) {
                connectCount++;
                if (!g_debugLogging) g_debugLogging = true;
                debugLog("HELLO!");
                midiSender->sendCc(CMD_HELLO, 3);
            }
            else {
                int answer = ShowMessageBox("Komplete Kontrol Keyboard detected but failed to connect. Please restart NI services (NIHostIntegrationAgent), then retry.", "ReaKontrol", 5);
                if (this->_midiIn) {
                    this->_midiIn->stop();
                    delete this->_midiIn;
                }
                if (this->_midiOut) {
                    delete this->_midiOut;
                }
                connectCount = 0;
                g_connectedState = (answer == 4) ? KK_NOT_CONNECTED : -1;
            }
        }
    }
    else if (g_connectedState == KK_NIHIA_CONNECTED) {
        /*----------------- We are successfully connected -----------------*/
        if (!g_actionListLoaded) {
            loadConfigFile();
            g_actionListLoaded = true;
        }

        if (getExtEditMode() == EXT_EDIT_OFF) {
            if (flashTimer != -1) {
                
                this->updateTransportAndNavButtons();
                allMixerUpdate(midiSender);
                peakMixerUpdate(midiSender);

                lightOn = false;
                // One time update
                flashTimer = -1;
                cycleTimer = -1;
                cyclePos = 0;
            }
        }
        else if (getExtEditMode() == EXT_EDIT_ON) {
            cycleEncoderLEDs(cycleTimer, cyclePos, CLOCKWISE, midiSender);
        }
        else if (getExtEditMode() == EXT_EDIT_LOOP) {
            if (cycleTimer == -1) {
                debugLog("RUN: EXT_EDIT_LOOP");
                this->updateTransportAndNavButtons();
                peakMixerUpdate(midiSender);
                midiSender->sendCc(CMD_NAV_TRACKS, 1);
                midiSender->sendCc(CMD_NAV_CLIPS, 0);
            }
            cycleEncoderLEDs(cycleTimer, cyclePos, CLOCKWISE, midiSender);
            
            // Flash LOOP button
            flashTimer += 1;
            if (flashTimer >= FLASH_T) {
                flashTimer = 0;
                lightOn = !lightOn;
                const unsigned char onOff = lightOn ? 1 : 0;
                midiSender->sendCc(CMD_LOOP, onOff);
            }
        }
        else if (getExtEditMode() == EXT_EDIT_TEMPO) {
            if (cycleTimer == -1) {
                debugLog("RUN: EXT_EDIT_TEMPO");
                this->updateTransportAndNavButtons();
                peakMixerUpdate(midiSender);
                midiSender->sendCc(CMD_NAV_TRACKS, 1);
                midiSender->sendCc(CMD_NAV_CLIPS, 0);
            }
            
            cycleEncoderLEDs(cycleTimer, cyclePos, COUNTER_CLOCKWISE, midiSender);
            
            // Flash METRO button
            flashTimer += 1;
            if (flashTimer >= FLASH_T) {
                flashTimer = 0;
                lightOn = !lightOn;
                const unsigned char onOff = lightOn ? 1 : 0;
                midiSender->sendCc(CMD_METRO, onOff);
            }
        }

        // Continuesly updating peak info
        if (getExtEditMode() != EXT_EDIT_ON) {
            peakMixerUpdate(midiSender);
        }

        // Fallback to master track when no track is selected
        if (trackDebouncer.shouldFallbackToMaster()) {
            g_trackInFocus = 0; // master track
            debugLog("[Debounce] Fallback to master track (no selection)");
            trackDebouncer.reset(); // clean after decision
        }

        // Timer (eg: for double click detection)
        timer++;
        processClickEvent();
        
        BaseSurface::Run();
    }
}

void NiMidiSurface::processClickEvent()
{
    if (!eventMap.empty()) {
        for (auto it = eventMap.begin(); it != eventMap.end(); ++it) {
            unsigned char command = it->first;
            std::vector<MidiEventData>& events = it->second;

            // If there are multiple events for the same command, treat it as a double-click
            if (events.size() > 1) {
                debugLog("Double Click Event '" + std::to_string(command) + "' with " + std::to_string(events.size()) + " events");
                auto value = events.back().value;

                // Clear all events for this command after double-click processing
                it->second.clear();
                nextOpenTimer = timer + CLICK_COOLDOWN;

                // Process the double-click
                static CommandProcessor processor(*midiSender);
                processor.Handle(command, value, EVENT_CLICK_DOUBLE); // Use last event for double-click
            }
            else if (events.size() == 1) {
                // If there's only one event, check for timeout
                MidiEventData& eventData = events.front();  // Get the single event

                if (timer - eventData.timer > DOUBLE_CLICK_THRESHOLD) {
                    // Event has expired (no second event arrived within the threshold), treat it as a single-click
                    debugLog("Single Click Event '" + std::to_string(command) + "': timed out");
                    auto value = eventData.value;

                    // Clear all events for this command after single-click processing
                    it->second.clear();
                    nextOpenTimer = timer + CLICK_COOLDOWN;

                    // Process the single-click logic
                    static CommandProcessor processor(*midiSender);
                    processor.Handle(command, value, EVENT_CLICK_SINGLE);
                }
            }
        }
    }
}

void NiMidiSurface::SetPlayState(bool play, bool pause, bool rec) {
    if (g_connectedState != KK_NIHIA_CONNECTED) return;
    debugLog("SetPlayState");
    
    // Update transport button lights
    if (rec) {
        midiSender->sendCc(CMD_REC, 1);
    }
    else {
        midiSender->sendCc(CMD_REC, 0);
    }
    if (pause) {
        midiSender->sendCc(CMD_PLAY, 1);
        midiSender->sendCc(CMD_STOP, 1);
    }
    else if (play) {
        midiSender->sendCc(CMD_PLAY, 1);
        midiSender->sendCc(CMD_STOP, 0);
    }
    else {
        midiSender->sendCc(CMD_PLAY, 0);
        midiSender->sendCc(CMD_STOP, 1);
        if (g_KKcountInTriggered) {
            disableRecCountIn(); // disable count-in for recording if it had been requested earlier by keyboard
            // Restore metronome state to last known state while COUNT IN was triggered
            if (g_KKcountInMetroState == 0) {
                Main_OnCommand(41746, 0); // Disable the metronome
            }
            else {
                Main_OnCommand(41745, 0); // Enable the metronome
            }
        }
    }
}

void NiMidiSurface::SetRepeatState(bool rep) {
    if (g_connectedState != KK_NIHIA_CONNECTED) return;
    debugLog("SetRepeatState");
    midiSender->sendCc(CMD_LOOP, rep ? 1 : 0);
}

void NiMidiSurface::SetTrackListChange() {
    if (g_connectedState != KK_NIHIA_CONNECTED) return;
    debugLog("SetTrackListChange");
    
    // If tracklist changes update Mixer View and ensure sanity of track and bank focus
    int numTracks = CSurf_NumTracks(false);
    // Protect against loosing track focus that could impede track navigation. Set focus on last track in this case.
    if (g_trackInFocus > numTracks) {
        g_trackInFocus = numTracks;
        // Unfortunately we cannot afford to explicitly select the last track automatically because this could screw up
        // running actions or macros. The plugin must not manipulate track selection without the user deliberately triggering
        // track selection/navigation on the keyboard (or from within Reaper).
    }
    // Protect against loosing bank focus. Set focus on last bank in this case.
    if (bankStart > numTracks) {
        int lastInLastBank = numTracks % BANK_NUM_TRACKS;
        bankStart = numTracks - lastInLastBank;
    }
    // If no track is selected at all (e.g. if previously selected track got removed), then this will now also show up in the
    // Mixer View. However, KK instance focus may still be present! This can be a little bit confusing for the user as typically
    // the track holding the focused KK instance will also be selected. This situation gets resolved as soon as any form of
    // track navigation/selection happens (from keyboard or from within Reaper).
    allMixerUpdate(midiSender);
    // ToDo: Consider sending some updates to force NIHIA to really fully update the display. Maybe in conjunction with changes to peakMixerUpdate?
    metronomeUpdate(midiSender); // check if metronome status has changed on project tab change
}

void NiMidiSurface::SetSurfaceSelected(MediaTrack* track, bool selected) {
    // Use this callback for:
        // - changed track selection and KK instance focus
        // - changed automation mode
        // - changed track name

        // This function is called for every track, so 4 tracks will call 4 times
        // Using SetSurfaceSelected() rather than OnTrackSelection() or SetTrackTitle():
        // SetSurfaceSelected() is less economical because it will be called multiple times when something changes (also for unselecting tracks, change of any record arm, change of any auto mode, change of name, ...).
        // However, SetSurfaceSelected() is the more robust choice because of: https://forum.cockos.com/showpost.php?p=2138446&postcount=15
        // A good solution for efficiency is to only evaluate messages with (selected == true).
    if (g_connectedState != KK_NIHIA_CONNECTED) return;
    int id = CSurf_TrackToID(track, false);
    int numInBank = id % BANK_NUM_TRACKS;
    trackDebouncer.update(id, selected);

    // ---------------- Track Selection and Instance Focus ----------------
    if (selected) {
        if (id != g_trackInFocus) {
            // Track selection has changed
            g_trackInFocus = id;
            debugLog("trackInFocus updated to: " + std::to_string(g_trackInFocus));
            
            if (getExtEditMode() != EXT_EDIT_ON) UpdateMixerScreenEncoder(id, numInBank);
        }
        
        // ------------------------- Automation Mode -----------------------
        // Update automation mode
        // AUTO = ON: touch, write, latch or latch preview
        // AUTO = OFF: trim or read
        int globalAutoMode = GetGlobalAutomationOverride();
        if ((id > 0) && (globalAutoMode == -1)) {
            // Check automation mode of currently focused track
            int autoMode = *(int*)GetSetMediaTrackInfo(track, "I_AUTOMODE", nullptr);
            midiSender->sendCc(CMD_AUTO, autoMode > 1 ? 1 : 0);
        }
        else {
            // Global Automation Override
            midiSender->sendCc(CMD_AUTO, globalAutoMode > 1 ? 1 : 0);
        }
        
        // --------------------------- Track Names --------------------------
        // Update selected track name
        // Note: Rather than using a callback SetTrackTitle(MediaTrack *track, const char *title) we update the name within
        // SetSurfaceSelected as it will be called anyway when the track name changes and SetTrackTitle sometimes receives 
        // cascades of calls for all tracks even if only one name changed
        if ((id > 0) && (id >= bankStart) && (id <= bankEnd) && getExtEditMode() != EXT_EDIT_ON) {
            char* name = (char*)GetSetMediaTrackInfo(track, "P_NAME", nullptr);
            if ((!name) || (*name == '\0')) {
                std::string s = "TRACK " + std::to_string(id);
                std::vector<char> nameGeneric(s.begin(), s.end()); // memory safe conversion to C style char
                nameGeneric.push_back('\0');
                name = &nameGeneric[0];
            }
            midiSender->sendSysex(CMD_TRACK_NAME, 0, numInBank, name);
        }
    }
}

void NiMidiSurface::SetSurfaceVolume(MediaTrack* track, double volume) {
    if (g_connectedState != KK_NIHIA_CONNECTED) return;
    debugLog("SetSurfaceVolume");
    
    int id = CSurf_TrackToID(track, false);
    if ((id >= bankStart) && (id <= bankEnd)) {
        int numInBank = id % BANK_NUM_TRACKS;
        char volText[64] = { 0 };
        mkvolstr(volText, volume);
        midiSender->sendSysex(CMD_TRACK_VOLUME_TEXT, 0, numInBank, volText);
        midiSender->sendCc((CMD_KNOB_VOLUME0 + numInBank), volToChar_KkMk3(volume * 1.05925));
    }
}

void NiMidiSurface::SetSurfacePan(MediaTrack* track, double pan) {
    if (g_connectedState != KK_NIHIA_CONNECTED) return;
    debugLog("SetSurfacePan");
    
    int id = CSurf_TrackToID(track, false);
    if (id < bankStart || id > bankEnd) return;
    int numInBank = id % BANK_NUM_TRACKS;
    char panText[64];
    mkpanstr(panText, pan);
    midiSender->sendSysex(CMD_TRACK_PAN_TEXT, 0, numInBank, panText);
    midiSender->sendCc(CMD_KNOB_PAN0 + numInBank, panToChar(pan));
}

void NiMidiSurface::SetSurfaceMute(MediaTrack* track, bool mute) {
    if (g_connectedState != KK_NIHIA_CONNECTED) return;
    debugLog("SetSurfaceMute");
    
    int id = CSurf_TrackToID(track, false);
    if (id == g_trackInFocus) {
        midiSender->sendSysex(CMD_TOGGLE_SEL_TRACK_MUTE, mute ? 1 : 0, 0); // Needed by NIHIA v1.8.7 (KK v2.1.2)
        midiSender->sendCc(CMD_TOGGLE_SEL_TRACK_MUTE, mute ? 1 : 0); // Needed by NIHIA v1.8.8 (KK v2.1.3)
    }
    if ((id >= bankStart) && (id <= bankEnd)) {
        int numInBank = id % BANK_NUM_TRACKS;
        if (g_muteStateBank[numInBank] != mute) { // Efficiency: only send updates if soemthing changed
            g_muteStateBank[numInBank] = mute;
            midiSender->sendSysex(CMD_TRACK_MUTED, mute ? 1 : 0, numInBank);
        }
    }
}

void NiMidiSurface::SetSurfaceSolo(MediaTrack* track, bool solo) {
    if (g_connectedState != KK_NIHIA_CONNECTED) return;
    debugLog("SetSurfaceSolo");
    
    // Note: Solo in Reaper can have different meanings (Solo In Place, Solo In Front and much more -> Reaper Preferences)
    int id = CSurf_TrackToID(track, false);

    // --------- MASTER: Ignore solo on master, id = 0 is only used as an "any track is soloed" change indicator ------------
    if (id == 0) {
        // If g_anySolo state has changed update the tracks' muted by solo states within the current bank
        if (g_anySolo != solo) {
            g_anySolo = solo;
            allMixerUpdate(midiSender); // Everything needs to be updated, not good enough to just update muted_by_solo states
        }
        // If any track is soloed the currently selected track will be muted by solo unless it is also soloed
        if (g_trackInFocus > 0) {
            if (g_anySolo) {
                MediaTrack* track = CSurf_TrackFromID(g_trackInFocus, false);
                if (!track) {
                    return;
                }
                int soloState = *(int*)GetSetMediaTrackInfo(track, "I_SOLO", nullptr);
                midiSender->sendSysex(CMD_SEL_TRACK_MUTED_BY_SOLO, (soloState == 0) ? 1 : 0, 0); // Needed by NIHIA v1.8.7 (KK v2.1.2)
                midiSender->sendCc(CMD_SEL_TRACK_MUTED_BY_SOLO, (soloState == 0) ? 1 : 0); // Needed by NIHIA v1.8.8 (KK v2.1.3)
            }
            else {
                midiSender->sendSysex(CMD_SEL_TRACK_MUTED_BY_SOLO, 0, 0); // Needed by NIHIA v1.8.7 (KK v2.1.2)
                midiSender->sendCc(CMD_SEL_TRACK_MUTED_BY_SOLO, 0); // Needed by NIHIA v1.8.8 (KK v2.1.3)
            }
        }
        return;
    }

    // ------------------------- TRACKS: Solo state has changed on individual tracks ----------------------------------------
    if (id == g_trackInFocus) {
        midiSender->sendSysex(CMD_TOGGLE_SEL_TRACK_SOLO, solo ? 1 : 0, 0); // Needed by NIHIA v1.8.7 (KK v2.1.2)
        midiSender->sendCc(CMD_TOGGLE_SEL_TRACK_SOLO, solo ? 1 : 0); // Needed by NIHIA v1.8.8 (KK v2.1.3)
    }
    if ((id >= bankStart) && (id <= bankEnd)) {
        int numInBank = id % BANK_NUM_TRACKS;
        if (solo) {
            if (g_soloStateBank[numInBank] != 1) {
                g_soloStateBank[numInBank] = 1;
                midiSender->sendSysex(CMD_TRACK_SOLOED, 1, numInBank);
                midiSender->sendSysex(CMD_TRACK_MUTED_BY_SOLO, 0, numInBank);
            }
        }
        else {
            if (g_soloStateBank[numInBank] != 0) {
                g_soloStateBank[numInBank] = 0;
                midiSender->sendSysex(CMD_TRACK_SOLOED, 0, numInBank);
                midiSender->sendSysex(CMD_TRACK_MUTED_BY_SOLO, g_anySolo ? 1 : 0, numInBank);
            }
        }
    }
}

void NiMidiSurface::SetSurfaceRecArm(MediaTrack* track, bool armed) {
    if (g_connectedState != KK_NIHIA_CONNECTED) return;
    // Note: record arm also leads to a cascade of other callbacks (-> filtering required!)
    int id = CSurf_TrackToID(track, false);
    if ((id >= bankStart) && (id <= bankEnd)) {
        int numInBank = id % BANK_NUM_TRACKS;
        midiSender->sendSysex(CMD_TRACK_ARMED, armed ? 1 : 0, numInBank);
    }
}

int NiMidiSurface::Extended(int call, void* parm1, void* parm2, void* parm3) {
    if (g_connectedState != KK_NIHIA_CONNECTED) return 0;
    if (call != CSURF_EXT_SETMETRONOME) {
        return 0; // we are only interested in the metronome. Note: This works fine but does not update the status when changing project tabs
    }
    midiSender->sendCc(CMD_METRO, (parm1 == 0) ? 0 : 1);
    return 1;
}

void NiMidiSurface::_onMidiEvent(MIDI_event_t* event) {
    if (event->midi_message[0] != MIDI_CC) return;

    unsigned char& command = event->midi_message[1];
    unsigned char& value = event->midi_message[2];

    // Handshake
    if (command == CMD_HELLO) {
        protocolVersion = value;
        if (value > 0) {
            debugLog("CMD_HELLO");
            // Turn on button lights
            midiSender->sendCc(CMD_UNDO, 1);
            midiSender->sendCc(CMD_REDO, 1);
            midiSender->sendCc(CMD_CLEAR, 1);
            midiSender->sendCc(CMD_QUANTIZE, 1);
            allMixerUpdate(midiSender);
            g_connectedState = KK_NIHIA_CONNECTED;
        }

        return;
    }
    else if (doubleClickCommands.find(command) != doubleClickCommands.end()) {
        addEventToMap(command, value);
        return;
    }

    static CommandProcessor processor(*midiSender);
    processor.Handle(command, value, EVENT_CLICK_SINGLE);
}

void NiMidiSurface::addEventToMap(unsigned char command, unsigned char value) {
    // Check if the command already has 2 events stored in the map & if still during cooldown to prevent multiple clicks
    if (eventMap[command].size() >= 2 || (nextOpenTimer > timer)) {
        debugLog("Max events reached for command: " + std::to_string(command));
        return;
    }

    MidiEventData eventData = { value, timer };
    eventMap[command].push_back(eventData);
}

void NiMidiSurface::UpdateMixerScreenEncoder(int id, int numInBank)
{
    debugLog("UpdateMixerScreenEncoder");
    
    int oldBankStart = bankStart;
    bankStart = id - numInBank;
    if (bankStart != oldBankStart) {
        // Update everything
        allMixerUpdate(midiSender); // Note: this will also update 4D track nav LEDs, g_muteStateBank and g_soloStateBank caches
    }
    else {
        // Update 4D Encoder track navigation LEDs
        int numTracks = CSurf_NumTracks(false);
        int trackNavLights = 3; // left and right on
        if (g_trackInFocus < 2) {
            trackNavLights &= 2; // left off
        }
        if (g_trackInFocus >= numTracks) {
            trackNavLights &= 1; // right off
        }
        midiSender->sendCc(CMD_NAV_TRACKS, trackNavLights);
    }
    if (g_trackInFocus != 0) {
        // Mark selected track as available and update Mute and Solo Button lights
        midiSender->sendSysex(CMD_SEL_TRACK_AVAILABLE, 1, 0); // Needed by NIHIA v1.8.7 (KK v2.1.2)
        midiSender->sendCc(CMD_SEL_TRACK_AVAILABLE, 1); // Needed by NIHIA v1.8.8 (KK v2.1.3)
        midiSender->sendSysex(CMD_TOGGLE_SEL_TRACK_MUTE, g_muteStateBank[numInBank] ? 1 : 0, 0); // Needed by NIHIA v1.8.7 (KK v2.1.2)
        midiSender->sendCc(CMD_TOGGLE_SEL_TRACK_MUTE, g_muteStateBank[numInBank] ? 1 : 0); // Needed by NIHIA v1.8.8 (KK v2.1.3)
        midiSender->sendSysex(CMD_TOGGLE_SEL_TRACK_SOLO, g_soloStateBank[numInBank], 0); // Needed by NIHIA v1.8.7 (KK v2.1.2)
        midiSender->sendCc(CMD_TOGGLE_SEL_TRACK_SOLO, g_soloStateBank[numInBank]); // Needed by NIHIA v1.8.8 (KK v2.1.3)
        if (g_anySolo) {
            midiSender->sendSysex(CMD_SEL_TRACK_MUTED_BY_SOLO, (g_soloStateBank[numInBank] == 0) ? 1 : 0, 0); // Needed by NIHIA v1.8.7 (KK v2.1.2)
            midiSender->sendCc(CMD_SEL_TRACK_MUTED_BY_SOLO, (g_soloStateBank[numInBank] == 0) ? 1 : 0); // Needed by NIHIA v1.8.8 (KK v2.1.3)
        }
        else {
            midiSender->sendSysex(CMD_SEL_TRACK_MUTED_BY_SOLO, 0, 0); // Needed by NIHIA v1.8.7 (KK v2.1.2)
            midiSender->sendCc(CMD_SEL_TRACK_MUTED_BY_SOLO, 0); // Needed by NIHIA v1.8.8 (KK v2.1.3)
        }
    }
    else {
        // Master track not available for Mute and Solo
        midiSender->sendSysex(CMD_SEL_TRACK_AVAILABLE, 0, 0); // Needed by NIHIA v1.8.7 (KK v2.1.2)
        midiSender->sendCc(CMD_SEL_TRACK_AVAILABLE, 0); // Needed by NIHIA v1.8.8 (KK v2.1.3)
    }
    // Let Keyboard know about changed track selection
    midiSender->sendSysex(CMD_TRACK_SELECTED, 1, numInBank);
}

void NiMidiSurface::updateTransportAndNavButtons() {
    debugLog("updateTransportAndNavButtons");
    
    midiSender->sendCc(CMD_CLEAR, 1);
    metronomeUpdate(midiSender);
    if (GetPlayState() & 4) {
        midiSender->sendCc(CMD_REC, 1);
    }
    else {
        midiSender->sendCc(CMD_REC, 0);
    }
    if (GetSetRepeat(-1)) {
        midiSender->sendCc(CMD_LOOP, 1);
    }
    else {
        midiSender->sendCc(CMD_LOOP, 0);
    }
    int numTracks = CSurf_NumTracks(false);
    int trackNavLights = 3; // left and right on
    if (g_trackInFocus < 2) {
        trackNavLights &= 2; // left off
    }
    if (g_trackInFocus >= numTracks) {
        trackNavLights &= 1; // right off
    }
    midiSender->sendCc(CMD_NAV_TRACKS, trackNavLights);
    midiSender->sendCc(CMD_NAV_CLIPS, 0); // ToDo: also restore  these lights to correct values
}

void NiMidiSurface::cycleEncoderLEDs(int& cycleTimer, int& cyclePos, CycleDirection direction, MidiSender* midiSender)
{
    debugLog("cycleEncoderLEDs");
    cycleTimer += 1;
    if (cycleTimer >= CYCLE_T) {
        cycleTimer = 0;
        cyclePos = (cyclePos + 1) % 4;

        switch (cyclePos) {
        case 0:
            midiSender->sendCc(CMD_NAV_TRACKS, 1);
            midiSender->sendCc(CMD_NAV_CLIPS, 0);
            break;
        case 1:
            if (direction == CLOCKWISE) {
                midiSender->sendCc(CMD_NAV_TRACKS, 0);
                midiSender->sendCc(CMD_NAV_CLIPS, 1);
            }
            else {
                midiSender->sendCc(CMD_NAV_TRACKS, 0);
                midiSender->sendCc(CMD_NAV_CLIPS, 2);
            }
            break;
        case 2:
            midiSender->sendCc(CMD_NAV_TRACKS, 2);
            midiSender->sendCc(CMD_NAV_CLIPS, 0);
            break;
        case 3:
            if (direction == CLOCKWISE) {
                midiSender->sendCc(CMD_NAV_TRACKS, 0);
                midiSender->sendCc(CMD_NAV_CLIPS, 2);
            }
            else {
                midiSender->sendCc(CMD_NAV_TRACKS, 0);
                midiSender->sendCc(CMD_NAV_CLIPS, 1);
            }
            break;
        }
    }
}
