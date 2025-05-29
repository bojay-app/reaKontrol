#include "ActionList.h"
#include "reaKontrol.h"
#include <string>
#include <sstream>
#include "Constants.h"
#include "Commands.h"

#ifdef __APPLE__
    #define strcpy_s(dest,dest_sz, src) strlcpy(dest,src, dest_sz)
    #define REAKONTROL_INI "/UserPlugins/ReaKontrolConfig/reakontrol.ini"
#else
    #define REAKONTROL_INI "\\UserPlugins\\ReaKontrolConfig\\reakontrol.ini"
#endif

aList g_actionList;
bool g_actionListLoaded = false; // action list will be populated from ini file after successful connection to allow Reaper main thread to load all extensions first

void loadActionList() {
    const char* pathname = GetResourcePath();
    std::string s_filename(pathname);
    s_filename += REAKONTROL_INI;
    s_filename.push_back('\0');
    pathname = &s_filename[0];

    if (!file_exists(pathname)) {
        WritePrivateProfileString("reakontrol_actions", "action_0_id", "40001", pathname);
        if (!WritePrivateProfileString("reakontrol_actions", "action_0_name", "Insert Default Track", pathname)) {
            std::ostringstream s;
            s << "Unable to write configuration file! Please read manual, subfolder must exist for the configuration file:\n\n"
              << s_filename;
            ShowMessageBox(s.str().c_str(), "ReaKontrol", 0);
            g_actionList.ID[0] = -1;
        }
    }

    if (file_exists(pathname)) {
        std::string s_keyName;
        char* key;
        char stringOut[128] = {};
        int stringOut_sz = sizeof(stringOut);

        for (int i = 0; i <= 7; ++i) {
            s_keyName = "action_" + std::to_string(i) + "_ID";
            s_keyName.push_back('\0');
            key = &s_keyName[0];
            GetPrivateProfileString("reakontrol_actions", key, nullptr, stringOut, stringOut_sz, pathname);

            if (stringOut[0] != '\0') {
                g_actionList.ID[i] = NamedCommandLookup(stringOut);
                if (g_actionList.ID[i]) {
                    s_keyName = "action_" + std::to_string(i) + "_name";
                    s_keyName.push_back('\0');
                    key = &s_keyName[0];
                    GetPrivateProfileString("reakontrol_actions", key, nullptr, stringOut, stringOut_sz, pathname);
                    if (stringOut[0] != '\0') {
                        strcpy_s(g_actionList.name[i], 128, stringOut);
                    } else {
                        g_actionList.name[i][0] = '\0';
                    }
                }
            } else {
                g_actionList.ID[i] = 0;
                g_actionList.name[i][0] = '\0';
            }
        }
    } else {
        std::ostringstream s;
        s << "Komplete Kontrol Keyboard successfully connected but configuration file not found! Please read manual how to configure custom actions via the configuration file:\n\n"
          << s_filename;
        ShowMessageBox(s.str().c_str(), "ReaKontrol", 0);
        g_actionList.ID[0] = -1;
    }
}

void showActionList(MidiSender* midiSender) {
    static char clearPeak[(BANK_NUM_TRACKS * 2) + 1] = { 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0 };
    if (!midiSender) return;

    midiSender->sendCc(CMD_NAV_BANKS, 0);
    for (int i = 0; i <= 7; ++i) {
        if (g_actionList.ID[i] > 0) {
            midiSender->sendSysex(CMD_TRACK_AVAIL, TRTYPE_MIDI, i);
            midiSender->sendSysex(CMD_TRACK_SOLOED, 1, i);
            midiSender->sendSysex(CMD_TRACK_MUTED_BY_SOLO, 0, i);
            midiSender->sendSysex(CMD_TRACK_MUTED, 0, i);
            midiSender->sendSysex(CMD_TRACK_ARMED, 0, i);
            midiSender->sendSysex(CMD_TRACK_SELECTED, 0, i);
            if (g_actionList.name[i][0] == '\0') {
                midiSender->sendSysex(CMD_TRACK_NAME, 0, i, "NAME?");
            } else {
                midiSender->sendSysex(CMD_TRACK_NAME, 0, i, &g_actionList.name[i][0]);
            }
            midiSender->sendSysex(CMD_TRACK_VOLUME_TEXT, 0, i, "Action");
            midiSender->sendSysex(CMD_TRACK_PAN_TEXT, 0, i, "Action");
            midiSender->sendCc((CMD_KNOB_VOLUME0 + i), 1);
            midiSender->sendCc((CMD_KNOB_PAN0 + i), 63);
        } else {
            midiSender->sendSysex(CMD_TRACK_AVAIL, 0, i);
            midiSender->sendSysex(CMD_TRACK_SOLOED, 0, i);
            midiSender->sendSysex(CMD_TRACK_MUTED_BY_SOLO, 0, i);
            midiSender->sendSysex(CMD_TRACK_MUTED, 0, i);
            midiSender->sendSysex(CMD_TRACK_ARMED, 0, i);
            midiSender->sendSysex(CMD_TRACK_SELECTED, 0, i);
            midiSender->sendSysex(CMD_TRACK_NAME, 0, i, "");
            midiSender->sendSysex(CMD_TRACK_VOLUME_TEXT, 0, i, " ");
            midiSender->sendSysex(CMD_TRACK_PAN_TEXT, 0, i, " ");
            midiSender->sendCc((CMD_KNOB_VOLUME0 + i), 1);
            midiSender->sendCc((CMD_KNOB_PAN0 + i), 63);
        }
    }

    if (g_actionList.ID[0] == -1) {
        midiSender->sendSysex(CMD_TRACK_AVAIL, TRTYPE_MIDI, 0);
        midiSender->sendSysex(CMD_TRACK_NAME, 0, 0, "Config file not found!");
    }
    midiSender->sendSysex(CMD_TRACK_VU, 2, 0, clearPeak);
}

void callAction(unsigned char actionSlot) {
    if (g_actionList.ID[actionSlot]) {
        Main_OnCommand(g_actionList.ID[actionSlot], 0);
    }
}