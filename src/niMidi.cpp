/*
 * ReaKontrol
 * Support for MIDI protocol used by Komplete Kontrol S-series Mk3
 * Author: Chieh Teng Wang <bojay.app@gmail.com>
 * Copyright 2019-2020 Pacific Peaks Studio
 * Previous Authors: James Teh <jamie@jantrid.net>, Leonard de Ruijter, brumbear@pacificpeaks
 * License: GNU General Public License version 2.0
 */

#include <string>
#include <sstream>
#include <cstring>
#include <vector>
#include "reaKontrol.h"

#include "Constants.h"
#include "Commands.h"
#include "Utils.h"
#include "ActionList.h"
#include "MidiSender.h"
#include "CommandProcessor.h"

using namespace std;

// ==============================================================================================================================
// Architectural Note: Callback vs polling Architecture. The approach here is to use a callback architecture when possible and poll only if
// callbacks are not available or behave inefficiently or do not report reliably
// Some Reaper callbacks (Set....) are called unecessarily (?) often in Reaper, see various forum reports & comments from schwa
// Notably this thread: https://forum.cockos.com/showthread.php?t=49536
// => Implement state checking / looking for changes at beginning of every callback to return immediately if call is not necessary -> save CPU
// ==============================================================================================================================

// ToDo: NIHIA shows a strange behaviour: The mixer view is sometimes not updated completely until any of the following 
// parameters change: peak, volume, pan, rec arm, mute, solo 
// It seems NIHIA caches certain values to save bandwidth, but this can lead to inconsistencies if we do not constantly push new data to NIHIA
// This would also explain the behaviour of the peak indicators...

// Entry point surface class (minimal stub to initialize)
class NiMidiSurface : public BaseSurface {
public:
	NiMidiSurface()
		: BaseSurface() {
		g_connectedState = KK_NOT_CONNECTED;
	}

	virtual ~NiMidiSurface() {
		for (int i = 0; i < 8; ++i) {
			if (midiSender) {
				midiSender->sendSysex(CMD_TRACK_AVAIL, 0, i);
			}
		}

		if (midiSender) {
			midiSender->sendCc(CMD_GOODBYE, 0);
		}

		delete processor;
		processor = nullptr;

		delete midiSender;
		midiSender = nullptr;

		protocolVersion = 0;
		g_connectedState = KK_NOT_CONNECTED;
	}

	virtual const char* GetTypeString() override {
		return "KompleteKontrolNiMidi";
	}

	virtual const char* GetDescString() override {
		return "Komplete Kontrol S-series Mk2/A-series/M-series";
	}

