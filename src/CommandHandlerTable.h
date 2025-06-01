#pragma once

#include <unordered_map>
#include <functional>

using HandlerFunc = std::function<bool(unsigned char command, unsigned char value, const char* info)>;

class CommandHandlerTable {
public:
    static CommandHandlerTable& get();

    void registerHandler(unsigned char command, HandlerFunc handler);
    bool dispatch(unsigned char command, unsigned char value, const char* info);

private:
    std::unordered_map<unsigned char, HandlerFunc> commandHandlers;
};
