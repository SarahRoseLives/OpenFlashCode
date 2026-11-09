// OpenFlashCode - Motorola FLASHcode encoder / decoder
//
// A native Win32 GUI clone of the akardam.net Flashcode Encoder and Decoder.
// It shares no code or data files with the original; the option database was
// reconstructed from the published encoder pages and all encoding rules are
// implemented locally in flashcode.cpp.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "flashcode.h"
#include "flashdata.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

constexpr wchar_t kAppTitle[] = L"OpenFlashCode - Motorola FLASHcode Encoder / Decoder";

enum : int {
    ID_TAB = 1000,

    ID_ENC_PLATFORM = 1001,
    ID_ENC_RB12 = 1002,
    ID_ENC_RB24 = 1003,
    ID_ENC_LIST = 1010,
    ID_ENC_ENCODE = 1020,
    ID_ENC_CLEAR = 1021,
    ID_ENC_COPY = 1022,
    ID_ENC_FLASHCODE = 1030,
    ID_ENC_LOADFLASH = 1031,
    ID_ENC_OPTIONS = 1032,
    ID_ENC_LOADOPTS = 1033,
    ID_ENC_RESULT = 1040,

    ID_DEC_CODE_BASE = 1100,
    ID_DEC_PLAT_BASE = 1110,
    ID_DEC_DECODE = 1120,
    ID_DEC_CLEAR = 1121,
    ID_DEC_REPORT = 1122,
};

HINSTANCE g_instance = nullptr;
HWND g_main = nullptr;
HWND g_tab = nullptr;
HFONT g_font = nullptr;

HWND g_encPlatformLabel = nullptr;
HWND g_encFormatLabel = nullptr;
HWND g_encPlatform = nullptr;
HWND g_encRb12 = nullptr;
HWND g_encRb24 = nullptr;
HWND g_encList = nullptr;
HWND g_encEncode = nullptr;
HWND g_encClear = nullptr;
HWND g_encCopy = nullptr;
HWND g_encPreloadGroup = nullptr;
HWND g_encFlashLabel = nullptr;
HWND g_encFlashcode = nullptr;
HWND g_encLoadFlash = nullptr;
HWND g_encOptionsLabel = nullptr;
HWND g_encOptions = nullptr;
HWND g_encLoadOpts = nullptr;
HWND g_encResultLabel = nullptr;
HWND g_encResult = nullptr;

constexpr int kDecoderCount = 1;

HWND g_decCodeLabel[kDecoderCount] = {};
HWND g_decCode[kDecoderCount] = {};
HWND g_decPlat[kDecoderCount] = {};
HWND g_decDecode = nullptr;
HWND g_decClear = nullptr;
HWND g_decReportLabel = nullptr;
HWND g_decReport = nullptr;

std::vector<HWND> g_encControls;
std::vector<HWND> g_decControls;

int g_encPlatformIndex = 0;

RECT g_content = {};

std::wstring Widen(const std::string& text) {
    if (text.empty()) {
        return std::wstring();
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), wide.data(), length);
    return wide;
}

std::string Narrow(const std::wstring& text) {
    if (text.empty()) {
        return std::string();
    }
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string narrow(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), narrow.data(), length, nullptr, nullptr);
    return narrow;
}

std::wstring GetText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    text.resize(static_cast<size_t>(length));
    return text;
}

std::string Trim(const std::string& text) {
    const char* whitespace = " \t\r\n";
    const size_t first = text.find_first_not_of(whitespace);
    if (first == std::string::npos) {
        return std::string();
    }
    const size_t last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1);
}

std::string ToLowerAscii(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return text;
}

HWND MakeControl(const wchar_t* className, const wchar_t* text, DWORD style, DWORD exStyle, int id, HWND parent) {
    HWND control = CreateWindowExW(exStyle, className, text, style, 0, 0, 10, 10,
                                   parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_instance, nullptr);
    if (control != nullptr && g_font != nullptr) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    }
    return control;
}