	virtual void Run() override {
		static int scanTimer = SCAN_T - 1; // first scan shall happen immediately
		static int connectCount = 0;
		static int inDev = -1;
		static int outDev = -1;

		static bool lightOn = false;
		static int flashTimer = -1; // EXT_EDIT_OFF: flashTimer = -1, EXT_EDIT_ACTIONS: flashTimer = -4
		static int cycleTimer = -1;
		static int cyclePos = 0;

		if (g_connectedState == KK_NOT_CONNECTED) {
			/*----------------- Scan for KK Keyboard -----------------*/
			scanTimer += 1;
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
			/*----------------- Try to connect and initialize -----------------*/
			BaseSurface::Run();
			scanTimer += 1;
			if (scanTimer >= SCAN_T) {
				scanTimer = 0;
				if (connectCount < CONNECT_N) {
					connectCount += 1;
					midiSender->sendCc(CMD_HELLO, 2); // Protocol version 2
					// ToDo: Find out if A and M series require or only support protocol version 1 or 3.
					// S Mk2 supports versions 1-3, but version 2 is best.
					// Version 1 does not fully support SHIFT button with encoder
					// Version 3 does not light up TEMPO button
				}
				else {
					int answer = ShowMessageBox("Komplete Kontrol Keyboard detected but failed to connect. Please restart NI services (NIHostIntegrationAgent), then retry. ", "ReaKontrol", 5);
					if (this->_midiIn) {
						this->_midiIn->stop();
						delete this->_midiIn;
					}
					if (this->_midiOut) {
						delete this->_midiOut;
					}
					connectCount = 0;
					if (answer == 4) {
						g_connectedState = KK_NOT_CONNECTED; // Retry -> Re-Scan
					}
					else {
						g_connectedState = -1; // Give up, plugin not doing anything
					}
				}
			}
		}
		else if (g_connectedState == KK_NIHIA_CONNECTED) {
			/*----------------- We are successfully connected -----------------*/
			if (!g_actionListLoaded) {
				loadActionList();
				g_actionListLoaded = true;
			}
		
			if (getExtEditMode()  == EXT_EDIT_OFF) {
				if (flashTimer != -1) { // are we returning from one of the Extended Edit Modes?
					this->_updateTransportAndNavButtons();
					allMixerUpdate(midiSender);
					this->_peakMixerUpdate();

					lightOn = false;
					flashTimer = -1;
					cycleTimer = -1;
					cyclePos = 0;
				}
			}
			else if (getExtEditMode()  == EXT_EDIT_ON) {
				// Flash all Ext Edit buttons
				flashTimer += 1;
				if (flashTimer >= FLASH_T) {
					flashTimer = 0;
					lightOn = !lightOn;

					const unsigned char navValue = lightOn ? 3 : 0;
					const unsigned char onOff = lightOn ? 1 : 0;

					midiSender->sendCc(CMD_NAV_TRACKS, navValue);
					midiSender->sendCc(CMD_NAV_CLIPS, navValue);
					midiSender->sendCc(CMD_REC, onOff);
					midiSender->sendCc(CMD_CLEAR, onOff);
					midiSender->sendCc(CMD_LOOP, onOff);
					midiSender->sendCc(CMD_METRO, onOff);
				}
			}
			else if (getExtEditMode()  == EXT_EDIT_LOOP) {
				if (cycleTimer == -1) {
					this->_updateTransportAndNavButtons();
					midiSender->sendCc(CMD_NAV_TRACKS, 1);
					midiSender->sendCc(CMD_NAV_CLIPS, 0);
				}
				// Cycle 4D Encoder LEDs
				cycleTimer += 1;
				if (cycleTimer >= CYCLE_T) {
					cycleTimer = 0;
					cyclePos += 1;
					if (cyclePos > 3) {
						cyclePos = 0;
					}
					switch (cyclePos) { // clockwise cycling
					case 0:
						midiSender->sendCc(CMD_NAV_TRACKS, 1);
						midiSender->sendCc(CMD_NAV_CLIPS, 0);
						break;
					case 1:
						midiSender->sendCc(CMD_NAV_TRACKS, 0);
						midiSender->sendCc(CMD_NAV_CLIPS, 1);
						break;
					case 2:
						midiSender->sendCc(CMD_NAV_TRACKS, 2);
						midiSender->sendCc(CMD_NAV_CLIPS, 0);
						break;
					case 3:
						midiSender->sendCc(CMD_NAV_TRACKS, 0);
						midiSender->sendCc(CMD_NAV_CLIPS, 2);
						break;
					}
				}
				// Flash LOOP button
				flashTimer += 1;
				if (flashTimer >= FLASH_T) {
					flashTimer = 0;
					if (lightOn) {
						lightOn = false;
						midiSender->sendCc(CMD_LOOP, 0);
					}
					else {
						lightOn = true;
						midiSender->sendCc(CMD_LOOP, 1);
					}
				}
			}
			else if (getExtEditMode()  == EXT_EDIT_TEMPO) {
				if (cycleTimer == -1) {
					this->_updateTransportAndNavButtons();
					midiSender->sendCc(CMD_NAV_TRACKS, 1);
					midiSender->sendCc(CMD_NAV_CLIPS, 0);
				}
				// Cycle 4D Encoder LEDs
				cycleTimer += 1;
				if (cycleTimer >= CYCLE_T) {
					cycleTimer = 0;
					cyclePos += 1;
					if (cyclePos > 3) {
						cyclePos = 0;
					}
					switch (cyclePos) { // counter clockwise cycling
					case 0:
						midiSender->sendCc(CMD_NAV_TRACKS, 1);
						midiSender->sendCc(CMD_NAV_CLIPS, 0);
						break;
					case 1:
						midiSender->sendCc(CMD_NAV_TRACKS, 0);
						midiSender->sendCc(CMD_NAV_CLIPS, 2);
						break;
					case 2:
						midiSender->sendCc(CMD_NAV_TRACKS, 2);
						midiSender->sendCc(CMD_NAV_CLIPS, 0);
						break;
					case 3:
						midiSender->sendCc(CMD_NAV_TRACKS, 0);
						midiSender->sendCc(CMD_NAV_CLIPS, 1);
						break;
					}
				}
				// Flash METRO button
				flashTimer += 1;
				if (flashTimer >= FLASH_T) {
					flashTimer = 0;
					if (lightOn) {
						lightOn = false;
						midiSender->sendCc(CMD_METRO, 0);
					}
					else {
						lightOn = true;
						midiSender->sendCc(CMD_METRO, 1);
					}
				}
			}
			else if (getExtEditMode()  == EXT_EDIT_ACTIONS) {
				if (flashTimer != -4) {
					this->_updateTransportAndNavButtons();
					lightOn = false;
					flashTimer = -4;
					cycleTimer = -1;
					cyclePos = 0;
				}
			}
			if (getExtEditMode() != EXT_EDIT_ACTIONS) {
				this->_peakMixerUpdate();
			}
			BaseSurface::Run();
		}
	}

