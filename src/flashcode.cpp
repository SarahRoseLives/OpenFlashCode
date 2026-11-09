#include "flashcode.h"

#include <cctype>
#include <cstring>

namespace fc {

const char* legacyAlphabet() {
    return "0123456789ABCDEF";
}

const char* base64Alphabet() {
    // Base-64 with the ambiguous glyphs removed (no I, O, i, l or o).
    return "0123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghjkmnpqrstuvwxyz#(+$)&%";
}

int charValue(char c, bool base64) {
    if (base64) {
        const char* alphabet = base64Alphabet();
        const char* found = std::strchr(alphabet, c);
        if (found == nullptr || c == '\0') {
            return -1;
        }
        return static_cast<int>(found - alphabet);
    }

    const unsigned char uch = static_cast<unsigned char>(c);
    const int upper = std::toupper(uch);
    if (upper >= '0' && upper <= '9') {
        return upper - '0';
    }
    if (upper >= 'A' && upper <= 'F') {
        return 10 + (upper - 'A');
    }
    return -1;
}

char valueChar(int value, bool base64) {
    if (value < 0 || value >= 64) {
        return '0';
    }
    return base64 ? base64Alphabet()[value] : legacyAlphabet()[value];
}

static int digitSum(int value) {
    int sum = 0;
    for (int rest = value; rest > 0; rest /= 10) {
        sum += rest % 10;
    }
    return sum;
}

int checkDigit(const int* values, int width) {
    int total = 0;
    for (int at = 0; at < width; ++at) {
        const int value = values[at];
        total += (at % 2 == 0) ? digitSum(value * 2) : digitSum(value);
    }
    return (10 - (total % 10)) % 10;
}

std::string formatCode(const int* values, int width, bool base64) {
    std::string out;
    out.reserve(width + 8);

    const int check = checkDigit(values, width);
    for (int at = 0; at < width; ++at) {
        if (at == kLegacyWidth) {
            out.push_back('-');
            out.push_back(static_cast<char>('0' + check));
            out.push_back('-');
        } else if (at > 0 && at % 6 == 0) {
            out.push_back('-');
        }
        out.push_back(valueChar(values[at], base64));
    }
    if (width == kLegacyWidth) {
        out.push_back('-');
        out.push_back(static_cast<char>('0' + check));
    }
    return out;
}

Decoded parseCode(const std::string& text, bool base64) {
    Decoded decoded;

    std::string characters;
    characters.reserve(text.size());
    for (char c : text) {
        const unsigned char uch = static_cast<unsigned char>(c);
        if (c == '-' || std::isspace(uch)) {
            continue;
        }
        characters.push_back(c);
    }

    const int length = static_cast<int>(characters.size());
    int width = 0;
    int checkAt = -1;

    if (length == kLegacyWidth) {
        width = kLegacyWidth;
    } else if (length == kLegacyWidth + 1) {
        width = kLegacyWidth;
        checkAt = kLegacyWidth;
    } else if (base64 && length == kFullWidth) {
        width = kFullWidth;
    } else if (base64 && length == kFullWidth + 1) {
        width = kFullWidth;
        checkAt = kLegacyWidth;
    } else {
        if (base64) {
            decoded.error = "A flashcode is 12 or 24 digits, with or without a check digit.";
        } else {
            decoded.error = "A flashcode is 12 digits, with or without a check digit.";
        }
        return decoded;
    }

    for (int at = 0; at < length; ++at) {
        const char c = characters[at];
        if (at == checkAt) {
            if (c < '0' || c > '9') {
                decoded.error = "The check digit must be a decimal digit (0-9).";
                return decoded;
            }
            decoded.hasCheck = true;
            decoded.statedCheck = c - '0';
            continue;
        }

        const int value = charValue(c, base64);
        if (value < 0) {
            decoded.error = std::string("'") + c + "' is not a valid flashcode character for this platform.";
            return decoded;
        }

        const int index = (checkAt >= 0 && at > checkAt) ? at - 1 : at;
        if (index < 0 || index >= kMaxWidth) {
            decoded.error = "Flashcode is too long.";
            return decoded;
        }
        decoded.values[index] = value;
    }

    decoded.ok = true;
    decoded.width = width;
    decoded.expectedCheck = checkDigit(decoded.values, width);
    decoded.formatted = formatCode(decoded.values, width, base64);
    return decoded;
}

} // namespace fc