HWND MakeEnc(const wchar_t* className, const wchar_t* text, DWORD style, DWORD exStyle, int id) {
    HWND control = MakeControl(className, text, style, exStyle, id, g_main);
    if (control != nullptr) {
        g_encControls.push_back(control);
    }
    return control;
}

HWND MakeDec(const wchar_t* className, const wchar_t* text, DWORD style, DWORD exStyle, int id) {
    HWND control = MakeControl(className, text, style, exStyle, id, g_main);
    if (control != nullptr) {
        g_decControls.push_back(control);
    }
    return control;
}

void FillPlatformCombo(HWND combo) {
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < g_platformCount; ++i) {
        const std::wstring name = Widen(g_platforms[i].name);
        const int index = static_cast<int>(SendMessageW(combo, CB_ADDSTRING, 0,
                                                        reinterpret_cast<LPARAM>(name.c_str())));
        SendMessageW(combo, CB_SETITEMDATA, static_cast<WPARAM>(index), static_cast<LPARAM>(i));
    }
    SendMessageW(combo, CB_SETCURSEL, 0, 0);
}

int ComboPlatform(HWND combo) {
    const int selection = static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
    if (selection < 0) {
        return 0;
    }
    const LRESULT data = SendMessageW(combo, CB_GETITEMDATA, static_cast<WPARAM>(selection), 0);
    if (data == CB_ERR || data < 0 || data >= g_platformCount) {
        return 0;
    }
    return static_cast<int>(data);
}

void SetListViewColumns(HWND list) {
    LVCOLUMNW column = {};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    column.pszText = const_cast<wchar_t*>(L"Digit");
    column.cx = 52;
    column.iSubItem = 0;
    ListView_InsertColumn(list, 0, &column);

    column.pszText = const_cast<wchar_t*>(L"Bit");
    column.cx = 40;
    column.iSubItem = 1;
    ListView_InsertColumn(list, 1, &column);

    column.pszText = const_cast<wchar_t*>(L"Option");
    column.cx = 82;
    column.iSubItem = 2;
    ListView_InsertColumn(list, 2, &column);

    column.pszText = const_cast<wchar_t*>(L"Description");
    column.cx = 520;
    column.iSubItem = 3;
    ListView_InsertColumn(list, 3, &column);
}

