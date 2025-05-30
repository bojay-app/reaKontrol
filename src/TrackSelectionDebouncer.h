#pragma once
#include <unordered_map>
#include <chrono>

class TrackSelectionDebouncer {
public:
    void update(int trackId, bool selected);
    bool shouldFallbackToMaster(); // call this from Run()
    void reset();

private:
    std::unordered_map<int, bool> selectionMap;
    std::chrono::steady_clock::time_point lastUpdate;
    const int debounceMs = 100;
};
