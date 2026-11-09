#pragma once

#include <string>

namespace fc {

constexpr int kLegacyWidth = 12;
constexpr int kFullWidth = 24;
constexpr int kMaxWidth = 24;
constexpr int kBitsPerChar = 6;

// Legacy radios use a 12-character hexadecimal flashcode.
// APX radios use a 24-character base-64 flashcode (a 12-character
// prefix is also accepted and is what older APX codeplugs carry).
const char* legacyAlphabet();
const char* base64Alphabet();

// Value of one flashcode character, or -1 when it is not in the alphabet.
// Legacy lookup is case-insensitive; base-64 lookup is case-sensitive.
int charValue(char c, bool base64);
char valueChar(int value, bool base64);

// Luhn variant used by the reference tool.  Even character positions
// contribute the decimal digit sum of twice the value, odd positions the
// decimal digit sum of the value.  The returned digit brings the total to a
// multiple of ten.
int checkDigit(const int* values, int width);

// Canonical text with dashes and its check digit recomputed.
std::string formatCode(const int* values, int width, bool base64);

struct Decoded {
    bool ok = false;
    std::string error;
    int width = 0;
    bool hasCheck = false;
    int statedCheck = -1;
    int expectedCheck = -1;
    int values[kMaxWidth] = {};
    std::string formatted;
};

// Accepts 12/13 character legacy codes and 12/13/24/25 character APX codes,
// with or without dashes and surrounding whitespace.
Decoded parseCode(const std::string& text, bool base64);

} // namespace fc