	virtual void SetPlayState(bool play, bool pause, bool rec) override {
		if (g_connectedState != KK_NIHIA_CONNECTED) return;
		// Update transport button lights
		if (rec) {
			midiSender->sendCc(CMD_REC, 1);
		}
		else {
			midiSender->sendCc(CMD_REC, 0);
		}
		if (pause) {
			midiSender->sendCc(CMD_PLAY, 1);
			midiSender->sendCc(CMD_STOP, 1); // since there is no Pause button on KK we indicate it with both Play and Stop lit
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

	virtual void SetRepeatState(bool rep) override {
		if (g_connectedState != KK_NIHIA_CONNECTED) return;
		// Update repeat (aka loop) button light
		if (rep) {
			midiSender->sendCc(CMD_LOOP, 1);
		}
		else {
			midiSender->sendCc(CMD_LOOP, 0);
		}
	}

	virtual void SetTrackListChange() override {
		if (g_connectedState != KK_NIHIA_CONNECTED) return;
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

	// ToDo: If a track has Solo_Defeat activated the lights in the Mixer are not always updated correctly when navigating

	virtual void SetSurfaceSelected(MediaTrack* track, bool selected) override {
		// Use this callback for:
		// - changed track selection and KK instance focus
		// - changed automation mode
		// - changed track name

		// Using SetSurfaceSelected() rather than OnTrackSelection() or SetTrackTitle():
		// SetSurfaceSelected() is less economical because it will be called multiple times when something changes (also for unselecting tracks, change of any record arm, change of any auto mode, change of name, ...).
		// However, SetSurfaceSelected() is the more robust choice because of: https://forum.cockos.com/showpost.php?p=2138446&postcount=15
		// A good solution for efficiency is to only evaluate messages with (selected == true).
		if (g_connectedState != KK_NIHIA_CONNECTED) return;
		if (selected) {
			int id = CSurf_TrackToID(track, false);
			int numInBank = id % BANK_NUM_TRACKS;
			// ---------------- Track Selection and Instance Focus ----------------
			if (id != g_trackInFocus) {
				// Track selection has changed
				g_trackInFocus = id;
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
				// Set KK Instance Focus
				midiSender->sendSysex(CMD_SET_KK_INSTANCE, 0, 0, getKkInstanceName(track));
			}
			// ------------------------- Automation Mode -----------------------
			// Update automation mode
			// AUTO = ON: touch, write, latch or latch preview
			// AUTO = OFF: trim or read
			int globalAutoMode = GetGlobalAutomationOverride();
			if ((id == g_trackInFocus) && (globalAutoMode == -1)) {
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
			if ((id > 0) && (id >= bankStart) && (id <= bankEnd)) {
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

	// ToDo: Consider also caching volume (MIDI value and string) to improve efficiency
	virtual void SetSurfaceVolume(MediaTrack* track, double volume) override {
		if (g_connectedState != KK_NIHIA_CONNECTED) return;
		int id = CSurf_TrackToID(track, false);
		if ((id >= bankStart) && (id <= bankEnd)) {
			int numInBank = id % BANK_NUM_TRACKS;
			char volText[64] = { 0 };
			mkvolstr(volText, volume);
			midiSender->sendSysex(CMD_TRACK_VOLUME_TEXT, 0, numInBank, volText);
			midiSender->sendCc((CMD_KNOB_VOLUME0 + numInBank), volToChar_KkMk2(volume * 1.05925));
		}
	}

	// ToDo: Consider also caching pan (MIDI value and string) to improve efficiency
	virtual void SetSurfacePan(MediaTrack* track, double pan) override {
		if (g_connectedState != KK_NIHIA_CONNECTED) return;
		int id = CSurf_TrackToID(track, false);
		if ((id >= bankStart) && (id <= bankEnd)) {
			int numInBank = id % BANK_NUM_TRACKS;
			char panText[64];
			mkpanstr(panText, pan);
			midiSender->sendSysex(CMD_TRACK_PAN_TEXT, 0, numInBank, panText); // NIHIA v1.8.7.135 uses internal text
			midiSender->sendCc((CMD_KNOB_PAN0 + numInBank), panToChar(pan));
		}
	}

	virtual void SetSurfaceMute(MediaTrack* track, bool mute) override {
		if (g_connectedState != KK_NIHIA_CONNECTED) return;
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

	// ToDo: If a track has Solo_Defeat activated the lights in the Mixer are not always updated correctly when navigating

	virtual void SetSurfaceSolo(MediaTrack* track, bool solo) override {
		if (g_connectedState != KK_NIHIA_CONNECTED) return;
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

	virtual void SetSurfaceRecArm(MediaTrack* track, bool armed) override {
		if (g_connectedState != KK_NIHIA_CONNECTED) return;
		// Note: record arm also leads to a cascade of other callbacks (-> filtering required!)
		int id = CSurf_TrackToID(track, false);
		if ((id >= bankStart) && (id <= bankEnd)) {
			int numInBank = id % BANK_NUM_TRACKS;
			midiSender->sendSysex(CMD_TRACK_ARMED, armed ? 1 : 0, numInBank);
		}
	}

	virtual int Extended(int call, void* parm1, void* parm2, void* parm3) override {
		if (g_connectedState != KK_NIHIA_CONNECTED) return 0;
		if (call != CSURF_EXT_SETMETRONOME) {
			return 0; // we are only interested in the metronome. Note: This works fine but does not update the status when changing project tabs
		}
		midiSender->sendCc(CMD_METRO, (parm1 == 0) ? 0 : 1);
		return 1;
	}

	//===============================================================================================================================

protected:
	void _onMidiEvent(MIDI_event_t* event) override {
		if (event->midi_message[0] != MIDI_CC) return;

		unsigned char& command = event->midi_message[1];
		unsigned char& value = event->midi_message[2];

		// Handshake
		if (command == CMD_HELLO) {
        	protocolVersion = value;
        if (value > 0) {
            midiSender->sendCc(CMD_UNDO, 1);
            midiSender->sendCc(CMD_REDO, 1);
            midiSender->sendCc(CMD_CLEAR, 1);
            midiSender->sendCc(CMD_QUANTIZE, 1);
            allMixerUpdate(midiSender);
            g_connectedState = KK_NIHIA_CONNECTED;
            Help_Set("ReaKontrol: KK-Keyboard connected", false);
		}

		return;
	}

		static CommandProcessor processor(*midiSender);
		processor.Handle(command, value);
	}

	//===============================================================================================================================

private:
	MidiSender* midiSender = nullptr;
	CommandProcessor* processor = nullptr;

	void _peakMixerUpdate() {
		// Peak meters. Note: Reaper reports peak, NOT VU	

		// ToDo: Peak Hold in KK display shall be erased immediately when changing bank
		// ToDo: Peak Hold in KK display shall be erased after decay time t when track muted or no signal.
		// ToDo: Explore the effect of sending CMD_SEL_TRACK_PARAMS_CHANGED after sending CMD_TRACK_VU
		// ToDo: Consider caching and not sending anything via SysEx if no values have changed.

		// Meter information is sent to KK as array (string of chars) for all 16 channels (8 x stereo) of one bank.
		// A value of 0 will result in stopping to refresh meters further to right as it is interpretated as "end of string".
		// peakBank[0]..peakBank[31] are used for data. The array needs one additional last char peakBank[32] set as "end of string" marker.
		static char peakBank[(BANK_NUM_TRACKS * 2) + 1];
		int j = 0;
		double peakValue = 0;
		int numInBank = 0;
		for (int id = bankStart; id <= bankEnd; ++id, ++numInBank) {
			MediaTrack* track = CSurf_TrackFromID(id, false);
			if (!track) {
				break;
			}
			j = 2 * numInBank;
			if (HIDE_MUTED_BY_SOLO) {
				// If any track is soloed then only soloed tracks and the master show peaks (irrespective of their mute state)
				if (g_anySolo) {
					if ((g_soloStateBank[numInBank] == 0) && (((numInBank != 0) && (bankStart == 0)) || (bankStart != 0))) {
						peakBank[j] = 1;
						peakBank[j + 1] = 1;
					}
					else {
						peakValue = Track_GetPeakInfo(track, 0); // left channel
						peakBank[j] = volToChar_KkMk2(peakValue); // returns value between 1 and 127
						peakValue = Track_GetPeakInfo(track, 1); // right channel
						peakBank[j + 1] = volToChar_KkMk2(peakValue); // returns value between 1 and 127
					}
				}
				// If no tracks are soloed then muted tracks shall show no peaks
				else {
					if (g_muteStateBank[numInBank]) {
						peakBank[j] = 1;
						peakBank[j + 1] = 1;
					}
					else {
						peakValue = Track_GetPeakInfo(track, 0); // left channel
						peakBank[j] = volToChar_KkMk2(peakValue); // returns value between 1 and 127
						peakValue = Track_GetPeakInfo(track, 1); // right channel
						peakBank[j + 1] = volToChar_KkMk2(peakValue); // returns value between 1 and 127					
					}
				}
			}
			else {
				// Muted tracks that are NOT soloed shall show no peaks. Tracks muted by solo show peaks but they appear greyed out.
				if ((g_soloStateBank[numInBank] == 0) && (g_muteStateBank[numInBank])) {
					peakBank[j] = 1;
					peakBank[j + 1] = 1;
				}
				else {
					peakValue = Track_GetPeakInfo(track, 0); // left channel
					peakBank[j] = volToChar_KkMk2(peakValue); // returns value between 1 and 127
					peakValue = Track_GetPeakInfo(track, 1); // right channel
					peakBank[j + 1] = volToChar_KkMk2(peakValue); // returns value between 1 and 127					
				}
			}
		}
		peakBank[j + 2] = '\0'; // end of string (no tracks available further to the right)
		midiSender->sendSysex(CMD_TRACK_VU, 2, 0, peakBank);
	}

	void _updateTransportAndNavButtons() {
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
};

extern "C" IReaperControlSurface* createNiMidiSurface() {
    return new NiMidiSurface();
}