void RebuildEncoderList() {
    ListView_DeleteAllItems(g_encList);
    const FcPlatform& platform = g_platforms[g_encPlatformIndex];
    for (int i = 0; i < platform.optionCount; ++i) {
        const FcOption& option = platform.options[i];

        wchar_t digit[16];
        wchar_t bit[16];
        swprintf_s(digit, L"D%d", option.digit + 1);
        swprintf_s(bit, L"B%d", option.bit + 1);

        LVITEMW item = {};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = i;
        item.iSubItem = 0;
        item.pszText = digit;
        item.lParam = i;
        const int row = ListView_InsertItem(g_encList, &item);

        std::wstring code = Widen(option.code);
        std::wstring desc = Widen(option.desc);
        ListView_SetItemText(g_encList, row, 1, bit);
        ListView_SetItemText(g_encList, row, 2, code.data());
        ListView_SetItemText(g_encList, row, 3, desc.data());
        ListView_SetCheckState(g_encList, row, FALSE);
    }

    const bool supports24 = platform.supports24;
    EnableWindow(g_encRb24, supports24 ? TRUE : FALSE);
    if (!supports24) {
        SendMessageW(g_encRb12, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(g_encRb24, BM_SETCHECK, BST_UNCHECKED, 0);
    }
    SetWindowTextW(g_encResult, L"");
}

const FcOption* FindOption(const FcPlatform& platform, int digit, int bit) {
    for (int i = 0; i < platform.optionCount; ++i) {
        if (platform.options[i].digit == digit && platform.options[i].bit == bit) {
            return &platform.options[i];
        }
    }
    return nullptr;
}

void ShowTab(int index) {
    for (HWND control : g_encControls) {
        ShowWindow(control, index == 0 ? SW_SHOW : SW_HIDE);
    }
    for (HWND control : g_decControls) {
        ShowWindow(control, index == 1 ? SW_SHOW : SW_HIDE);
    }
}

void LayoutControls();

void Position(HWND control, int x, int y, int width, int height) {
    MoveWindow(control, g_content.left + x, g_content.top + y, width, height, TRUE);
}

void LayoutControls() {
    if (g_main == nullptr || g_tab == nullptr) {
        return;
    }

    RECT client = {};
    GetClientRect(g_main, &client);
    const int mainWidth = client.right - client.left;
    const int mainHeight = client.bottom - client.top;

    MoveWindow(g_tab, 8, 8, mainWidth - 16, mainHeight - 16, TRUE);

    RECT tabClient = {0, 0, mainWidth - 16, mainHeight - 16};
    GetClientRect(g_tab, &tabClient);
    RECT content = tabClient;
    TabCtrl_AdjustRect(g_tab, FALSE, &content);

    POINT origin = {0, 0};
    ClientToScreen(g_tab, &origin);
    ScreenToClient(g_main, &origin);
    OffsetRect(&content, origin.x, origin.y);
    g_content = content;

    const int width = content.right - content.left;
    const int height = content.bottom - content.top;

    // ---- Encoder -------------------------------------------------------
    Position(g_encPlatformLabel, 0, 6, 70, 20);
    Position(g_encPlatform, 74, 2, 260, 240);
    Position(g_encFormatLabel, 350, 6, 52, 20);
    Position(g_encRb12, 404, 2, 110, 24);
    Position(g_encRb24, 518, 2, 130, 24);

    const int sidebarWidth = 268;
    const int sidebarX = width - sidebarWidth;
    const int listWidth = sidebarX - 20;

    const int resultHeight = 84;
    const int resultY = height - resultHeight;
    const int listTop = 40;
    const int listBottom = resultY - 24;
    int listHeight = listBottom - listTop;
    if (listHeight < 120) {
        listHeight = 120;
    }

    Position(g_encList, 0, listTop, listWidth, listHeight);

    Position(g_encPreloadGroup, sidebarX, 40, sidebarWidth, 250);
    Position(g_encFlashLabel, sidebarX + 12, 60, sidebarWidth - 24, 18);
    Position(g_encFlashcode, sidebarX + 12, 78, sidebarWidth - 24, 24);
    Position(g_encLoadFlash, sidebarX + 12, 108, 140, 26);
    Position(g_encOptionsLabel, sidebarX + 12, 142, sidebarWidth - 24, 18);
    Position(g_encOptions, sidebarX + 12, 160, sidebarWidth - 24, 58);
    Position(g_encLoadOpts, sidebarX + 12, 224, 140, 26);

    Position(g_encEncode, sidebarX, 300, 128, 32);
    Position(g_encClear, sidebarX + 140, 300, 128, 32);
    Position(g_encCopy, sidebarX, 340, sidebarWidth, 28);

    Position(g_encResultLabel, 0, resultY - 20, 200, 18);
    Position(g_encResult, 0, resultY, width, resultHeight - 6);

    // ---- Decoder -------------------------------------------------------
    for (int row = 0; row < kDecoderCount; ++row) {
        const int y = 2 + row * 34;
        Position(g_decCodeLabel[row], 0, y + 4, 68, 20);
        Position(g_decCode[row], 72, y, 364, 26);
        Position(g_decPlat[row], 444, y, 310, 240);
    }
    Position(g_decDecode, 0, 44, 128, 30);
    Position(g_decClear, 140, 44, 100, 30);

    const int reportY = 100;
    Position(g_decReportLabel, 0, reportY - 22, 200, 18);
    int reportHeight = height - reportY - 4;
    if (reportHeight < 100) {
        reportHeight = 100;
    }
    Position(g_decReport, 0, reportY, width, reportHeight);

    InvalidateRect(g_main, nullptr, TRUE);
}

void DoEncode() {
    const FcPlatform& platform = g_platforms[g_encPlatformIndex];

    int values[fc::kMaxWidth] = {};
    const int itemCount = ListView_GetItemCount(g_encList);
    for (int i = 0; i < itemCount; ++i) {
        if (ListView_GetCheckState(g_encList, i) == FALSE) {
            continue;
        }
        int optionIndex = i;
        if (optionIndex < 0 || optionIndex >= platform.optionCount) {
            continue;
        }
        const FcOption& option = platform.options[optionIndex];
        if (option.digit < fc::kMaxWidth && option.bit < fc::kBitsPerChar) {
            values[option.digit] |= (1 << option.bit);
        }
    }

    std::string result;
    if (platform.supports24) {
        result = "12-digit: " + fc::formatCode(values, fc::kLegacyWidth, true) + "\r\n";
        result += "24-digit: " + fc::formatCode(values, fc::kFullWidth, true);
    } else {
        result = fc::formatCode(values, fc::kLegacyWidth, false);
    }
    SetWindowTextW(g_encResult, Widen(result).c_str());
}

void DoClearEncoder() {
    const int itemCount = ListView_GetItemCount(g_encList);
    for (int i = 0; i < itemCount; ++i) {
        ListView_SetCheckState(g_encList, i, FALSE);
    }
    SetWindowTextW(g_encResult, L"");
}

void DoCopyResult() {
    std::wstring text = GetText(g_encResult);
    if (text.empty()) {
        return;
    }

    const FcPlatform& platform = g_platforms[g_encPlatformIndex];
    if (platform.supports24) {
        const bool use24 = SendMessageW(g_encRb24, BM_GETCHECK, 0, 0) == BST_CHECKED;
        const std::wstring prefix = use24 ? L"24-digit: " : L"12-digit: ";
        const size_t at = text.find(prefix);
        if (at != std::wstring::npos) {
            const size_t start = at + prefix.size();
            size_t end = text.find(L'\n', start);
            if (end == std::wstring::npos) {
                end = text.size();
            }
            text = text.substr(start, end - start);
            while (!text.empty() && (text.back() == L'\r' || text.back() == L' ')) {
                text.pop_back();
            }
        }
    } else {
        while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n')) {
            text.pop_back();
        }
    }

    if (OpenClipboard(g_main)) {
        EmptyClipboard();
        const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (memory != nullptr) {
            void* target = GlobalLock(memory);
            if (target != nullptr) {
                memcpy(target, text.c_str(), bytes);
                GlobalUnlock(memory);
                SetClipboardData(CF_UNICODETEXT, memory);
            }
        }
        CloseClipboard();
    }
}

