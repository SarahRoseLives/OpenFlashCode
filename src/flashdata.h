#pragma once

// One selectable radio option as published by the reference encoder.
struct FcOption {
    unsigned char digit;   // 0-based character position in the flashcode
    unsigned char bit;     // 0-based bit within that character
    const char* code;      // option code, e.g. H260 / QA02006
    const char* desc;      // short description
    const char* note;      // optional compatibility note / tooltip
};

struct FcPlatform {
    const char* id;        // wire id, e.g. Astro_APX8000
    const char* name;      // friendly name
    bool supports24;       // true for APX (base-64, 12 or 24 digit)
    const FcOption* options;
    int optionCount;
};

extern const FcPlatform g_platforms[];
extern const int g_platformCount;
