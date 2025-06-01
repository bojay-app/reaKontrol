#pragma once
#include "MidiSender.h"


struct aList {
    int ID[8];
    char name[8][128];
};

extern aList g_actionList;
extern bool g_actionListLoaded;

void loadConfigFile();
void loadReaKontrolSettings(const std::string& iniPath);
void loadActions(const char* pathname);
void showActionList(MidiSender* midiSender);
void callAction(unsigned char actionSlot, MidiSender* midiSender);
