/*
 * ReaKontrol
 * Main plug-in code
 * Author: brumbear@pacificpeaks
 * Copyright 2019-2020 Pacific Peaks Studio
 * Previous Authors: James Teh <jamie@jantrid.net>, Leonard de Ruijter, brumbear@pacificpeaks, Copyright 2018-2019 James Teh
 * License: GNU General Public License version 2.0
 */

#ifdef _WIN32
#include <windows.h>
#include <SetupAPI.h>
#include <initguid.h>
#include <Usbiodef.h>

#else

#include <sys/types.h>
#include <sys/time.h>

typedef unsigned int DWORD;

DWORD GetTickCount()
{
  // could switch to mach_getabsolutetime() maybe
  struct timeval tm={0,};
  gettimeofday(&tm,NULL);
  return (DWORD) (tm.tv_sec*1000 + tm.tv_usec/1000);
}

#endif

#include <string>
#include <cstring>
#include <sstream>
#define REAPERAPI_IMPLEMENT
#include "reaKontrol.h"
#include "NiMidiSurface.h"
#include "Utils.h"


using namespace std;
class NiMidiSurface;

extern "C" IReaperControlSurface* createNiMidiSurface();

BaseSurface::BaseSurface() {
	// ToDo: ???
}

BaseSurface::~BaseSurface() {
	if (this->_midiIn)  {
		this->_midiIn->stop();
		delete this->_midiIn;
	}
	if (this->_midiOut) {
		delete this->_midiOut;
	}
}

void BaseSurface::Run() {
	if (!this->_midiIn) {
		return;
	}
	this->_midiIn->SwapBufs(timeGetTime());
	MIDI_eventlist* list = this->_midiIn->GetReadBuf();
	MIDI_event_t* evt;
	int i = 0;
	while ((evt = list->EnumItems(&i))) {
		this->_onMidiEvent(evt);
	}
}

IReaperControlSurface* surface = nullptr;

extern "C" {
	REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t* rec) {
		if (rec) {
			// Load
			if (rec->caller_version != REAPER_PLUGIN_VERSION || !rec->GetFunc || REAPERAPI_LoadAPI(rec->GetFunc) != 0) {
				return 0;
			}

			// Keep a pointer to IReaperControlSurface
			surface = createNiMidiSurface();

			// Register the control surface
			rec->Register("csurf_inst", (void*)surface);
			
			// Initialize the action registry
			InitActionRegistry(rec);

			// Cast to NiMidiSurface to access custom methods
			NiMidiSurface* niSurface = dynamic_cast<NiMidiSurface*>(surface);

			// Register custom actions
			RegisterAction({
				"ReaKontrol_Toggle_DAW",
				"ReaKontrol: Toggle KK Keyboard DAW Control",
				[niSurface]() {
					toggleDAW(niSurface->GetMidiSender());
				}
			});

			return 1;
		}
		else {
			// Unload
			if (surface) {
				delete surface;
				surface = nullptr;
			}

			// Unregister all actions
			UnregisterAllActions();

			return 0;
		}
	}
}

IReaperControlSurface* createNiMidiSurface()
{
	return new NiMidiSurface();
}
