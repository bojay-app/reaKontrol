#include "TrackSelectionDebouncer.h"

void TrackSelectionDebouncer::update(int trackId, bool selected) {
    selectionMap[trackId] = selected;
    lastUpdate = std::chrono::steady_clock::now();
}

bool TrackSelectionDebouncer::shouldFallbackToMaster() {
    if (selectionMap.empty()) return false;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count();

    if (elapsed < debounceMs) return false;

    for (const auto& [_, isSelected] : selectionMap)
    {
        if (isSelected) return false;
    }

    return true; // no track selected → fallback to master
}

void TrackSelectionDebouncer::reset() {
    selectionMap.clear();
}
