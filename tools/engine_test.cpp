#include <cstdio>
#include <cstring>
#include <string>

#include "flashcode.h"
#include "flashdata.h"

static int g_failures = 0;

static void Expect(const std::string& actual, const std::string& expected, const char* what) {
    if (actual == expected) {
        std::printf("ok   %s\n", what);
    } else {
        std::printf("FAIL %s\n     expected: %s\n     actual:   %s\n", what, expected.c_str(), actual.c_str());
        ++g_failures;
    }
}

int main() {
    int values[fc::kMaxWidth] = {};

    // Legacy: MTS2000 D1 B2 (value 2 in character 1).
    values[0] = 2;
    Expect(fc::formatCode(values, 12, false), "200000-000000-6", "legacy D1B2");

    std::memset(values, 0, sizeof(values));
    values[1] = 1;
    Expect(fc::formatCode(values, 12, false), "010000-000000-9", "legacy D2B1");

    std::memset(values, 0, sizeof(values));
    values[5] = 15;
    Expect(fc::formatCode(values, 12, false), "00000F-000000-4", "legacy D6 all bits");

    // APX base-64 single bits and high values.
    std::memset(values, 0, sizeof(values));
    values[0] = 1;
    Expect(fc::formatCode(values, 12, true), "100000-000000-8", "apx 12 D1B1");
    Expect(fc::formatCode(values, 24, true), "100000-000000-8-000000-000000", "apx 24 D1B1");

    std::memset(values, 0, sizeof(values));
    values[0] = 32;
    Expect(fc::formatCode(values, 12, true), "Y00000-000000-0", "apx 12 D1B6");
    Expect(fc::formatCode(values, 24, true), "Y00000-000000-0-000000-000000", "apx 24 D1B6");

    std::memset(values, 0, sizeof(values));
    values[0] = 40;
    Expect(fc::formatCode(values, 12, true), "g00000-000000-2", "apx 12 value 40");

    std::memset(values, 0, sizeof(values));
    values[13] = 4;
    values[15] = 2;
    Expect(fc::formatCode(values, 24, true), "000000-000000-4-040200-000000", "apx 24 second block");

    // Parse round trips and check-digit reporting.
    fc::Decoded decoded = fc::parseCode("123456-789ABC-0", false);
    Expect(decoded.ok ? "ok" : decoded.error, "ok", "legacy parse validity");
    Expect(decoded.formatted, "123456-789ABC-9", "legacy parse reformat");
    Expect(std::to_string(decoded.statedCheck), "0", "legacy stated check");
    Expect(std::to_string(decoded.expectedCheck), "9", "legacy expected check");

    decoded = fc::parseCode("100000-000000-8-000000-000000", true);
    Expect(decoded.ok ? "ok" : decoded.error, "ok", "apx 24 parse validity");
    Expect(decoded.formatted, "100000-000000-8-000000-000000", "apx 24 parse round trip");

    decoded = fc::parseCode("000000-000000-8-000000-000000", true);
    Expect(decoded.ok ? "ok" : decoded.error, "ok", "apx 24 mismatch parse validity");
    Expect(decoded.formatted, "000000-000000-0-000000-000000", "apx 24 recomputed check");

    decoded = fc::parseCode("g00000-000000-2", true);
    Expect(decoded.ok ? "ok" : decoded.error, "ok", "apx base64 parse");
    Expect(std::to_string(decoded.values[0]), "40", "apx base64 value of g");

    decoded = fc::parseCode("G00000-000000-5", false);
    Expect(std::to_string(decoded.ok), "0", "legacy rejects G");

    decoded = fc::parseCode("G00000-000000-5", true);
    Expect(decoded.ok ? "ok" : decoded.error, "ok", "apx accepts G");

    // Platform database sanity.
    std::printf("platforms: %d\n", g_platformCount);
    if (g_platformCount != 37) {
        std::printf("FAIL platform count\n");
        ++g_failures;
    }
    for (int i = 0; i < g_platformCount; ++i) {
        if (g_platforms[i].optionCount <= 0 || g_platforms[i].options == nullptr) {
            std::printf("FAIL platform %s has no options\n", g_platforms[i].id);
            ++g_failures;
        }
    }

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures,
                g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}

