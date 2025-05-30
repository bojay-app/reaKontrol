#include "CommandHandlerTable.h"
#include <reaper/reaper_plugin_functions.h>
#include <sstream>

CommandHandlerTable& CommandHandlerTable::get() {
    static CommandHandlerTable instance;
    return instance;
}

void CommandHandlerTable::registerHandler(unsigned char command, HandlerFunc handler) {
    commandHandlers[command] = std::move(handler);
}

bool CommandHandlerTable::dispatch(unsigned char command, unsigned char value) {
    auto it = commandHandlers.find(command);
    if (it != commandHandlers.end()) {
        return it->second(command, value);
    }
    return false;
}