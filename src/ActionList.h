#pragma once
#include "MidiSender.h"

struct aList {
    int ID[8];
    char name[8][128];
};

extern aList g_actionList;
extern bool g_actionListLoaded;

void loadActionList();
void showActionList(MidiSender* midiSender);
