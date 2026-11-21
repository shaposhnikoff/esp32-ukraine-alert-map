#pragma once
#include <cstddef>
#include <cstdint>

namespace alerts::geo {
struct Point { float x; float y; };
struct Area {
    const char* slug;
    const char* name;
    const char* parent;
    Point label;
    bool home;
};
struct MapMask {
    const uint16_t* runs;
    size_t word_count;
    uint16_t width;
    uint16_t height;
};
extern const Area areas[];
extern const size_t area_count;
extern const char directory_json[];
extern const MapMask home_mask;
extern const MapMask national_mask;
}  // namespace alerts::geo