void DoLoadFlashcode() {
    const FcPlatform& platform = g_platforms[g_encPlatformIndex];
    const std::string text = Narrow(GetText(g_encFlashcode));
    const fc::Decoded decoded = fc::parseCode(text, platform.supports24);
    if (!decoded.ok) {
        MessageBoxW(g_main, Widen(decoded.error).c_str(), L"Load Flashcode", MB_ICONWARNING | MB_OK);
        return;
    }

    if (decoded.width == fc::kFullWidth && platform.supports24) {
        SendMessageW(g_encRb24, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(g_encRb12, BM_SETCHECK, BST_UNCHECKED, 0);
    } else {
        SendMessageW(g_encRb12, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(g_encRb24, BM_SETCHECK, BST_UNCHECKED, 0);
    }

    const int itemCount = ListView_GetItemCount(g_encList);
    for (int i = 0; i < itemCount; ++i) {
        const FcOption& option = platform.options[i];
        const bool set = option.digit < decoded.width && option.bit < fc::kBitsPerChar &&
                         ((decoded.values[option.digit] >> option.bit) & 1) != 0;
        ListView_SetCheckState(g_encList, i, set ? TRUE : FALSE);
    }

    std::string result = decoded.formatted;
    if (decoded.hasCheck) {
        result += decoded.statedCheck == decoded.expectedCheck ? "  (check digit valid)"
                                                               : "  (check digit mismatch)";
    }
    SetWindowTextW(g_encResult, Widen(result).c_str());
}

void DoLoadOptions() {
    const FcPlatform& platform = g_platforms[g_encPlatformIndex];
    const std::wstring wide = GetText(g_encOptions);
    const std::string all = Narrow(wide);

    int matched = 0;
    std::string unknown;
    size_t start = 0;
    while (start <= all.size()) {
        size_t end = all.find(L'\n', start);
        if (end == std::string::npos) {
            end = all.size();
        }
        const std::string line = Trim(all.substr(start, end - start));
        start = end + 1;

        if (line.empty()) {
            continue;
        }

        const std::string want = ToLowerAscii(line);
        const std::string firstWord = ToLowerAscii(Trim(line.substr(0, line.find(' '))));

        bool found = false;
        for (int i = 0; i < platform.optionCount; ++i) {
            const std::string code = ToLowerAscii(platform.options[i].code);
            if (code == want || code == firstWord) {
                ListView_SetCheckState(g_encList, i, TRUE);
                found = true;
            }
        }

        if (found) {
            ++matched;
        } else {
            unknown += line + "\r\n";
        }
    }

    std::string message = "Loaded " + std::to_string(matched) + " option(s).";
    if (!unknown.empty()) {
        message += "\r\n\r\nNot found:\r\n" + unknown;
    }
    MessageBoxW(g_main, Widen(message).c_str(), L"Load Options", MB_ICONINFORMATION | MB_OK);
}

void DoDecode() {
    std::string report;
    bool any = false;

    for (int row = 0; row < kDecoderCount; ++row) {
        const std::string code = Trim(Narrow(GetText(g_decCode[row])));
        if (code.empty()) {
            continue;
        }
        any = true;

        const int platformIndex = ComboPlatform(g_decPlat[row]);
        const FcPlatform& platform = g_platforms[platformIndex];
        const fc::Decoded decoded = fc::parseCode(code, platform.supports24);

        report += std::string("=== Flashcode (") + platform.name + ") ===\r\n";
        if (!decoded.ok) {
            report += "Error: " + decoded.error + "\r\n\r\n";
            continue;
        }

        report += std::string("Format: ") + (decoded.width == fc::kFullWidth ? "24-digit" : "12-digit") + "\r\n";
        report += "Code: " + decoded.formatted + "\r\n";
        if (decoded.hasCheck) {
            report += "Check digit: stated " + std::to_string(decoded.statedCheck) +
                      ", expected " + std::to_string(decoded.expectedCheck);
            report += decoded.statedCheck == decoded.expectedCheck ? " (valid)\r\n" : " (MISMATCH)\r\n";
        } else {
            report += "Check digit: not supplied, computed " + std::to_string(decoded.expectedCheck) + "\r\n";
        }

        int bits = 0;
        for (int digit = 0; digit < decoded.width; ++digit) {
            for (int bit = 0; bit < fc::kBitsPerChar; ++bit) {
                if (((decoded.values[digit] >> bit) & 1) == 0) {
                    continue;
                }
                ++bits;

                char label[32];
                sprintf_s(label, "D%d B%d", digit + 1, bit + 1);
                const FcOption* option = FindOption(platform, digit, bit);
                if (option != nullptr) {
                    report += std::string("  ") + label + "  " + option->code + "  " + option->desc + "\r\n";
                } else {
                    report += std::string("  ") + label + "  Invalid (undefined for this platform)\r\n";
                }
            }
        }
        if (bits == 0) {
            report += "  (no options set)\r\n";
        }
        report += "\r\n";
    }

    if (!any) {
        report = "Enter a flashcode above, choose a platform, then press Decode.";
    }
    SetWindowTextW(g_decReport, Widen(report).c_str());
}

void ClearDecoder() {
    for (int row = 0; row < kDecoderCount; ++row) {
        SetWindowTextW(g_decCode[row], L"");
    }
    SetWindowTextW(g_decReport, L"");
}

void CreateFonts() {
    NONCLIENTMETRICSW metrics = {};
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
        g_font = CreateFontIndirectW(&metrics.lfMessageFont);
    }
    if (g_font == nullptr) {
        g_font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }
}

void CreateTabs() {
    g_tab = MakeControl(WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
                        0, ID_TAB, g_main);

    TCITEMW item = {};
    item.mask = TCIF_TEXT;
    item.pszText = const_cast<wchar_t*>(L"Encoder");
    TabCtrl_InsertItem(g_tab, 0, &item);
    item.pszText = const_cast<wchar_t*>(L"Decoder");
    TabCtrl_InsertItem(g_tab, 1, &item);
    TabCtrl_SetCurSel(g_tab, 0);
}

void CreateEncoderControls() {
    g_encPlatformLabel = MakeEnc(WC_STATICW, L"Platform:", WS_CHILD | SS_LEFT, 0, -1);
    g_encPlatform = MakeEnc(WC_COMBOBOXW, L"", WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                            0, ID_ENC_PLATFORM);

    g_encFormatLabel = MakeEnc(WC_STATICW, L"Format:", WS_CHILD | SS_LEFT, 0, -1);
    g_encRb12 = MakeEnc(WC_BUTTONW, L"12-digit", WS_CHILD | WS_TABSTOP | BS_AUTORADIOBUTTON | WS_GROUP,
                        0, ID_ENC_RB12);
    g_encRb24 = MakeEnc(WC_BUTTONW, L"24-digit (APX)", WS_CHILD | WS_TABSTOP | BS_AUTORADIOBUTTON,
                        0, ID_ENC_RB24);

    g_encPreloadGroup = MakeEnc(WC_BUTTONW, L"Preload", WS_CHILD | BS_GROUPBOX, 0, -1);
    g_encFlashLabel = MakeEnc(WC_STATICW, L"Flashcode:", WS_CHILD | SS_LEFT, 0, -1);
    g_encFlashcode = MakeEnc(WC_EDITW, L"", WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                             0, ID_ENC_FLASHCODE);
    g_encLoadFlash = MakeEnc(WC_BUTTONW, L"Load Flashcode", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
                             0, ID_ENC_LOADFLASH);

    g_encOptionsLabel = MakeEnc(WC_STATICW, L"Options (one per line):", WS_CHILD | SS_LEFT, 0, -1);
    g_encOptions = MakeEnc(WC_EDITW, L"", WS_CHILD | WS_TABSTOP | WS_BORDER |
                           ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
                           0, ID_ENC_OPTIONS);
    g_encLoadOpts = MakeEnc(WC_BUTTONW, L"Load Options", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
                            0, ID_ENC_LOADOPTS);

    g_encList = MakeEnc(WC_LISTVIEWW, L"", WS_CHILD | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL |
                        LVS_SHOWSELALWAYS | WS_BORDER | WS_CLIPSIBLINGS,
                        WS_EX_CLIENTEDGE, ID_ENC_LIST);

    g_encEncode = MakeEnc(WC_BUTTONW, L"Encode", WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON,
                          0, ID_ENC_ENCODE);
    g_encClear = MakeEnc(WC_BUTTONW, L"Clear", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
                         0, ID_ENC_CLEAR);
    g_encCopy = MakeEnc(WC_BUTTONW, L"Copy result", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
                        0, ID_ENC_COPY);

    g_encResultLabel = MakeEnc(WC_STATICW, L"Result:", WS_CHILD | SS_LEFT, 0, -1);
    g_encResult = MakeEnc(WC_EDITW, L"", WS_CHILD | WS_BORDER | ES_MULTILINE | ES_READONLY |
                          ES_AUTOVSCROLL | WS_VSCROLL, 0, ID_ENC_RESULT);

    ListView_SetExtendedListViewStyle(g_encList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES);
    SetListViewColumns(g_encList);
    FillPlatformCombo(g_encPlatform);
}

void CreateDecoderControls() {
    for (int row = 0; row < kDecoderCount; ++row) {
        g_decCodeLabel[row] = MakeDec(WC_STATICW, L"Flashcode:", WS_CHILD | SS_LEFT, 0, -1);

        const int codeId = ID_DEC_CODE_BASE + row;
        const int platId = ID_DEC_PLAT_BASE + row;

        g_decCode[row] = MakeDec(WC_EDITW, L"", WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                                 0, codeId);
        g_decPlat[row] = MakeDec(WC_COMBOBOXW, L"", WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                                 0, platId);
    }

    g_decDecode = MakeDec(WC_BUTTONW, L"Decode", WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON,
                          0, ID_DEC_DECODE);
    g_decClear = MakeDec(WC_BUTTONW, L"Clear", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
                         0, ID_DEC_CLEAR);

    g_decReportLabel = MakeDec(WC_STATICW, L"Report:", WS_CHILD | SS_LEFT, 0, -1);
    g_decReport = MakeDec(WC_EDITW, L"", WS_CHILD | WS_BORDER | ES_MULTILINE | ES_READONLY |
                          ES_AUTOVSCROLL | WS_VSCROLL, 0, ID_DEC_REPORT);
}

void OnCommand(int id, int notification, HWND control) {
    switch (id) {
    case ID_ENC_PLATFORM:
        if (notification == CBN_SELCHANGE) {
            g_encPlatformIndex = ComboPlatform(g_encPlatform);
            RebuildEncoderList();
        }
        break;
    case ID_ENC_ENCODE:
        DoEncode();
        break;
    case ID_ENC_CLEAR:
        DoClearEncoder();
        break;
    case ID_ENC_COPY:
        DoCopyResult();
        break;
    case ID_ENC_LOADFLASH:
        DoLoadFlashcode();
        break;
    case ID_ENC_LOADOPTS:
        DoLoadOptions();
        break;
    case ID_DEC_DECODE:
        DoDecode();
        break;
    case ID_DEC_CLEAR:
        ClearDecoder();
        break;
    default:
        if (id >= ID_DEC_CODE_BASE && id < ID_DEC_CODE_BASE + 4 && notification == EN_SETFOCUS) {
            // Nothing to do; kept for completeness.
        }
        break;
    }
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    SetProcessDPIAware();

    g_instance = instance;

    INITCOMMONCONTROLSEX common = {};
    common.dwSize = sizeof(common);
    common.dwICC = ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&common);

    CreateFonts();

    const wchar_t* className = L"OpenFlashCodeWindow";
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = className;
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
    RegisterClassExW(&windowClass);

    g_main = CreateWindowExW(0, className, kAppTitle,
                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             CW_USEDEFAULT, CW_USEDEFAULT, 1040, 760,
                             nullptr, nullptr, instance, nullptr);
    if (g_main == nullptr) {
        return 0;
    }

    ShowWindow(g_main, showCommand);
    UpdateWindow(g_main);

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

namespace {

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        g_main = window;
        CreateTabs();
        CreateEncoderControls();
        CreateDecoderControls();

        // Keep the tab control behind every page control.  It shares the same
        // parent as them and would otherwise paint its body over the list view.
        SetWindowPos(g_tab, HWND_BOTTOM, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

        g_encPlatformIndex = 0;
        for (int row = 0; row < kDecoderCount; ++row) {
            FillPlatformCombo(g_decPlat[row]);
        }
        RebuildEncoderList();

        SendMessageW(g_encRb12, BM_SETCHECK, BST_CHECKED, 0);

        LayoutControls();
        ShowTab(0);
        return 0;
    }

    case WM_SIZE:
        LayoutControls();
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = 900;
        info->ptMinTrackSize.y = 640;
        return 0;
    }

    case WM_NOTIFY: {
        LPNMHDR header = reinterpret_cast<LPNMHDR>(lParam);
        if (header != nullptr && header->idFrom == ID_TAB && header->code == TCN_SELCHANGE) {
            ShowTab(TabCtrl_GetCurSel(g_tab));
            InvalidateRect(window, nullptr, TRUE);
            return 0;
        }
        break;
    }

    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        const int notification = HIWORD(wParam);
        HWND control = reinterpret_cast<HWND>(lParam);
        OnCommand(id, notification, control);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace


