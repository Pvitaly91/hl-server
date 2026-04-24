#include "MainWindow.h"

#include <algorithm>
#include <commctrl.h>
#include <commdlg.h>
#include <cctype>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <windowsx.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>

#include "CfgExport.h"
#include "ConfigProject.h"
#include "JsonLite.h"
#include "Presets.h"

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

constexpr wchar_t kWindowClassName[] = L"HlConfigEditorCppWindow";
constexpr wchar_t kPageWindowClassName[] = L"HlConfigEditorCppPage";
constexpr wchar_t kWindowTitle[] = L"HL Weapon Config Editor (C++)";
constexpr int kWindowWidth = 1160;
constexpr int kWindowHeight = 860;
constexpr int kRconTimeoutMilliseconds = 1500;

double ParseConfigDouble(const std::wstring& value, double fallbackValue) {
    if (value.empty()) {
        return fallbackValue;
    }

    wchar_t* end = nullptr;
    const double parsed = std::wcstod(value.c_str(), &end);
    return end != value.c_str() ? parsed : fallbackValue;
}

enum ControlId : int {
    IDM_FILE_NEW = 100,
    IDM_FILE_OPEN,
    IDM_FILE_SAVE,
    IDM_FILE_SAVE_AS,
    IDM_FILE_EXIT,

    IDC_TAB = 200,

    IDC_PROJECT_NAME = 1000,
    IDC_PROJECT_AUTHOR,
    IDC_PROJECT_NOTES,
    IDC_WEAPON_UNDER_TEST,
    IDC_SESSION_TAG,
    IDC_DEBUG_WEAPON_LOG,
    IDC_DEBUG_WEAPON_LOG_REJECTIONS,

    IDC_GLOCK_PROFILE_NAME = 1100,
    IDC_GLOCK_TAP_FIRE,
    IDC_GLOCK_FIRST_SHOT_ACCURACY,
    IDC_GLOCK_SPREAD_RECOVERY,
    IDC_GLOCK_MOVE_SPREAD_SCALE,
    IDC_GLOCK_PRIMARY_BASE_SPREAD,
    IDC_GLOCK_PRIMARY_GROUND_MOVE_PENALTY,
    IDC_GLOCK_PRIMARY_AIR_MOVE_PENALTY,
    IDC_GLOCK_PRIMARY_DUCK_PENALTY_SCALE,
    IDC_GLOCK_PRIMARY_SHOT_GROWTH,
    IDC_GLOCK_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD,
    IDC_GLOCK_PRIMARY_MAX_SPREAD,
    IDC_GLOCK_PRIMARY_CADENCE_MODE,
    IDC_GLOCK_PRIMARY_CYCLE_TIME,
    IDC_GLOCK_PRIMARY_CLICK_PENALTY,
    IDC_GLOCK_PRIMARY_CLICK_PENALTY_SCALE,
    IDC_GLOCK_PRIMARY_CLICK_RESET_TIME,
    IDC_GLOCK_PRIMARY_HOLD_PENALTY_SCALE,
    IDC_GLOCK_PATTERN_MODE,
    IDC_GLOCK_PATTERN_SCALE_X,
    IDC_GLOCK_PATTERN_SCALE_Y,
    IDC_GLOCK_PATTERN_RESET_TIME,
    IDC_GLOCK_PATTERN_MAX_INDEX,
    IDC_GLOCK_PRIMARY_DAMAGE,
    IDC_GLOCK_PRIMARY_HEADSHOT_SCALE,
    IDC_GLOCK_PRIMARY_HEADSHOT_LETHAL,
    IDC_GLOCK_PRESET_DEFAULT,
    IDC_GLOCK_PRESET_CS_LIKE_SOFT,
    IDC_GLOCK_PRESET_CS_TIGHT,
    IDC_GLOCK_PRESET_HEADSHOT_TEST,

    IDC_MP5_PRIMARY_ENABLED = 1200,
    IDC_MP5_PROFILE_NAME,
    IDC_MP5_PRIMARY_BASE_SPREAD,
    IDC_MP5_PRIMARY_GROUND_MOVE_PENALTY,
    IDC_MP5_PRIMARY_AIR_MOVE_PENALTY,
    IDC_MP5_PRIMARY_DUCK_PENALTY_SCALE,
    IDC_MP5_PRIMARY_BURST_GROWTH,
    IDC_MP5_PRIMARY_BURST_MAX_ADDITIONAL_SPREAD,
    IDC_MP5_PRIMARY_SPREAD_RECOVERY,
    IDC_MP5_PRIMARY_BURST_RESET_TIME,
    IDC_MP5_PRIMARY_HOLD_PENALTY_SCALE,
    IDC_MP5_PRIMARY_FIRST_SHOT_ACCURACY,
    IDC_MP5_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD,
    IDC_MP5_PRIMARY_MAX_SPREAD,
    IDC_MP5_PATTERN_MODE,
    IDC_MP5_PATTERN_SCALE_X,
    IDC_MP5_PATTERN_SCALE_Y,
    IDC_MP5_PATTERN_RESET_TIME,
    IDC_MP5_PATTERN_MAX_INDEX,
    IDC_MP5_PRIMARY_DAMAGE,
    IDC_MP5_PRIMARY_HEADSHOT_SCALE,
    IDC_MP5_PRIMARY_HEADSHOT_LETHAL,
    IDC_MP5_LAB_LOADOUT,
    IDC_MP5_LAB_AMMO,
    IDC_MP5_LAB_AUTOSWITCH,
    IDC_MP5_PRESET_DEFAULT,
    IDC_MP5_PRESET_CS_BURST,
    IDC_MP5_PRESET_CS_MOBILE,
    IDC_MP5_PRESET_SPRAY_TEST,

    IDC_357_PRIMARY_ENABLED = 1250,
    IDC_357_PROFILE_NAME,
    IDC_357_PRIMARY_BASE_SPREAD,
    IDC_357_PRIMARY_GROUND_MOVE_PENALTY,
    IDC_357_PRIMARY_AIR_MOVE_PENALTY,
    IDC_357_PRIMARY_DUCK_PENALTY_SCALE,
    IDC_357_PRIMARY_FIRST_SHOT_ACCURACY,
    IDC_357_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD,
    IDC_357_PRIMARY_SPREAD_RECOVERY,
    IDC_357_PRIMARY_MAX_SPREAD,
    IDC_357_PRIMARY_CADENCE_MODE,
    IDC_357_PRIMARY_CYCLE_TIME,
    IDC_357_PRIMARY_CLICK_PENALTY,
    IDC_357_PRIMARY_CLICK_PENALTY_SCALE,
    IDC_357_PRIMARY_CLICK_RESET_TIME,
    IDC_357_PRIMARY_HOLD_PENALTY_SCALE,
    IDC_357_PATTERN_MODE,
    IDC_357_PATTERN_SCALE_X,
    IDC_357_PATTERN_SCALE_Y,
    IDC_357_PATTERN_RESET_TIME,
    IDC_357_PATTERN_MAX_INDEX,
    IDC_357_PRIMARY_DAMAGE,
    IDC_357_PRIMARY_HEADSHOT_SCALE,
    IDC_357_PRIMARY_HEADSHOT_LETHAL,
    IDC_357_LAB_LOADOUT,
    IDC_357_LAB_AMMO,
    IDC_357_LAB_AUTOSWITCH,
    IDC_357_PRESET_DEFAULT,
    IDC_357_PRESET_PRECISION_TEST,
    IDC_357_PRESET_HEADSHOT_TEST,
    IDC_357_PRESET_PATTERN_SOFT,
    IDC_357_PRESET_PATTERN_TIGHT,

    IDC_SHOTGUN_PRIMARY_ENABLED = 1280,
    IDC_SHOTGUN_PROFILE_NAME,
    IDC_SHOTGUN_PRIMARY_BASE_SPREAD,
    IDC_SHOTGUN_PRIMARY_GROUND_MOVE_PENALTY,
    IDC_SHOTGUN_PRIMARY_AIR_MOVE_PENALTY,
    IDC_SHOTGUN_PRIMARY_DUCK_PENALTY_SCALE,
    IDC_SHOTGUN_PRIMARY_FIRST_SHOT_ACCURACY,
    IDC_SHOTGUN_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD,
    IDC_SHOTGUN_PRIMARY_SPREAD_RECOVERY,
    IDC_SHOTGUN_PRIMARY_MAX_SPREAD,
    IDC_SHOTGUN_PRIMARY_SHOT_GROWTH,
    IDC_SHOTGUN_PATTERN_MODE,
    IDC_SHOTGUN_PATTERN_SCALE_X,
    IDC_SHOTGUN_PATTERN_SCALE_Y,
    IDC_SHOTGUN_PATTERN_RESET_TIME,
    IDC_SHOTGUN_PATTERN_MAX_INDEX,
    IDC_SHOTGUN_PRIMARY_PELLET_SPREAD_MODE,
    IDC_SHOTGUN_PRIMARY_DAMAGE_PER_PELLET,
    IDC_SHOTGUN_PRIMARY_PELLET_COUNT,
    IDC_SHOTGUN_PRIMARY_HEADSHOT_SCALE,
    IDC_SHOTGUN_PRIMARY_HEADSHOT_LETHAL,
    IDC_SHOTGUN_LAB_LOADOUT,
    IDC_SHOTGUN_LAB_AMMO,
    IDC_SHOTGUN_LAB_AUTOSWITCH,
    IDC_SHOTGUN_PRESET_DEFAULT,
    IDC_SHOTGUN_PRESET_CLOSE_QUICKKILL,
    IDC_SHOTGUN_PRESET_PRECISION_TEST,
    IDC_SHOTGUN_PRESET_PATTERN_SOFT,
    IDC_SHOTGUN_PRESET_PATTERN_TIGHT,

    IDC_DUMMY_ENABLED = 1320,
    IDC_DUMMY_TARGET_PROFILE_NAME,
    IDC_DUMMY_HEALTH,
    IDC_DUMMY_ARMOR,
    IDC_DUMMY_HEAD_PROTECTED,
    IDC_DUMMY_ARMOR_HEALTH_FRACTION,
    IDC_DUMMY_ARMOR_DRAIN_SCALE,
    IDC_DUMMY_AUTORESPAWN,
    IDC_DUMMY_RESPAWN_DELAY,
    IDC_DUMMY_SPAWN_DISTANCE,
    IDC_DUMMY_OFFSET_RIGHT,
    IDC_DUMMY_OFFSET_UP,
    IDC_DUMMY_FACE_PLAYER,
    IDC_DUMMY_MODEL,
    IDC_DUMMY_PRESET_UNARMORED,
    IDC_DUMMY_PRESET_VEST,
    IDC_DUMMY_PRESET_VEST_HEADPROTECTED,

    IDC_MATCH_SUMMARY = 1350,
    IDC_MATCH_PACK_NAME,
    IDC_MATCH_PACK_DESCRIPTION,
    IDC_MATCH_PACK_TAGS,

    IDC_MATCH_PRESET_DUEL_GLOCK = 1450,
    IDC_MATCH_PRESET_DUEL_357,
    IDC_MATCH_PRESET_TEAM_MP5,
    IDC_MATCH_PRESET_TEAM_SHOTGUN,
    IDC_MATCH_PRESET_ARMOR_TEST,
    IDC_MATCH_PRESET_BUY_TEST,
    IDC_ROUND_MODE,
    IDC_ROUND_FREEZE_TIME,
    IDC_ROUND_RESTART_DELAY,
    IDC_ROUND_START_HEALTH,
    IDC_ROUND_START_ARMOR,
    IDC_ROUND_NO_RESPAWN,
    IDC_ROUND_FRIENDLY_FIRE,
    IDC_ROUND_WEAPON_PROFILE,
    IDC_ROUND_LOADOUT_MODE,

    IDC_TEAM_ROUND_MODE = 1500,
    IDC_TEAM_ROUND_TEAMPLAY,
    IDC_TEAM_ROUND_SPAWN_MODE,
    IDC_TEAM_ROUND_TEAM1_NAME,
    IDC_TEAM_ROUND_TEAM2_NAME,
    IDC_TEAM_ROUND_TEAM1_LOADOUT,
    IDC_TEAM_ROUND_TEAM2_LOADOUT,
    IDC_TEAM_ROUND_TEAM1_HEALTH,
    IDC_TEAM_ROUND_TEAM2_HEALTH,
    IDC_TEAM_ROUND_TEAM1_ARMOR,
    IDC_TEAM_ROUND_TEAM2_ARMOR,

    IDC_BUY_MODE = 1550,
    IDC_BUY_FREEZE_ONLY,
    IDC_BUY_TEAM_SHARED_CATALOG,
    IDC_BUY_START_MONEY,
    IDC_BUY_ROUND_WIN_REWARD,
    IDC_BUY_ROUND_LOSS_REWARD,
    IDC_BUY_MAX_MONEY,
    IDC_BUY_ALLOW_GLOCK,
    IDC_BUY_ALLOW_MP5,
    IDC_BUY_ALLOW_357,
    IDC_BUY_ALLOW_SHOTGUN,
    IDC_BUY_ALLOW_ARMOR,
    IDC_BUY_ALLOW_HELMET,
    IDC_BUY_ALLOW_HANDGRENADE,
    IDC_BUY_COST_GLOCK,
    IDC_BUY_COST_MP5,
    IDC_BUY_COST_357,
    IDC_BUY_COST_SHOTGUN,
    IDC_BUY_COST_ARMOR,
    IDC_BUY_COST_HELMET,
    IDC_BUY_COST_HANDGRENADE,
    IDC_ARMOR_MODE,
    IDC_ARMOR_START_VALUE,
    IDC_ARMOR_MAX_VALUE,
    IDC_ARMOR_HEALTH_FRACTION,
    IDC_ARMOR_DRAIN_SCALE,
    IDC_HELMET_MODE,
    IDC_HELMET_START_ENABLED,
    IDC_HELMET_HEADSHOT_PROTECTION,

    IDC_BROWSER_PRESET_CATEGORY = 1600,
    IDC_BROWSER_PRESET_LIST,
    IDC_BROWSER_PRESET_OPEN_FOLDER,
    IDC_BROWSER_PRESET_LOAD,
    IDC_BROWSER_MATCH_PACK_LIST,
    IDC_BROWSER_MATCH_PACK_OPEN_FOLDER,
    IDC_BROWSER_MATCH_PACK_LOAD,
    IDC_BROWSER_REFRESH,
    IDC_BROWSER_DETAILS,
    IDC_BROWSER_DIFF,
    IDC_BROWSER_APPLY_COMMAND,
    IDC_BROWSER_COPY_APPLY_COMMAND,
    IDC_BROWSER_QUICK_EXPORT_COPY,
    IDC_BROWSER_OPEN_LIVE_MOD_FOLDER,
    IDC_BROWSER_STATUS,

    IDC_LIVE_SERVER_HOST = 1700,
    IDC_LIVE_SERVER_PORT,
    IDC_LIVE_SERVER_PASSWORD,
    IDC_LIVE_CFG_FILE,
    IDC_LIVE_MATCH_PACK,
    IDC_LIVE_SANDBOX_WEAPON,
    IDC_LIVE_SANDBOX_TARGET,
    IDC_LIVE_SANDBOX_SPOT,
    IDC_LIVE_CUSTOM_COMMAND,
    IDC_LIVE_STATUS,
    IDC_LIVE_TEST_CONNECTION,
    IDC_LIVE_APPLY_CFG,
    IDC_LIVE_APPLY_MATCH_PACK,
    IDC_LIVE_SANDBOX_RESET,
    IDC_LIVE_APPLY_CFG_SANDBOX_RESET,
    IDC_LIVE_APPLY_PACK_SANDBOX_RESET,
    IDC_LIVE_APPLY_SANDBOX_SETUP,
    IDC_LIVE_COPY_SANDBOX_SEQUENCE,
    IDC_LIVE_SEND_CUSTOM,
    IDC_LIVE_COPY_FALLBACK,

    IDC_TELEMETRY_LATEST_LOG = 1800,
    IDC_TELEMETRY_SELECTED_LOG,
    IDC_TELEMETRY_WEAPON_FILTER,
    IDC_TELEMETRY_ANALYSIS_TEXT,
    IDC_TELEMETRY_STATUS,
    IDC_TELEMETRY_FIND_LATEST,
    IDC_TELEMETRY_BROWSE_LOG,
    IDC_TELEMETRY_ANALYZE_LATEST,
    IDC_TELEMETRY_ANALYZE_SELECTED,
    IDC_TELEMETRY_OPEN_LOG_FOLDER,
    IDC_TELEMETRY_COPY_LOG_PATH,
    IDC_TELEMETRY_COPY_ANALYSIS,

    IDC_GUIDED_WEAPON = 1900,
    IDC_GUIDED_SOURCE_TYPE,
    IDC_GUIDED_SOURCE_VALUE,
    IDC_GUIDED_TARGET_PROFILE,
    IDC_GUIDED_TARGET_SPOT,
    IDC_GUIDED_SCENARIO,
    IDC_GUIDED_COMMAND_SEQUENCE,
    IDC_GUIDED_INSTRUCTIONS,
    IDC_GUIDED_STATUS,
    IDC_GUIDED_ANALYSIS,
    IDC_GUIDED_START_TEST,
    IDC_GUIDED_FINISH_ANALYZE,
    IDC_GUIDED_COPY_COMMANDS,
    IDC_GUIDED_SAVE_REPORT,
    IDC_GUIDED_REFRESH_PREVIEW,

    IDC_REPORT_LIST = 2000,
    IDC_REPORT_DETAILS,
    IDC_REPORT_COMPARISON,
    IDC_REPORT_STATUS,
    IDC_REPORT_REFRESH,
    IDC_REPORT_COMPARE,
    IDC_REPORT_OPEN_REPORT,
    IDC_REPORT_OPEN_LOG,
    IDC_REPORT_COPY_REPORT_PATH,
    IDC_REPORT_COPY_LOG_PATH,
    IDC_REPORT_COPY_COMPARISON,
    IDC_REPORT_EXPORT_COMPARISON,

    IDC_LIVE_MOD_FOLDER_PREVIEW = 1400,
    IDC_EXPORT_FOLDER,
    IDC_BROWSE_EXPORT_FOLDER,
    IDC_EXPORT_FILE_NAME,
    IDC_QUICK_EXPORT_LIVE_MOD,
    IDC_EXPORT_CFG,
    IDC_COPY_EXEC_COMMAND,
    IDC_COPY_LAUNCHER_COMMAND,
    IDC_COPY_RAW_CFG,
    IDC_OPEN_EXPORT_FOLDER,
    IDC_OPEN_LIVE_MOD_FOLDER,
    IDC_OPEN_LOGS_FOLDER,
    IDC_LAUNCHER_PREVIEW,
    IDC_EXEC_PREVIEW,
    IDC_CFG_PREVIEW,
    IDC_HALF_LIFE_ROOT_PREVIEW,
    IDC_QUICK_EXPORT_TARGET_PREVIEW,
    IDC_EDITOR_EXE_PATH_PREVIEW,
    IDC_EXPORT_STATUS,
};

constexpr int kPageCount = 15;
constexpr int kRoundPageIndex = 6;
constexpr int kTeamRoundPageIndex = 7;
constexpr int kBuyEquipmentPageIndex = 8;
constexpr int kBrowserPageIndex = 9;
constexpr int kLiveServerPageIndex = 10;
constexpr int kGuidedTestsPageIndex = 11;
constexpr int kReportsPageIndex = 12;
constexpr int kTelemetryPageIndex = 13;
constexpr int kExportPageIndex = 14;

HFONT GetUiFont() {
    return static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
}

void SetControlFont(HWND window) {
    SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(GetUiFont()), TRUE);
}

HWND CreateChildControl(
    HWND parent,
    const wchar_t* className,
    const wchar_t* text,
    DWORD style,
    DWORD exStyle,
    int x,
    int y,
    int width,
    int height,
    int id) {
    HWND control = CreateWindowExW(
        exStyle,
        className,
        text,
        style,
        x,
        y,
        width,
        height,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr),
        nullptr);

    SetControlFont(control);
    return control;
}

HWND CreateLabel(HWND parent, const wchar_t* text, int x, int y, int width, int height) {
    return CreateChildControl(parent, L"STATIC", text, WS_CHILD | WS_VISIBLE, 0, x, y, width, height, 0);
}

HWND CreateGroupBox(HWND parent, const wchar_t* text, int x, int y, int width, int height) {
    return CreateChildControl(parent, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, x, y, width, height, 0);
}

HWND CreateEdit(HWND parent, int id, int x, int y, int width, int height, DWORD extraStyle = 0) {
    return CreateChildControl(
        parent,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | extraStyle,
        WS_EX_CLIENTEDGE,
        x,
        y,
        width,
        height,
        id);
}

HWND CreateMultiLineEdit(HWND parent, int id, int x, int y, int width, int height, bool readOnly) {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL;
    if (readOnly) {
        style |= ES_READONLY;
    }

    return CreateChildControl(parent, L"EDIT", L"", style, WS_EX_CLIENTEDGE, x, y, width, height, id);
}

HWND CreateCheckBox(HWND parent, const wchar_t* text, int id, int x, int y, int width, int height) {
    return CreateChildControl(parent, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, x, y, width, height, id);
}

HWND CreateButton(HWND parent, const wchar_t* text, int id, int x, int y, int width, int height) {
    return CreateChildControl(parent, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, x, y, width, height, id);
}

HWND CreateCombo(HWND parent, int id, int x, int y, int width, int height) {
    return CreateChildControl(parent, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 0, x, y, width, height, id);
}

HWND CreateListBox(HWND parent, int id, int x, int y, int width, int height) {
    return CreateChildControl(
        parent,
        L"LISTBOX",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | LBS_NOTIFY | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
        WS_EX_CLIENTEDGE,
        x,
        y,
        width,
        height,
        id);
}

HWND CreateMultiSelectListBox(HWND parent, int id, int x, int y, int width, int height) {
    return CreateChildControl(
        parent,
        L"LISTBOX",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | LBS_NOTIFY | LBS_EXTENDEDSEL | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
        WS_EX_CLIENTEDGE,
        x,
        y,
        width,
        height,
        id);
}

std::wstring GetTextFromWindow(HWND window) {
    const int length = GetWindowTextLengthW(window);
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    if (length > 0) {
        GetWindowTextW(window, text.data(), length + 1);
    }
    text.resize(static_cast<std::size_t>(length));
    return text;
}

void SetTextOnWindow(HWND window, const std::wstring& value) {
    SetWindowTextW(window, value.c_str());
}

bool CopyTextToClipboard(HWND owner, const std::wstring& text) {
    if (!OpenClipboard(owner)) {
        return false;
    }

    EmptyClipboard();

    const std::size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL globalHandle = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (globalHandle == nullptr) {
        CloseClipboard();
        return false;
    }

    void* lockedMemory = GlobalLock(globalHandle);
    memcpy(lockedMemory, text.c_str(), bytes);
    GlobalUnlock(globalHandle);

    if (SetClipboardData(CF_UNICODETEXT, globalHandle) == nullptr) {
        GlobalFree(globalHandle);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}

std::wstring BrowseForFolder(HWND owner, const std::wstring& initialFolder) {
    IFileOpenDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    if (FAILED(hr) || dialog == nullptr) {
        return {};
    }

    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);

    if (!initialFolder.empty()) {
        IShellItem* folderItem = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(initialFolder.c_str(), nullptr, IID_PPV_ARGS(&folderItem))) && folderItem != nullptr) {
            dialog->SetFolder(folderItem);
            folderItem->Release();
        }
    }

    std::wstring selectedPath;
    if (SUCCEEDED(dialog->Show(owner))) {
        IShellItem* result = nullptr;
        if (SUCCEEDED(dialog->GetResult(&result)) && result != nullptr) {
            PWSTR pathBuffer = nullptr;
            if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &pathBuffer)) && pathBuffer != nullptr) {
                selectedPath = pathBuffer;
                CoTaskMemFree(pathBuffer);
            }
            result->Release();
        }
    }

    dialog->Release();
    return selectedPath;
}

std::wstring ShowOpenProjectDialog(HWND owner) {
    std::wstring buffer(32768, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = L"HL Config Project (*.hlcfg.json)\0*.hlcfg.json\0JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    dialog.lpstrDefExt = L"json";

    if (!GetOpenFileNameW(&dialog)) {
        return {};
    }

    return buffer.c_str();
}

std::wstring ShowSaveProjectDialog(HWND owner, const std::wstring& initialPath) {
    std::wstring buffer(32768, L'\0');
    const std::wstring candidate = initialPath.empty() ? L"untitled.hlcfg.json" : initialPath;
    wcsncpy_s(buffer.data(), buffer.size(), candidate.c_str(), _TRUNCATE);

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = L"HL Config Project (*.hlcfg.json)\0*.hlcfg.json\0JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    dialog.lpstrDefExt = L"json";

    if (!GetSaveFileNameW(&dialog)) {
        return {};
    }

    return buffer.c_str();
}

std::wstring ShowOpenLogDialog(HWND owner, const std::wstring& initialFolder) {
    std::wstring buffer(32768, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = L"Weapon Debug Logs (weapon-debug-*.log)\0weapon-debug-*.log\0Log Files (*.log)\0*.log\0All Files (*.*)\0*.*\0";
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    dialog.lpstrDefExt = L"log";

    std::wstring initial = initialFolder;
    if (!initial.empty()) {
        dialog.lpstrInitialDir = initial.c_str();
    }

    if (!GetOpenFileNameW(&dialog)) {
        return {};
    }

    return buffer.c_str();
}

std::wstring FileNameFromPath(const std::wstring& path) {
    return path.empty() ? std::wstring() : std::filesystem::path(path).filename().wstring();
}

bool IsAsciiAlphaNumeric(wchar_t value) {
    return (value >= L'0' && value <= L'9') ||
           (value >= L'A' && value <= L'Z') ||
           (value >= L'a' && value <= L'z');
}

std::wstring BuildSafeCfgStem(const std::wstring& value) {
    std::wstring stem;
    stem.reserve(value.size());

    bool previousWasSeparator = false;
    for (const wchar_t ch : value) {
        if (IsAsciiAlphaNumeric(ch)) {
            stem.push_back(static_cast<wchar_t>(std::towlower(ch)));
            previousWasSeparator = false;
            continue;
        }

        if (stem.empty() || previousWasSeparator) {
            continue;
        }

        stem.push_back(L'_');
        previousWasSeparator = true;
    }

    while (!stem.empty() && stem.back() == L'_') {
        stem.pop_back();
    }

    return stem;
}

std::wstring BuildSuggestedCfgFileName(const hlcfg::ProjectDocument& document) {
    const hlcfg::ProjectDocument defaults = hlcfg::CreateDefaultProject();
    const std::wstring weaponUnderTest = hlcfg::Trimmed(document.general.weaponUnderTest);

    std::wstring candidate = hlcfg::Trimmed(document.matchPack.name);
    if (candidate.empty()) {
        candidate = hlcfg::Trimmed(document.metadata.projectName);
    }
    if (candidate.empty() || candidate == defaults.metadata.projectName) {
        if (weaponUnderTest == L"glock") {
            const std::wstring profileName = hlcfg::Trimmed(document.glock.profileName);
            if (!profileName.empty() && profileName != defaults.glock.profileName) {
                candidate = profileName;
            }
        } else if (weaponUnderTest == L"mp5") {
            const std::wstring profileName = hlcfg::Trimmed(document.mp5.profileName);
            if (!profileName.empty() && profileName != defaults.mp5.profileName) {
                candidate = profileName;
            }
        } else if (weaponUnderTest == L"357") {
            const std::wstring profileName = hlcfg::Trimmed(document.weapon357.profileName);
            if (!profileName.empty() && profileName != defaults.weapon357.profileName) {
                candidate = profileName;
            }
        } else if (weaponUnderTest == L"shotgun") {
            const std::wstring profileName = hlcfg::Trimmed(document.shotgun.profileName);
            if (!profileName.empty() && profileName != defaults.shotgun.profileName) {
                candidate = profileName;
            }
        }
    }

    if (candidate.empty()) {
        candidate = hlcfg::Trimmed(document.general.sessionTag);
    }

    if (candidate.empty()) {
        candidate = weaponUnderTest.empty() ? L"weapon_config" : (weaponUnderTest + L"_config");
    }

    std::wstring safeStem = BuildSafeCfgStem(candidate);
    if (safeStem.empty()) {
        safeStem = L"weapon_config";
    }

    return hlcfg::EnsureCfgFileName(safeStem);
}

enum class BrowserEntryKind {
    None,
    WeaponPreset,
    MatchPack,
};

struct BrowserEntry {
    BrowserEntryKind kind = BrowserEntryKind::None;
    std::wstring category;
    std::wstring name;
    std::wstring description;
    std::wstring tags;
    std::wstring notes;
    std::wstring sourcePath;
    std::wstring referencedCfgPath;
    hlcfg::CvarMap cvars;
};

struct GuidedReportMetrics {
    int acceptedShots = -1;
    int hitEvents = -1;
    int killEvents = -1;
    int headshotHits = -1;
    int headshotKills = -1;
    int patternEvidence = -1;
    int cadenceEvidence = -1;
    int burstGrowthEvidence = -1;
    int armorEvidence = -1;
    std::wstring spreadSummary;
    std::wstring damageSummary;
    std::wstring consistencyWarnings;
    std::wstring analyzerSummaryText;
};

struct GuidedReportScorecard {
    bool hasScorecard = false;
    bool skipped = false;
    std::wstring scorecardPath;
    std::wstring scorecardTextPath;
    std::wstring skipReason;
    std::wstring telemetryFresh;
    int telemetryEventCount = -1;
    std::wstring telemetryReason;
    std::wstring analyzerReportPath;
    std::wstring analyzerStatus;
    std::wstring analyzerError;
    int overallFeel = -1;
    int singleShotAccuracy = -1;
    int spamSprayPenalty = -1;
    int movementPenaltyFeel = -1;
    int headshotFeel = -1;
    int clientVisualSync = -1;
    int shortBurstFeel = -1;
    int longSprayPunishment = -1;
    int pelletConsistencyFeel = -1;
    int closeRangeLethalityFeel = -1;
    std::wstring notes;
    std::wstring globalNotes;
    std::wstring warnings;
};

struct GuidedReportEntry {
    std::wstring timestamp;
    std::wstring weapon;
    std::wstring reportKind;
    std::wstring sourceType;
    std::wstring sourceValue;
    std::wstring targetProfile;
    std::wstring targetSpot;
    std::wstring scenario;
    std::wstring baselineLogPath;
    std::wstring analyzedLogPath;
    std::wstring reportPath;
    std::wstring jsonPath;
    bool hasJsonSidecar = false;
    bool parsedFromText = false;
    GuidedReportMetrics metrics;
    GuidedReportScorecard scorecard;
};

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }

    const int required = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) {
        return {};
    }

    std::wstring result(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), required);
    return result;
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int required = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }

    std::string result(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), required, nullptr, nullptr);
    return result;
}

std::string BuildGoldSrcConnectionlessPacket(const std::string& payload) {
    return std::string(4, static_cast<char>(0xFF)) + payload;
}

std::string StripGoldSrcConnectionlessHeader(const std::string& packet) {
    if (packet.size() >= 4 &&
        static_cast<unsigned char>(packet[0]) == 0xFF &&
        static_cast<unsigned char>(packet[1]) == 0xFF &&
        static_cast<unsigned char>(packet[2]) == 0xFF &&
        static_cast<unsigned char>(packet[3]) == 0xFF) {
        return packet.substr(4);
    }

    return packet;
}

std::string StripGoldSrcPrintPrefix(const std::string& payload) {
    // GoldSrc RCON command replies are usually S2A_PRINT packets with a leading 'l' marker.
    if (!payload.empty() && payload.front() == 'l') {
        return payload.substr(1);
    }
    return payload;
}

std::wstring TrimRconResponseText(const std::wstring& value) {
    std::wstring result = value;
    while (!result.empty() && (result.back() == L'\0' || result.back() == L'\n' || result.back() == L'\r')) {
        result.pop_back();
    }
    return result;
}

bool ParseUnsignedShortPort(const std::wstring& value, unsigned short& port) {
    const std::wstring trimmed = hlcfg::Trimmed(value);
    if (trimmed.empty()) {
        return false;
    }

    wchar_t* end = nullptr;
    const long parsed = std::wcstol(trimmed.c_str(), &end, 10);
    if (end == trimmed.c_str() || parsed <= 0 || parsed > 65535) {
        return false;
    }

    port = static_cast<unsigned short>(parsed);
    return true;
}

struct LiveRconResult {
    bool success = false;
    std::wstring response;
    std::wstring error;
};

class WsaSession {
public:
    WsaSession() {
        initialized_ = WSAStartup(MAKEWORD(2, 2), &data_) == 0;
    }

    ~WsaSession() {
        if (initialized_) {
            WSACleanup();
        }
    }

    bool IsInitialized() const {
        return initialized_;
    }

private:
    WSADATA data_{};
    bool initialized_ = false;
};

struct SocketHandleCloser {
    void operator()(SOCKET* socketHandle) const {
        if (socketHandle != nullptr) {
            if (*socketHandle != INVALID_SOCKET) {
                closesocket(*socketHandle);
            }
            delete socketHandle;
        }
    }
};

using SocketHandle = std::unique_ptr<SOCKET, SocketHandleCloser>;

bool ResolveRconAddress(const std::wstring& host, unsigned short port, sockaddr_in& address, std::wstring& errorMessage) {
    const std::string hostUtf8 = WideToUtf8(hlcfg::Trimmed(host));
    if (hostUtf8.empty()) {
        errorMessage = L"Server host is empty.";
        return false;
    }

    address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    const unsigned long parsedAddress = inet_addr(hostUtf8.c_str());
    if (parsedAddress != INADDR_NONE) {
        address.sin_addr.s_addr = parsedAddress;
        return true;
    }

    hostent* hostEntry = gethostbyname(hostUtf8.c_str());
    if (hostEntry == nullptr || hostEntry->h_addr_list == nullptr || hostEntry->h_addr_list[0] == nullptr) {
        errorMessage = L"Unable to resolve RCON host: " + host;
        return false;
    }

    memcpy(&address.sin_addr, hostEntry->h_addr_list[0], static_cast<std::size_t>(hostEntry->h_length));
    return true;
}

bool SendUdpPacket(SOCKET socketHandle, const sockaddr_in& address, const std::string& packet, std::wstring& errorMessage) {
    const int sent = sendto(
        socketHandle,
        packet.data(),
        static_cast<int>(packet.size()),
        0,
        reinterpret_cast<const sockaddr*>(&address),
        sizeof(address));
    if (sent == SOCKET_ERROR || sent != static_cast<int>(packet.size())) {
        errorMessage = L"UDP send failed with WinSock error " + std::to_wstring(WSAGetLastError()) + L".";
        return false;
    }

    return true;
}

bool ReceiveUdpPacket(SOCKET socketHandle, std::string& packet, std::wstring& errorMessage) {
    char buffer[8192]{};
    sockaddr_in fromAddress{};
    int fromLength = sizeof(fromAddress);
    const int received = recvfrom(socketHandle, buffer, static_cast<int>(sizeof(buffer)), 0, reinterpret_cast<sockaddr*>(&fromAddress), &fromLength);
    if (received == SOCKET_ERROR) {
        const int errorCode = WSAGetLastError();
        if (errorCode == WSAETIMEDOUT) {
            errorMessage = L"RCON response timed out.";
        } else {
            errorMessage = L"UDP receive failed with WinSock error " + std::to_wstring(errorCode) + L".";
        }
        return false;
    }

    packet.assign(buffer, buffer + received);
    return true;
}

bool ExtractRconChallenge(const std::string& packet, std::string& challenge, std::wstring& errorMessage) {
    const std::string text = StripGoldSrcConnectionlessHeader(packet);
    const std::string marker = "challenge rcon";
    const std::size_t markerPosition = text.find(marker);
    if (markerPosition == std::string::npos) {
        errorMessage = L"HLDS did not return an RCON challenge. Response was:\n" + Utf8ToWide(text);
        return false;
    }

    std::size_t start = markerPosition + marker.size();
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
        ++start;
    }

    std::size_t end = start;
    while (end < text.size() && !std::isspace(static_cast<unsigned char>(text[end]))) {
        ++end;
    }

    challenge = text.substr(start, end - start);
    if (challenge.empty()) {
        errorMessage = L"HLDS returned an empty RCON challenge.";
        return false;
    }

    return true;
}

bool SendGoldSrcRconCommands(
    const std::wstring& host,
    const std::wstring& portText,
    const std::wstring& password,
    const std::vector<std::wstring>& commands,
    LiveRconResult& result) {
    result = {};

    unsigned short port = 0;
    if (!ParseUnsignedShortPort(portText, port)) {
        result.error = L"Server port must be a number from 1 to 65535.";
        return false;
    }

    if (hlcfg::Trimmed(password).empty()) {
        result.error = L"RCON password is empty. Enter the server rcon_password or copy the fallback commands.";
        return false;
    }

    std::vector<std::wstring> filteredCommands;
    for (const std::wstring& command : commands) {
        const std::wstring trimmed = hlcfg::Trimmed(command);
        if (!trimmed.empty()) {
            filteredCommands.push_back(trimmed);
        }
    }

    if (filteredCommands.empty()) {
        result.error = L"No live command was provided.";
        return false;
    }

    WsaSession wsa;
    if (!wsa.IsInitialized()) {
        result.error = L"Unable to initialize WinSock.";
        return false;
    }

    sockaddr_in address{};
    if (!ResolveRconAddress(host, port, address, result.error)) {
        return false;
    }

    SocketHandle socketHandle(new SOCKET(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)));
    if (*socketHandle == INVALID_SOCKET) {
        result.error = L"Unable to create UDP socket. WinSock error " + std::to_wstring(WSAGetLastError()) + L".";
        return false;
    }

    int timeout = kRconTimeoutMilliseconds;
    setsockopt(*socketHandle, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(*socketHandle, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    if (!SendUdpPacket(*socketHandle, address, BuildGoldSrcConnectionlessPacket("challenge rcon\n"), result.error)) {
        return false;
    }

    std::string challengePacket;
    if (!ReceiveUdpPacket(*socketHandle, challengePacket, result.error)) {
        result.error += L"\nConfirm HLDS is running, the host/port are correct, and rcon_password is set.";
        return false;
    }

    std::string challenge;
    if (!ExtractRconChallenge(challengePacket, challenge, result.error)) {
        return false;
    }

    const std::string passwordUtf8 = WideToUtf8(password);
    if (passwordUtf8.find('"') != std::string::npos || passwordUtf8.find('\n') != std::string::npos || passwordUtf8.find('\r') != std::string::npos) {
        result.error = L"RCON password contains unsupported quote or newline characters.";
        return false;
    }

    std::wstring response;
    for (const std::wstring& command : filteredCommands) {
        const std::string commandUtf8 = WideToUtf8(command);
        if (commandUtf8.find('\n') != std::string::npos || commandUtf8.find('\r') != std::string::npos) {
            result.error = L"Live command contains an unsupported newline. Send one command per action.";
            return false;
        }

        response += L"> " + command + L"\r\n";
        const std::string packet = BuildGoldSrcConnectionlessPacket("rcon " + challenge + " \"" + passwordUtf8 + "\" " + commandUtf8 + "\n");
        if (!SendUdpPacket(*socketHandle, address, packet, result.error)) {
            return false;
        }

        std::string commandPacket;
        std::wstring receiveError;
        if (ReceiveUdpPacket(*socketHandle, commandPacket, receiveError)) {
            std::wstring payload = TrimRconResponseText(Utf8ToWide(StripGoldSrcPrintPrefix(StripGoldSrcConnectionlessHeader(commandPacket))));
            if (!payload.empty()) {
                response += payload + L"\r\n";
            }
        } else {
            response += L"(no response before timeout; command may still have been accepted)\r\n";
        }
    }

    if (response.find(L"Bad rcon_password") != std::wstring::npos ||
        response.find(L"Bad Password") != std::wstring::npos ||
        response.find(L"No password set") != std::wstring::npos) {
        result.error = L"HLDS rejected the RCON command. Check rcon_password.";
        result.response = response;
        return false;
    }

    result.success = true;
    result.response = response;
    return true;
}

std::wstring QuoteWindowsCommandLineArgument(const std::wstring& value) {
    if (value.empty()) {
        return L"\"\"";
    }

    bool needsQuotes = false;
    for (const wchar_t ch : value) {
        if (std::iswspace(ch) || ch == L'"') {
            needsQuotes = true;
            break;
        }
    }

    if (!needsQuotes) {
        return value;
    }

    std::wstring quoted = L"\"";
    std::size_t backslashes = 0;
    for (const wchar_t ch : value) {
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }

        if (ch == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted.push_back(ch);
            backslashes = 0;
            continue;
        }

        quoted.append(backslashes, L'\\');
        backslashes = 0;
        quoted.push_back(ch);
    }

    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
}

std::wstring QuotePowerShellLiteral(const std::wstring& value) {
    std::wstring quoted = L"'";
    for (const wchar_t ch : value) {
        if (ch == L'\'') {
            quoted += L"''";
        } else {
            quoted.push_back(ch);
        }
    }
    quoted.push_back(L'\'');
    return quoted;
}

bool CreateTemporaryTextFilePath(std::wstring& path, std::wstring& errorMessage) {
    std::wstring tempFolder(MAX_PATH + 1, L'\0');
    const DWORD tempLength = GetTempPathW(static_cast<DWORD>(tempFolder.size()), tempFolder.data());
    if (tempLength == 0 || tempLength >= tempFolder.size()) {
        errorMessage = L"Unable to resolve a temporary folder for analyzer output.";
        return false;
    }
    tempFolder.resize(tempLength);

    std::wstring tempFile(MAX_PATH + 1, L'\0');
    if (GetTempFileNameW(tempFolder.c_str(), L"hla", 0, tempFile.data()) == 0) {
        errorMessage = L"Unable to create a temporary analyzer output file.";
        return false;
    }

    path = tempFile.c_str();
    return true;
}

bool ReadUtf8TextFile(const std::filesystem::path& path, std::wstring& text, std::wstring& errorMessage) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        errorMessage = L"Unable to open " + path.wstring();
        return false;
    }

    const std::string bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    text = Utf8ToWide(bytes);
    if (!text.empty() && text.front() == 0xFEFF) {
        text.erase(text.begin());
    }
    return true;
}

bool WriteUtf8TextFile(const std::filesystem::path& path, const std::wstring& text, std::wstring& errorMessage) {
    std::error_code directoryError;
    std::filesystem::create_directories(path.parent_path(), directoryError);
    if (directoryError) {
        errorMessage = L"Unable to create folder: " + path.parent_path().wstring();
        return false;
    }

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        errorMessage = L"Unable to write file: " + path.wstring();
        return false;
    }

    const std::string bytes = WideToUtf8(text);
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!stream.good()) {
        errorMessage = L"Unable to finish writing file: " + path.wstring();
        return false;
    }

    return true;
}

std::wstring BuildLocalTimestampText(bool safeForFileName) {
    SYSTEMTIME time{};
    GetLocalTime(&time);

    wchar_t buffer[64]{};
    if (safeForFileName) {
        swprintf_s(
            buffer,
            L"%04hu%02hu%02hu-%02hu%02hu%02hu",
            time.wYear,
            time.wMonth,
            time.wDay,
            time.wHour,
            time.wMinute,
            time.wSecond);
    } else {
        swprintf_s(
            buffer,
            L"%04hu-%02hu-%02hu %02hu:%02hu:%02hu",
            time.wYear,
            time.wMonth,
            time.wDay,
            time.wHour,
            time.wMinute,
            time.wSecond);
    }

    return buffer;
}

std::wstring SanitizeFileNamePart(std::wstring value) {
    value = hlcfg::Trimmed(value);
    if (value.empty()) {
        return L"guided-test";
    }

    for (wchar_t& ch : value) {
        if (ch == L'<' || ch == L'>' || ch == L':' || ch == L'"' || ch == L'/' || ch == L'\\' ||
            ch == L'|' || ch == L'?' || ch == L'*' || std::iswspace(ch)) {
            ch = L'_';
        }
    }

    return value;
}

const hlcfg::JsonValue* FindJsonMember(const hlcfg::JsonValue::Object& object, const std::wstring& key) {
    const auto it = object.find(key);
    return it == object.end() ? nullptr : &it->second;
}

std::wstring ReadJsonString(const hlcfg::JsonValue::Object& object, const std::wstring& key) {
    const hlcfg::JsonValue* value = FindJsonMember(object, key);
    return value != nullptr && value->IsString() ? value->AsString() : std::wstring();
}

int ReadJsonInt(const hlcfg::JsonValue::Object& object, const std::wstring& key, int fallbackValue = -1) {
    const hlcfg::JsonValue* value = FindJsonMember(object, key);
    if (value == nullptr || !value->IsNumber()) {
        return fallbackValue;
    }
    return static_cast<int>(value->AsNumber());
}

bool ReadJsonBool(const hlcfg::JsonValue::Object& object, const std::wstring& key, bool fallbackValue = false) {
    const hlcfg::JsonValue* value = FindJsonMember(object, key);
    if (value == nullptr || !value->IsBool()) {
        return fallbackValue;
    }
    return value->AsBool();
}

std::wstring UnquoteCfgValue(const std::wstring& value) {
    const std::wstring trimmed = hlcfg::Trimmed(value);
    if (trimmed.size() < 2 || trimmed.front() != L'"' || trimmed.back() != L'"') {
        return trimmed;
    }

    std::wstring unquoted;
    unquoted.reserve(trimmed.size() - 2);
    bool escaping = false;
    for (std::size_t index = 1; index + 1 < trimmed.size(); ++index) {
        const wchar_t ch = trimmed[index];
        if (escaping) {
            unquoted.push_back(ch);
            escaping = false;
            continue;
        }

        if (ch == L'\\') {
            escaping = true;
            continue;
        }

        unquoted.push_back(ch);
    }

    return unquoted;
}

bool ParseCfgCvarMap(const std::wstring& cfgText, hlcfg::CvarMap& cvars) {
    std::wstringstream stream(cfgText);
    std::wstring line;
    while (std::getline(stream, line)) {
        const std::wstring trimmed = hlcfg::Trimmed(line);
        if (trimmed.empty() || trimmed[0] == L'#' || (trimmed.size() >= 2 && trimmed[0] == L'/' && trimmed[1] == L'/')) {
            continue;
        }

        const std::size_t split = trimmed.find_first_of(L" \t");
        if (split == std::wstring::npos) {
            continue;
        }

        const std::wstring name = hlcfg::Trimmed(trimmed.substr(0, split));
        const std::wstring rawValue = hlcfg::Trimmed(trimmed.substr(split + 1));
        if (name.empty() || rawValue.empty()) {
            continue;
        }

        cvars[name] = UnquoteCfgValue(rawValue);
    }

    return true;
}

std::filesystem::path ResolveMatchPackCfgPath(
    const std::filesystem::path& metadataPath,
    const std::wstring& cfgPath,
    const hlcfg::EnvironmentPaths& environment) {
    const std::filesystem::path candidate(cfgPath);
    std::error_code error;
    if (candidate.is_absolute() && std::filesystem::exists(candidate, error)) {
        return candidate;
    }

    const std::filesystem::path metadataParent = metadataPath.parent_path();
    std::vector<std::filesystem::path> candidates;
    candidates.emplace_back(metadataParent / candidate);

    if (!environment.liveModRoot.empty()) {
        candidates.emplace_back(std::filesystem::path(environment.liveModRoot) / candidate);
    }
    if (!environment.stagedLiveModRoot.empty()) {
        candidates.emplace_back(std::filesystem::path(environment.stagedLiveModRoot) / candidate);
    }
    if (!environment.repoRoot.empty()) {
        const std::filesystem::path repoMatchPackRoot = std::filesystem::path(environment.repoRoot) / L"configs" / L"match-packs";
        candidates.emplace_back(repoMatchPackRoot / candidate.filename());
        if (candidate.generic_wstring().rfind(L"match_packs/", 0) == 0) {
            candidates.emplace_back(repoMatchPackRoot / candidate.filename());
        }
    }

    for (const std::filesystem::path& current : candidates) {
        if (current.empty()) {
            continue;
        }

        if (std::filesystem::exists(current, error) && !error) {
            return current;
        }
    }

    return {};
}

bool LoadWeaponPresetEntry(const std::filesystem::path& path, const std::wstring& category, BrowserEntry& entry, std::wstring& errorMessage) {
    std::wstring text;
    if (!ReadUtf8TextFile(path, text, errorMessage)) {
        return false;
    }

    hlcfg::JsonValue root;
    if (!hlcfg::ParseJsonText(text, root, errorMessage) || !root.IsObject()) {
        errorMessage = L"Unable to parse preset JSON: " + path.wstring();
        return false;
    }

    const hlcfg::JsonValue::Object& object = root.AsObject();
    const hlcfg::JsonValue* cvarsValue = FindJsonMember(object, L"cvars");
    if (cvarsValue == nullptr || !cvarsValue->IsObject()) {
        errorMessage = L"Preset JSON is missing a cvars object: " + path.wstring();
        return false;
    }

    entry = {};
    entry.kind = BrowserEntryKind::WeaponPreset;
    entry.category = category;
    entry.name = ReadJsonString(object, L"name");
    entry.description = ReadJsonString(object, L"description");
    entry.sourcePath = path.wstring();

    if (entry.name.empty()) {
        entry.name = path.stem().wstring();
    }

    for (const auto& [name, value] : cvarsValue->AsObject()) {
        if (value.IsString()) {
            entry.cvars[name] = value.AsString();
        } else if (value.IsBool()) {
            entry.cvars[name] = value.AsBool() ? L"1" : L"0";
        } else if (value.IsNumber()) {
            std::wostringstream stream;
            stream.precision(15);
            stream << value.AsNumber();
            entry.cvars[name] = stream.str();
        }
    }

    return true;
}

bool LoadMatchPackEntry(const std::filesystem::path& path, const hlcfg::EnvironmentPaths& environment, BrowserEntry& entry, std::wstring& errorMessage) {
    std::wstring text;
    if (!ReadUtf8TextFile(path, text, errorMessage)) {
        return false;
    }

    hlcfg::JsonValue root;
    if (!hlcfg::ParseJsonText(text, root, errorMessage) || !root.IsObject()) {
        errorMessage = L"Unable to parse match-pack JSON: " + path.wstring();
        return false;
    }

    const hlcfg::JsonValue::Object& object = root.AsObject();
    entry = {};
    entry.kind = BrowserEntryKind::MatchPack;
    entry.name = ReadJsonString(object, L"name");
    entry.description = ReadJsonString(object, L"description");
    entry.tags = ReadJsonString(object, L"tags");
    entry.notes = ReadJsonString(object, L"notes");
    entry.category = ReadJsonString(object, L"mode");
    entry.sourcePath = path.wstring();

    const std::wstring cfgPath = ReadJsonString(object, L"cfg");
    std::filesystem::path resolvedCfg = ResolveMatchPackCfgPath(path, cfgPath, environment);
    entry.referencedCfgPath = resolvedCfg.wstring();

    if (entry.name.empty()) {
        entry.name = path.stem().wstring();
    }

    if (!resolvedCfg.empty()) {
        std::wstring cfgText;
        if (ReadUtf8TextFile(resolvedCfg, cfgText, errorMessage)) {
            ParseCfgCvarMap(cfgText, entry.cvars);
        } else {
            errorMessage.clear();
        }
    }

    return true;
}

class EditorWindow {
public:
    EditorWindow(HINSTANCE instance, const std::wstring& moduleFilePath)
        : instance_(instance), moduleFilePath_(moduleFilePath), environment_(hlcfg::ResolveEnvironmentPaths(moduleFilePath)) {
        ResetToNewDocument();
    }

    int Run(int commandShow) {
        INITCOMMONCONTROLSEX controls{};
        controls.dwSize = sizeof(controls);
        controls.dwICC = ICC_TAB_CLASSES | ICC_STANDARD_CLASSES;
        InitCommonControlsEx(&controls);

        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = &EditorWindow::WindowProc;
        windowClass.hInstance = instance_;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        windowClass.lpszClassName = kWindowClassName;
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        RegisterClassExW(&windowClass);

        WNDCLASSEXW pageClass{};
        pageClass.cbSize = sizeof(pageClass);
        pageClass.lpfnWndProc = &EditorWindow::PageWindowProc;
        pageClass.hInstance = instance_;
        pageClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        pageClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        pageClass.lpszClassName = kPageWindowClassName;
        RegisterClassExW(&pageClass);

        hwnd_ = CreateWindowExW(
            0,
            kWindowClassName,
            kWindowTitle,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            kWindowWidth,
            kWindowHeight,
            nullptr,
            nullptr,
            instance_,
            this);

        if (hwnd_ == nullptr) {
            return 1;
        }

        ShowWindow(hwnd_, commandShow);
        UpdateWindow(hwnd_);

        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        return static_cast<int>(message.wParam);
    }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        EditorWindow* self = nullptr;
        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            self = static_cast<EditorWindow*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->hwnd_ = hwnd;
        } else {
            self = reinterpret_cast<EditorWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (self == nullptr) {
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }

        return self->HandleMessage(message, wParam, lParam);
    }

    static LRESULT CALLBACK PageWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
        case WM_COMMAND:
        case WM_NOTIFY: {
            if (HWND root = GetAncestor(hwnd, GA_ROOT)) {
                return SendMessageW(root, message, wParam, lParam);
            }
            break;
        }
        default:
            break;
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
        case WM_CREATE:
            CreateMenus();
            CreateUi();
            ReloadBrowserEntries();
            LoadDocumentToControls();
            UpdateWindowTitle();
            return 0;
        case WM_SIZE:
            LayoutPages(LOWORD(lParam), HIWORD(lParam));
            return 0;
        case WM_NOTIFY:
            if (reinterpret_cast<LPNMHDR>(lParam)->idFrom == IDC_TAB &&
                reinterpret_cast<LPNMHDR>(lParam)->code == TCN_SELCHANGE) {
                ShowActivePage(TabCtrl_GetCurSel(tab_));
            }
            return 0;
        case WM_COMMAND:
            return HandleCommand(wParam, lParam);
        case WM_CLOSE:
            if (PromptToSaveIfDirty()) {
                DestroyWindow(hwnd_);
            }
            return 0;
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = 1120;
            info->ptMinTrackSize.y = 780;
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        }
    }

    LRESULT HandleCommand(WPARAM wParam, LPARAM lParam) {
        const int controlId = LOWORD(wParam);
        const int notifyCode = HIWORD(wParam);

        switch (controlId) {
        case IDM_FILE_NEW:
            if (PromptToSaveIfDirty()) {
                ResetToNewDocument();
                LoadDocumentToControls();
                currentProjectPath_.clear();
                dirty_ = false;
                UpdateWindowTitle();
            }
            return 0;
        case IDM_FILE_OPEN:
            OpenProject();
            return 0;
        case IDM_FILE_SAVE:
            SaveProject(false);
            return 0;
        case IDM_FILE_SAVE_AS:
            SaveProject(true);
            return 0;
        case IDM_FILE_EXIT:
            SendMessageW(hwnd_, WM_CLOSE, 0, 0);
            return 0;
        case IDC_GLOCK_PRESET_DEFAULT:
            ApplyPreset([&] { hlcfg::ApplyGlockPreset(document_, L"default"); });
            return 0;
        case IDC_GLOCK_PRESET_CS_LIKE_SOFT:
            ApplyPreset([&] { hlcfg::ApplyGlockPreset(document_, L"glock_cadence_soft"); });
            return 0;
        case IDC_GLOCK_PRESET_CS_TIGHT:
            ApplyPreset([&] { hlcfg::ApplyGlockPreset(document_, L"glock_cadence_tight"); });
            return 0;
        case IDC_GLOCK_PRESET_HEADSHOT_TEST:
            ApplyPreset([&] { hlcfg::ApplyGlockPreset(document_, L"headshot_test"); });
            return 0;
        case IDC_MP5_PRESET_DEFAULT:
            ApplyPreset([&] { hlcfg::ApplyMp5Preset(document_, L"default"); });
            return 0;
        case IDC_MP5_PRESET_CS_BURST:
            ApplyPreset([&] { hlcfg::ApplyMp5Preset(document_, L"mp5_pattern_burst"); });
            return 0;
        case IDC_MP5_PRESET_CS_MOBILE:
            ApplyPreset([&] { hlcfg::ApplyMp5Preset(document_, L"mp5_pattern_mobile"); });
            return 0;
        case IDC_MP5_PRESET_SPRAY_TEST:
            ApplyPreset([&] { hlcfg::ApplyMp5Preset(document_, L"mp5_spray_harsh"); });
            return 0;
        case IDC_357_PRESET_DEFAULT:
            ApplyPreset([&] { hlcfg::Apply357Preset(document_, L"default"); });
            return 0;
        case IDC_357_PRESET_PRECISION_TEST:
            ApplyPreset([&] { hlcfg::Apply357Preset(document_, L"357_precision_duel"); });
            return 0;
            case IDC_357_PRESET_HEADSHOT_TEST:
                ApplyPreset([&] { hlcfg::Apply357Preset(document_, L"357_cadence_headshot"); });
                return 0;
            case IDC_357_PRESET_PATTERN_SOFT:
                ApplyPreset([&] { hlcfg::Apply357Preset(document_, L"357_pattern_soft"); });
                return 0;
            case IDC_357_PRESET_PATTERN_TIGHT:
                ApplyPreset([&] { hlcfg::Apply357Preset(document_, L"357_pattern_tight"); });
                return 0;
        case IDC_SHOTGUN_PRESET_DEFAULT:
            ApplyPreset([&] { hlcfg::ApplyShotgunPreset(document_, L"default"); });
            return 0;
        case IDC_SHOTGUN_PRESET_CLOSE_QUICKKILL:
            ApplyPreset([&] { hlcfg::ApplyShotgunPreset(document_, L"close_quickkill"); });
            return 0;
        case IDC_SHOTGUN_PRESET_PRECISION_TEST:
            ApplyPreset([&] { hlcfg::ApplyShotgunPreset(document_, L"precision_test"); });
            return 0;
        case IDC_SHOTGUN_PRESET_PATTERN_SOFT:
            ApplyPreset([&] { hlcfg::ApplyShotgunPreset(document_, L"shotgun_pattern_soft"); });
            return 0;
        case IDC_SHOTGUN_PRESET_PATTERN_TIGHT:
            ApplyPreset([&] { hlcfg::ApplyShotgunPreset(document_, L"shotgun_pattern_tight"); });
            return 0;
        case IDC_DUMMY_PRESET_UNARMORED:
            ApplyPreset([&] { hlcfg::ApplyDummyPreset(document_, L"unarmored"); });
            return 0;
        case IDC_DUMMY_PRESET_VEST:
            ApplyPreset([&] { hlcfg::ApplyDummyPreset(document_, L"vest"); });
            return 0;
        case IDC_DUMMY_PRESET_VEST_HEADPROTECTED:
            ApplyPreset([&] { hlcfg::ApplyDummyPreset(document_, L"vest_headprotected"); });
            return 0;
        case IDC_MATCH_PRESET_DUEL_GLOCK:
            ApplyPreset([&] { hlcfg::ApplyMatchPreset(document_, L"duel_glock"); });
            return 0;
        case IDC_MATCH_PRESET_DUEL_357:
            ApplyPreset([&] { hlcfg::ApplyMatchPreset(document_, L"duel_357"); });
            return 0;
        case IDC_MATCH_PRESET_TEAM_MP5:
            ApplyPreset([&] { hlcfg::ApplyMatchPreset(document_, L"team_mp5"); });
            return 0;
        case IDC_MATCH_PRESET_TEAM_SHOTGUN:
            ApplyPreset([&] { hlcfg::ApplyMatchPreset(document_, L"team_shotgun"); });
            return 0;
        case IDC_MATCH_PRESET_ARMOR_TEST:
            ApplyPreset([&] { hlcfg::ApplyMatchPreset(document_, L"armor_test"); });
            return 0;
        case IDC_MATCH_PRESET_BUY_TEST:
            ApplyPreset([&] { hlcfg::ApplyMatchPreset(document_, L"buy_test"); });
            return 0;
        case IDC_BROWSER_PRESET_CATEGORY:
            if (notifyCode == CBN_SELCHANGE) {
                PopulatePresetBrowserList();
                RefreshBrowserPanels();
            }
            return 0;
        case IDC_BROWSER_PRESET_LIST:
            if (notifyCode == LBN_SELCHANGE) {
                CapturePresetBrowserSelection();
                RefreshBrowserPanels();
            }
            return 0;
        case IDC_BROWSER_MATCH_PACK_LIST:
            if (notifyCode == LBN_SELCHANGE) {
                CaptureMatchPackBrowserSelection();
                RefreshBrowserPanels();
            }
            return 0;
        case IDC_BROWSER_PRESET_LOAD:
            LoadPresetBrowserEntry();
            return 0;
        case IDC_BROWSER_MATCH_PACK_LOAD:
            LoadMatchPackBrowserEntry();
            return 0;
        case IDC_BROWSER_PRESET_OPEN_FOLDER:
            OpenPresetBrowserFolder();
            return 0;
        case IDC_BROWSER_MATCH_PACK_OPEN_FOLDER:
            OpenMatchPackBrowserFolder();
            return 0;
        case IDC_BROWSER_REFRESH:
            ReloadBrowserEntries();
            RefreshBrowserControls();
            return 0;
        case IDC_BROWSER_COPY_APPLY_COMMAND:
            CopyBrowserApplyCommand();
            return 0;
        case IDC_BROWSER_QUICK_EXPORT_COPY:
            QuickExportAndCopyApplyCommand();
            return 0;
        case IDC_BROWSER_OPEN_LIVE_MOD_FOLDER:
            OpenResolvedFolder(GetLiveModFolder(), L"The live mod folder could not be resolved.", L"Opened live mod folder");
            return 0;
        case IDC_LIVE_TEST_CONNECTION:
            TestLiveServerConnection();
            return 0;
        case IDC_LIVE_APPLY_CFG:
            ApplyCurrentCfgToLiveServer(false);
            return 0;
        case IDC_LIVE_APPLY_MATCH_PACK:
            ApplySelectedMatchPackToLiveServer(false);
            return 0;
        case IDC_LIVE_SANDBOX_RESET:
            SendLiveCommandSequence({L"exp_sandbox_reset"}, L"Sandbox reset");
            return 0;
        case IDC_LIVE_APPLY_CFG_SANDBOX_RESET:
            ApplyCurrentCfgToLiveServer(true);
            return 0;
        case IDC_LIVE_APPLY_PACK_SANDBOX_RESET:
            ApplySelectedMatchPackToLiveServer(true);
            return 0;
        case IDC_LIVE_APPLY_SANDBOX_SETUP:
            SendSandboxSetupToLiveServer();
            return 0;
        case IDC_LIVE_COPY_SANDBOX_SEQUENCE:
            CopySandboxSequence();
            return 0;
        case IDC_LIVE_SEND_CUSTOM:
            SendCustomLiveCommand();
            return 0;
        case IDC_LIVE_COPY_FALLBACK:
            CopyLastLiveFallbackCommands();
            return 0;
        case IDC_GUIDED_START_TEST:
            StartGuidedWeaponTest();
            return 0;
        case IDC_GUIDED_FINISH_ANALYZE:
            FinishAndAnalyzeGuidedWeaponTest();
            return 0;
        case IDC_GUIDED_COPY_COMMANDS:
            CopyGuidedTestCommands();
            return 0;
        case IDC_GUIDED_SAVE_REPORT:
            SaveGuidedTestReport();
            return 0;
        case IDC_GUIDED_REFRESH_PREVIEW:
            RefreshGuidedTestPreview(false);
            return 0;
        case IDC_REPORT_LIST:
            if (notifyCode == LBN_SELCHANGE) {
                RefreshReportSelectionDetails();
            }
            return 0;
        case IDC_REPORT_REFRESH:
            RefreshReportHistory(true);
            return 0;
        case IDC_REPORT_COMPARE:
            CompareSelectedReports();
            return 0;
        case IDC_REPORT_OPEN_REPORT:
            OpenSelectedReportFile();
            return 0;
        case IDC_REPORT_OPEN_LOG:
            OpenSelectedReportLog();
            return 0;
        case IDC_REPORT_COPY_REPORT_PATH:
            CopySelectedReportPath();
            return 0;
        case IDC_REPORT_COPY_LOG_PATH:
            CopySelectedReportLogPath();
            return 0;
        case IDC_REPORT_COPY_COMPARISON:
            CopyReportComparisonSummary();
            return 0;
        case IDC_REPORT_EXPORT_COMPARISON:
            ExportReportComparison();
            return 0;
        case IDC_TELEMETRY_FIND_LATEST:
            FindLatestTelemetryLog(true);
            return 0;
        case IDC_TELEMETRY_BROWSE_LOG:
            BrowseTelemetryLog();
            return 0;
        case IDC_TELEMETRY_ANALYZE_LATEST:
            AnalyzeLatestTelemetryLog();
            return 0;
        case IDC_TELEMETRY_ANALYZE_SELECTED:
            AnalyzeSelectedTelemetryLog();
            return 0;
        case IDC_TELEMETRY_OPEN_LOG_FOLDER:
            OpenTelemetryLogFolder();
            return 0;
        case IDC_TELEMETRY_COPY_LOG_PATH:
            CopyTelemetryLogPath();
            return 0;
        case IDC_TELEMETRY_COPY_ANALYSIS:
            CopyTelemetryAnalysisText();
            return 0;
        case IDC_BROWSE_EXPORT_FOLDER:
            BrowseExportFolder();
            return 0;
        case IDC_QUICK_EXPORT_LIVE_MOD:
            QuickExportToLiveMod();
            return 0;
        case IDC_EXPORT_CFG:
            ExportCfg();
            return 0;
        case IDC_COPY_EXEC_COMMAND:
            CopyExecCommand();
            return 0;
        case IDC_COPY_LAUNCHER_COMMAND:
            CopyLauncherCommand();
            return 0;
        case IDC_COPY_RAW_CFG:
            CopyRawCfg();
            return 0;
        case IDC_OPEN_EXPORT_FOLDER:
            OpenExportFolder();
            return 0;
        case IDC_OPEN_LIVE_MOD_FOLDER:
            OpenResolvedFolder(GetLiveModFolder(), L"The live mod folder could not be resolved.", L"Opened live mod folder");
            return 0;
        case IDC_OPEN_LOGS_FOLDER:
            OpenResolvedFolder(environment_.logsRoot, L"The repo logs folder could not be resolved.", L"Opened logs folder");
            return 0;
        default:
            break;
        }

        if (loadingControls_) {
            return 0;
        }

        if ((notifyCode == EN_CHANGE || notifyCode == CBN_SELCHANGE) && IsLiveServerControl(controlId)) {
            if (controlId == IDC_LIVE_CUSTOM_COMMAND) {
                return 0;
            }
            RefreshLiveServerCommandPreview(false);
            return 0;
        }

        if ((notifyCode == EN_CHANGE || notifyCode == CBN_SELCHANGE) && IsTelemetryControl(controlId)) {
            RefreshTelemetryStatusPreview(false);
            return 0;
        }

        if ((notifyCode == EN_CHANGE || notifyCode == CBN_SELCHANGE) && IsGuidedTestControl(controlId)) {
            RefreshGuidedTestPreview(false);
            return 0;
        }

        if ((notifyCode == EN_CHANGE || notifyCode == CBN_SELCHANGE) && IsReportControl(controlId)) {
            return 0;
        }

        if (notifyCode == EN_CHANGE || notifyCode == BN_CLICKED || notifyCode == CBN_SELCHANGE) {
            if (controlId != IDC_LIVE_MOD_FOLDER_PREVIEW && controlId != IDC_LAUNCHER_PREVIEW &&
                controlId != IDC_EXEC_PREVIEW && controlId != IDC_CFG_PREVIEW &&
                controlId != IDC_HALF_LIFE_ROOT_PREVIEW && controlId != IDC_QUICK_EXPORT_TARGET_PREVIEW &&
                controlId != IDC_EDITOR_EXE_PATH_PREVIEW && controlId != IDC_EXPORT_STATUS &&
                !IsLiveServerControl(controlId) && !IsTelemetryControl(controlId) && !IsGuidedTestControl(controlId) &&
                !IsReportControl(controlId)) {
                MaybeRefreshSuggestedCfgFileName(controlId);
                dirty_ = true;
                UpdateWindowTitle();
                RefreshMatchSummary();
                RefreshBrowserPanels();
                if (controlId == IDC_EXPORT_FOLDER || controlId == IDC_EXPORT_FILE_NAME || TabCtrl_GetCurSel(tab_) == kExportPageIndex) {
                    RefreshExportPreview(true);
                }
            }
        }

        (void)lParam;
        return 0;
    }

    template <typename TAction>
    void ApplyPreset(TAction action) {
        SyncDocumentFromControls();
        action();
        LoadDocumentToControls();
        dirty_ = true;
        UpdateWindowTitle();
    }

    void CreateMenus() {
        HMENU menuBar = CreateMenu();
        HMENU fileMenu = CreatePopupMenu();
        AppendMenuW(fileMenu, MF_STRING, IDM_FILE_NEW, L"&New");
        AppendMenuW(fileMenu, MF_STRING, IDM_FILE_OPEN, L"&Open...");
        AppendMenuW(fileMenu, MF_STRING, IDM_FILE_SAVE, L"&Save");
        AppendMenuW(fileMenu, MF_STRING, IDM_FILE_SAVE_AS, L"Save &As...");
        AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(fileMenu, MF_STRING, IDM_FILE_EXIT, L"E&xit");
        AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"&File");
        SetMenu(hwnd_, menuBar);
    }

    void CreateUi() {
        tab_ = CreateChildControl(hwnd_, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS, 0, 10, 10, 100, 100, IDC_TAB);

        const wchar_t* pageTitles[kPageCount] = {
            L"General",
            L"Glock",
            L"MP5",
            L"357",
            L"Shotgun",
            L"Target Dummy",
            L"Round Mode",
            L"Team Round",
            L"Buy & Equipment",
            L"Browser",
            L"Live Server",
            L"Guided Tests",
            L"Reports",
            L"Telemetry",
            L"Export",
        };
        for (int index = 0; index < kPageCount; ++index) {
            TCITEMW item{};
            item.mask = TCIF_TEXT;
            item.pszText = const_cast<LPWSTR>(pageTitles[index]);
            TabCtrl_InsertItem(tab_, index, &item);

            pages_[index] = CreateWindowExW(WS_EX_CONTROLPARENT, kPageWindowClassName, L"", WS_CHILD | WS_VISIBLE, 0, 0, 100, 100, tab_, nullptr, instance_, nullptr);
            SetControlFont(pages_[index]);
        }

        CreateGeneralPage();
        CreateGlockPage();
        CreateMp5Page();
        Create357Page();
        CreateShotgunPage();
        CreateDummyPage();
        CreateRoundPage();
        CreateTeamRoundPage();
        CreateBuyEquipmentPage();
        CreateBrowserPage();
        CreateLiveServerPage();
        CreateGuidedTestsPage();
        CreateReportsPage();
        CreateTelemetryPage();
        CreateExportPage();
        ShowActivePage(0);
    }

    void CreateGeneralPage() {
        HWND page = pages_[0];
        CreateGroupBox(page, L"Project", 20, 20, 500, 175);
        CreateLabel(page, L"Project name", 40, 55, 100, 20);
        CreateEdit(page, IDC_PROJECT_NAME, 150, 50, 330, 24);
        CreateLabel(page, L"Author", 40, 90, 100, 20);
        CreateEdit(page, IDC_PROJECT_AUTHOR, 150, 85, 330, 24);
        CreateLabel(page, L"Notes", 40, 125, 100, 20);
        CreateMultiLineEdit(page, IDC_PROJECT_NOTES, 150, 120, 330, 55, false);

        CreateGroupBox(page, L"Session", 540, 20, 500, 130);
        CreateLabel(page, L"Weapon under test", 560, 55, 120, 20);
        HWND combo = CreateCombo(page, IDC_WEAPON_UNDER_TEST, 710, 50, 280, 200);
        ComboBox_AddString(combo, L"");
        ComboBox_AddString(combo, L"glock");
        ComboBox_AddString(combo, L"mp5");
        ComboBox_AddString(combo, L"357");
        ComboBox_AddString(combo, L"shotgun");
        CreateLabel(page, L"Session tag", 560, 95, 120, 20);
        CreateEdit(page, IDC_SESSION_TAG, 710, 90, 280, 24);

        CreateGroupBox(page, L"Debug", 540, 170, 500, 110);
        CreateCheckBox(page, L"Enable weapon log", IDC_DEBUG_WEAPON_LOG, 560, 205, 220, 20);
        CreateCheckBox(page, L"Include rejection reasons", IDC_DEBUG_WEAPON_LOG_REJECTIONS, 560, 235, 220, 20);

        CreateGroupBox(page, L"How this editor works", 20, 220, 500, 120);
        CreateLabel(page, L"Save .hlcfg.json files for editing. Quick Export writes a live-mod .cfg that HLDS can load with exec my_config.cfg.", 40, 255, 450, 40);
        CreateLabel(page, L"This build now covers weapon tuning, dummy settings, round rules, team rules, buy rules, and armor or helmet equipment.", 40, 295, 450, 30);

        CreateGroupBox(page, L"Match Pack Metadata", 20, 360, 500, 140);
        CreateLabel(page, L"Pack name", 40, 395, 100, 20);
        CreateEdit(page, IDC_MATCH_PACK_NAME, 150, 390, 330, 24);
        CreateLabel(page, L"Description", 40, 430, 100, 20);
        CreateEdit(page, IDC_MATCH_PACK_DESCRIPTION, 150, 425, 330, 24);
        CreateLabel(page, L"Tags", 40, 465, 100, 20);
        CreateEdit(page, IDC_MATCH_PACK_TAGS, 150, 460, 330, 24);

        CreateGroupBox(page, L"What This Config Will Affect", 540, 300, 500, 200);
        CreateMultiLineEdit(page, IDC_MATCH_SUMMARY, 560, 330, 450, 140, true);
    }

    void CreateGlockPage() {
        HWND page = pages_[1];
        CreateGroupBox(page, L"Editor Templates", 20, 20, 1040, 70);
        CreateButton(page, L"Default", IDC_GLOCK_PRESET_DEFAULT, 40, 45, 120, 24);
        CreateButton(page, L"Cadence Soft", IDC_GLOCK_PRESET_CS_LIKE_SOFT, 175, 45, 150, 24);
        CreateButton(page, L"Cadence Tight", IDC_GLOCK_PRESET_CS_TIGHT, 340, 45, 140, 24);
        CreateButton(page, L"Headshot Test", IDC_GLOCK_PRESET_HEADSHOT_TEST, 475, 45, 140, 24);

        CreateGroupBox(page, L"General Glock Settings", 20, 110, 500, 220);
        CreateLabel(page, L"Profile name", 40, 145, 120, 20);
        CreateEdit(page, IDC_GLOCK_PROFILE_NAME, 220, 140, 260, 24);
        CreateCheckBox(page, L"Legacy tap-fire gate", IDC_GLOCK_TAP_FIRE, 40, 180, 180, 20);
        CreateCheckBox(page, L"First-shot accuracy", IDC_GLOCK_FIRST_SHOT_ACCURACY, 220, 180, 180, 20);
        CreateLabel(page, L"Recovery seconds to clear cadence bloom", 40, 220, 170, 20);
        CreateEdit(page, IDC_GLOCK_SPREAD_RECOVERY, 220, 215, 120, 24);
        CreateLabel(page, L"Movement penalty scale", 40, 255, 170, 20);
        CreateEdit(page, IDC_GLOCK_MOVE_SPREAD_SCALE, 220, 250, 120, 24);
        CreateLabel(page, L"Recommended: leave the legacy tap-fire gate off. Tune single-shot feel with cadence bloom, recovery, and the learnable follow-up pattern.", 40, 290, 440, 36);

        CreateGroupBox(page, L"Primary Spread Model", 540, 110, 520, 290);
        CreateLabel(page, L"Base spread", 560, 145, 180, 20);
        CreateEdit(page, IDC_GLOCK_PRIMARY_BASE_SPREAD, 790, 140, 220, 24);
        CreateLabel(page, L"Ground move penalty", 560, 180, 180, 20);
        CreateEdit(page, IDC_GLOCK_PRIMARY_GROUND_MOVE_PENALTY, 790, 175, 220, 24);
        CreateLabel(page, L"Air move penalty", 560, 215, 180, 20);
        CreateEdit(page, IDC_GLOCK_PRIMARY_AIR_MOVE_PENALTY, 790, 210, 220, 24);
        CreateLabel(page, L"Crouch stability scale", 560, 250, 180, 20);
        CreateEdit(page, IDC_GLOCK_PRIMARY_DUCK_PENALTY_SCALE, 790, 245, 220, 24);
        CreateLabel(page, L"Shot growth (cadence bloom per shot)", 560, 285, 180, 20);
        CreateEdit(page, IDC_GLOCK_PRIMARY_SHOT_GROWTH, 790, 280, 220, 24);
        CreateLabel(page, L"First-shot speed threshold", 560, 320, 180, 20);
        CreateEdit(page, IDC_GLOCK_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD, 790, 315, 220, 24);
        CreateLabel(page, L"Max spread clamp", 560, 355, 180, 20);
        CreateEdit(page, IDC_GLOCK_PRIMARY_MAX_SPREAD, 790, 350, 220, 24);

        CreateGroupBox(page, L"Damage Model", 20, 350, 500, 130);
        CreateLabel(page, L"Damage", 40, 385, 120, 20);
        CreateEdit(page, IDC_GLOCK_PRIMARY_DAMAGE, 220, 380, 120, 24);
        CreateLabel(page, L"Headshot scale", 40, 420, 120, 20);
        CreateEdit(page, IDC_GLOCK_PRIMARY_HEADSHOT_SCALE, 220, 415, 120, 24);
        CreateCheckBox(page, L"Lethal headshot", IDC_GLOCK_PRIMARY_HEADSHOT_LETHAL, 40, 450, 160, 20);

        CreateGroupBox(page, L"Cadence Feel Layer", 20, 500, 500, 220);
        CreateCheckBox(page, L"Cadence-sensitive mode", IDC_GLOCK_PRIMARY_CADENCE_MODE, 40, 535, 220, 20);
        CreateLabel(page, L"Ideal shot cycle time", 40, 570, 170, 20);
        CreateEdit(page, IDC_GLOCK_PRIMARY_CYCLE_TIME, 220, 565, 120, 24);
        CreateLabel(page, L"Fast-click penalty", 40, 605, 170, 20);
        CreateEdit(page, IDC_GLOCK_PRIMARY_CLICK_PENALTY, 220, 600, 120, 24);
        CreateLabel(page, L"Fast-click penalty scale", 40, 640, 170, 20);
        CreateEdit(page, IDC_GLOCK_PRIMARY_CLICK_PENALTY_SCALE, 220, 635, 120, 24);
        CreateLabel(page, L"Cadence reset time", 40, 675, 170, 20);
        CreateEdit(page, IDC_GLOCK_PRIMARY_CLICK_RESET_TIME, 220, 670, 120, 24);
        CreateLabel(page, L"Hold / spam penalty scale", 40, 710, 170, 20);
        CreateEdit(page, IDC_GLOCK_PRIMARY_HOLD_PENALTY_SCALE, 220, 705, 120, 24);

        CreateGroupBox(page, L"Deterministic Pattern Layer", 540, 435, 520, 255);
        CreateCheckBox(page, L"Deterministic pattern mode", IDC_GLOCK_PATTERN_MODE, 560, 470, 220, 20);
        CreateLabel(page, L"Horizontal pattern scale", 560, 505, 170, 20);
        CreateEdit(page, IDC_GLOCK_PATTERN_SCALE_X, 790, 500, 220, 24);
        CreateLabel(page, L"Vertical pattern scale", 560, 540, 170, 20);
        CreateEdit(page, IDC_GLOCK_PATTERN_SCALE_Y, 790, 535, 220, 24);
        CreateLabel(page, L"Pattern reset time", 560, 575, 170, 20);
        CreateEdit(page, IDC_GLOCK_PATTERN_RESET_TIME, 790, 570, 220, 24);
        CreateLabel(page, L"Pattern max index", 560, 610, 170, 20);
        CreateEdit(page, IDC_GLOCK_PATTERN_MAX_INDEX, 790, 605, 220, 24);
    }

    void CreateMp5Page() {
        HWND page = pages_[2];
        CreateGroupBox(page, L"Editor Templates", 20, 20, 1040, 70);
        CreateButton(page, L"Default", IDC_MP5_PRESET_DEFAULT, 40, 45, 120, 24);
        CreateButton(page, L"Pattern Burst", IDC_MP5_PRESET_CS_BURST, 175, 45, 150, 24);
        CreateButton(page, L"Pattern Mobile", IDC_MP5_PRESET_CS_MOBILE, 340, 45, 130, 24);
        CreateButton(page, L"Spray Harsh", IDC_MP5_PRESET_SPRAY_TEST, 455, 45, 130, 24);

        CreateGroupBox(page, L"General MP5 Settings", 20, 110, 500, 235);
        CreateCheckBox(page, L"Enable experimental MP5 primary", IDC_MP5_PRIMARY_ENABLED, 40, 145, 250, 20);
        CreateLabel(page, L"Profile name", 40, 180, 120, 20);
        CreateEdit(page, IDC_MP5_PROFILE_NAME, 220, 175, 260, 24);
        CreateCheckBox(page, L"First-shot accuracy", IDC_MP5_PRIMARY_FIRST_SHOT_ACCURACY, 40, 215, 180, 20);
        CreateCheckBox(page, L"Spawn MP5 loadout", IDC_MP5_LAB_LOADOUT, 40, 245, 180, 20);
        CreateLabel(page, L"Lab ammo", 40, 280, 120, 20);
        CreateEdit(page, IDC_MP5_LAB_AMMO, 220, 275, 120, 24);
        CreateCheckBox(page, L"Enable autoswitch", IDC_MP5_LAB_AUTOSWITCH, 40, 310, 180, 20);

        CreateGroupBox(page, L"Primary Spread Model", 540, 110, 520, 325);
        CreateLabel(page, L"Base spread", 560, 145, 180, 20);
        CreateEdit(page, IDC_MP5_PRIMARY_BASE_SPREAD, 790, 140, 220, 24);
        CreateLabel(page, L"Ground move penalty", 560, 180, 180, 20);
        CreateEdit(page, IDC_MP5_PRIMARY_GROUND_MOVE_PENALTY, 790, 175, 220, 24);
        CreateLabel(page, L"Air move penalty", 560, 215, 180, 20);
        CreateEdit(page, IDC_MP5_PRIMARY_AIR_MOVE_PENALTY, 790, 210, 220, 24);
        CreateLabel(page, L"Crouch stability scale", 560, 250, 180, 20);
        CreateEdit(page, IDC_MP5_PRIMARY_DUCK_PENALTY_SCALE, 790, 245, 220, 24);
        CreateLabel(page, L"Burst growth (cadence bloom per shot)", 560, 285, 180, 20);
        CreateEdit(page, IDC_MP5_PRIMARY_BURST_GROWTH, 790, 280, 220, 24);
        CreateLabel(page, L"Max extra spread from bloom", 560, 320, 180, 20);
        CreateEdit(page, IDC_MP5_PRIMARY_BURST_MAX_ADDITIONAL_SPREAD, 790, 315, 220, 24);
        CreateLabel(page, L"Recovery seconds to clear bloom", 560, 355, 180, 20);
        CreateEdit(page, IDC_MP5_PRIMARY_SPREAD_RECOVERY, 790, 350, 220, 24);
        CreateLabel(page, L"First-shot speed threshold", 560, 390, 180, 20);
        CreateEdit(page, IDC_MP5_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD, 790, 385, 220, 24);
        CreateLabel(page, L"Max spread", 560, 425, 180, 20);
        CreateEdit(page, IDC_MP5_PRIMARY_MAX_SPREAD, 790, 420, 220, 24);

        CreateGroupBox(page, L"Damage Model", 20, 365, 500, 110);
        CreateLabel(page, L"Damage", 40, 400, 120, 20);
        CreateEdit(page, IDC_MP5_PRIMARY_DAMAGE, 220, 395, 120, 24);
        CreateLabel(page, L"Headshot scale", 40, 435, 120, 20);
        CreateEdit(page, IDC_MP5_PRIMARY_HEADSHOT_SCALE, 220, 430, 120, 24);
        CreateCheckBox(page, L"Lethal headshot", IDC_MP5_PRIMARY_HEADSHOT_LETHAL, 360, 430, 120, 20);

        CreateGroupBox(page, L"Pattern And Spray Control", 20, 500, 500, 290);
        CreateCheckBox(page, L"Deterministic pattern mode", IDC_MP5_PATTERN_MODE, 40, 535, 220, 20);
        CreateLabel(page, L"Horizontal pattern scale", 40, 570, 170, 20);
        CreateEdit(page, IDC_MP5_PATTERN_SCALE_X, 220, 565, 120, 24);
        CreateLabel(page, L"Vertical pattern scale", 40, 605, 170, 20);
        CreateEdit(page, IDC_MP5_PATTERN_SCALE_Y, 220, 600, 120, 24);
        CreateLabel(page, L"Pattern reset time", 40, 640, 170, 20);
        CreateEdit(page, IDC_MP5_PATTERN_RESET_TIME, 220, 635, 120, 24);
        CreateLabel(page, L"Pattern max index", 40, 675, 170, 20);
        CreateEdit(page, IDC_MP5_PATTERN_MAX_INDEX, 220, 670, 120, 24);
        CreateLabel(page, L"Burst reset time", 40, 710, 170, 20);
        CreateEdit(page, IDC_MP5_PRIMARY_BURST_RESET_TIME, 220, 705, 120, 24);
        CreateLabel(page, L"Spray hold penalty scale", 40, 745, 170, 20);
        CreateEdit(page, IDC_MP5_PRIMARY_HOLD_PENALTY_SCALE, 220, 740, 120, 24);
    }

    void Create357Page() {
        HWND page = pages_[3];
        CreateGroupBox(page, L"Editor Templates", 20, 20, 1040, 70);
        CreateButton(page, L"Default", IDC_357_PRESET_DEFAULT, 40, 45, 120, 24);
        CreateButton(page, L"Precision Duel", IDC_357_PRESET_PRECISION_TEST, 175, 45, 145, 24);
        CreateButton(page, L"Cadence Headshot", IDC_357_PRESET_HEADSHOT_TEST, 335, 45, 160, 24);
        CreateButton(page, L"Pattern Soft", IDC_357_PRESET_PATTERN_SOFT, 510, 45, 140, 24);
        CreateButton(page, L"Pattern Tight", IDC_357_PRESET_PATTERN_TIGHT, 665, 45, 140, 24);

        CreateGroupBox(page, L"General 357 Settings", 20, 110, 500, 235);
        CreateCheckBox(page, L"Enable experimental 357 primary", IDC_357_PRIMARY_ENABLED, 40, 145, 260, 20);
        CreateLabel(page, L"Profile name", 40, 180, 120, 20);
        CreateEdit(page, IDC_357_PROFILE_NAME, 220, 175, 260, 24);
        CreateCheckBox(page, L"First-shot accuracy", IDC_357_PRIMARY_FIRST_SHOT_ACCURACY, 40, 215, 180, 20);
        CreateCheckBox(page, L"Spawn 357 loadout", IDC_357_LAB_LOADOUT, 40, 245, 180, 20);
        CreateLabel(page, L"Lab ammo", 40, 280, 120, 20);
        CreateEdit(page, IDC_357_LAB_AMMO, 220, 275, 120, 24);
        CreateCheckBox(page, L"Enable autoswitch", IDC_357_LAB_AUTOSWITCH, 40, 310, 180, 20);

        CreateGroupBox(page, L"Primary Spread Model", 540, 110, 520, 290);
        CreateLabel(page, L"Base spread", 560, 145, 180, 20);
        CreateEdit(page, IDC_357_PRIMARY_BASE_SPREAD, 790, 140, 220, 24);
        CreateLabel(page, L"Ground move penalty", 560, 180, 180, 20);
        CreateEdit(page, IDC_357_PRIMARY_GROUND_MOVE_PENALTY, 790, 175, 220, 24);
        CreateLabel(page, L"Air move penalty", 560, 215, 180, 20);
        CreateEdit(page, IDC_357_PRIMARY_AIR_MOVE_PENALTY, 790, 210, 220, 24);
        CreateLabel(page, L"Duck penalty scale", 560, 250, 180, 20);
        CreateEdit(page, IDC_357_PRIMARY_DUCK_PENALTY_SCALE, 790, 245, 220, 24);
        CreateLabel(page, L"Spread recovery", 560, 285, 180, 20);
        CreateEdit(page, IDC_357_PRIMARY_SPREAD_RECOVERY, 790, 280, 220, 24);
        CreateLabel(page, L"First-shot speed threshold", 560, 320, 180, 20);
        CreateEdit(page, IDC_357_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD, 790, 315, 220, 24);
        CreateLabel(page, L"Max spread", 560, 355, 180, 20);
        CreateEdit(page, IDC_357_PRIMARY_MAX_SPREAD, 790, 350, 220, 24);

        CreateGroupBox(page, L"Damage Model", 20, 365, 500, 110);
        CreateLabel(page, L"Damage", 40, 400, 120, 20);
        CreateEdit(page, IDC_357_PRIMARY_DAMAGE, 220, 395, 120, 24);
        CreateLabel(page, L"Headshot scale", 40, 435, 120, 20);
        CreateEdit(page, IDC_357_PRIMARY_HEADSHOT_SCALE, 220, 430, 120, 24);
        CreateCheckBox(page, L"Lethal headshot", IDC_357_PRIMARY_HEADSHOT_LETHAL, 360, 430, 120, 20);

        CreateGroupBox(page, L"Cadence And Pattern Layer", 20, 500, 1040, 220);
        CreateCheckBox(page, L"Cadence-sensitive mode", IDC_357_PRIMARY_CADENCE_MODE, 40, 535, 220, 20);
        CreateLabel(page, L"Ideal shot cycle time", 40, 570, 170, 20);
        CreateEdit(page, IDC_357_PRIMARY_CYCLE_TIME, 220, 565, 120, 24);
        CreateLabel(page, L"Fast-click penalty", 40, 605, 170, 20);
        CreateEdit(page, IDC_357_PRIMARY_CLICK_PENALTY, 220, 600, 120, 24);
        CreateLabel(page, L"Fast-click penalty scale", 40, 640, 170, 20);
        CreateEdit(page, IDC_357_PRIMARY_CLICK_PENALTY_SCALE, 220, 635, 120, 24);
        CreateLabel(page, L"Cadence reset time", 40, 675, 170, 20);
        CreateEdit(page, IDC_357_PRIMARY_CLICK_RESET_TIME, 220, 670, 120, 24);
        CreateLabel(page, L"Hold / spam penalty scale", 40, 710, 170, 20);
        CreateEdit(page, IDC_357_PRIMARY_HOLD_PENALTY_SCALE, 220, 705, 120, 24);
        CreateCheckBox(page, L"Deterministic pattern mode", IDC_357_PATTERN_MODE, 560, 535, 230, 20);
        CreateLabel(page, L"Horizontal pattern scale", 560, 570, 190, 20);
        CreateEdit(page, IDC_357_PATTERN_SCALE_X, 790, 565, 120, 24);
        CreateLabel(page, L"Vertical pattern scale", 560, 605, 190, 20);
        CreateEdit(page, IDC_357_PATTERN_SCALE_Y, 790, 600, 120, 24);
        CreateLabel(page, L"Pattern reset time", 560, 640, 190, 20);
        CreateEdit(page, IDC_357_PATTERN_RESET_TIME, 790, 635, 120, 24);
        CreateLabel(page, L"Pattern max index", 560, 675, 190, 20);
        CreateEdit(page, IDC_357_PATTERN_MAX_INDEX, 790, 670, 120, 24);
    }

    void CreateShotgunPage() {
        HWND page = pages_[4];
        CreateGroupBox(page, L"Editor Templates", 20, 20, 1040, 70);
        CreateButton(page, L"Default", IDC_SHOTGUN_PRESET_DEFAULT, 40, 45, 110, 24);
        CreateButton(page, L"Pattern Soft", IDC_SHOTGUN_PRESET_PATTERN_SOFT, 165, 45, 140, 24);
        CreateButton(page, L"Pattern Tight", IDC_SHOTGUN_PRESET_PATTERN_TIGHT, 320, 45, 140, 24);
        CreateButton(page, L"Close Quickkill", IDC_SHOTGUN_PRESET_CLOSE_QUICKKILL, 475, 45, 150, 24);
        CreateButton(page, L"Precision Test", IDC_SHOTGUN_PRESET_PRECISION_TEST, 640, 45, 145, 24);

        CreateGroupBox(page, L"General Shotgun Settings", 20, 110, 500, 235);
        CreateCheckBox(page, L"Enable experimental shotgun primary", IDC_SHOTGUN_PRIMARY_ENABLED, 40, 145, 290, 20);
        CreateLabel(page, L"Profile name", 40, 180, 120, 20);
        CreateEdit(page, IDC_SHOTGUN_PROFILE_NAME, 220, 175, 260, 24);
        CreateCheckBox(page, L"First-shot accuracy", IDC_SHOTGUN_PRIMARY_FIRST_SHOT_ACCURACY, 40, 215, 180, 20);
        CreateCheckBox(page, L"Spawn shotgun loadout", IDC_SHOTGUN_LAB_LOADOUT, 40, 245, 190, 20);
        CreateLabel(page, L"Lab ammo", 40, 280, 120, 20);
        CreateEdit(page, IDC_SHOTGUN_LAB_AMMO, 220, 275, 120, 24);
        CreateCheckBox(page, L"Enable autoswitch", IDC_SHOTGUN_LAB_AUTOSWITCH, 40, 310, 180, 20);

        CreateGroupBox(page, L"Primary Spread Model", 540, 110, 520, 290);
        CreateLabel(page, L"Base spread", 560, 145, 180, 20);
        CreateEdit(page, IDC_SHOTGUN_PRIMARY_BASE_SPREAD, 790, 140, 220, 24);
        CreateLabel(page, L"Ground move penalty", 560, 180, 180, 20);
        CreateEdit(page, IDC_SHOTGUN_PRIMARY_GROUND_MOVE_PENALTY, 790, 175, 220, 24);
        CreateLabel(page, L"Air move penalty", 560, 215, 180, 20);
        CreateEdit(page, IDC_SHOTGUN_PRIMARY_AIR_MOVE_PENALTY, 790, 210, 220, 24);
        CreateLabel(page, L"Duck penalty scale", 560, 250, 180, 20);
        CreateEdit(page, IDC_SHOTGUN_PRIMARY_DUCK_PENALTY_SCALE, 790, 245, 220, 24);
        CreateLabel(page, L"Spread recovery", 560, 285, 180, 20);
        CreateEdit(page, IDC_SHOTGUN_PRIMARY_SPREAD_RECOVERY, 790, 280, 220, 24);
        CreateLabel(page, L"First-shot speed threshold", 560, 320, 180, 20);
        CreateEdit(page, IDC_SHOTGUN_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD, 790, 315, 220, 24);
        CreateLabel(page, L"Max spread", 560, 355, 180, 20);
        CreateEdit(page, IDC_SHOTGUN_PRIMARY_MAX_SPREAD, 790, 350, 220, 24);
        CreateLabel(page, L"Per-shot spread growth", 560, 390, 180, 20);
        CreateEdit(page, IDC_SHOTGUN_PRIMARY_SHOT_GROWTH, 790, 385, 220, 24);

        CreateGroupBox(page, L"Pattern And Pellet Layout", 20, 365, 500, 250);
        CreateCheckBox(page, L"Deterministic pellet pattern", IDC_SHOTGUN_PATTERN_MODE, 40, 400, 220, 20);
        CreateLabel(page, L"Pellet spread mode", 40, 435, 140, 20);
        CreateCombo(page, IDC_SHOTGUN_PRIMARY_PELLET_SPREAD_MODE, 220, 430, 140, 160);
        ComboBox_AddString(FindControl(IDC_SHOTGUN_PRIMARY_PELLET_SPREAD_MODE), L"0");
        ComboBox_AddString(FindControl(IDC_SHOTGUN_PRIMARY_PELLET_SPREAD_MODE), L"1");
        CreateLabel(page, L"Pellet pattern X scale", 40, 470, 160, 20);
        CreateEdit(page, IDC_SHOTGUN_PATTERN_SCALE_X, 220, 465, 120, 24);
        CreateLabel(page, L"Pellet pattern Y scale", 40, 505, 160, 20);
        CreateEdit(page, IDC_SHOTGUN_PATTERN_SCALE_Y, 220, 500, 120, 24);
        CreateLabel(page, L"Pattern reset time", 40, 540, 160, 20);
        CreateEdit(page, IDC_SHOTGUN_PATTERN_RESET_TIME, 220, 535, 120, 24);
        CreateLabel(page, L"Pattern max index", 40, 575, 160, 20);
        CreateEdit(page, IDC_SHOTGUN_PATTERN_MAX_INDEX, 220, 570, 120, 24);

        CreateGroupBox(page, L"Pellet And Damage Model", 540, 430, 520, 185);
        CreateLabel(page, L"Damage per pellet", 560, 470, 160, 20);
        CreateEdit(page, IDC_SHOTGUN_PRIMARY_DAMAGE_PER_PELLET, 790, 465, 140, 24);
        CreateLabel(page, L"Pellet count", 560, 505, 160, 20);
        CreateEdit(page, IDC_SHOTGUN_PRIMARY_PELLET_COUNT, 790, 500, 140, 24);
        CreateLabel(page, L"Headshot scale", 560, 540, 160, 20);
        CreateEdit(page, IDC_SHOTGUN_PRIMARY_HEADSHOT_SCALE, 790, 535, 140, 24);
        CreateCheckBox(page, L"Lethal headshot", IDC_SHOTGUN_PRIMARY_HEADSHOT_LETHAL, 560, 575, 160, 20);
    }

    void CreateDummyPage() {
        HWND page = pages_[5];
        CreateGroupBox(page, L"Editor Templates", 20, 20, 1040, 70);
        CreateButton(page, L"Unarmored", IDC_DUMMY_PRESET_UNARMORED, 40, 45, 130, 24);
        CreateButton(page, L"Vest", IDC_DUMMY_PRESET_VEST, 185, 45, 120, 24);
        CreateButton(page, L"Vest + Head Protected", IDC_DUMMY_PRESET_VEST_HEADPROTECTED, 320, 45, 200, 24);

        CreateGroupBox(page, L"Target Stats", 20, 110, 500, 255);
        CreateCheckBox(page, L"Enable target dummy", IDC_DUMMY_ENABLED, 40, 145, 180, 20);
        CreateLabel(page, L"Target profile name", 40, 180, 140, 20);
        CreateEdit(page, IDC_DUMMY_TARGET_PROFILE_NAME, 220, 175, 260, 24);
        CreateLabel(page, L"Dummy health", 40, 215, 140, 20);
        CreateEdit(page, IDC_DUMMY_HEALTH, 220, 210, 120, 24);
        CreateLabel(page, L"Dummy armor", 40, 250, 140, 20);
        CreateEdit(page, IDC_DUMMY_ARMOR, 220, 245, 120, 24);
        CreateCheckBox(page, L"Head protected", IDC_DUMMY_HEAD_PROTECTED, 360, 245, 120, 20);
        CreateLabel(page, L"Armor health fraction", 40, 285, 140, 20);
        CreateEdit(page, IDC_DUMMY_ARMOR_HEALTH_FRACTION, 220, 280, 120, 24);
        CreateLabel(page, L"Armor drain scale", 40, 320, 140, 20);
        CreateEdit(page, IDC_DUMMY_ARMOR_DRAIN_SCALE, 220, 315, 120, 24);

        CreateGroupBox(page, L"Respawn And Placement", 540, 110, 520, 290);
        CreateCheckBox(page, L"Autorespawn", IDC_DUMMY_AUTORESPAWN, 560, 145, 140, 20);
        CreateLabel(page, L"Respawn delay", 560, 180, 180, 20);
        CreateEdit(page, IDC_DUMMY_RESPAWN_DELAY, 790, 175, 220, 24);
        CreateLabel(page, L"Spawn distance", 560, 215, 180, 20);
        CreateEdit(page, IDC_DUMMY_SPAWN_DISTANCE, 790, 210, 220, 24);
        CreateLabel(page, L"Offset right", 560, 250, 180, 20);
        CreateEdit(page, IDC_DUMMY_OFFSET_RIGHT, 790, 245, 220, 24);
        CreateLabel(page, L"Offset up", 560, 285, 180, 20);
        CreateEdit(page, IDC_DUMMY_OFFSET_UP, 790, 280, 220, 24);
        CreateCheckBox(page, L"Face the player", IDC_DUMMY_FACE_PLAYER, 560, 320, 180, 20);
        CreateLabel(page, L"Dummy model", 560, 355, 180, 20);
        CreateEdit(page, IDC_DUMMY_MODEL, 790, 350, 220, 24);
    }

    void CreateRoundPage() {
        HWND page = pages_[kRoundPageIndex];
        CreateGroupBox(page, L"Match Templates", 20, 20, 1040, 70);
        CreateButton(page, L"Duel Glock", IDC_MATCH_PRESET_DUEL_GLOCK, 40, 45, 135, 24);
        CreateButton(page, L"Duel 357", IDC_MATCH_PRESET_DUEL_357, 190, 45, 135, 24);
        CreateButton(page, L"Team MP5", IDC_MATCH_PRESET_TEAM_MP5, 340, 45, 135, 24);
        CreateButton(page, L"Team Shotgun", IDC_MATCH_PRESET_TEAM_SHOTGUN, 490, 45, 145, 24);
        CreateButton(page, L"Armor Test", IDC_MATCH_PRESET_ARMOR_TEST, 650, 45, 135, 24);
        CreateButton(page, L"Buy Test", IDC_MATCH_PRESET_BUY_TEST, 800, 45, 135, 24);

        CreateGroupBox(page, L"Round Core", 20, 110, 500, 235);
        CreateCheckBox(page, L"Enable round mode", IDC_ROUND_MODE, 40, 145, 180, 20);
        CreateCheckBox(page, L"No respawn during round", IDC_ROUND_NO_RESPAWN, 240, 145, 220, 20);
        CreateCheckBox(page, L"Friendly fire", IDC_ROUND_FRIENDLY_FIRE, 40, 180, 180, 20);
        CreateLabel(page, L"Freeze time", 40, 220, 140, 20);
        CreateEdit(page, IDC_ROUND_FREEZE_TIME, 220, 215, 120, 24);
        CreateLabel(page, L"Restart delay", 40, 255, 140, 20);
        CreateEdit(page, IDC_ROUND_RESTART_DELAY, 220, 250, 120, 24);
        CreateLabel(page, L"Start health", 40, 290, 140, 20);
        CreateEdit(page, IDC_ROUND_START_HEALTH, 220, 285, 120, 24);
        CreateLabel(page, L"Start armor", 40, 325, 140, 20);
        CreateEdit(page, IDC_ROUND_START_ARMOR, 220, 320, 120, 24);

        CreateGroupBox(page, L"Loadout And Profile", 540, 110, 520, 180);
        CreateLabel(page, L"Round weapon profile", 560, 145, 170, 20);
        CreateEdit(page, IDC_ROUND_WEAPON_PROFILE, 770, 140, 240, 24);
        CreateLabel(page, L"Round loadout mode", 560, 180, 170, 20);
        HWND loadoutCombo = CreateCombo(page, IDC_ROUND_LOADOUT_MODE, 770, 175, 240, 200);
        ComboBox_AddString(loadoutCombo, L"none");
        ComboBox_AddString(loadoutCombo, L"glock");
        ComboBox_AddString(loadoutCombo, L"mp5");
        ComboBox_AddString(loadoutCombo, L"357");
        ComboBox_AddString(loadoutCombo, L"shotgun");

        CreateGroupBox(page, L"Notes", 20, 370, 1040, 110);
        CreateLabel(page, L"Round mode settings export directly to sv_exp_round_* cvars. Use the team and buy pages to layer team-play and freeze-phase buying onto the same cfg.", 40, 405, 980, 36);
    }

    void CreateTeamRoundPage() {
        HWND page = pages_[kTeamRoundPageIndex];
        CreateGroupBox(page, L"Team Round Core", 20, 20, 500, 220);
        CreateCheckBox(page, L"Enable team round mode", IDC_TEAM_ROUND_MODE, 40, 55, 220, 20);
        CreateCheckBox(page, L"Enable teamplay rules", IDC_TEAM_ROUND_TEAMPLAY, 280, 55, 200, 20);
        CreateLabel(page, L"Spawn mode", 40, 95, 120, 20);
        HWND spawnCombo = CreateCombo(page, IDC_TEAM_ROUND_SPAWN_MODE, 220, 90, 220, 200);
        ComboBox_AddString(spawnCombo, L"dm_spawns");
        ComboBox_AddString(spawnCombo, L"manual_spots");
        CreateLabel(page, L"Team 1 name", 40, 130, 120, 20);
        CreateEdit(page, IDC_TEAM_ROUND_TEAM1_NAME, 220, 125, 220, 24);
        CreateLabel(page, L"Team 2 name", 40, 165, 120, 20);
        CreateEdit(page, IDC_TEAM_ROUND_TEAM2_NAME, 220, 160, 220, 24);

        CreateGroupBox(page, L"Team 1 Setup", 540, 20, 250, 220);
        CreateLabel(page, L"Loadout", 560, 60, 80, 20);
        HWND team1LoadoutCombo = CreateCombo(page, IDC_TEAM_ROUND_TEAM1_LOADOUT, 640, 55, 120, 200);
        ComboBox_AddString(team1LoadoutCombo, L"");
        ComboBox_AddString(team1LoadoutCombo, L"none");
        ComboBox_AddString(team1LoadoutCombo, L"glock");
        ComboBox_AddString(team1LoadoutCombo, L"mp5");
        ComboBox_AddString(team1LoadoutCombo, L"357");
        ComboBox_AddString(team1LoadoutCombo, L"shotgun");
        CreateLabel(page, L"Health", 560, 100, 80, 20);
        CreateEdit(page, IDC_TEAM_ROUND_TEAM1_HEALTH, 640, 95, 120, 24);
        CreateLabel(page, L"Armor", 560, 140, 80, 20);
        CreateEdit(page, IDC_TEAM_ROUND_TEAM1_ARMOR, 640, 135, 120, 24);

        CreateGroupBox(page, L"Team 2 Setup", 810, 20, 250, 220);
        CreateLabel(page, L"Loadout", 830, 60, 80, 20);
        HWND team2LoadoutCombo = CreateCombo(page, IDC_TEAM_ROUND_TEAM2_LOADOUT, 910, 55, 120, 200);
        ComboBox_AddString(team2LoadoutCombo, L"");
        ComboBox_AddString(team2LoadoutCombo, L"none");
        ComboBox_AddString(team2LoadoutCombo, L"glock");
        ComboBox_AddString(team2LoadoutCombo, L"mp5");
        ComboBox_AddString(team2LoadoutCombo, L"357");
        ComboBox_AddString(team2LoadoutCombo, L"shotgun");
        CreateLabel(page, L"Health", 830, 100, 80, 20);
        CreateEdit(page, IDC_TEAM_ROUND_TEAM2_HEALTH, 910, 95, 120, 24);
        CreateLabel(page, L"Armor", 830, 140, 80, 20);
        CreateEdit(page, IDC_TEAM_ROUND_TEAM2_ARMOR, 910, 135, 120, 24);

        CreateGroupBox(page, L"Notes", 20, 270, 1040, 110);
        CreateLabel(page, L"Use dm_spawns for simple team testing or manual_spots when the server has persisted team spawn markers. Empty per-team loadouts fall back to the round loadout or the server defaults.", 40, 305, 980, 36);
    }

    void CreateBuyEquipmentPage() {
        HWND page = pages_[kBuyEquipmentPageIndex];
        CreateGroupBox(page, L"Buy Phase", 20, 20, 500, 235);
        CreateCheckBox(page, L"Enable buy mode", IDC_BUY_MODE, 40, 55, 180, 20);
        CreateCheckBox(page, L"Freeze-time only buying", IDC_BUY_FREEZE_ONLY, 240, 55, 220, 20);
        CreateCheckBox(page, L"Shared team catalog", IDC_BUY_TEAM_SHARED_CATALOG, 40, 90, 200, 20);
        CreateLabel(page, L"Start money", 40, 130, 120, 20);
        CreateEdit(page, IDC_BUY_START_MONEY, 220, 125, 120, 24);
        CreateLabel(page, L"Round win reward", 40, 165, 120, 20);
        CreateEdit(page, IDC_BUY_ROUND_WIN_REWARD, 220, 160, 120, 24);
        CreateLabel(page, L"Round loss reward", 40, 200, 120, 20);
        CreateEdit(page, IDC_BUY_ROUND_LOSS_REWARD, 220, 195, 120, 24);
        CreateLabel(page, L"Max money", 40, 235, 120, 20);
        CreateEdit(page, IDC_BUY_MAX_MONEY, 220, 230, 120, 24);

        CreateGroupBox(page, L"Weapon Catalog", 540, 20, 520, 270);
        CreateCheckBox(page, L"Glock", IDC_BUY_ALLOW_GLOCK, 560, 55, 120, 20);
        CreateCheckBox(page, L"MP5", IDC_BUY_ALLOW_MP5, 560, 90, 120, 20);
        CreateCheckBox(page, L"357", IDC_BUY_ALLOW_357, 560, 125, 120, 20);
        CreateCheckBox(page, L"Shotgun", IDC_BUY_ALLOW_SHOTGUN, 560, 160, 120, 20);
        CreateLabel(page, L"Glock cost", 760, 55, 110, 20);
        CreateEdit(page, IDC_BUY_COST_GLOCK, 900, 50, 120, 24);
        CreateLabel(page, L"MP5 cost", 760, 90, 110, 20);
        CreateEdit(page, IDC_BUY_COST_MP5, 900, 85, 120, 24);
        CreateLabel(page, L"357 cost", 760, 125, 110, 20);
        CreateEdit(page, IDC_BUY_COST_357, 900, 120, 120, 24);
        CreateLabel(page, L"Shotgun cost", 760, 160, 110, 20);
        CreateEdit(page, IDC_BUY_COST_SHOTGUN, 900, 155, 120, 24);

        CreateGroupBox(page, L"Armor, Helmet, And Utility", 20, 285, 500, 255);
        CreateCheckBox(page, L"Enable armor mode", IDC_ARMOR_MODE, 40, 320, 180, 20);
        CreateCheckBox(page, L"Enable helmet mode", IDC_HELMET_MODE, 240, 320, 180, 20);
        CreateCheckBox(page, L"Helmet at round start", IDC_HELMET_START_ENABLED, 240, 355, 180, 20);
        CreateCheckBox(page, L"Helmet headshot protection", IDC_HELMET_HEADSHOT_PROTECTION, 240, 390, 220, 20);
        CreateLabel(page, L"Armor start value", 40, 355, 140, 20);
        CreateEdit(page, IDC_ARMOR_START_VALUE, 40, 380, 120, 24);
        CreateLabel(page, L"Armor max value", 40, 415, 140, 20);
        CreateEdit(page, IDC_ARMOR_MAX_VALUE, 40, 440, 120, 24);
        CreateLabel(page, L"Armor health fraction", 180, 415, 140, 20);
        CreateEdit(page, IDC_ARMOR_HEALTH_FRACTION, 180, 440, 120, 24);
        CreateLabel(page, L"Armor drain scale", 320, 415, 140, 20);
        CreateEdit(page, IDC_ARMOR_DRAIN_SCALE, 320, 440, 120, 24);

        CreateGroupBox(page, L"Equipment Catalog", 540, 310, 520, 230);
        CreateCheckBox(page, L"Armor", IDC_BUY_ALLOW_ARMOR, 560, 345, 120, 20);
        CreateCheckBox(page, L"Helmet", IDC_BUY_ALLOW_HELMET, 560, 380, 120, 20);
        CreateCheckBox(page, L"Hand grenade", IDC_BUY_ALLOW_HANDGRENADE, 560, 415, 140, 20);
        CreateLabel(page, L"Armor cost", 760, 345, 110, 20);
        CreateEdit(page, IDC_BUY_COST_ARMOR, 900, 340, 120, 24);
        CreateLabel(page, L"Helmet cost", 760, 380, 110, 20);
        CreateEdit(page, IDC_BUY_COST_HELMET, 900, 375, 120, 24);
        CreateLabel(page, L"Hand grenade cost", 760, 415, 125, 20);
        CreateEdit(page, IDC_BUY_COST_HANDGRENADE, 900, 410, 120, 24);
    }

    void CreateBrowserPage() {
        HWND page = pages_[kBrowserPageIndex];

        CreateGroupBox(page, L"Weapon Preset Browser", 20, 20, 500, 290);
        CreateLabel(page, L"Category", 40, 55, 90, 20);
        HWND categoryCombo = CreateCombo(page, IDC_BROWSER_PRESET_CATEGORY, 120, 50, 220, 200);
        ComboBox_AddString(categoryCombo, L"all");
        ComboBox_AddString(categoryCombo, L"glock");
        ComboBox_AddString(categoryCombo, L"mp5");
        ComboBox_AddString(categoryCombo, L"357");
        ComboBox_AddString(categoryCombo, L"shotgun");
        ComboBox_SetCurSel(categoryCombo, 0);
        CreateButton(page, L"Refresh", IDC_BROWSER_REFRESH, 360, 50, 120, 24);
        CreateListBox(page, IDC_BROWSER_PRESET_LIST, 40, 90, 440, 150);
        CreateButton(page, L"Load Into Current Project", IDC_BROWSER_PRESET_LOAD, 40, 255, 200, 24);
        CreateButton(page, L"Open Presets Folder", IDC_BROWSER_PRESET_OPEN_FOLDER, 255, 255, 160, 24);

        CreateGroupBox(page, L"Match-Pack Browser", 540, 20, 520, 290);
        CreateLabel(page, L"Source", 560, 55, 60, 20);
        CreateLabel(page, L"Prefers live mod packs when available, otherwise checked-in configs\\match-packs\\", 620, 55, 390, 20);
        CreateListBox(page, IDC_BROWSER_MATCH_PACK_LIST, 560, 90, 460, 150);
        CreateButton(page, L"Load Into Current Project", IDC_BROWSER_MATCH_PACK_LOAD, 560, 255, 200, 24);
        CreateButton(page, L"Open Match Packs Folder", IDC_BROWSER_MATCH_PACK_OPEN_FOLDER, 775, 255, 180, 24);

        CreateGroupBox(page, L"Selected Entry Preview", 20, 330, 500, 240);
        CreateMultiLineEdit(page, IDC_BROWSER_DETAILS, 40, 360, 440, 180, true);

        CreateGroupBox(page, L"Current vs Incoming Diff", 540, 330, 520, 240);
        CreateMultiLineEdit(page, IDC_BROWSER_DIFF, 560, 360, 460, 180, true);

        CreateGroupBox(page, L"Live Apply Helper", 20, 585, 1040, 130);
        CreateLabel(page, L"After quick export, apply with", 40, 620, 160, 20);
        CreateEdit(page, IDC_BROWSER_APPLY_COMMAND, 210, 615, 810, 24, ES_READONLY);
        CreateButton(page, L"Quick Export + Copy Apply Command", IDC_BROWSER_QUICK_EXPORT_COPY, 40, 650, 260, 24);
        CreateButton(page, L"Copy Apply Command", IDC_BROWSER_COPY_APPLY_COMMAND, 315, 650, 180, 24);
        CreateButton(page, L"Open Live Mod Folder", IDC_BROWSER_OPEN_LIVE_MOD_FOLDER, 510, 650, 170, 24);
        CreateMultiLineEdit(page, IDC_BROWSER_STATUS, 700, 645, 320, 38, true);
    }

    void CreateLiveServerPage() {
        HWND page = pages_[kLiveServerPageIndex];

        CreateGroupBox(page, L"Connection", 20, 20, 500, 165);
        CreateLabel(page, L"Server host", 40, 55, 120, 20);
        CreateEdit(page, IDC_LIVE_SERVER_HOST, 170, 50, 200, 24);
        CreateLabel(page, L"Port", 390, 55, 40, 20);
        CreateEdit(page, IDC_LIVE_SERVER_PORT, 435, 50, 60, 24);
        CreateLabel(page, L"RCON password", 40, 90, 120, 20);
        CreateEdit(page, IDC_LIVE_SERVER_PASSWORD, 170, 85, 325, 24, ES_PASSWORD);
        CreateButton(page, L"Test Connection", IDC_LIVE_TEST_CONNECTION, 40, 125, 150, 24);
        CreateButton(page, L"Copy Last Fallback", IDC_LIVE_COPY_FALLBACK, 205, 125, 160, 24);
        CreateLabel(page, L"If RCON is missing or rejected, the exact commands are copied for manual HLDS paste.", 40, 155, 430, 20);

        CreateGroupBox(page, L"Live Apply", 540, 20, 520, 205);
        CreateLabel(page, L"CFG file", 560, 55, 100, 20);
        CreateEdit(page, IDC_LIVE_CFG_FILE, 690, 50, 310, 24);
        CreateButton(page, L"Apply Current CFG", IDC_LIVE_APPLY_CFG, 560, 85, 165, 24);
        CreateButton(page, L"Apply CFG + Sandbox Reset", IDC_LIVE_APPLY_CFG_SANDBOX_RESET, 740, 85, 220, 24);
        CreateLabel(page, L"Match pack", 560, 125, 100, 20);
        CreateEdit(page, IDC_LIVE_MATCH_PACK, 690, 120, 310, 24);
        CreateButton(page, L"Apply Selected Match Pack", IDC_LIVE_APPLY_MATCH_PACK, 560, 155, 210, 24);
        CreateButton(page, L"Apply Pack + Sandbox Reset", IDC_LIVE_APPLY_PACK_SANDBOX_RESET, 785, 155, 210, 24);
        CreateLabel(page, L"CFG actions quick-export current project before sending exp_cfg_apply.", 560, 190, 430, 20);

        CreateGroupBox(page, L"Sandbox Setup", 20, 215, 1040, 185);
        CreateLabel(page, L"Weapon", 40, 250, 80, 20);
        HWND weaponCombo = CreateCombo(page, IDC_LIVE_SANDBOX_WEAPON, 120, 245, 140, 140);
        ComboBox_AddString(weaponCombo, L"glock");
        ComboBox_AddString(weaponCombo, L"mp5");
        ComboBox_AddString(weaponCombo, L"357");
        ComboBox_AddString(weaponCombo, L"shotgun");
        ComboBox_SetCurSel(weaponCombo, 0);
        CreateLabel(page, L"Target profile", 285, 250, 110, 20);
        CreateEdit(page, IDC_LIVE_SANDBOX_TARGET, 395, 245, 190, 24);
        CreateLabel(page, L"Target spot", 610, 250, 90, 20);
        CreateEdit(page, IDC_LIVE_SANDBOX_SPOT, 700, 245, 170, 24);
        CreateButton(page, L"Sandbox Reset", IDC_LIVE_SANDBOX_RESET, 890, 245, 140, 24);
        CreateLabel(page, L"Generated sandbox sequence", 40, 290, 160, 20);
        CreateMultiLineEdit(page, IDC_LIVE_CUSTOM_COMMAND, 205, 285, 470, 70, false);
        CreateButton(page, L"Apply Sandbox Setup", IDC_LIVE_APPLY_SANDBOX_SETUP, 700, 285, 180, 24);
        CreateButton(page, L"Copy Sandbox Sequence", IDC_LIVE_COPY_SANDBOX_SEQUENCE, 700, 320, 180, 24);
        CreateButton(page, L"Send Custom Command", IDC_LIVE_SEND_CUSTOM, 895, 285, 150, 24);
        CreateLabel(page, L"The sequence uses exp_sandbox_* commands and the selected cfg or match pack above.", 700, 355, 320, 20);

        CreateGroupBox(page, L"Live Status", 20, 420, 1040, 250);
        CreateMultiLineEdit(page, IDC_LIVE_STATUS, 40, 450, 990, 190, true);
    }

    void CreateGuidedTestsPage() {
        HWND page = pages_[kGuidedTestsPageIndex];

        CreateGroupBox(page, L"Guided Weapon Test Setup", 20, 20, 1040, 185);
        CreateLabel(page, L"Weapon", 40, 55, 80, 20);
        HWND weaponCombo = CreateCombo(page, IDC_GUIDED_WEAPON, 120, 50, 140, 160);
        ComboBox_AddString(weaponCombo, L"glock");
        ComboBox_AddString(weaponCombo, L"mp5");
        ComboBox_AddString(weaponCombo, L"357");
        ComboBox_AddString(weaponCombo, L"shotgun");
        ComboBox_SetCurSel(weaponCombo, 0);

        CreateLabel(page, L"Source", 285, 55, 80, 20);
        HWND sourceCombo = CreateCombo(page, IDC_GUIDED_SOURCE_TYPE, 365, 50, 190, 180);
        ComboBox_AddString(sourceCombo, L"current editor cfg");
        ComboBox_AddString(sourceCombo, L"preset");
        ComboBox_AddString(sourceCombo, L"match pack");
        ComboBox_AddString(sourceCombo, L"manual cfg filename");
        ComboBox_SetCurSel(sourceCombo, 0);
        CreateLabel(page, L"Source value", 585, 55, 95, 20);
        CreateEdit(page, IDC_GUIDED_SOURCE_VALUE, 690, 50, 310, 24);

        CreateLabel(page, L"Target profile", 40, 95, 100, 20);
        HWND targetCombo = CreateCombo(page, IDC_GUIDED_TARGET_PROFILE, 150, 90, 190, 160);
        ComboBox_AddString(targetCombo, L"unarmored");
        ComboBox_AddString(targetCombo, L"vest");
        ComboBox_AddString(targetCombo, L"vest_headprotected");
        ComboBox_SetCurSel(targetCombo, 0);
        CreateLabel(page, L"Target spot", 365, 95, 90, 20);
        CreateEdit(page, IDC_GUIDED_TARGET_SPOT, 455, 90, 150, 24);

        CreateLabel(page, L"Scenario", 630, 95, 75, 20);
        HWND scenarioCombo = CreateCombo(page, IDC_GUIDED_SCENARIO, 710, 90, 290, 180);
        ComboBox_AddString(scenarioCombo, L"single-shot precision");
        ComboBox_AddString(scenarioCombo, L"burst/spam control");
        ComboBox_AddString(scenarioCombo, L"movement test");
        ComboBox_AddString(scenarioCombo, L"headshot/armor test");
        ComboBox_AddString(scenarioCombo, L"shotgun pellet test");
        ComboBox_SetCurSel(scenarioCombo, 0);

        CreateButton(page, L"Start Test", IDC_GUIDED_START_TEST, 40, 145, 130, 24);
        CreateButton(page, L"Finish && Analyze", IDC_GUIDED_FINISH_ANALYZE, 190, 145, 155, 24);
        CreateButton(page, L"Copy Commands", IDC_GUIDED_COPY_COMMANDS, 365, 145, 140, 24);
        CreateButton(page, L"Save Test Report", IDC_GUIDED_SAVE_REPORT, 525, 145, 150, 24);
        CreateButton(page, L"Refresh Preview", IDC_GUIDED_REFRESH_PREVIEW, 695, 145, 145, 24);
        CreateLabel(page, L"Uses host, port, and RCON password from the Live Server tab.", 855, 149, 190, 20);

        CreateGroupBox(page, L"Shooting Instructions", 20, 225, 500, 180);
        CreateMultiLineEdit(page, IDC_GUIDED_INSTRUCTIONS, 40, 255, 440, 110, true);

        CreateGroupBox(page, L"Command Sequence", 540, 225, 520, 180);
        CreateMultiLineEdit(page, IDC_GUIDED_COMMAND_SEQUENCE, 560, 255, 460, 110, false);

        CreateGroupBox(page, L"Guided Test Status", 20, 425, 1040, 90);
        CreateMultiLineEdit(page, IDC_GUIDED_STATUS, 40, 455, 990, 35, true);

        CreateGroupBox(page, L"Compact Result And Analyzer Output", 20, 540, 1040, 165);
        CreateMultiLineEdit(page, IDC_GUIDED_ANALYSIS, 40, 570, 990, 100, true);
    }

    void CreateReportsPage() {
        HWND page = pages_[kReportsPageIndex];

        CreateGroupBox(page, L"Reports / Test History", 20, 20, 500, 310);
        CreateMultiSelectListBox(page, IDC_REPORT_LIST, 40, 55, 460, 210);
        CreateButton(page, L"Refresh", IDC_REPORT_REFRESH, 40, 280, 90, 24);
        CreateButton(page, L"Compare Selected", IDC_REPORT_COMPARE, 145, 280, 145, 24);
        CreateButton(page, L"Export Comparison", IDC_REPORT_EXPORT_COMPARISON, 310, 280, 155, 24);

        CreateGroupBox(page, L"Selected Report Details", 540, 20, 520, 310);
        CreateMultiLineEdit(page, IDC_REPORT_DETAILS, 560, 55, 460, 165, true);
        CreateButton(page, L"Open Report", IDC_REPORT_OPEN_REPORT, 560, 235, 120, 24);
        CreateButton(page, L"Open Log", IDC_REPORT_OPEN_LOG, 700, 235, 105, 24);
        CreateButton(page, L"Copy Report Path", IDC_REPORT_COPY_REPORT_PATH, 825, 235, 135, 24);
        CreateButton(page, L"Copy Log Path", IDC_REPORT_COPY_LOG_PATH, 560, 275, 120, 24);
        CreateButton(page, L"Copy Comparison", IDC_REPORT_COPY_COMPARISON, 700, 275, 145, 24);

        CreateGroupBox(page, L"Comparison Table", 20, 350, 1040, 270);
        CreateMultiLineEdit(page, IDC_REPORT_COMPARISON, 40, 380, 990, 195, true);

        CreateGroupBox(page, L"Report History Status", 20, 640, 1040, 70);
        CreateMultiLineEdit(page, IDC_REPORT_STATUS, 40, 665, 990, 25, true);
    }

    void CreateTelemetryPage() {
        HWND page = pages_[kTelemetryPageIndex];

        CreateGroupBox(page, L"Weapon Log Selection", 20, 20, 1040, 165);
        CreateLabel(page, L"Latest log", 40, 55, 100, 20);
        CreateEdit(page, IDC_TELEMETRY_LATEST_LOG, 150, 50, 740, 24, ES_READONLY);
        CreateButton(page, L"Find Latest Log", IDC_TELEMETRY_FIND_LATEST, 910, 50, 130, 24);
        CreateLabel(page, L"Selected log", 40, 90, 100, 20);
        CreateEdit(page, IDC_TELEMETRY_SELECTED_LOG, 150, 85, 740, 24);
        CreateButton(page, L"Browse...", IDC_TELEMETRY_BROWSE_LOG, 910, 85, 130, 24);
        CreateLabel(page, L"Weapon filter", 40, 125, 100, 20);
        HWND filterCombo = CreateCombo(page, IDC_TELEMETRY_WEAPON_FILTER, 150, 120, 180, 180);
        ComboBox_AddString(filterCombo, L"all");
        ComboBox_AddString(filterCombo, L"glock");
        ComboBox_AddString(filterCombo, L"mp5");
        ComboBox_AddString(filterCombo, L"357");
        ComboBox_AddString(filterCombo, L"shotgun");
        ComboBox_SetCurSel(filterCombo, 0);
        CreateButton(page, L"Analyze Latest", IDC_TELEMETRY_ANALYZE_LATEST, 360, 120, 130, 24);
        CreateButton(page, L"Analyze Selected", IDC_TELEMETRY_ANALYZE_SELECTED, 505, 120, 140, 24);
        CreateButton(page, L"Open Log Folder", IDC_TELEMETRY_OPEN_LOG_FOLDER, 660, 120, 140, 24);
        CreateButton(page, L"Copy Log Path", IDC_TELEMETRY_COPY_LOG_PATH, 815, 120, 120, 24);
        CreateButton(page, L"Copy Analysis", IDC_TELEMETRY_COPY_ANALYSIS, 945, 120, 105, 24);
        CreateLabel(page, L"Search order: Half-Life logs, live mod logs, then repo testbed logs.", 40, 155, 700, 20);

        CreateGroupBox(page, L"Telemetry Status", 20, 205, 1040, 85);
        CreateMultiLineEdit(page, IDC_TELEMETRY_STATUS, 40, 230, 990, 35, true);

        CreateGroupBox(page, L"Analyzer Output", 20, 315, 1040, 360);
        CreateMultiLineEdit(page, IDC_TELEMETRY_ANALYSIS_TEXT, 40, 345, 990, 295, true);
    }

    void CreateExportPage() {
        HWND page = pages_[kExportPageIndex];
        CreateGroupBox(page, L"Resolved Paths And Targets", 20, 20, 1040, 255);
        CreateLabel(page, L"Half-Life root", 40, 55, 120, 20);
        CreateEdit(page, IDC_HALF_LIFE_ROOT_PREVIEW, 160, 50, 850, 24, ES_READONLY);
        CreateLabel(page, L"Live mod root", 40, 90, 120, 20);
        CreateEdit(page, IDC_LIVE_MOD_FOLDER_PREVIEW, 160, 85, 850, 24, ES_READONLY);
        CreateLabel(page, L"Quick export target", 40, 125, 120, 20);
        CreateEdit(page, IDC_QUICK_EXPORT_TARGET_PREVIEW, 160, 120, 850, 24, ES_READONLY);
        CreateLabel(page, L"Running editor EXE", 40, 160, 120, 20);
        CreateEdit(page, IDC_EDITOR_EXE_PATH_PREVIEW, 160, 155, 850, 24, ES_READONLY);
        CreateLabel(page, L"Custom folder", 40, 195, 120, 20);
        CreateEdit(page, IDC_EXPORT_FOLDER, 160, 190, 720, 24);
        CreateButton(page, L"Browse...", IDC_BROWSE_EXPORT_FOLDER, 900, 190, 110, 24);
        CreateLabel(page, L"CFG file name", 40, 230, 120, 20);
        CreateEdit(page, IDC_EXPORT_FILE_NAME, 160, 225, 250, 24);
        CreateLabel(page, L"Quick Export writes <live mod root>\\<name>.cfg so HLDS can run exec <name>.cfg.", 430, 228, 560, 24);
        CreateLabel(page, L"If Match Pack Metadata is filled in and the cfg goes into the live mod, export also writes match_packs\\<pack>.json.", 40, 255, 970, 20);

        CreateGroupBox(page, L"Actions", 20, 290, 1040, 95);
        CreateButton(page, L"Quick Export to Live Mod", IDC_QUICK_EXPORT_LIVE_MOD, 40, 320, 210, 24);
        CreateButton(page, L"Copy exec command", IDC_COPY_EXEC_COMMAND, 265, 320, 160, 24);
        CreateButton(page, L"Export to chosen folder", IDC_EXPORT_CFG, 440, 320, 185, 24);
        CreateButton(page, L"Copy launcher", IDC_COPY_LAUNCHER_COMMAND, 640, 320, 135, 24);
        CreateButton(page, L"Open live mod", IDC_OPEN_LIVE_MOD_FOLDER, 790, 320, 120, 24);
        CreateButton(page, L"Open export folder", IDC_OPEN_EXPORT_FOLDER, 40, 350, 160, 24);
        CreateButton(page, L"Copy raw cfg", IDC_COPY_RAW_CFG, 215, 350, 135, 24);
        CreateButton(page, L"Open logs", IDC_OPEN_LOGS_FOLDER, 365, 350, 110, 24);

        CreateGroupBox(page, L"Last Action", 20, 400, 1040, 80);
        CreateMultiLineEdit(page, IDC_EXPORT_STATUS, 40, 425, 970, 30, true);

        CreateGroupBox(page, L"Preview", 20, 495, 1040, 175);
        CreateLabel(page, L"Launcher command", 40, 530, 120, 20);
        CreateEdit(page, IDC_LAUNCHER_PREVIEW, 160, 525, 850, 24, ES_READONLY);
        CreateLabel(page, L"Exec command", 40, 565, 120, 20);
        CreateEdit(page, IDC_EXEC_PREVIEW, 160, 560, 850, 24, ES_READONLY);
        CreateLabel(page, L"Generated cfg", 40, 600, 120, 20);
        CreateMultiLineEdit(page, IDC_CFG_PREVIEW, 160, 595, 850, 55, true);
    }

    void LayoutPages(int clientWidth, int clientHeight) {
        if (tab_ == nullptr) {
            return;
        }

        MoveWindow(tab_, 10, 10, clientWidth - 20, clientHeight - 20, TRUE);

        RECT tabRect{};
        GetClientRect(tab_, &tabRect);
        TabCtrl_AdjustRect(tab_, FALSE, &tabRect);

        for (HWND page : pages_) {
            MoveWindow(page, tabRect.left, tabRect.top, tabRect.right - tabRect.left, tabRect.bottom - tabRect.top, TRUE);
        }
    }

    void ShowActivePage(int pageIndex) {
        for (int index = 0; index < kPageCount; ++index) {
            ShowWindow(pages_[index], index == pageIndex ? SW_SHOW : SW_HIDE);
        }

        if (pageIndex == kExportPageIndex) {
            RefreshExportPreview(true);
        } else if (pageIndex == kBrowserPageIndex) {
            RefreshBrowserPanels();
        } else if (pageIndex == kLiveServerPageIndex) {
            RefreshLiveServerCommandPreview(false);
        } else if (pageIndex == kGuidedTestsPageIndex) {
            RefreshGuidedTestPreview(false);
        } else if (pageIndex == kReportsPageIndex) {
            RefreshReportHistory(false);
        } else if (pageIndex == kTelemetryPageIndex) {
            RefreshTelemetryStatusPreview(false);
        }
    }

    void ResetToNewDocument() {
        document_ = hlcfg::CreateDefaultProject();
        document_.exportSettings.exportFolder = environment_.defaultExportFolder;
        document_.exportSettings.cfgFileName = BuildSuggestedCfgFileName(document_);
        lastBrowserStatus_.clear();
        lastLiveStatus_.clear();
        lastLiveFallbackCommands_.clear();
        lastGuidedStatus_.clear();
        lastGuidedAnalysisText_.clear();
        lastGuidedCommandSequence_.clear();
        lastGuidedInstructions_.clear();
        lastGuidedBaselineLogPath_.clear();
        lastGuidedAnalyzedLogPath_.clear();
        lastGuidedBaselineLogWriteTime_ = {};
        reportEntries_.clear();
        lastReportStatus_.clear();
        lastReportComparisonText_.clear();
        lastTelemetryStatus_.clear();
        lastTelemetryAnalysisText_.clear();
        dirty_ = false;
    }

    void MaybeRefreshSuggestedCfgFileName(int controlId) {
        if (controlId != IDC_PROJECT_NAME &&
            controlId != IDC_MATCH_PACK_NAME &&
            controlId != IDC_SESSION_TAG &&
            controlId != IDC_WEAPON_UNDER_TEST &&
            controlId != IDC_GLOCK_PROFILE_NAME &&
            controlId != IDC_MP5_PROFILE_NAME &&
            controlId != IDC_357_PROFILE_NAME &&
            controlId != IDC_SHOTGUN_PROFILE_NAME) {
            return;
        }

        const std::wstring currentFileName = hlcfg::EnsureCfgFileName(GetTextValue(IDC_EXPORT_FILE_NAME));
        if (!currentFileName.empty() &&
            currentFileName != hlcfg::CreateDefaultProject().exportSettings.cfgFileName &&
            currentFileName != lastSuggestedCfgFileName_) {
            return;
        }

        hlcfg::ProjectDocument previewDocument = document_;
        previewDocument.metadata.projectName = GetTextValue(IDC_PROJECT_NAME);
        previewDocument.matchPack.name = GetTextValue(IDC_MATCH_PACK_NAME);
        previewDocument.general.sessionTag = GetTextValue(IDC_SESSION_TAG);
        previewDocument.general.weaponUnderTest = GetWeaponSelection();
        previewDocument.glock.profileName = GetTextValue(IDC_GLOCK_PROFILE_NAME);
        previewDocument.mp5.profileName = GetTextValue(IDC_MP5_PROFILE_NAME);
        previewDocument.weapon357.profileName = GetTextValue(IDC_357_PROFILE_NAME);
        previewDocument.shotgun.profileName = GetTextValue(IDC_SHOTGUN_PROFILE_NAME);

        const std::wstring suggestedFileName = BuildSuggestedCfgFileName(previewDocument);
        lastSuggestedCfgFileName_ = suggestedFileName;

        loadingControls_ = true;
        SetTextValue(IDC_EXPORT_FILE_NAME, suggestedFileName);
        loadingControls_ = false;
    }

    void PrepareDocumentForExport(hlcfg::ProjectDocument& exportDocument, const std::wstring* forcedExportFolder) {
        SyncDocumentFromControls();
        exportDocument = document_;

        if (forcedExportFolder != nullptr) {
            exportDocument.exportSettings.exportFolder = *forcedExportFolder;
        }

        const std::wstring normalizedFileName = hlcfg::EnsureCfgFileName(exportDocument.exportSettings.cfgFileName);
        if (hlcfg::Trimmed(exportDocument.exportSettings.cfgFileName).empty() ||
            normalizedFileName == hlcfg::CreateDefaultProject().exportSettings.cfgFileName ||
            normalizedFileName == lastSuggestedCfgFileName_) {
            exportDocument.exportSettings.cfgFileName = BuildSuggestedCfgFileName(exportDocument);
        } else {
            exportDocument.exportSettings.cfgFileName = normalizedFileName;
        }
    }

    bool ConfirmOverwrite(const std::wstring& exportPath, const wchar_t* actionLabel) const {
        std::error_code error;
        if (!std::filesystem::exists(exportPath, error) || error) {
            return true;
        }

        std::wstring message = std::wstring(actionLabel) + L" will overwrite:\n" + exportPath + L"\n\nContinue?";
        return MessageBoxW(hwnd_, message.c_str(), kWindowTitle, MB_ICONWARNING | MB_YESNO) == IDYES;
    }

    void ShowActionError(
        const wchar_t* actionLabel,
        const std::wstring& primaryMessage,
        const std::wstring& attemptedPath,
        const std::wstring& fixHint) {
        std::wstring message = std::wstring(actionLabel) + L" failed.\n\n" + primaryMessage;
        if (!attemptedPath.empty()) {
            message += L"\n\nTarget path:\n" + attemptedPath;
        }
        if (!fixHint.empty()) {
            message += L"\n\nHow to fix it:\n" + fixHint;
        }

        SetActionStatus(message);
        MessageBoxW(hwnd_, message.c_str(), kWindowTitle, MB_ICONERROR | MB_OK);
    }

    void ShowActionInfo(const std::wstring& statusText, const std::wstring& dialogText) {
        SetActionStatus(statusText);
        MessageBoxW(hwnd_, dialogText.c_str(), kWindowTitle, MB_ICONINFORMATION | MB_OK);
    }

    bool RunExportWorkflow(const std::wstring* forcedExportFolder, bool quickExport) {
        hlcfg::ProjectDocument exportDocument;
        PrepareDocumentForExport(exportDocument, forcedExportFolder);

        const wchar_t* actionLabel = quickExport ? L"Quick Export to Live Mod" : L"Export to chosen folder";
        hlcfg::ExportResult preview;
        std::wstring errorMessage;
        if (!hlcfg::BuildExportResult(exportDocument, environment_, preview, errorMessage)) {
            RefreshExportPreview(true);
            ShowActionError(
                actionLabel,
                errorMessage,
                quickExport ? BuildQuickExportTargetPathPreview() : std::wstring(),
                quickExport ? L"Verify the Half-Life root, the live mod path, and the cfg file name."
                            : L"Choose a writable folder and confirm the cfg file name.");
            return false;
        }

        if (!ConfirmOverwrite(preview.exportPath, actionLabel)) {
            SetActionStatus(std::wstring(actionLabel) + L" was cancelled.\n\nTarget path:\n" + preview.exportPath);
            return false;
        }

        hlcfg::ExportResult result;
        if (!hlcfg::ExportCfgToFile(exportDocument, environment_, result, errorMessage)) {
            RefreshExportPreview(true);
            ShowActionError(
                actionLabel,
                errorMessage,
                preview.exportPath,
                L"Check that the target folder exists and is writable, then try again.");
            return false;
        }

        std::error_code verifyError;
        if (!std::filesystem::exists(result.exportPath, verifyError) || verifyError) {
            RefreshExportPreview(true);
            ShowActionError(
                actionLabel,
                L"The cfg export completed, but the written file could not be confirmed on disk.",
                result.exportPath,
                L"Check file permissions, confirm the target folder, and try again.");
            return false;
        }

        document_ = std::move(exportDocument);
        document_.exportSettings.exportFolder = std::filesystem::path(result.exportPath).parent_path().wstring();
        document_.exportSettings.cfgFileName = std::filesystem::path(result.exportPath).filename().wstring();
        LoadDocumentToControls();
        dirty_ = true;
        UpdateWindowTitle();
        ReloadBrowserEntries();
        RefreshBrowserControls();

        std::wstring successMessage = (quickExport ? L"Quick-exported cfg to:\n" : L"Exported cfg to:\n") + result.exportPath +
                                      L"\n\nLoad it in HLDS with:\n" + result.execCommand;
        if (!result.matchPackPath.empty()) {
            successMessage += L"\n\nWrote match pack metadata to:\n" + result.matchPackPath;
        }
        if (!result.launcherCommand.empty()) {
            successMessage += L"\n\nLaunch it directly with:\n" + result.launcherCommand;
        }

        std::wstring statusText = std::wstring(actionLabel) + L" succeeded.\n\nWritten cfg:\n" + result.exportPath +
                                  L"\n\nExec command:\n" + result.execCommand;
        if (!result.matchPackPath.empty()) {
            statusText += L"\n\nMatch pack metadata:\n" + result.matchPackPath;
        }
        if (!result.launcherCommand.empty()) {
            statusText += L"\n\nLauncher command:\n" + result.launcherCommand;
        }

        ShowActionInfo(statusText, successMessage);
        return true;
    }

    HWND FindControl(int controlId) const {
        if (HWND control = GetDlgItem(hwnd_, controlId)) {
            return control;
        }

        for (HWND page : pages_) {
            if (page != nullptr) {
                if (HWND control = GetDlgItem(page, controlId)) {
                    return control;
                }
            }
        }

        return nullptr;
    }

    void SetCheckValue(int controlId, bool checked) {
        CheckDlgButton(GetParent(FindControl(controlId)), controlId, checked ? BST_CHECKED : BST_UNCHECKED);
    }

    bool GetCheckValue(int controlId) const {
        HWND control = FindControl(controlId);
        return control != nullptr && Button_GetCheck(control) == BST_CHECKED;
    }

    void SetTextValue(int controlId, const std::wstring& value) {
        if (HWND control = FindControl(controlId)) {
            SetTextOnWindow(control, value);
        }
    }

    std::wstring GetTextValue(int controlId) const {
        HWND control = FindControl(controlId);
        return control == nullptr ? std::wstring() : GetTextFromWindow(control);
    }

    void SetActionStatus(const std::wstring& value) {
        lastActionStatus_ = value;
        const bool wasLoadingControls = loadingControls_;
        loadingControls_ = true;
        SetTextValue(IDC_EXPORT_STATUS, value);
        loadingControls_ = wasLoadingControls;
    }

    std::wstring GetHalfLifeRootFolder() const {
        if (!environment_.liveModRoot.empty()) {
            return std::filesystem::path(environment_.liveModRoot).parent_path().wstring();
        }

        return {};
    }

    std::wstring GetEffectiveCfgFileNameForQuickExport() const {
        std::wstring fileName = hlcfg::EnsureCfgFileName(GetTextValue(IDC_EXPORT_FILE_NAME));
        if (!hlcfg::Trimmed(fileName).empty()) {
            return fileName;
        }

        if (!lastSuggestedCfgFileName_.empty()) {
            return lastSuggestedCfgFileName_;
        }

        return BuildSuggestedCfgFileName(document_);
    }

    std::wstring BuildQuickExportTargetPathPreview() const {
        const std::wstring liveModFolder = GetLiveModFolder();
        if (liveModFolder.empty()) {
            return L"(live mod root unresolved; set HL_EXE or HLDS_EXE, or use Export to chosen folder)";
        }

        return (std::filesystem::path(liveModFolder) / GetEffectiveCfgFileNameForQuickExport()).wstring();
    }

    void RefreshResolvedExportInfo() {
        const bool wasLoadingControls = loadingControls_;
        loadingControls_ = true;

        const std::wstring halfLifeRoot = GetHalfLifeRootFolder();
        SetTextValue(IDC_HALF_LIFE_ROOT_PREVIEW, halfLifeRoot.empty() ? L"(not resolved; using staged fallback when needed)" : halfLifeRoot);

        const std::wstring liveModFolder = GetLiveModFolder();
        SetTextValue(IDC_LIVE_MOD_FOLDER_PREVIEW, liveModFolder.empty() ? L"(not resolved)" : liveModFolder);
        SetTextValue(IDC_QUICK_EXPORT_TARGET_PREVIEW, BuildQuickExportTargetPathPreview());
        SetTextValue(IDC_EDITOR_EXE_PATH_PREVIEW, moduleFilePath_.empty() ? L"(unknown)" : moduleFilePath_);

        loadingControls_ = wasLoadingControls;
    }

    void SetWeaponSelection(const std::wstring& weapon) {
        HWND control = FindControl(IDC_WEAPON_UNDER_TEST);
        if (control == nullptr) {
            return;
        }

        const int itemCount = ComboBox_GetCount(control);
        for (int index = 0; index < itemCount; ++index) {
            wchar_t buffer[64];
            ComboBox_GetLBText(control, index, buffer);
            if (weapon == buffer) {
                ComboBox_SetCurSel(control, index);
                return;
            }
        }

        ComboBox_SetCurSel(control, 0);
    }

    void SetComboSelectionValue(int controlId, const std::wstring& value) {
        HWND control = FindControl(controlId);
        if (control == nullptr) {
            return;
        }

        const int itemCount = ComboBox_GetCount(control);
        for (int index = 0; index < itemCount; ++index) {
            wchar_t buffer[128];
            ComboBox_GetLBText(control, index, buffer);
            if (value == buffer) {
                ComboBox_SetCurSel(control, index);
                return;
            }
        }

        if (itemCount > 0) {
            ComboBox_SetCurSel(control, 0);
        }
    }

    std::wstring GetWeaponSelection() const {
        HWND control = FindControl(IDC_WEAPON_UNDER_TEST);
        if (control == nullptr) {
            return {};
        }

        const int index = ComboBox_GetCurSel(control);
        if (index == CB_ERR) {
            return {};
        }

        wchar_t buffer[64];
        ComboBox_GetLBText(control, index, buffer);
        return buffer;
    }

    std::wstring GetComboSelectionValue(int controlId) const {
        HWND control = FindControl(controlId);
        if (control == nullptr) {
            return {};
        }

        const int index = ComboBox_GetCurSel(control);
        if (index == CB_ERR) {
            return {};
        }

        wchar_t buffer[128];
        ComboBox_GetLBText(control, index, buffer);
        return buffer;
    }

    std::wstring BuildMatchSummaryText() const {
        hlcfg::ProjectDocument preview = document_;
        preview.matchPack.name = GetTextValue(IDC_MATCH_PACK_NAME);
        preview.matchPack.description = GetTextValue(IDC_MATCH_PACK_DESCRIPTION);
        preview.matchPack.tags = GetTextValue(IDC_MATCH_PACK_TAGS);
        preview.general.weaponUnderTest = GetWeaponSelection();
        preview.glock.tapFire = GetCheckValue(IDC_GLOCK_TAP_FIRE);
        preview.glock.firstShotAccuracy = GetCheckValue(IDC_GLOCK_FIRST_SHOT_ACCURACY);
        preview.glock.spreadRecovery = GetTextValue(IDC_GLOCK_SPREAD_RECOVERY);
        preview.glock.primaryShotGrowth = GetTextValue(IDC_GLOCK_PRIMARY_SHOT_GROWTH);
        preview.glock.cadenceMode = GetCheckValue(IDC_GLOCK_PRIMARY_CADENCE_MODE);
        preview.glock.primaryCadenceCycleTime = GetTextValue(IDC_GLOCK_PRIMARY_CYCLE_TIME);
        preview.glock.primaryClickPenalty = GetTextValue(IDC_GLOCK_PRIMARY_CLICK_PENALTY);
        preview.glock.primaryClickPenaltyScale = GetTextValue(IDC_GLOCK_PRIMARY_CLICK_PENALTY_SCALE);
        preview.glock.primaryClickResetTime = GetTextValue(IDC_GLOCK_PRIMARY_CLICK_RESET_TIME);
        preview.glock.primaryHoldPenaltyScale = GetTextValue(IDC_GLOCK_PRIMARY_HOLD_PENALTY_SCALE);
        preview.glock.patternMode = GetCheckValue(IDC_GLOCK_PATTERN_MODE);
        preview.glock.patternResetTime = GetTextValue(IDC_GLOCK_PATTERN_RESET_TIME);
        preview.weapon357.primaryEnabled = GetCheckValue(IDC_357_PRIMARY_ENABLED);
        preview.weapon357.primaryFirstShotAccuracy = GetCheckValue(IDC_357_PRIMARY_FIRST_SHOT_ACCURACY);
        preview.weapon357.primarySpreadRecovery = GetTextValue(IDC_357_PRIMARY_SPREAD_RECOVERY);
        preview.weapon357.primaryCadenceCycleTime = GetTextValue(IDC_357_PRIMARY_CYCLE_TIME);
        preview.weapon357.primaryClickPenalty = GetTextValue(IDC_357_PRIMARY_CLICK_PENALTY);
        preview.weapon357.primaryClickPenaltyScale = GetTextValue(IDC_357_PRIMARY_CLICK_PENALTY_SCALE);
        preview.weapon357.primaryClickResetTime = GetTextValue(IDC_357_PRIMARY_CLICK_RESET_TIME);
        preview.weapon357.primaryHoldPenaltyScale = GetTextValue(IDC_357_PRIMARY_HOLD_PENALTY_SCALE);
        preview.weapon357.cadenceMode = GetCheckValue(IDC_357_PRIMARY_CADENCE_MODE);
        preview.weapon357.patternMode = GetCheckValue(IDC_357_PATTERN_MODE);
        preview.weapon357.patternResetTime = GetTextValue(IDC_357_PATTERN_RESET_TIME);
        preview.mp5.primaryFirstShotAccuracy = GetCheckValue(IDC_MP5_PRIMARY_FIRST_SHOT_ACCURACY);
        preview.mp5.primaryBurstGrowth = GetTextValue(IDC_MP5_PRIMARY_BURST_GROWTH);
        preview.mp5.primarySpreadRecovery = GetTextValue(IDC_MP5_PRIMARY_SPREAD_RECOVERY);
        preview.mp5.patternMode = GetCheckValue(IDC_MP5_PATTERN_MODE);
        preview.mp5.patternResetTime = GetTextValue(IDC_MP5_PATTERN_RESET_TIME);
        preview.shotgun.primaryFirstShotAccuracy = GetCheckValue(IDC_SHOTGUN_PRIMARY_FIRST_SHOT_ACCURACY);
        preview.shotgun.primarySpreadRecovery = GetTextValue(IDC_SHOTGUN_PRIMARY_SPREAD_RECOVERY);
        preview.shotgun.primaryShotGrowth = GetTextValue(IDC_SHOTGUN_PRIMARY_SHOT_GROWTH);
        preview.shotgun.patternMode = GetCheckValue(IDC_SHOTGUN_PATTERN_MODE);
        preview.shotgun.patternResetTime = GetTextValue(IDC_SHOTGUN_PATTERN_RESET_TIME);
        preview.shotgun.primaryPelletSpreadMode = GetComboSelectionValue(IDC_SHOTGUN_PRIMARY_PELLET_SPREAD_MODE);
        preview.roundMode.enabled = GetCheckValue(IDC_ROUND_MODE);
        preview.roundMode.loadoutMode = GetComboSelectionValue(IDC_ROUND_LOADOUT_MODE);
        preview.roundMode.weaponProfile = GetTextValue(IDC_ROUND_WEAPON_PROFILE);
        preview.teamRound.enabled = GetCheckValue(IDC_TEAM_ROUND_MODE);
        preview.teamRound.team1Loadout = GetComboSelectionValue(IDC_TEAM_ROUND_TEAM1_LOADOUT);
        preview.teamRound.team2Loadout = GetComboSelectionValue(IDC_TEAM_ROUND_TEAM2_LOADOUT);
        preview.buy.enabled = GetCheckValue(IDC_BUY_MODE);
        preview.armorEquipment.armorMode = GetCheckValue(IDC_ARMOR_MODE);
        preview.armorEquipment.helmetMode = GetCheckValue(IDC_HELMET_MODE);

        std::wstring primaryLoadout = hlcfg::Trimmed(preview.roundMode.loadoutMode);
        if (preview.teamRound.enabled) {
            const std::wstring team1Loadout = hlcfg::Trimmed(preview.teamRound.team1Loadout);
            const std::wstring team2Loadout = hlcfg::Trimmed(preview.teamRound.team2Loadout);
            if (!team1Loadout.empty() || !team2Loadout.empty()) {
                primaryLoadout = team1Loadout + L" / " + team2Loadout;
            }
        }

        if (primaryLoadout.empty()) {
            primaryLoadout = L"(server default)";
        }

        const std::wstring weaponProfile = hlcfg::Trimmed(preview.roundMode.weaponProfile).empty()
                                               ? L"(none)"
                                               : hlcfg::Trimmed(preview.roundMode.weaponProfile);
        const std::wstring packName = hlcfg::Trimmed(preview.matchPack.name).empty()
                                          ? L"(none)"
                                          : hlcfg::Trimmed(preview.matchPack.name);
        const std::wstring weaponUnderTest = hlcfg::Trimmed(preview.general.weaponUnderTest);
        std::wstring weaponFeel = L"(no weapon feel summary)";
        if (weaponUnderTest == L"glock") {
            const double shotGrowth = ParseConfigDouble(preview.glock.primaryShotGrowth, 0.0);
            const double recoverySeconds = ParseConfigDouble(preview.glock.spreadRecovery, 0.0);
            const double cadenceCycleSeconds = ParseConfigDouble(preview.glock.primaryCadenceCycleTime, 0.0);
            const double cadencePenalty = ParseConfigDouble(preview.glock.primaryClickPenalty, 0.0);
            const double holdPenaltyScale = ParseConfigDouble(preview.glock.primaryHoldPenaltyScale, 0.0);
            const double patternResetSeconds = ParseConfigDouble(preview.glock.patternResetTime, 0.0);
            weaponFeel = preview.glock.tapFire
                             ? L"legacy tap-fire gated"
                             : (preview.glock.firstShotAccuracy ? L"single-shot favored" : L"first-shot neutral");
            if (preview.glock.cadenceMode) {
                weaponFeel += L", cadence-sensitive";
            }
            if (preview.glock.patternMode) {
                weaponFeel += L", learnable follow-up pattern";
            }
            if (shotGrowth >= 0.020) {
                weaponFeel += L", spam punished strongly";
            } else if (shotGrowth >= 0.012) {
                weaponFeel += L", soft cadence growth";
            }
            if (preview.glock.cadenceMode && cadenceCycleSeconds >= 0.400) {
                weaponFeel += L", patient clicks rewarded";
            }
            if (preview.glock.cadenceMode && cadencePenalty >= 0.035) {
                weaponFeel += L", fast spam punished";
            }
            if (preview.glock.cadenceMode && holdPenaltyScale >= 0.450) {
                weaponFeel += L", hold-fire weakened";
            }
            if (recoverySeconds > 0.0 && recoverySeconds <= 0.360) {
                weaponFeel += L", quick settle";
            } else if (recoverySeconds >= 0.500) {
                weaponFeel += L", patient pacing rewarded";
            }
            if (preview.glock.patternMode && patternResetSeconds > 0.0 && patternResetSeconds <= 0.400) {
                weaponFeel += L", pattern resets quickly after a pause";
            }
        } else if (weaponUnderTest == L"357") {
            const double recoverySeconds = ParseConfigDouble(preview.weapon357.primarySpreadRecovery, 0.0);
            const double cadenceCycleSeconds = ParseConfigDouble(preview.weapon357.primaryCadenceCycleTime, 0.0);
            const double cadencePenalty = ParseConfigDouble(preview.weapon357.primaryClickPenalty, 0.0);
            const double holdPenaltyScale = ParseConfigDouble(preview.weapon357.primaryHoldPenaltyScale, 0.0);
            const double patternResetSeconds = ParseConfigDouble(preview.weapon357.patternResetTime, 0.0);
            weaponFeel = preview.weapon357.primaryFirstShotAccuracy ? L"precision favored" : L"first-shot neutral";
            if (preview.weapon357.cadenceMode) {
                weaponFeel += L", cadence-sensitive";
            }
            if (preview.weapon357.patternMode) {
                weaponFeel += L", deterministic follow-ups";
            }
            if (preview.weapon357.cadenceMode && cadenceCycleSeconds >= 1.000) {
                weaponFeel += L", deliberate clicks rewarded";
            }
            if (preview.weapon357.cadenceMode && cadencePenalty >= 0.040) {
                weaponFeel += L", rapid clicks punished";
            }
            if (preview.weapon357.cadenceMode && holdPenaltyScale >= 0.350) {
                weaponFeel += L", hold-fire softened";
            }
            if (preview.weapon357.patternMode && patternResetSeconds > 0.0 && patternResetSeconds <= 1.100) {
                weaponFeel += L", pattern resets after a patient pause";
            }
            if (recoverySeconds > 0.0 && recoverySeconds >= 0.650) {
                weaponFeel += L", recovery requires patience";
            }
        } else if (weaponUnderTest == L"mp5") {
            const double burstGrowth = ParseConfigDouble(preview.mp5.primaryBurstGrowth, 0.0);
            const double recoverySeconds = ParseConfigDouble(preview.mp5.primarySpreadRecovery, 0.0);
            const double burstResetSeconds = ParseConfigDouble(preview.mp5.primaryBurstResetTime, 0.0);
            const double holdPenaltyScale = ParseConfigDouble(preview.mp5.primaryHoldPenaltyScale, 0.0);
            const double patternResetSeconds = ParseConfigDouble(preview.mp5.patternResetTime, 0.0);
            weaponFeel = preview.mp5.primaryFirstShotAccuracy ? L"burst favored" : L"spray heavy";
            if (preview.mp5.patternMode) {
                weaponFeel += L", learnable early burst";
            }
            if (burstGrowth >= 0.011) {
                weaponFeel += L", short bursts beat spray";
            } else if (burstGrowth <= 0.007) {
                weaponFeel += L", softer bloom";
            }
            if (recoverySeconds > 0.0 && recoverySeconds <= 0.320) {
                weaponFeel += L", quick reset";
            } else if (recoverySeconds >= 0.420) {
                weaponFeel += L", recoil settles slowly";
            }
            if (burstResetSeconds > 0.0 && burstResetSeconds <= 0.320) {
                weaponFeel += L", burst windows reset quickly";
            } else if (burstResetSeconds >= 0.500) {
                weaponFeel += L", sustained spray lingers";
            }
            if (holdPenaltyScale >= 0.900) {
                weaponFeel += L", long spray punished hard";
            } else if (holdPenaltyScale <= 0.450) {
                weaponFeel += L", spray stays softer";
            }
            if (preview.mp5.patternMode && patternResetSeconds > 0.0 && patternResetSeconds <= 0.320) {
                weaponFeel += L", burst pattern resets quickly";
            }
        } else if (weaponUnderTest == L"shotgun") {
            const double shotGrowth = ParseConfigDouble(preview.shotgun.primaryShotGrowth, 0.0);
            const double recoverySeconds = ParseConfigDouble(preview.shotgun.primarySpreadRecovery, 0.0);
            const double patternResetSeconds = ParseConfigDouble(preview.shotgun.patternResetTime, 0.0);
            weaponFeel = preview.shotgun.primaryFirstShotAccuracy ? L"first shot centered" : L"wide default cone";
            if (preview.shotgun.patternMode) {
                weaponFeel += L", learnable pellet layout";
            }
            if (hlcfg::Trimmed(preview.shotgun.primaryPelletSpreadMode) == L"1") {
                weaponFeel += L", deterministic pellet spread";
            }
            if (shotGrowth >= 0.020) {
                weaponFeel += L", quick follow-ups widen hard";
            } else if (shotGrowth >= 0.010) {
                weaponFeel += L", soft repeated-shot bloom";
            }
            if (recoverySeconds > 0.0 && recoverySeconds <= 0.750) {
                weaponFeel += L", settles between deliberate shots";
            }
            if (preview.shotgun.patternMode && patternResetSeconds > 0.0 && patternResetSeconds <= 1.000) {
                weaponFeel += L", pattern resets after a pause";
            }
        }

        std::wstring summary;
        summary += L"Match pack: ";
        summary += packName;
        summary += L"\r\n";
        summary += L"Round mode: ";
        summary += preview.roundMode.enabled ? L"enabled" : L"off";
        summary += L"\r\nTeam round mode: ";
        summary += preview.teamRound.enabled ? L"enabled" : L"off";
        summary += L"\r\nBuy mode: ";
        summary += preview.buy.enabled ? L"enabled" : L"off";
        summary += L"\r\nMain loadout: ";
        summary += primaryLoadout;
        summary += L"\r\nWeapon under test: ";
        summary += weaponUnderTest.empty() ? L"(unset)" : weaponUnderTest;
        summary += L"\r\nWeapon feel: ";
        summary += weaponFeel;
        summary += L"\r\nRound weapon profile: ";
        summary += weaponProfile;
        summary += L"\r\nArmor: ";
        summary += preview.armorEquipment.armorMode ? L"enabled" : L"off";
        summary += L"\r\nHelmet: ";
        summary += preview.armorEquipment.helmetMode ? L"enabled" : L"off";
        return summary;
    }

    void RefreshMatchSummary() {
        if (HWND control = FindControl(IDC_MATCH_SUMMARY)) {
            const bool wasLoadingControls = loadingControls_;
            loadingControls_ = true;
            SetTextOnWindow(control, BuildMatchSummaryText());
            loadingControls_ = wasLoadingControls;
        }
    }

    void SetBrowserStatus(const std::wstring& value) {
        lastBrowserStatus_ = value;
        if (HWND control = FindControl(IDC_BROWSER_STATUS)) {
            const bool wasLoadingControls = loadingControls_;
            loadingControls_ = true;
            SetTextOnWindow(control, value.c_str());
            loadingControls_ = wasLoadingControls;
        }
    }

    std::wstring GetPresetBrowserCategory() const {
        std::wstring category = GetComboSelectionValue(IDC_BROWSER_PRESET_CATEGORY);
        if (category.empty()) {
            category = L"all";
        }
        return category;
    }

    void ReloadBrowserEntries() {
        presetBrowserEntries_.clear();
        matchPackBrowserEntries_.clear();

        const std::filesystem::path repoRoot(environment_.repoRoot);
        const std::pair<const wchar_t*, const wchar_t*> presetRoots[] = {
            {L"glock", L"glock-presets"},
            {L"mp5", L"mp5-presets"},
            {L"357", L"357-presets"},
            {L"shotgun", L"shotgun-presets"},
        };

        std::wstring ignoredError;
        for (const auto& [category, folderName] : presetRoots) {
            const std::filesystem::path folder = repoRoot / L"configs" / folderName;
            std::error_code error;
            if (!std::filesystem::exists(folder, error)) {
                continue;
            }

            for (const auto& entry : std::filesystem::directory_iterator(folder, error)) {
                if (error || !entry.is_regular_file() || entry.path().extension() != L".json") {
                    continue;
                }

                BrowserEntry browserEntry;
                if (LoadWeaponPresetEntry(entry.path(), category, browserEntry, ignoredError)) {
                    presetBrowserEntries_.push_back(std::move(browserEntry));
                }
            }
        }

        std::sort(presetBrowserEntries_.begin(), presetBrowserEntries_.end(), [](const BrowserEntry& left, const BrowserEntry& right) {
            if (left.category != right.category) {
                return left.category < right.category;
            }

            return left.name < right.name;
        });

        std::map<std::wstring, BrowserEntry> matchPackByName;
        auto loadMatchPackFolder = [&](const std::filesystem::path& folder) {
            std::error_code error;
            if (!std::filesystem::exists(folder, error)) {
                return;
            }

            for (const auto& entry : std::filesystem::directory_iterator(folder, error)) {
                if (error || !entry.is_regular_file() || entry.path().extension() != L".json") {
                    continue;
                }

                BrowserEntry browserEntry;
                if (!LoadMatchPackEntry(entry.path(), environment_, browserEntry, ignoredError)) {
                    continue;
                }

                std::wstring key = browserEntry.name.empty() ? entry.path().stem().wstring() : browserEntry.name;
                std::transform(key.begin(), key.end(), key.begin(), [](wchar_t ch) {
                    return static_cast<wchar_t>(::towlower(ch));
                });
                matchPackByName[key] = std::move(browserEntry);
            }
        };

        loadMatchPackFolder(repoRoot / L"configs" / L"match-packs");
        if (!environment_.matchPacksRoot.empty()) {
            loadMatchPackFolder(std::filesystem::path(environment_.matchPacksRoot));
        }

        for (auto& [key, value] : matchPackByName) {
            (void)key;
            matchPackBrowserEntries_.push_back(std::move(value));
        }

        std::sort(matchPackBrowserEntries_.begin(), matchPackBrowserEntries_.end(), [](const BrowserEntry& left, const BrowserEntry& right) {
            return left.name < right.name;
        });
    }

    void PopulatePresetBrowserList() {
        HWND list = FindControl(IDC_BROWSER_PRESET_LIST);
        if (list == nullptr) {
            return;
        }

        const BrowserEntryKind previousKind = activeBrowserEntryKind_;
        std::wstring previousName;
        if (activeBrowserEntryKind_ == BrowserEntryKind::WeaponPreset) {
            if (const BrowserEntry* active = GetActiveBrowserEntry()) {
                previousName = active->name;
            }
        }

        presetBrowserVisibleIndices_.clear();
        ListBox_ResetContent(list);

        const std::wstring category = GetPresetBrowserCategory();
        for (std::size_t index = 0; index < presetBrowserEntries_.size(); ++index) {
            const BrowserEntry& entry = presetBrowserEntries_[index];
            if (category != L"all" && entry.category != category) {
                continue;
            }

            std::wstring label = entry.name;
            if (category == L"all" && !entry.category.empty()) {
                label += L" [" + entry.category + L"]";
            }

            const int row = static_cast<int>(SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str())));
            if (row >= 0) {
                SendMessageW(list, LB_SETITEMDATA, row, static_cast<LPARAM>(index));
                presetBrowserVisibleIndices_.push_back(index);
            }
        }

        int selection = LB_ERR;
        for (int row = 0; row < static_cast<int>(presetBrowserVisibleIndices_.size()); ++row) {
            const BrowserEntry& entry = presetBrowserEntries_[presetBrowserVisibleIndices_[static_cast<std::size_t>(row)]];
            if (!previousName.empty() && entry.name == previousName) {
                selection = row;
                break;
            }
        }

        if (selection == LB_ERR && !presetBrowserVisibleIndices_.empty()) {
            selection = 0;
        }

        if (selection != LB_ERR && (previousKind == BrowserEntryKind::WeaponPreset || previousKind == BrowserEntryKind::None)) {
            SendMessageW(list, LB_SETCURSEL, selection, 0);
            activeBrowserEntryKind_ = BrowserEntryKind::WeaponPreset;
            activeBrowserEntryIndex_ = presetBrowserVisibleIndices_[static_cast<std::size_t>(selection)];
        } else if (activeBrowserEntryKind_ == BrowserEntryKind::WeaponPreset) {
            activeBrowserEntryKind_ = BrowserEntryKind::None;
            activeBrowserEntryIndex_ = 0;
        }
    }

    void PopulateMatchPackBrowserList() {
        HWND list = FindControl(IDC_BROWSER_MATCH_PACK_LIST);
        if (list == nullptr) {
            return;
        }

        const BrowserEntryKind previousKind = activeBrowserEntryKind_;
        std::wstring previousName;
        if (activeBrowserEntryKind_ == BrowserEntryKind::MatchPack) {
            if (const BrowserEntry* active = GetActiveBrowserEntry()) {
                previousName = active->name;
            }
        }

        ListBox_ResetContent(list);
        for (std::size_t index = 0; index < matchPackBrowserEntries_.size(); ++index) {
            const BrowserEntry& entry = matchPackBrowserEntries_[index];
            std::wstring label = entry.name;
            if (!entry.category.empty()) {
                label += L" [" + entry.category + L"]";
            }

            const int row = static_cast<int>(SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str())));
            if (row >= 0) {
                SendMessageW(list, LB_SETITEMDATA, row, static_cast<LPARAM>(index));
            }
        }

        int selection = LB_ERR;
        for (int row = 0; row < static_cast<int>(matchPackBrowserEntries_.size()); ++row) {
            const BrowserEntry& entry = matchPackBrowserEntries_[static_cast<std::size_t>(row)];
            if (!previousName.empty() && entry.name == previousName) {
                selection = row;
                break;
            }
        }

        if (selection == LB_ERR && !matchPackBrowserEntries_.empty()) {
            selection = 0;
        }

        if (selection != LB_ERR && (previousKind == BrowserEntryKind::MatchPack || previousKind == BrowserEntryKind::None)) {
            SendMessageW(list, LB_SETCURSEL, selection, 0);
            activeBrowserEntryKind_ = BrowserEntryKind::MatchPack;
            activeBrowserEntryIndex_ = static_cast<std::size_t>(selection);
        } else if (activeBrowserEntryKind_ == BrowserEntryKind::MatchPack) {
            activeBrowserEntryKind_ = BrowserEntryKind::None;
            activeBrowserEntryIndex_ = 0;
        }
    }

    const BrowserEntry* GetActiveBrowserEntry() const {
        switch (activeBrowserEntryKind_) {
        case BrowserEntryKind::WeaponPreset:
            return activeBrowserEntryIndex_ < presetBrowserEntries_.size() ? &presetBrowserEntries_[activeBrowserEntryIndex_] : nullptr;
        case BrowserEntryKind::MatchPack:
            return activeBrowserEntryIndex_ < matchPackBrowserEntries_.size() ? &matchPackBrowserEntries_[activeBrowserEntryIndex_] : nullptr;
        default:
            return nullptr;
        }
    }

    void CapturePresetBrowserSelection() {
        HWND list = FindControl(IDC_BROWSER_PRESET_LIST);
        if (list == nullptr) {
            return;
        }

        const int row = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
        if (row == LB_ERR) {
            return;
        }

        const LRESULT itemData = SendMessageW(list, LB_GETITEMDATA, row, 0);
        if (itemData == LB_ERR) {
            return;
        }

        activeBrowserEntryKind_ = BrowserEntryKind::WeaponPreset;
        activeBrowserEntryIndex_ = static_cast<std::size_t>(itemData);
    }

    void CaptureMatchPackBrowserSelection() {
        HWND list = FindControl(IDC_BROWSER_MATCH_PACK_LIST);
        if (list == nullptr) {
            return;
        }

        const int row = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
        if (row == LB_ERR) {
            return;
        }

        const LRESULT itemData = SendMessageW(list, LB_GETITEMDATA, row, 0);
        if (itemData == LB_ERR) {
            return;
        }

        activeBrowserEntryKind_ = BrowserEntryKind::MatchPack;
        activeBrowserEntryIndex_ = static_cast<std::size_t>(itemData);
    }

    std::wstring BuildBrowserDetailsText(const BrowserEntry* entry) {
        if (entry == nullptr) {
            return L"Select a preset or match pack to preview it inside the editor.";
        }

        std::wstring details;
        details += L"Name: " + entry->name + L"\r\n";
        details += L"Type: " + std::wstring(entry->kind == BrowserEntryKind::WeaponPreset ? L"weapon preset" : L"match pack") + L"\r\n";
        if (!entry->category.empty()) {
            details += L"Category: " + entry->category + L"\r\n";
        }
        if (!entry->description.empty()) {
            details += L"Description: " + entry->description + L"\r\n";
        }
        if (!entry->tags.empty()) {
            details += L"Tags: " + entry->tags + L"\r\n";
        }
        if (!entry->notes.empty()) {
            details += L"Notes: " + entry->notes + L"\r\n";
        }
        if (!entry->sourcePath.empty()) {
            details += L"Source: " + entry->sourcePath + L"\r\n";
        }
        if (!entry->referencedCfgPath.empty()) {
            details += L"Referenced cfg: " + entry->referencedCfgPath + L"\r\n";
        }
        details += L"Cvar count: " + std::to_wstring(entry->cvars.size()) + L"\r\n";

        int previewCount = 0;
        for (const auto& [name, value] : entry->cvars) {
            if (previewCount++ >= 10) {
                details += L"...";
                break;
            }

            details += name + L" = " + value + L"\r\n";
        }

        return details;
    }

    std::wstring BuildBrowserDiffText(const BrowserEntry* entry) {
        if (entry == nullptr) {
            return L"Diff preview appears here after selecting a preset or match pack.";
        }

        SyncDocumentFromControls();
        const hlcfg::CvarMap currentCvars = hlcfg::BuildKnownCvarMap(document_);

        std::wstring diff;
        int changedCount = 0;
        int unchangedCount = 0;
        int unknownCount = 0;
        for (const auto& [name, incomingValue] : entry->cvars) {
            const auto currentIt = currentCvars.find(name);
            const bool known = currentIt != currentCvars.end();
            const std::wstring currentValue = known ? currentIt->second : L"(not represented in the editor)";
            if (!known) {
                ++unknownCount;
            }

            if (currentValue == incomingValue) {
                ++unchangedCount;
                continue;
            }

            ++changedCount;
            diff += name + L"\r\n";
            diff += L"  current : " + currentValue + L"\r\n";
            diff += L"  incoming: " + incomingValue + L"\r\n\r\n";
        }

        if (changedCount == 0) {
            diff = L"The selected entry already matches the current project for all represented cvars.";
            if (unchangedCount > 0) {
                diff += L"\r\n\r\nMatched cvars: " + std::to_wstring(unchangedCount);
            }
        } else {
            diff += L"Changed cvars: " + std::to_wstring(changedCount);
            diff += L"\r\nMatched cvars: " + std::to_wstring(unchangedCount);
        }

        if (unknownCount > 0) {
            diff += L"\r\nUnknown-to-editor cvars: " + std::to_wstring(unknownCount);
        }

        return diff;
    }

    bool BuildBrowserApplyCommand(std::wstring& command, std::wstring& details) {
        const std::wstring liveModFolder = GetLiveModFolder();
        if (liveModFolder.empty()) {
            details = L"Live mod root is unresolved. Quick export first needs a valid Half-Life root.";
            return false;
        }

        hlcfg::ProjectDocument exportDocument;
        PrepareDocumentForExport(exportDocument, &liveModFolder);

        hlcfg::ExportResult result;
        std::wstring errorMessage;
        if (!hlcfg::BuildExportResult(exportDocument, environment_, result, errorMessage)) {
            details = errorMessage;
            return false;
        }

        if (!hlcfg::Trimmed(exportDocument.matchPack.name).empty() && !result.matchPackPath.empty()) {
            command = L"exp_matchcfg_apply " + hlcfg::Trimmed(exportDocument.matchPack.name);
            details = L"Quick export will write cfg plus match-pack sidecar.";
            return true;
        }

        command = L"exp_cfg_apply " + std::filesystem::path(result.exportPath).filename().wstring();
        details = L"Quick export will write a plain cfg to the live mod root.";
        return true;
    }

    void RefreshBrowserPanels() {
        const BrowserEntry* entry = GetActiveBrowserEntry();
        const bool wasLoadingControls = loadingControls_;
        loadingControls_ = true;
        SetTextValue(IDC_BROWSER_DETAILS, BuildBrowserDetailsText(entry));
        SetTextValue(IDC_BROWSER_DIFF, BuildBrowserDiffText(entry));
        std::wstring command;
        std::wstring details;
        if (BuildBrowserApplyCommand(command, details)) {
            SetTextValue(IDC_BROWSER_APPLY_COMMAND, command);
            if (lastBrowserStatus_.empty()) {
                SetTextValue(IDC_BROWSER_STATUS, details);
            }
        } else {
            SetTextValue(IDC_BROWSER_APPLY_COMMAND, L"(quick export target unresolved)");
            if (lastBrowserStatus_.empty()) {
                SetTextValue(IDC_BROWSER_STATUS, details);
            }
        }
        loadingControls_ = wasLoadingControls;
    }

    void RefreshBrowserControls() {
        PopulatePresetBrowserList();
        PopulateMatchPackBrowserList();
        if (lastBrowserStatus_.empty()) {
            lastBrowserStatus_ = L"Select a preset or match pack, review the diff, then load it into the current project.";
        }
        RefreshBrowserPanels();
        SetBrowserStatus(lastBrowserStatus_);
    }

    bool ConfirmBrowserLoad(const BrowserEntry& entry) {
        if (!dirty_) {
            return true;
        }

        std::wstring message = L"Unsaved changes are present.\n\nLoading ";
        message += entry.name;
        message += L" will merge its cvars into the current project and overwrite matching fields.\n\nContinue?";
        return MessageBoxW(hwnd_, message.c_str(), kWindowTitle, MB_ICONWARNING | MB_YESNO) == IDYES;
    }

    void ApplyBrowserEntryToDocument(const BrowserEntry& entry) {
        SyncDocumentFromControls();

        const hlcfg::ProjectDocument defaultProject = hlcfg::CreateDefaultProject();
        const std::wstring previousSuggested = BuildSuggestedCfgFileName(document_);
        const std::wstring trimmedProjectName = hlcfg::Trimmed(document_.metadata.projectName);
        const std::wstring defaultProjectName = hlcfg::Trimmed(defaultProject.metadata.projectName);
        if (trimmedProjectName.empty() || trimmedProjectName == defaultProjectName) {
            document_.metadata.projectName = entry.name;
        }

        if (entry.kind == BrowserEntryKind::WeaponPreset && !entry.category.empty()) {
            document_.general.weaponUnderTest = entry.category;
        }
        if (entry.kind == BrowserEntryKind::MatchPack) {
            document_.matchPack.name = entry.name;
            document_.matchPack.description = entry.description;
            document_.matchPack.tags = entry.tags;
        }

        std::vector<std::wstring> unknownCvars;
        hlcfg::MergeKnownCvarMap(document_, entry.cvars, &unknownCvars);

        const std::wstring currentFileName = hlcfg::EnsureCfgFileName(document_.exportSettings.cfgFileName);
        if (hlcfg::Trimmed(document_.exportSettings.cfgFileName).empty() ||
            currentFileName == defaultProject.exportSettings.cfgFileName ||
            currentFileName == previousSuggested) {
            document_.exportSettings.cfgFileName = BuildSuggestedCfgFileName(document_);
        }

        LoadDocumentToControls();
        dirty_ = true;
        UpdateWindowTitle();

        std::wstring status = L"Loaded " + entry.name + L" into the current project.";
        if (!unknownCvars.empty()) {
            status += L"\r\nIgnored unknown cvars: " + std::to_wstring(unknownCvars.size());
        }
        SetBrowserStatus(status);
    }

    void LoadPresetBrowserEntry() {
        const BrowserEntry* entry = GetActiveBrowserEntry();
        if (entry == nullptr || entry->kind != BrowserEntryKind::WeaponPreset) {
            SetBrowserStatus(L"Select a weapon preset first.");
            return;
        }

        if (!ConfirmBrowserLoad(*entry)) {
            SetBrowserStatus(L"Preset load cancelled.");
            return;
        }

        ApplyBrowserEntryToDocument(*entry);
    }

    void LoadMatchPackBrowserEntry() {
        const BrowserEntry* entry = GetActiveBrowserEntry();
        if (entry == nullptr || entry->kind != BrowserEntryKind::MatchPack) {
            SetBrowserStatus(L"Select a match pack first.");
            return;
        }

        if (!ConfirmBrowserLoad(*entry)) {
            SetBrowserStatus(L"Match-pack load cancelled.");
            return;
        }

        ApplyBrowserEntryToDocument(*entry);
    }

    void OpenPresetBrowserFolder() {
        const BrowserEntry* entry = GetActiveBrowserEntry();
        if (entry != nullptr && entry->kind == BrowserEntryKind::WeaponPreset && !entry->sourcePath.empty()) {
            OpenResolvedFolder(std::filesystem::path(entry->sourcePath).parent_path().wstring(), L"The preset folder could not be resolved.", L"Opened preset folder");
            return;
        }

        const std::wstring category = GetPresetBrowserCategory();
        std::filesystem::path folder = std::filesystem::path(environment_.repoRoot) / L"configs";
        if (category != L"all") {
            folder /= category + L"-presets";
        }
        OpenResolvedFolder(folder.wstring(), L"The preset folder could not be resolved.", L"Opened preset folder");
    }

    void OpenMatchPackBrowserFolder() {
        const BrowserEntry* entry = GetActiveBrowserEntry();
        if (entry != nullptr && entry->kind == BrowserEntryKind::MatchPack && !entry->sourcePath.empty()) {
            OpenResolvedFolder(std::filesystem::path(entry->sourcePath).parent_path().wstring(), L"The match-pack folder could not be resolved.", L"Opened match-pack folder");
            return;
        }

        std::wstring folder = environment_.matchPacksRoot;
        if (folder.empty()) {
            folder = (std::filesystem::path(environment_.repoRoot) / L"configs" / L"match-packs").wstring();
        }
        OpenResolvedFolder(folder, L"The match-pack folder could not be resolved.", L"Opened match-pack folder");
    }

    void CopyBrowserApplyCommand() {
        std::wstring command;
        std::wstring details;
        if (!BuildBrowserApplyCommand(command, details)) {
            SetBrowserStatus(details);
            MessageBoxW(hwnd_, details.c_str(), kWindowTitle, MB_ICONWARNING | MB_OK);
            return;
        }

        if (!CopyTextToClipboard(hwnd_, command)) {
            SetBrowserStatus(L"Unable to copy the live apply command to the clipboard.");
            MessageBoxW(hwnd_, L"Unable to copy the live apply command to the clipboard.", kWindowTitle, MB_ICONERROR | MB_OK);
            return;
        }

        SetBrowserStatus(L"Copied live apply command:\r\n" + command);
    }

    void QuickExportAndCopyApplyCommand() {
        const std::wstring liveModFolder = GetLiveModFolder();
        if (liveModFolder.empty()) {
            SetBrowserStatus(L"Quick export is unavailable because the live mod root is unresolved.");
            MessageBoxW(hwnd_, L"Quick export is unavailable because the live mod root is unresolved.", kWindowTitle, MB_ICONWARNING | MB_OK);
            return;
        }

        std::wstring commandBeforeExport;
        std::wstring details;
        if (!BuildBrowserApplyCommand(commandBeforeExport, details)) {
            SetBrowserStatus(details);
            MessageBoxW(hwnd_, details.c_str(), kWindowTitle, MB_ICONWARNING | MB_OK);
            return;
        }

        if (!RunExportWorkflow(&liveModFolder, true)) {
            RefreshBrowserPanels();
            return;
        }

        std::wstring commandAfterExport;
        if (!BuildBrowserApplyCommand(commandAfterExport, details)) {
            SetBrowserStatus(details);
            return;
        }

        if (!CopyTextToClipboard(hwnd_, commandAfterExport)) {
            SetBrowserStatus(L"Quick export succeeded, but the apply command could not be copied.");
            MessageBoxW(hwnd_, L"Quick export succeeded, but the apply command could not be copied.", kWindowTitle, MB_ICONWARNING | MB_OK);
            return;
        }

        SetBrowserStatus(L"Quick export succeeded and copied:\r\n" + commandAfterExport);
    }

    void LoadDocumentToControls() {
        loadingControls_ = true;
        lastSuggestedCfgFileName_ = BuildSuggestedCfgFileName(document_);

        SetTextValue(IDC_PROJECT_NAME, document_.metadata.projectName);
        SetTextValue(IDC_PROJECT_AUTHOR, document_.metadata.author);
        SetTextValue(IDC_PROJECT_NOTES, document_.metadata.notes);
        SetTextValue(IDC_MATCH_PACK_NAME, document_.matchPack.name);
        SetTextValue(IDC_MATCH_PACK_DESCRIPTION, document_.matchPack.description);
        SetTextValue(IDC_MATCH_PACK_TAGS, document_.matchPack.tags);
        SetWeaponSelection(document_.general.weaponUnderTest);
        SetTextValue(IDC_SESSION_TAG, document_.general.sessionTag);
        Button_SetCheck(FindControl(IDC_DEBUG_WEAPON_LOG), document_.general.debugWeaponLog ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(FindControl(IDC_DEBUG_WEAPON_LOG_REJECTIONS), document_.general.debugWeaponLogRejections ? BST_CHECKED : BST_UNCHECKED);

        SetTextValue(IDC_GLOCK_PROFILE_NAME, document_.glock.profileName);
        Button_SetCheck(FindControl(IDC_GLOCK_TAP_FIRE), document_.glock.tapFire ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(FindControl(IDC_GLOCK_FIRST_SHOT_ACCURACY), document_.glock.firstShotAccuracy ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_GLOCK_SPREAD_RECOVERY, document_.glock.spreadRecovery);
        SetTextValue(IDC_GLOCK_MOVE_SPREAD_SCALE, document_.glock.moveSpreadScale);
        SetTextValue(IDC_GLOCK_PRIMARY_BASE_SPREAD, document_.glock.primaryBaseSpread);
        SetTextValue(IDC_GLOCK_PRIMARY_GROUND_MOVE_PENALTY, document_.glock.primaryGroundMovePenalty);
        SetTextValue(IDC_GLOCK_PRIMARY_AIR_MOVE_PENALTY, document_.glock.primaryAirMovePenalty);
        SetTextValue(IDC_GLOCK_PRIMARY_DUCK_PENALTY_SCALE, document_.glock.primaryDuckPenaltyScale);
        SetTextValue(IDC_GLOCK_PRIMARY_SHOT_GROWTH, document_.glock.primaryShotGrowth);
        SetTextValue(IDC_GLOCK_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD, document_.glock.primaryFirstShotSpeedThreshold);
        SetTextValue(IDC_GLOCK_PRIMARY_MAX_SPREAD, document_.glock.primaryMaxSpread);
        Button_SetCheck(FindControl(IDC_GLOCK_PRIMARY_CADENCE_MODE), document_.glock.cadenceMode ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_GLOCK_PRIMARY_CYCLE_TIME, document_.glock.primaryCadenceCycleTime);
        SetTextValue(IDC_GLOCK_PRIMARY_CLICK_PENALTY, document_.glock.primaryClickPenalty);
        SetTextValue(IDC_GLOCK_PRIMARY_CLICK_PENALTY_SCALE, document_.glock.primaryClickPenaltyScale);
        SetTextValue(IDC_GLOCK_PRIMARY_CLICK_RESET_TIME, document_.glock.primaryClickResetTime);
        SetTextValue(IDC_GLOCK_PRIMARY_HOLD_PENALTY_SCALE, document_.glock.primaryHoldPenaltyScale);
        Button_SetCheck(FindControl(IDC_GLOCK_PATTERN_MODE), document_.glock.patternMode ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_GLOCK_PATTERN_SCALE_X, document_.glock.patternScaleX);
        SetTextValue(IDC_GLOCK_PATTERN_SCALE_Y, document_.glock.patternScaleY);
        SetTextValue(IDC_GLOCK_PATTERN_RESET_TIME, document_.glock.patternResetTime);
        SetTextValue(IDC_GLOCK_PATTERN_MAX_INDEX, document_.glock.patternMaxIndex);
        SetTextValue(IDC_GLOCK_PRIMARY_DAMAGE, document_.glock.primaryDamage);
        SetTextValue(IDC_GLOCK_PRIMARY_HEADSHOT_SCALE, document_.glock.primaryHeadshotScale);
        Button_SetCheck(FindControl(IDC_GLOCK_PRIMARY_HEADSHOT_LETHAL), document_.glock.primaryHeadshotLethal ? BST_CHECKED : BST_UNCHECKED);

        Button_SetCheck(FindControl(IDC_MP5_PRIMARY_ENABLED), document_.mp5.primaryEnabled ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_MP5_PROFILE_NAME, document_.mp5.profileName);
        SetTextValue(IDC_MP5_PRIMARY_BASE_SPREAD, document_.mp5.primaryBaseSpread);
        SetTextValue(IDC_MP5_PRIMARY_GROUND_MOVE_PENALTY, document_.mp5.primaryGroundMovePenalty);
        SetTextValue(IDC_MP5_PRIMARY_AIR_MOVE_PENALTY, document_.mp5.primaryAirMovePenalty);
        SetTextValue(IDC_MP5_PRIMARY_DUCK_PENALTY_SCALE, document_.mp5.primaryDuckPenaltyScale);
        SetTextValue(IDC_MP5_PRIMARY_BURST_GROWTH, document_.mp5.primaryBurstGrowth);
        SetTextValue(IDC_MP5_PRIMARY_BURST_MAX_ADDITIONAL_SPREAD, document_.mp5.primaryBurstMaxAdditionalSpread);
        SetTextValue(IDC_MP5_PRIMARY_SPREAD_RECOVERY, document_.mp5.primarySpreadRecovery);
        SetTextValue(IDC_MP5_PRIMARY_BURST_RESET_TIME, document_.mp5.primaryBurstResetTime);
        SetTextValue(IDC_MP5_PRIMARY_HOLD_PENALTY_SCALE, document_.mp5.primaryHoldPenaltyScale);
        Button_SetCheck(FindControl(IDC_MP5_PRIMARY_FIRST_SHOT_ACCURACY), document_.mp5.primaryFirstShotAccuracy ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_MP5_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD, document_.mp5.primaryFirstShotSpeedThreshold);
        SetTextValue(IDC_MP5_PRIMARY_MAX_SPREAD, document_.mp5.primaryMaxSpread);
        Button_SetCheck(FindControl(IDC_MP5_PATTERN_MODE), document_.mp5.patternMode ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_MP5_PATTERN_SCALE_X, document_.mp5.patternScaleX);
        SetTextValue(IDC_MP5_PATTERN_SCALE_Y, document_.mp5.patternScaleY);
        SetTextValue(IDC_MP5_PATTERN_RESET_TIME, document_.mp5.patternResetTime);
        SetTextValue(IDC_MP5_PATTERN_MAX_INDEX, document_.mp5.patternMaxIndex);
        SetTextValue(IDC_MP5_PRIMARY_DAMAGE, document_.mp5.primaryDamage);
        SetTextValue(IDC_MP5_PRIMARY_HEADSHOT_SCALE, document_.mp5.primaryHeadshotScale);
        Button_SetCheck(FindControl(IDC_MP5_PRIMARY_HEADSHOT_LETHAL), document_.mp5.primaryHeadshotLethal ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(FindControl(IDC_MP5_LAB_LOADOUT), document_.mp5.labLoadout ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_MP5_LAB_AMMO, document_.mp5.labAmmo);
        Button_SetCheck(FindControl(IDC_MP5_LAB_AUTOSWITCH), document_.mp5.labAutoswitch ? BST_CHECKED : BST_UNCHECKED);

        Button_SetCheck(FindControl(IDC_357_PRIMARY_ENABLED), document_.weapon357.primaryEnabled ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_357_PROFILE_NAME, document_.weapon357.profileName);
        SetTextValue(IDC_357_PRIMARY_BASE_SPREAD, document_.weapon357.primaryBaseSpread);
        SetTextValue(IDC_357_PRIMARY_GROUND_MOVE_PENALTY, document_.weapon357.primaryGroundMovePenalty);
        SetTextValue(IDC_357_PRIMARY_AIR_MOVE_PENALTY, document_.weapon357.primaryAirMovePenalty);
        SetTextValue(IDC_357_PRIMARY_DUCK_PENALTY_SCALE, document_.weapon357.primaryDuckPenaltyScale);
        Button_SetCheck(FindControl(IDC_357_PRIMARY_FIRST_SHOT_ACCURACY), document_.weapon357.primaryFirstShotAccuracy ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_357_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD, document_.weapon357.primaryFirstShotSpeedThreshold);
        SetTextValue(IDC_357_PRIMARY_SPREAD_RECOVERY, document_.weapon357.primarySpreadRecovery);
        SetTextValue(IDC_357_PRIMARY_MAX_SPREAD, document_.weapon357.primaryMaxSpread);
        Button_SetCheck(FindControl(IDC_357_PRIMARY_CADENCE_MODE), document_.weapon357.cadenceMode ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_357_PRIMARY_CYCLE_TIME, document_.weapon357.primaryCadenceCycleTime);
        SetTextValue(IDC_357_PRIMARY_CLICK_PENALTY, document_.weapon357.primaryClickPenalty);
        SetTextValue(IDC_357_PRIMARY_CLICK_PENALTY_SCALE, document_.weapon357.primaryClickPenaltyScale);
        SetTextValue(IDC_357_PRIMARY_CLICK_RESET_TIME, document_.weapon357.primaryClickResetTime);
        SetTextValue(IDC_357_PRIMARY_HOLD_PENALTY_SCALE, document_.weapon357.primaryHoldPenaltyScale);
        Button_SetCheck(FindControl(IDC_357_PATTERN_MODE), document_.weapon357.patternMode ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_357_PATTERN_SCALE_X, document_.weapon357.patternScaleX);
        SetTextValue(IDC_357_PATTERN_SCALE_Y, document_.weapon357.patternScaleY);
        SetTextValue(IDC_357_PATTERN_RESET_TIME, document_.weapon357.patternResetTime);
        SetTextValue(IDC_357_PATTERN_MAX_INDEX, document_.weapon357.patternMaxIndex);
        SetTextValue(IDC_357_PRIMARY_DAMAGE, document_.weapon357.primaryDamage);
        SetTextValue(IDC_357_PRIMARY_HEADSHOT_SCALE, document_.weapon357.primaryHeadshotScale);
        Button_SetCheck(FindControl(IDC_357_PRIMARY_HEADSHOT_LETHAL), document_.weapon357.primaryHeadshotLethal ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(FindControl(IDC_357_LAB_LOADOUT), document_.weapon357.labLoadout ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_357_LAB_AMMO, document_.weapon357.labAmmo);
        Button_SetCheck(FindControl(IDC_357_LAB_AUTOSWITCH), document_.weapon357.labAutoswitch ? BST_CHECKED : BST_UNCHECKED);

        Button_SetCheck(FindControl(IDC_SHOTGUN_PRIMARY_ENABLED), document_.shotgun.primaryEnabled ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_SHOTGUN_PROFILE_NAME, document_.shotgun.profileName);
        SetTextValue(IDC_SHOTGUN_PRIMARY_BASE_SPREAD, document_.shotgun.primaryBaseSpread);
        SetTextValue(IDC_SHOTGUN_PRIMARY_GROUND_MOVE_PENALTY, document_.shotgun.primaryGroundMovePenalty);
        SetTextValue(IDC_SHOTGUN_PRIMARY_AIR_MOVE_PENALTY, document_.shotgun.primaryAirMovePenalty);
        SetTextValue(IDC_SHOTGUN_PRIMARY_DUCK_PENALTY_SCALE, document_.shotgun.primaryDuckPenaltyScale);
        Button_SetCheck(FindControl(IDC_SHOTGUN_PRIMARY_FIRST_SHOT_ACCURACY), document_.shotgun.primaryFirstShotAccuracy ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_SHOTGUN_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD, document_.shotgun.primaryFirstShotSpeedThreshold);
        SetTextValue(IDC_SHOTGUN_PRIMARY_SPREAD_RECOVERY, document_.shotgun.primarySpreadRecovery);
        SetTextValue(IDC_SHOTGUN_PRIMARY_MAX_SPREAD, document_.shotgun.primaryMaxSpread);
        SetTextValue(IDC_SHOTGUN_PRIMARY_SHOT_GROWTH, document_.shotgun.primaryShotGrowth);
        Button_SetCheck(FindControl(IDC_SHOTGUN_PATTERN_MODE), document_.shotgun.patternMode ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_SHOTGUN_PATTERN_SCALE_X, document_.shotgun.patternScaleX);
        SetTextValue(IDC_SHOTGUN_PATTERN_SCALE_Y, document_.shotgun.patternScaleY);
        SetTextValue(IDC_SHOTGUN_PATTERN_RESET_TIME, document_.shotgun.patternResetTime);
        SetTextValue(IDC_SHOTGUN_PATTERN_MAX_INDEX, document_.shotgun.patternMaxIndex);
        SetComboSelectionValue(IDC_SHOTGUN_PRIMARY_PELLET_SPREAD_MODE, document_.shotgun.primaryPelletSpreadMode);
        SetTextValue(IDC_SHOTGUN_PRIMARY_DAMAGE_PER_PELLET, document_.shotgun.primaryDamagePerPellet);
        SetTextValue(IDC_SHOTGUN_PRIMARY_PELLET_COUNT, document_.shotgun.primaryPelletCount);
        SetTextValue(IDC_SHOTGUN_PRIMARY_HEADSHOT_SCALE, document_.shotgun.primaryHeadshotScale);
        Button_SetCheck(FindControl(IDC_SHOTGUN_PRIMARY_HEADSHOT_LETHAL), document_.shotgun.primaryHeadshotLethal ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(FindControl(IDC_SHOTGUN_LAB_LOADOUT), document_.shotgun.labLoadout ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_SHOTGUN_LAB_AMMO, document_.shotgun.labAmmo);
        Button_SetCheck(FindControl(IDC_SHOTGUN_LAB_AUTOSWITCH), document_.shotgun.labAutoswitch ? BST_CHECKED : BST_UNCHECKED);

        Button_SetCheck(FindControl(IDC_DUMMY_ENABLED), document_.targetDummy.enabled ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_DUMMY_TARGET_PROFILE_NAME, document_.targetDummy.targetProfileName);
        SetTextValue(IDC_DUMMY_HEALTH, document_.targetDummy.dummyHealth);
        SetTextValue(IDC_DUMMY_ARMOR, document_.targetDummy.dummyArmor);
        Button_SetCheck(FindControl(IDC_DUMMY_HEAD_PROTECTED), document_.targetDummy.dummyHeadProtected ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_DUMMY_ARMOR_HEALTH_FRACTION, document_.targetDummy.armorHealthFraction);
        SetTextValue(IDC_DUMMY_ARMOR_DRAIN_SCALE, document_.targetDummy.armorDrainScale);
        Button_SetCheck(FindControl(IDC_DUMMY_AUTORESPAWN), document_.targetDummy.autorespawn ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_DUMMY_RESPAWN_DELAY, document_.targetDummy.respawnDelay);
        SetTextValue(IDC_DUMMY_SPAWN_DISTANCE, document_.targetDummy.spawnDistance);
        SetTextValue(IDC_DUMMY_OFFSET_RIGHT, document_.targetDummy.offsetRight);
        SetTextValue(IDC_DUMMY_OFFSET_UP, document_.targetDummy.offsetUp);
        Button_SetCheck(FindControl(IDC_DUMMY_FACE_PLAYER), document_.targetDummy.facePlayer ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_DUMMY_MODEL, document_.targetDummy.model);

        Button_SetCheck(FindControl(IDC_ROUND_MODE), document_.roundMode.enabled ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_ROUND_FREEZE_TIME, document_.roundMode.freezeTime);
        SetTextValue(IDC_ROUND_RESTART_DELAY, document_.roundMode.restartDelay);
        SetTextValue(IDC_ROUND_START_HEALTH, document_.roundMode.startHealth);
        SetTextValue(IDC_ROUND_START_ARMOR, document_.roundMode.startArmor);
        Button_SetCheck(FindControl(IDC_ROUND_NO_RESPAWN), document_.roundMode.noRespawn ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(FindControl(IDC_ROUND_FRIENDLY_FIRE), document_.roundMode.friendlyFire ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_ROUND_WEAPON_PROFILE, document_.roundMode.weaponProfile);
        SetComboSelectionValue(IDC_ROUND_LOADOUT_MODE, document_.roundMode.loadoutMode);

        Button_SetCheck(FindControl(IDC_TEAM_ROUND_MODE), document_.teamRound.enabled ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(FindControl(IDC_TEAM_ROUND_TEAMPLAY), document_.teamRound.teamplay ? BST_CHECKED : BST_UNCHECKED);
        SetComboSelectionValue(IDC_TEAM_ROUND_SPAWN_MODE, document_.teamRound.spawnMode);
        SetTextValue(IDC_TEAM_ROUND_TEAM1_NAME, document_.teamRound.team1Name);
        SetTextValue(IDC_TEAM_ROUND_TEAM2_NAME, document_.teamRound.team2Name);
        SetComboSelectionValue(IDC_TEAM_ROUND_TEAM1_LOADOUT, document_.teamRound.team1Loadout);
        SetComboSelectionValue(IDC_TEAM_ROUND_TEAM2_LOADOUT, document_.teamRound.team2Loadout);
        SetTextValue(IDC_TEAM_ROUND_TEAM1_HEALTH, document_.teamRound.team1Health);
        SetTextValue(IDC_TEAM_ROUND_TEAM2_HEALTH, document_.teamRound.team2Health);
        SetTextValue(IDC_TEAM_ROUND_TEAM1_ARMOR, document_.teamRound.team1Armor);
        SetTextValue(IDC_TEAM_ROUND_TEAM2_ARMOR, document_.teamRound.team2Armor);

        Button_SetCheck(FindControl(IDC_BUY_MODE), document_.buy.enabled ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(FindControl(IDC_BUY_FREEZE_ONLY), document_.buy.freezeOnly ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(FindControl(IDC_BUY_TEAM_SHARED_CATALOG), document_.buy.teamSharedCatalog ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_BUY_START_MONEY, document_.buy.startMoney);
        SetTextValue(IDC_BUY_ROUND_WIN_REWARD, document_.buy.roundWinReward);
        SetTextValue(IDC_BUY_ROUND_LOSS_REWARD, document_.buy.roundLossReward);
        SetTextValue(IDC_BUY_MAX_MONEY, document_.buy.maxMoney);
        Button_SetCheck(FindControl(IDC_BUY_ALLOW_GLOCK), document_.buy.allowGlock ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(FindControl(IDC_BUY_ALLOW_MP5), document_.buy.allowMp5 ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(FindControl(IDC_BUY_ALLOW_357), document_.buy.allow357 ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(FindControl(IDC_BUY_ALLOW_SHOTGUN), document_.buy.allowShotgun ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(FindControl(IDC_BUY_ALLOW_ARMOR), document_.buy.allowArmor ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(FindControl(IDC_BUY_ALLOW_HELMET), document_.buy.allowHelmet ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(FindControl(IDC_BUY_ALLOW_HANDGRENADE), document_.buy.allowHandgrenade ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_BUY_COST_GLOCK, document_.buy.costGlock);
        SetTextValue(IDC_BUY_COST_MP5, document_.buy.costMp5);
        SetTextValue(IDC_BUY_COST_357, document_.buy.cost357);
        SetTextValue(IDC_BUY_COST_SHOTGUN, document_.buy.costShotgun);
        SetTextValue(IDC_BUY_COST_ARMOR, document_.buy.costArmor);
        SetTextValue(IDC_BUY_COST_HELMET, document_.buy.costHelmet);
        SetTextValue(IDC_BUY_COST_HANDGRENADE, document_.buy.costHandgrenade);

        Button_SetCheck(FindControl(IDC_ARMOR_MODE), document_.armorEquipment.armorMode ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_ARMOR_START_VALUE, document_.armorEquipment.armorStartValue);
        SetTextValue(IDC_ARMOR_MAX_VALUE, document_.armorEquipment.armorMaxValue);
        SetTextValue(IDC_ARMOR_HEALTH_FRACTION, document_.armorEquipment.armorHealthFraction);
        SetTextValue(IDC_ARMOR_DRAIN_SCALE, document_.armorEquipment.armorDrainScale);
        Button_SetCheck(FindControl(IDC_HELMET_MODE), document_.armorEquipment.helmetMode ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(FindControl(IDC_HELMET_START_ENABLED), document_.armorEquipment.helmetStartEnabled ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(
            FindControl(IDC_HELMET_HEADSHOT_PROTECTION),
            document_.armorEquipment.helmetHeadshotProtection ? BST_CHECKED : BST_UNCHECKED);

        SetTextValue(IDC_EXPORT_FOLDER, document_.exportSettings.exportFolder);
        SetTextValue(IDC_EXPORT_FILE_NAME, document_.exportSettings.cfgFileName);
        RefreshResolvedExportInfo();
        RefreshMatchSummary();
        RefreshBrowserControls();
        RefreshLiveServerCommandPreview(true);
        RefreshGuidedTestPreview(true);
        RefreshTelemetryStatusPreview(true);

        if (lastActionStatus_.empty()) {
            lastActionStatus_ = L"Ready.\n\nQuick export target:\n" + BuildQuickExportTargetPathPreview();
        }
        SetTextValue(IDC_EXPORT_STATUS, lastActionStatus_);

        loadingControls_ = false;
        RefreshExportPreview(true);
    }

    void SyncDocumentFromControls() {
        document_.metadata.projectName = GetTextValue(IDC_PROJECT_NAME);
        document_.metadata.author = GetTextValue(IDC_PROJECT_AUTHOR);
        document_.metadata.notes = GetTextValue(IDC_PROJECT_NOTES);
        document_.matchPack.name = GetTextValue(IDC_MATCH_PACK_NAME);
        document_.matchPack.description = GetTextValue(IDC_MATCH_PACK_DESCRIPTION);
        document_.matchPack.tags = GetTextValue(IDC_MATCH_PACK_TAGS);
        document_.general.weaponUnderTest = GetWeaponSelection();
        document_.general.sessionTag = GetTextValue(IDC_SESSION_TAG);
        document_.general.debugWeaponLog = GetCheckValue(IDC_DEBUG_WEAPON_LOG);
        document_.general.debugWeaponLogRejections = GetCheckValue(IDC_DEBUG_WEAPON_LOG_REJECTIONS);

        document_.glock.profileName = GetTextValue(IDC_GLOCK_PROFILE_NAME);
        document_.glock.tapFire = GetCheckValue(IDC_GLOCK_TAP_FIRE);
        document_.glock.firstShotAccuracy = GetCheckValue(IDC_GLOCK_FIRST_SHOT_ACCURACY);
        document_.glock.spreadRecovery = GetTextValue(IDC_GLOCK_SPREAD_RECOVERY);
        document_.glock.moveSpreadScale = GetTextValue(IDC_GLOCK_MOVE_SPREAD_SCALE);
        document_.glock.primaryBaseSpread = GetTextValue(IDC_GLOCK_PRIMARY_BASE_SPREAD);
        document_.glock.primaryGroundMovePenalty = GetTextValue(IDC_GLOCK_PRIMARY_GROUND_MOVE_PENALTY);
        document_.glock.primaryAirMovePenalty = GetTextValue(IDC_GLOCK_PRIMARY_AIR_MOVE_PENALTY);
        document_.glock.primaryDuckPenaltyScale = GetTextValue(IDC_GLOCK_PRIMARY_DUCK_PENALTY_SCALE);
        document_.glock.primaryShotGrowth = GetTextValue(IDC_GLOCK_PRIMARY_SHOT_GROWTH);
        document_.glock.primaryFirstShotSpeedThreshold = GetTextValue(IDC_GLOCK_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD);
        document_.glock.primaryMaxSpread = GetTextValue(IDC_GLOCK_PRIMARY_MAX_SPREAD);
        document_.glock.cadenceMode = GetCheckValue(IDC_GLOCK_PRIMARY_CADENCE_MODE);
        document_.glock.primaryCadenceCycleTime = GetTextValue(IDC_GLOCK_PRIMARY_CYCLE_TIME);
        document_.glock.primaryClickPenalty = GetTextValue(IDC_GLOCK_PRIMARY_CLICK_PENALTY);
        document_.glock.primaryClickPenaltyScale = GetTextValue(IDC_GLOCK_PRIMARY_CLICK_PENALTY_SCALE);
        document_.glock.primaryClickResetTime = GetTextValue(IDC_GLOCK_PRIMARY_CLICK_RESET_TIME);
        document_.glock.primaryHoldPenaltyScale = GetTextValue(IDC_GLOCK_PRIMARY_HOLD_PENALTY_SCALE);
        document_.glock.patternMode = GetCheckValue(IDC_GLOCK_PATTERN_MODE);
        document_.glock.patternScaleX = GetTextValue(IDC_GLOCK_PATTERN_SCALE_X);
        document_.glock.patternScaleY = GetTextValue(IDC_GLOCK_PATTERN_SCALE_Y);
        document_.glock.patternResetTime = GetTextValue(IDC_GLOCK_PATTERN_RESET_TIME);
        document_.glock.patternMaxIndex = GetTextValue(IDC_GLOCK_PATTERN_MAX_INDEX);
        document_.glock.primaryDamage = GetTextValue(IDC_GLOCK_PRIMARY_DAMAGE);
        document_.glock.primaryHeadshotScale = GetTextValue(IDC_GLOCK_PRIMARY_HEADSHOT_SCALE);
        document_.glock.primaryHeadshotLethal = GetCheckValue(IDC_GLOCK_PRIMARY_HEADSHOT_LETHAL);

        document_.mp5.primaryEnabled = GetCheckValue(IDC_MP5_PRIMARY_ENABLED);
        document_.mp5.profileName = GetTextValue(IDC_MP5_PROFILE_NAME);
        document_.mp5.primaryBaseSpread = GetTextValue(IDC_MP5_PRIMARY_BASE_SPREAD);
        document_.mp5.primaryGroundMovePenalty = GetTextValue(IDC_MP5_PRIMARY_GROUND_MOVE_PENALTY);
        document_.mp5.primaryAirMovePenalty = GetTextValue(IDC_MP5_PRIMARY_AIR_MOVE_PENALTY);
        document_.mp5.primaryDuckPenaltyScale = GetTextValue(IDC_MP5_PRIMARY_DUCK_PENALTY_SCALE);
        document_.mp5.primaryBurstGrowth = GetTextValue(IDC_MP5_PRIMARY_BURST_GROWTH);
        document_.mp5.primaryBurstMaxAdditionalSpread = GetTextValue(IDC_MP5_PRIMARY_BURST_MAX_ADDITIONAL_SPREAD);
        document_.mp5.primarySpreadRecovery = GetTextValue(IDC_MP5_PRIMARY_SPREAD_RECOVERY);
        document_.mp5.primaryBurstResetTime = GetTextValue(IDC_MP5_PRIMARY_BURST_RESET_TIME);
        document_.mp5.primaryHoldPenaltyScale = GetTextValue(IDC_MP5_PRIMARY_HOLD_PENALTY_SCALE);
        document_.mp5.primaryFirstShotAccuracy = GetCheckValue(IDC_MP5_PRIMARY_FIRST_SHOT_ACCURACY);
        document_.mp5.primaryFirstShotSpeedThreshold = GetTextValue(IDC_MP5_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD);
        document_.mp5.primaryMaxSpread = GetTextValue(IDC_MP5_PRIMARY_MAX_SPREAD);
        document_.mp5.patternMode = GetCheckValue(IDC_MP5_PATTERN_MODE);
        document_.mp5.patternScaleX = GetTextValue(IDC_MP5_PATTERN_SCALE_X);
        document_.mp5.patternScaleY = GetTextValue(IDC_MP5_PATTERN_SCALE_Y);
        document_.mp5.patternResetTime = GetTextValue(IDC_MP5_PATTERN_RESET_TIME);
        document_.mp5.patternMaxIndex = GetTextValue(IDC_MP5_PATTERN_MAX_INDEX);
        document_.mp5.primaryDamage = GetTextValue(IDC_MP5_PRIMARY_DAMAGE);
        document_.mp5.primaryHeadshotScale = GetTextValue(IDC_MP5_PRIMARY_HEADSHOT_SCALE);
        document_.mp5.primaryHeadshotLethal = GetCheckValue(IDC_MP5_PRIMARY_HEADSHOT_LETHAL);
        document_.mp5.labLoadout = GetCheckValue(IDC_MP5_LAB_LOADOUT);
        document_.mp5.labAmmo = GetTextValue(IDC_MP5_LAB_AMMO);
        document_.mp5.labAutoswitch = GetCheckValue(IDC_MP5_LAB_AUTOSWITCH);

        document_.weapon357.primaryEnabled = GetCheckValue(IDC_357_PRIMARY_ENABLED);
        document_.weapon357.profileName = GetTextValue(IDC_357_PROFILE_NAME);
        document_.weapon357.primaryBaseSpread = GetTextValue(IDC_357_PRIMARY_BASE_SPREAD);
        document_.weapon357.primaryGroundMovePenalty = GetTextValue(IDC_357_PRIMARY_GROUND_MOVE_PENALTY);
        document_.weapon357.primaryAirMovePenalty = GetTextValue(IDC_357_PRIMARY_AIR_MOVE_PENALTY);
        document_.weapon357.primaryDuckPenaltyScale = GetTextValue(IDC_357_PRIMARY_DUCK_PENALTY_SCALE);
        document_.weapon357.primaryFirstShotAccuracy = GetCheckValue(IDC_357_PRIMARY_FIRST_SHOT_ACCURACY);
        document_.weapon357.primaryFirstShotSpeedThreshold = GetTextValue(IDC_357_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD);
        document_.weapon357.primarySpreadRecovery = GetTextValue(IDC_357_PRIMARY_SPREAD_RECOVERY);
        document_.weapon357.primaryMaxSpread = GetTextValue(IDC_357_PRIMARY_MAX_SPREAD);
        document_.weapon357.cadenceMode = GetCheckValue(IDC_357_PRIMARY_CADENCE_MODE);
        document_.weapon357.primaryCadenceCycleTime = GetTextValue(IDC_357_PRIMARY_CYCLE_TIME);
        document_.weapon357.primaryClickPenalty = GetTextValue(IDC_357_PRIMARY_CLICK_PENALTY);
        document_.weapon357.primaryClickPenaltyScale = GetTextValue(IDC_357_PRIMARY_CLICK_PENALTY_SCALE);
        document_.weapon357.primaryClickResetTime = GetTextValue(IDC_357_PRIMARY_CLICK_RESET_TIME);
        document_.weapon357.primaryHoldPenaltyScale = GetTextValue(IDC_357_PRIMARY_HOLD_PENALTY_SCALE);
        document_.weapon357.patternMode = GetCheckValue(IDC_357_PATTERN_MODE);
        document_.weapon357.patternScaleX = GetTextValue(IDC_357_PATTERN_SCALE_X);
        document_.weapon357.patternScaleY = GetTextValue(IDC_357_PATTERN_SCALE_Y);
        document_.weapon357.patternResetTime = GetTextValue(IDC_357_PATTERN_RESET_TIME);
        document_.weapon357.patternMaxIndex = GetTextValue(IDC_357_PATTERN_MAX_INDEX);
        document_.weapon357.primaryDamage = GetTextValue(IDC_357_PRIMARY_DAMAGE);
        document_.weapon357.primaryHeadshotScale = GetTextValue(IDC_357_PRIMARY_HEADSHOT_SCALE);
        document_.weapon357.primaryHeadshotLethal = GetCheckValue(IDC_357_PRIMARY_HEADSHOT_LETHAL);
        document_.weapon357.labLoadout = GetCheckValue(IDC_357_LAB_LOADOUT);
        document_.weapon357.labAmmo = GetTextValue(IDC_357_LAB_AMMO);
        document_.weapon357.labAutoswitch = GetCheckValue(IDC_357_LAB_AUTOSWITCH);

        document_.shotgun.primaryEnabled = GetCheckValue(IDC_SHOTGUN_PRIMARY_ENABLED);
        document_.shotgun.profileName = GetTextValue(IDC_SHOTGUN_PROFILE_NAME);
        document_.shotgun.primaryBaseSpread = GetTextValue(IDC_SHOTGUN_PRIMARY_BASE_SPREAD);
        document_.shotgun.primaryGroundMovePenalty = GetTextValue(IDC_SHOTGUN_PRIMARY_GROUND_MOVE_PENALTY);
        document_.shotgun.primaryAirMovePenalty = GetTextValue(IDC_SHOTGUN_PRIMARY_AIR_MOVE_PENALTY);
        document_.shotgun.primaryDuckPenaltyScale = GetTextValue(IDC_SHOTGUN_PRIMARY_DUCK_PENALTY_SCALE);
        document_.shotgun.primaryFirstShotAccuracy = GetCheckValue(IDC_SHOTGUN_PRIMARY_FIRST_SHOT_ACCURACY);
        document_.shotgun.primaryFirstShotSpeedThreshold = GetTextValue(IDC_SHOTGUN_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD);
        document_.shotgun.primarySpreadRecovery = GetTextValue(IDC_SHOTGUN_PRIMARY_SPREAD_RECOVERY);
        document_.shotgun.primaryMaxSpread = GetTextValue(IDC_SHOTGUN_PRIMARY_MAX_SPREAD);
        document_.shotgun.primaryShotGrowth = GetTextValue(IDC_SHOTGUN_PRIMARY_SHOT_GROWTH);
        document_.shotgun.patternMode = GetCheckValue(IDC_SHOTGUN_PATTERN_MODE);
        document_.shotgun.patternScaleX = GetTextValue(IDC_SHOTGUN_PATTERN_SCALE_X);
        document_.shotgun.patternScaleY = GetTextValue(IDC_SHOTGUN_PATTERN_SCALE_Y);
        document_.shotgun.patternResetTime = GetTextValue(IDC_SHOTGUN_PATTERN_RESET_TIME);
        document_.shotgun.patternMaxIndex = GetTextValue(IDC_SHOTGUN_PATTERN_MAX_INDEX);
        document_.shotgun.primaryPelletSpreadMode = GetComboSelectionValue(IDC_SHOTGUN_PRIMARY_PELLET_SPREAD_MODE);
        document_.shotgun.primaryDamagePerPellet = GetTextValue(IDC_SHOTGUN_PRIMARY_DAMAGE_PER_PELLET);
        document_.shotgun.primaryPelletCount = GetTextValue(IDC_SHOTGUN_PRIMARY_PELLET_COUNT);
        document_.shotgun.primaryHeadshotScale = GetTextValue(IDC_SHOTGUN_PRIMARY_HEADSHOT_SCALE);
        document_.shotgun.primaryHeadshotLethal = GetCheckValue(IDC_SHOTGUN_PRIMARY_HEADSHOT_LETHAL);
        document_.shotgun.labLoadout = GetCheckValue(IDC_SHOTGUN_LAB_LOADOUT);
        document_.shotgun.labAmmo = GetTextValue(IDC_SHOTGUN_LAB_AMMO);
        document_.shotgun.labAutoswitch = GetCheckValue(IDC_SHOTGUN_LAB_AUTOSWITCH);

        document_.targetDummy.enabled = GetCheckValue(IDC_DUMMY_ENABLED);
        document_.targetDummy.targetProfileName = GetTextValue(IDC_DUMMY_TARGET_PROFILE_NAME);
        document_.targetDummy.dummyHealth = GetTextValue(IDC_DUMMY_HEALTH);
        document_.targetDummy.dummyArmor = GetTextValue(IDC_DUMMY_ARMOR);
        document_.targetDummy.dummyHeadProtected = GetCheckValue(IDC_DUMMY_HEAD_PROTECTED);
        document_.targetDummy.armorHealthFraction = GetTextValue(IDC_DUMMY_ARMOR_HEALTH_FRACTION);
        document_.targetDummy.armorDrainScale = GetTextValue(IDC_DUMMY_ARMOR_DRAIN_SCALE);
        document_.targetDummy.autorespawn = GetCheckValue(IDC_DUMMY_AUTORESPAWN);
        document_.targetDummy.respawnDelay = GetTextValue(IDC_DUMMY_RESPAWN_DELAY);
        document_.targetDummy.spawnDistance = GetTextValue(IDC_DUMMY_SPAWN_DISTANCE);
        document_.targetDummy.offsetRight = GetTextValue(IDC_DUMMY_OFFSET_RIGHT);
        document_.targetDummy.offsetUp = GetTextValue(IDC_DUMMY_OFFSET_UP);
        document_.targetDummy.facePlayer = GetCheckValue(IDC_DUMMY_FACE_PLAYER);
        document_.targetDummy.model = GetTextValue(IDC_DUMMY_MODEL);

        document_.roundMode.enabled = GetCheckValue(IDC_ROUND_MODE);
        document_.roundMode.freezeTime = GetTextValue(IDC_ROUND_FREEZE_TIME);
        document_.roundMode.restartDelay = GetTextValue(IDC_ROUND_RESTART_DELAY);
        document_.roundMode.startHealth = GetTextValue(IDC_ROUND_START_HEALTH);
        document_.roundMode.startArmor = GetTextValue(IDC_ROUND_START_ARMOR);
        document_.roundMode.noRespawn = GetCheckValue(IDC_ROUND_NO_RESPAWN);
        document_.roundMode.friendlyFire = GetCheckValue(IDC_ROUND_FRIENDLY_FIRE);
        document_.roundMode.weaponProfile = GetTextValue(IDC_ROUND_WEAPON_PROFILE);
        document_.roundMode.loadoutMode = GetComboSelectionValue(IDC_ROUND_LOADOUT_MODE);

        document_.teamRound.enabled = GetCheckValue(IDC_TEAM_ROUND_MODE);
        document_.teamRound.teamplay = GetCheckValue(IDC_TEAM_ROUND_TEAMPLAY);
        document_.teamRound.spawnMode = GetComboSelectionValue(IDC_TEAM_ROUND_SPAWN_MODE);
        document_.teamRound.team1Name = GetTextValue(IDC_TEAM_ROUND_TEAM1_NAME);
        document_.teamRound.team2Name = GetTextValue(IDC_TEAM_ROUND_TEAM2_NAME);
        document_.teamRound.team1Loadout = GetComboSelectionValue(IDC_TEAM_ROUND_TEAM1_LOADOUT);
        document_.teamRound.team2Loadout = GetComboSelectionValue(IDC_TEAM_ROUND_TEAM2_LOADOUT);
        document_.teamRound.team1Health = GetTextValue(IDC_TEAM_ROUND_TEAM1_HEALTH);
        document_.teamRound.team2Health = GetTextValue(IDC_TEAM_ROUND_TEAM2_HEALTH);
        document_.teamRound.team1Armor = GetTextValue(IDC_TEAM_ROUND_TEAM1_ARMOR);
        document_.teamRound.team2Armor = GetTextValue(IDC_TEAM_ROUND_TEAM2_ARMOR);

        document_.buy.enabled = GetCheckValue(IDC_BUY_MODE);
        document_.buy.freezeOnly = GetCheckValue(IDC_BUY_FREEZE_ONLY);
        document_.buy.teamSharedCatalog = GetCheckValue(IDC_BUY_TEAM_SHARED_CATALOG);
        document_.buy.startMoney = GetTextValue(IDC_BUY_START_MONEY);
        document_.buy.roundWinReward = GetTextValue(IDC_BUY_ROUND_WIN_REWARD);
        document_.buy.roundLossReward = GetTextValue(IDC_BUY_ROUND_LOSS_REWARD);
        document_.buy.maxMoney = GetTextValue(IDC_BUY_MAX_MONEY);
        document_.buy.allowGlock = GetCheckValue(IDC_BUY_ALLOW_GLOCK);
        document_.buy.allowMp5 = GetCheckValue(IDC_BUY_ALLOW_MP5);
        document_.buy.allow357 = GetCheckValue(IDC_BUY_ALLOW_357);
        document_.buy.allowShotgun = GetCheckValue(IDC_BUY_ALLOW_SHOTGUN);
        document_.buy.allowArmor = GetCheckValue(IDC_BUY_ALLOW_ARMOR);
        document_.buy.allowHelmet = GetCheckValue(IDC_BUY_ALLOW_HELMET);
        document_.buy.allowHandgrenade = GetCheckValue(IDC_BUY_ALLOW_HANDGRENADE);
        document_.buy.costGlock = GetTextValue(IDC_BUY_COST_GLOCK);
        document_.buy.costMp5 = GetTextValue(IDC_BUY_COST_MP5);
        document_.buy.cost357 = GetTextValue(IDC_BUY_COST_357);
        document_.buy.costShotgun = GetTextValue(IDC_BUY_COST_SHOTGUN);
        document_.buy.costArmor = GetTextValue(IDC_BUY_COST_ARMOR);
        document_.buy.costHelmet = GetTextValue(IDC_BUY_COST_HELMET);
        document_.buy.costHandgrenade = GetTextValue(IDC_BUY_COST_HANDGRENADE);

        document_.armorEquipment.armorMode = GetCheckValue(IDC_ARMOR_MODE);
        document_.armorEquipment.armorStartValue = GetTextValue(IDC_ARMOR_START_VALUE);
        document_.armorEquipment.armorMaxValue = GetTextValue(IDC_ARMOR_MAX_VALUE);
        document_.armorEquipment.armorHealthFraction = GetTextValue(IDC_ARMOR_HEALTH_FRACTION);
        document_.armorEquipment.armorDrainScale = GetTextValue(IDC_ARMOR_DRAIN_SCALE);
        document_.armorEquipment.helmetMode = GetCheckValue(IDC_HELMET_MODE);
        document_.armorEquipment.helmetStartEnabled = GetCheckValue(IDC_HELMET_START_ENABLED);
        document_.armorEquipment.helmetHeadshotProtection = GetCheckValue(IDC_HELMET_HEADSHOT_PROTECTION);

        document_.exportSettings.exportFolder = GetTextValue(IDC_EXPORT_FOLDER);
        document_.exportSettings.cfgFileName = hlcfg::EnsureCfgFileName(GetTextValue(IDC_EXPORT_FILE_NAME));
    }

    void UpdateWindowTitle() {
        const std::wstring fileName = currentProjectPath_.empty() ? L"Untitled" : FileNameFromPath(currentProjectPath_);
        std::wstring title = fileName + (dirty_ ? L" * - " : L" - ");
        title += kWindowTitle;
        SetWindowTextW(hwnd_, title.c_str());
    }

    bool PromptToSaveIfDirty() {
        if (!dirty_) {
            return true;
        }

        const int choice = MessageBoxW(hwnd_, L"This project has unsaved changes. Save them now?", kWindowTitle, MB_YESNOCANCEL | MB_ICONQUESTION);
        if (choice == IDCANCEL) {
            return false;
        }

        if (choice == IDYES) {
            return SaveProject(false);
        }

        return true;
    }

    bool SaveProject(bool saveAs) {
        SyncDocumentFromControls();

        std::wstring destination = currentProjectPath_;
        if (saveAs || destination.empty()) {
            destination = ShowSaveProjectDialog(hwnd_, destination);
            if (destination.empty()) {
                return false;
            }

            destination = (std::filesystem::path(destination).parent_path() /
                           hlcfg::EnsureProjectFileName(std::filesystem::path(destination).filename().wstring()))
                              .wstring();
        }

        std::wstring errorMessage;
        if (!hlcfg::SaveProjectDocumentToFile(document_, destination, errorMessage)) {
            MessageBoxW(hwnd_, errorMessage.c_str(), kWindowTitle, MB_ICONERROR | MB_OK);
            return false;
        }

        currentProjectPath_ = destination;
        dirty_ = false;
        UpdateWindowTitle();
        return true;
    }

    void OpenProject() {
        if (!PromptToSaveIfDirty()) {
            return;
        }

        const std::wstring path = ShowOpenProjectDialog(hwnd_);
        if (path.empty()) {
            return;
        }

        hlcfg::ProjectDocument loaded;
        std::wstring errorMessage;
        if (!hlcfg::LoadProjectDocumentFromFile(path, loaded, errorMessage)) {
            MessageBoxW(hwnd_, errorMessage.c_str(), kWindowTitle, MB_ICONERROR | MB_OK);
            return;
        }

        document_ = std::move(loaded);
        if (document_.exportSettings.exportFolder.empty()) {
            document_.exportSettings.exportFolder = environment_.defaultExportFolder;
        }
        if (document_.exportSettings.cfgFileName.empty() ||
            hlcfg::EnsureCfgFileName(document_.exportSettings.cfgFileName) == hlcfg::CreateDefaultProject().exportSettings.cfgFileName) {
            document_.exportSettings.cfgFileName = BuildSuggestedCfgFileName(document_);
        }

        currentProjectPath_ = path;
        dirty_ = false;
        LoadDocumentToControls();
        UpdateWindowTitle();
    }

    void BrowseExportFolder() {
        const std::wstring selected = BrowseForFolder(hwnd_, GetTextValue(IDC_EXPORT_FOLDER));
        if (selected.empty()) {
            return;
        }

        SetTextValue(IDC_EXPORT_FOLDER, selected);
        dirty_ = true;
        UpdateWindowTitle();
        RefreshExportPreview(true);
    }

    bool BuildPreview(hlcfg::ExportResult& result, bool silentErrors) {
        SyncDocumentFromControls();
        std::wstring errorMessage;
        if (!hlcfg::BuildExportResult(document_, environment_, result, errorMessage)) {
            RefreshResolvedExportInfo();
            SetTextValue(IDC_LAUNCHER_PREVIEW, L"");
            SetTextValue(IDC_EXEC_PREVIEW, L"");
            SetTextValue(IDC_CFG_PREVIEW, errorMessage);
            if (!silentErrors) {
                ShowActionError(
                    L"Export preview",
                    errorMessage,
                    BuildQuickExportTargetPathPreview(),
                    L"Verify the cfg file name and the export target path.");
            }
            return false;
        }

        RefreshResolvedExportInfo();
        return true;
    }

    void RefreshExportPreview(bool silentErrors) {
        hlcfg::ExportResult result;
        if (BuildPreview(result, silentErrors)) {
            SetTextValue(IDC_LAUNCHER_PREVIEW, result.launcherCommand);
            SetTextValue(IDC_EXEC_PREVIEW, result.execCommand);
            SetTextValue(IDC_CFG_PREVIEW, result.cfgText);
        }
    }

    void QuickExportToLiveMod() {
        const std::wstring liveModFolder = GetLiveModFolder();
        if (liveModFolder.empty()) {
            ShowActionError(
                L"Quick Export to Live Mod",
                L"The live mod folder could not be resolved.",
                BuildQuickExportTargetPathPreview(),
                L"Set HL_EXE or HLDS_EXE, or use Export to chosen folder for advanced mode.");
            return;
        }

        RunExportWorkflow(&liveModFolder, true);
    }

    void ExportCfg() {
        RunExportWorkflow(nullptr, false);
    }

    void CopyExecCommand() {
        hlcfg::ExportResult result;
        if (!BuildPreview(result, false)) {
            return;
        }

        if (!CopyTextToClipboard(hwnd_, result.execCommand)) {
            ShowActionError(
                L"Copy exec command",
                L"Unable to copy the exec command to the clipboard.",
                result.exportPath,
                L"Try again after closing any application that is holding the clipboard open.");
            return;
        }

        ShowActionInfo(L"Copied exec command:\n" + result.execCommand, L"Copied:\n" + result.execCommand);
    }

    void CopyLauncherCommand() {
        hlcfg::ExportResult result;
        if (!BuildPreview(result, false)) {
            return;
        }

        if (result.launcherCommand.empty()) {
            ShowActionError(
                L"Copy launcher command",
                L"The current export path does not map to a launcher-friendly cfg profile path.",
                result.exportPath,
                L"Export into the live mod root or cfg_profiles to enable a launcher command.");
            return;
        }

        if (!CopyTextToClipboard(hwnd_, result.launcherCommand)) {
            ShowActionError(
                L"Copy launcher command",
                L"Unable to copy the launcher command to the clipboard.",
                result.exportPath,
                L"Try again after closing any application that is holding the clipboard open.");
            return;
        }

        ShowActionInfo(L"Copied launcher command:\n" + result.launcherCommand, L"Copied:\n" + result.launcherCommand);
    }

    void CopyRawCfg() {
        hlcfg::ExportResult result;
        if (!BuildPreview(result, false)) {
            return;
        }

        if (!CopyTextToClipboard(hwnd_, result.cfgText)) {
            ShowActionError(
                L"Copy raw cfg",
                L"Unable to copy the cfg contents to the clipboard.",
                result.exportPath,
                L"Try again after closing any application that is holding the clipboard open.");
            return;
        }

        ShowActionInfo(L"Copied generated cfg text.\n\nPreview source:\n" + result.exportPath, L"Copied the generated cfg text to the clipboard.");
    }

    void OpenExportFolder() {
        SyncDocumentFromControls();
        std::wstring folder = document_.exportSettings.exportFolder.empty() ? environment_.defaultExportFolder : document_.exportSettings.exportFolder;
        OpenResolvedFolder(folder, L"The export folder is not set.", L"Opened export folder");
    }

    std::wstring GetLiveModFolder() const {
        if (!environment_.liveModRoot.empty()) {
            return environment_.liveModRoot;
        }

        return environment_.stagedLiveModRoot;
    }

    void OpenResolvedFolder(const std::wstring& folder, const wchar_t* emptyMessage, const wchar_t* actionLabel) {
        if (folder.empty()) {
            ShowActionError(actionLabel, emptyMessage, std::wstring(), L"Choose or resolve a folder first.");
            return;
        }

        std::error_code error;
        std::filesystem::create_directories(folder, error);
        const HINSTANCE openResult = ShellExecuteW(hwnd_, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(openResult) <= 32) {
            ShowActionError(
                actionLabel,
                L"Windows could not open the folder.",
                folder,
                L"Check that the folder exists and that Explorer can access it.");
            return;
        }

        SetActionStatus(std::wstring(actionLabel) + L":\n" + folder);
    }

    bool IsLiveServerControl(int controlId) const {
        return controlId >= IDC_LIVE_SERVER_HOST && controlId <= IDC_LIVE_COPY_FALLBACK;
    }

    void SetLiveStatus(const std::wstring& value) {
        lastLiveStatus_ = value;
        const bool wasLoadingControls = loadingControls_;
        loadingControls_ = true;
        SetTextValue(IDC_LIVE_STATUS, value);
        loadingControls_ = wasLoadingControls;
    }

    std::wstring JoinCommandsForClipboard(const std::vector<std::wstring>& commands) const {
        std::wstring text;
        for (const std::wstring& command : commands) {
            const std::wstring trimmed = hlcfg::Trimmed(command);
            if (trimmed.empty()) {
                continue;
            }

            if (!text.empty()) {
                text += L"\r\n";
            }
            text += trimmed;
        }
        return text;
    }

    std::vector<std::wstring> SplitLiveCommandLines(const std::wstring& value) const {
        std::vector<std::wstring> commands;
        std::wstringstream stream(value);
        std::wstring line;
        while (std::getline(stream, line)) {
            const std::wstring trimmed = hlcfg::Trimmed(line);
            if (!trimmed.empty()) {
                commands.push_back(trimmed);
            }
        }
        return commands;
    }

    std::wstring GetLiveServerHost() const {
        std::wstring host = hlcfg::Trimmed(GetTextValue(IDC_LIVE_SERVER_HOST));
        return host.empty() ? L"127.0.0.1" : host;
    }

    std::wstring GetLiveServerPort() const {
        std::wstring port = hlcfg::Trimmed(GetTextValue(IDC_LIVE_SERVER_PORT));
        return port.empty() ? L"27015" : port;
    }

    std::wstring GetLiveCfgFileName() const {
        std::wstring cfg = hlcfg::Trimmed(GetTextValue(IDC_LIVE_CFG_FILE));
        if (cfg.empty()) {
            cfg = GetEffectiveCfgFileNameForQuickExport();
        }
        return hlcfg::EnsureCfgFileName(cfg);
    }

    std::wstring GetLiveMatchPackName() const {
        std::wstring pack = hlcfg::Trimmed(GetTextValue(IDC_LIVE_MATCH_PACK));
        if (pack.empty()) {
            pack = hlcfg::Trimmed(GetTextValue(IDC_MATCH_PACK_NAME));
        }
        return pack;
    }

    std::wstring GetLiveSandboxWeapon() const {
        std::wstring weapon = GetComboSelectionValue(IDC_LIVE_SANDBOX_WEAPON);
        if (weapon.empty()) {
            weapon = GetWeaponSelection();
        }
        return weapon.empty() ? L"glock" : weapon;
    }

    std::wstring GetLiveSandboxTargetProfile() const {
        std::wstring profile = hlcfg::Trimmed(GetTextValue(IDC_LIVE_SANDBOX_TARGET));
        if (profile.empty()) {
            profile = hlcfg::Trimmed(GetTextValue(IDC_DUMMY_TARGET_PROFILE_NAME));
        }
        return profile;
    }

    std::wstring GetLiveSandboxSpot() const {
        return hlcfg::Trimmed(GetTextValue(IDC_LIVE_SANDBOX_SPOT));
    }

    bool CopyCommandListToClipboard(const std::vector<std::wstring>& commands, const wchar_t* actionLabel) {
        const std::wstring text = JoinCommandsForClipboard(commands);
        if (text.empty()) {
            SetLiveStatus(std::wstring(actionLabel) + L": no command to copy.");
            return false;
        }

        if (!CopyTextToClipboard(hwnd_, text)) {
            SetLiveStatus(std::wstring(actionLabel) + L": unable to copy commands to the clipboard.\r\n\r\n" + text);
            return false;
        }

        lastLiveFallbackCommands_ = commands;
        SetLiveStatus(std::wstring(actionLabel) + L": copied commands.\r\n\r\n" + text);
        return true;
    }

    void CopyLastLiveFallbackCommands() {
        if (lastLiveFallbackCommands_.empty()) {
            RefreshLiveServerCommandPreview(false);
        }

        if (lastLiveFallbackCommands_.empty()) {
            SetLiveStatus(L"No fallback commands are available yet.");
            return;
        }

        CopyCommandListToClipboard(lastLiveFallbackCommands_, L"Copy fallback");
    }

    bool SendLiveCommandSequence(const std::vector<std::wstring>& commands, const wchar_t* actionLabel) {
        std::vector<std::wstring> filteredCommands;
        for (const std::wstring& command : commands) {
            const std::wstring trimmed = hlcfg::Trimmed(command);
            if (!trimmed.empty()) {
                filteredCommands.push_back(trimmed);
            }
        }

        if (filteredCommands.empty()) {
            SetLiveStatus(std::wstring(actionLabel) + L": no commands to send.");
            return false;
        }

        lastLiveFallbackCommands_ = filteredCommands;

        LiveRconResult rconResult;
        const bool sent = SendGoldSrcRconCommands(
            GetLiveServerHost(),
            GetLiveServerPort(),
            GetTextValue(IDC_LIVE_SERVER_PASSWORD),
            filteredCommands,
            rconResult);

        if (!sent) {
            const bool copied = CopyTextToClipboard(hwnd_, JoinCommandsForClipboard(filteredCommands));
            std::wstring status = std::wstring(actionLabel) + L" could not be sent through RCON.\r\n\r\n";
            status += rconResult.error.empty() ? L"RCON failed without a detailed error." : rconResult.error;
            if (!rconResult.response.empty()) {
                status += L"\r\n\r\nServer response:\r\n" + rconResult.response;
            }
            status += copied ? L"\r\n\r\nFallback commands copied for manual HLDS paste:\r\n"
                             : L"\r\n\r\nFallback commands could not be copied; paste manually:\r\n";
            status += JoinCommandsForClipboard(filteredCommands);
            SetLiveStatus(status);
            return false;
        }

        std::wstring status = std::wstring(actionLabel) + L" sent to ";
        status += GetLiveServerHost() + L":" + GetLiveServerPort();
        status += L".\r\n\r\n";
        status += rconResult.response.empty() ? JoinCommandsForClipboard(filteredCommands) : rconResult.response;
        SetLiveStatus(status);
        return true;
    }

    std::vector<std::wstring> BuildSandboxSetupCommands() const {
        std::vector<std::wstring> commands;
        commands.push_back(L"exp_sandbox_start");
        commands.push_back(L"exp_sandbox_weapon " + GetLiveSandboxWeapon());

        const std::wstring pack = GetLiveMatchPackName();
        const std::wstring cfg = GetLiveCfgFileName();
        if (!pack.empty()) {
            commands.push_back(L"exp_sandbox_pack " + pack);
        } else if (!cfg.empty()) {
            commands.push_back(L"exp_sandbox_cfg " + cfg);
        }

        const std::wstring target = GetLiveSandboxTargetProfile();
        if (!target.empty()) {
            commands.push_back(L"exp_sandbox_target " + target);
        }

        const std::wstring spot = GetLiveSandboxSpot();
        if (!spot.empty()) {
            commands.push_back(L"exp_sandbox_spot " + spot);
        }

        commands.push_back(L"exp_sandbox_reset");
        return commands;
    }

    void RefreshLiveServerCommandPreview(bool overwriteUserFields) {
        const bool wasLoadingControls = loadingControls_;
        loadingControls_ = true;

        if (overwriteUserFields || GetTextValue(IDC_LIVE_SERVER_HOST).empty()) {
            SetTextValue(IDC_LIVE_SERVER_HOST, L"127.0.0.1");
        }
        if (overwriteUserFields || GetTextValue(IDC_LIVE_SERVER_PORT).empty()) {
            SetTextValue(IDC_LIVE_SERVER_PORT, L"27015");
        }
        if (overwriteUserFields || GetTextValue(IDC_LIVE_CFG_FILE).empty()) {
            SetTextValue(IDC_LIVE_CFG_FILE, GetEffectiveCfgFileNameForQuickExport());
        }
        if (overwriteUserFields || GetTextValue(IDC_LIVE_MATCH_PACK).empty()) {
            SetTextValue(IDC_LIVE_MATCH_PACK, hlcfg::Trimmed(document_.matchPack.name));
        }

        const std::wstring weapon = GetWeaponSelection().empty() ? document_.general.weaponUnderTest : GetWeaponSelection();
        if (overwriteUserFields || GetComboSelectionValue(IDC_LIVE_SANDBOX_WEAPON).empty()) {
            SetComboSelectionValue(IDC_LIVE_SANDBOX_WEAPON, weapon.empty() ? L"glock" : weapon);
        }
        if (overwriteUserFields || GetTextValue(IDC_LIVE_SANDBOX_TARGET).empty()) {
            SetTextValue(IDC_LIVE_SANDBOX_TARGET, document_.targetDummy.targetProfileName);
        }
        if (overwriteUserFields || GetTextValue(IDC_LIVE_SANDBOX_SPOT).empty()) {
            SetTextValue(IDC_LIVE_SANDBOX_SPOT, L"default");
        }

        const std::vector<std::wstring> sandboxCommands = BuildSandboxSetupCommands();
        SetTextValue(IDC_LIVE_CUSTOM_COMMAND, JoinCommandsForClipboard(sandboxCommands));
        lastLiveFallbackCommands_ = sandboxCommands;

        if (lastLiveStatus_.empty()) {
            std::wstring status = L"Ready to send live commands through GoldSrc RCON.\r\n";
            status += L"Set host, port, and rcon_password, then use Apply CFG or Sandbox Setup.\r\n\r\n";
            status += L"Current sandbox sequence:\r\n" + JoinCommandsForClipboard(sandboxCommands);
            SetTextValue(IDC_LIVE_STATUS, status);
        } else {
            SetTextValue(IDC_LIVE_STATUS, lastLiveStatus_);
        }

        loadingControls_ = wasLoadingControls;
    }

    bool QuickExportForLiveApply(hlcfg::ExportResult& result) {
        const std::wstring liveModFolder = GetLiveModFolder();
        if (liveModFolder.empty()) {
            ShowActionError(
                L"Live apply quick export",
                L"The live mod folder could not be resolved.",
                BuildQuickExportTargetPathPreview(),
                L"Resolve HL_EXE or HLDS_EXE, then try again.");
            return false;
        }

        hlcfg::ProjectDocument exportDocument;
        PrepareDocumentForExport(exportDocument, &liveModFolder);

        hlcfg::ExportResult preview;
        std::wstring errorMessage;
        if (!hlcfg::BuildExportResult(exportDocument, environment_, preview, errorMessage)) {
            ShowActionError(
                L"Live apply quick export",
                errorMessage,
                BuildQuickExportTargetPathPreview(),
                L"Verify the cfg filename and live mod path.");
            return false;
        }

        if (!ConfirmOverwrite(preview.exportPath, L"Live apply quick export")) {
            SetLiveStatus(L"Live apply quick export was cancelled.\r\n\r\nTarget path:\r\n" + preview.exportPath);
            return false;
        }

        if (!hlcfg::ExportCfgToFile(exportDocument, environment_, result, errorMessage)) {
            ShowActionError(
                L"Live apply quick export",
                errorMessage,
                preview.exportPath,
                L"Check that the live mod folder is writable.");
            return false;
        }

        std::error_code verifyError;
        if (!std::filesystem::exists(result.exportPath, verifyError) || verifyError) {
            ShowActionError(
                L"Live apply quick export",
                L"The cfg export completed, but the file could not be confirmed on disk.",
                result.exportPath,
                L"Check file permissions and the live mod folder.");
            return false;
        }

        document_ = std::move(exportDocument);
        document_.exportSettings.exportFolder = std::filesystem::path(result.exportPath).parent_path().wstring();
        document_.exportSettings.cfgFileName = std::filesystem::path(result.exportPath).filename().wstring();

        const bool wasLoadingControls = loadingControls_;
        loadingControls_ = true;
        SetTextValue(IDC_EXPORT_FOLDER, document_.exportSettings.exportFolder);
        SetTextValue(IDC_EXPORT_FILE_NAME, document_.exportSettings.cfgFileName);
        SetTextValue(IDC_LIVE_CFG_FILE, document_.exportSettings.cfgFileName);
        loadingControls_ = wasLoadingControls;

        dirty_ = true;
        UpdateWindowTitle();
        RefreshResolvedExportInfo();
        RefreshExportPreview(true);
        ReloadBrowserEntries();
        RefreshBrowserControls();
        RefreshLiveServerCommandPreview(false);
        RefreshGuidedTestPreview(false);
        return true;
    }

    void TestLiveServerConnection() {
        SendLiveCommandSequence({L"status"}, L"Test connection");
    }

    void ApplyCurrentCfgToLiveServer(bool resetSandbox) {
        hlcfg::ExportResult exportResult;
        if (!QuickExportForLiveApply(exportResult)) {
            return;
        }

        const std::wstring cfgFileName = std::filesystem::path(exportResult.exportPath).filename().wstring();
        std::vector<std::wstring> commands{L"exp_cfg_apply " + cfgFileName};
        if (resetSandbox) {
            commands.push_back(L"exp_sandbox_reset");
        }

        SendLiveCommandSequence(commands, resetSandbox ? L"Apply CFG + sandbox reset" : L"Apply current CFG");
    }

    void ApplySelectedMatchPackToLiveServer(bool resetSandbox) {
        const std::wstring pack = GetLiveMatchPackName();
        if (pack.empty()) {
            SetLiveStatus(L"Select or enter a match pack name before applying it.");
            return;
        }

        std::vector<std::wstring> commands{L"exp_matchcfg_apply " + pack};
        if (resetSandbox) {
            commands.push_back(L"exp_sandbox_reset");
        }
        SendLiveCommandSequence(commands, resetSandbox ? L"Apply match pack + sandbox reset" : L"Apply selected match pack");
    }

    void SendSandboxSetupToLiveServer() {
        SendLiveCommandSequence(BuildSandboxSetupCommands(), L"Apply sandbox setup");
    }

    void CopySandboxSequence() {
        CopyCommandListToClipboard(BuildSandboxSetupCommands(), L"Copy sandbox sequence");
    }

    void SendCustomLiveCommand() {
        std::vector<std::wstring> commands = SplitLiveCommandLines(GetTextValue(IDC_LIVE_CUSTOM_COMMAND));
        if (commands.empty()) {
            commands.push_back(hlcfg::Trimmed(GetTextValue(IDC_LIVE_CUSTOM_COMMAND)));
        }
        SendLiveCommandSequence(commands, L"Send custom command");
    }

    bool IsGuidedTestControl(int controlId) const {
        return controlId >= IDC_GUIDED_WEAPON && controlId <= IDC_GUIDED_REFRESH_PREVIEW;
    }

    void SetGuidedStatus(const std::wstring& value) {
        lastGuidedStatus_ = value;
        const bool wasLoadingControls = loadingControls_;
        loadingControls_ = true;
        SetTextValue(IDC_GUIDED_STATUS, value);
        loadingControls_ = wasLoadingControls;
    }

    std::wstring GetGuidedWeapon() const {
        std::wstring weapon = GetComboSelectionValue(IDC_GUIDED_WEAPON);
        if (weapon.empty()) {
            weapon = GetWeaponSelection();
        }
        return weapon.empty() ? L"glock" : weapon;
    }

    std::wstring GetGuidedSourceType() const {
        std::wstring sourceType = GetComboSelectionValue(IDC_GUIDED_SOURCE_TYPE);
        return sourceType.empty() ? L"current editor cfg" : sourceType;
    }

    std::wstring GetGuidedSourceValue() const {
        return hlcfg::Trimmed(GetTextValue(IDC_GUIDED_SOURCE_VALUE));
    }

    std::wstring GetGuidedTargetProfile() const {
        std::wstring profile = GetComboSelectionValue(IDC_GUIDED_TARGET_PROFILE);
        if (profile.empty()) {
            profile = hlcfg::Trimmed(GetTextValue(IDC_DUMMY_TARGET_PROFILE_NAME));
        }
        return profile.empty() ? L"unarmored" : profile;
    }

    std::wstring GetGuidedTargetSpot() const {
        const std::wstring spot = hlcfg::Trimmed(GetTextValue(IDC_GUIDED_TARGET_SPOT));
        return spot.empty() ? L"default" : spot;
    }

    std::wstring GetGuidedScenario() const {
        std::wstring scenario = GetComboSelectionValue(IDC_GUIDED_SCENARIO);
        return scenario.empty() ? L"single-shot precision" : scenario;
    }

    std::wstring BuildGuidedScenarioInstructions(const std::wstring& weapon, const std::wstring& scenario) const {
        std::wstring instructions;
        if (scenario == L"burst/spam control") {
            instructions += L"1. Stand still and fire a 3-5 shot burst.\r\n";
            instructions += L"2. Pause long enough for recovery/reset.\r\n";
            instructions += L"3. Fire a longer held spray.\r\n";
            instructions += L"4. Finish & Analyze and compare burst growth, pattern index, and reset evidence.";
        } else if (scenario == L"movement test") {
            instructions += L"1. Fire a controlled shot or short burst while standing still.\r\n";
            instructions += L"2. Repeat while moving.\r\n";
            instructions += L"3. Repeat crouched if useful.\r\n";
            instructions += L"4. Finish & Analyze and compare movement/air/crouch contribution fields.";
        } else if (scenario == L"headshot/armor test") {
            instructions += L"1. Use vest_headprotected when testing helmet/protected-head evidence.\r\n";
            instructions += L"2. Fire one body hit, then one head hit.\r\n";
            instructions += L"3. If needed, respawn/reset the dummy and repeat.\r\n";
            instructions += L"4. Finish & Analyze and inspect armor/head-protection consistency.";
        } else if (scenario == L"shotgun pellet test") {
            instructions += L"1. Select shotgun and stand at close range.\r\n";
            instructions += L"2. Fire one shell, wait for pattern reset, then fire another.\r\n";
            instructions += L"3. Try the same target profile again after sandbox reset.\r\n";
            instructions += L"4. Finish & Analyze and inspect pellet/pattern evidence.";
        } else if (weapon == L"357") {
            instructions += L"1. Fire one careful shot while still.\r\n";
            instructions += L"2. Fire fast follow-up clicks.\r\n";
            instructions += L"3. Wait for cadence/pattern reset, then fire again.\r\n";
            instructions += L"4. Finish & Analyze and inspect cadence plus pattern fields.";
        } else {
            instructions += L"1. Stand still and fire one deliberate shot.\r\n";
            instructions += L"2. Wait for recovery/reset, then fire again.\r\n";
            instructions += L"3. Fire a quicker follow-up string.\r\n";
            instructions += L"4. Finish & Analyze and compare accepted shots, cadence/pattern evidence, and hits.";
        }

        return instructions;
    }

    std::vector<std::wstring> BuildGuidedTestCommands(bool quickExportCurrentCfg, std::wstring& resolvedSourceDescription) {
        std::vector<std::wstring> commands;
        const std::wstring weapon = GetGuidedWeapon();
        const std::wstring sourceType = GetGuidedSourceType();
        const std::wstring sourceValue = GetGuidedSourceValue();
        const std::wstring targetProfile = GetGuidedTargetProfile();
        const std::wstring targetSpot = GetGuidedTargetSpot();

        std::wstring cfgFileName;
        std::wstring matchPackName;

        if (sourceType == L"match pack") {
            matchPackName = sourceValue.empty() ? GetLiveMatchPackName() : sourceValue;
            resolvedSourceDescription = matchPackName.empty() ? L"match pack: <not selected>" : L"match pack: " + matchPackName;
        } else if (sourceType == L"current editor cfg") {
            if (quickExportCurrentCfg) {
                hlcfg::ExportResult exportResult;
                if (!QuickExportForLiveApply(exportResult)) {
                    resolvedSourceDescription = L"current editor cfg export failed";
                    return {};
                }
                cfgFileName = std::filesystem::path(exportResult.exportPath).filename().wstring();
            } else {
                cfgFileName = GetLiveCfgFileName();
            }
            resolvedSourceDescription = L"current editor cfg: " + cfgFileName;
        } else {
            cfgFileName = hlcfg::EnsureCfgFileName(sourceValue.empty() ? GetLiveCfgFileName() : sourceValue);
            resolvedSourceDescription = sourceType + L": " + cfgFileName;
        }

        if (!matchPackName.empty()) {
            commands.push_back(L"exp_matchcfg_apply " + matchPackName);
        }

        commands.push_back(L"exp_sandbox_start");
        commands.push_back(L"exp_sandbox_weapon " + weapon);

        if (!matchPackName.empty()) {
            commands.push_back(L"exp_sandbox_pack " + matchPackName);
        } else if (!cfgFileName.empty()) {
            commands.push_back(L"exp_sandbox_cfg " + cfgFileName);
        }

        if (!targetProfile.empty()) {
            commands.push_back(L"exp_sandbox_target " + targetProfile);
        }
        if (!targetSpot.empty()) {
            commands.push_back(L"exp_sandbox_spot " + targetSpot);
        }

        commands.push_back(L"exp_sandbox_reset");
        return commands;
    }

    bool GetFileWriteTime(const std::filesystem::path& path, std::filesystem::file_time_type& writeTime) const {
        if (path.empty()) {
            return false;
        }

        std::error_code error;
        writeTime = std::filesystem::last_write_time(path, error);
        return !error;
    }

    void RefreshGuidedTestPreview(bool overwriteFields) {
        const bool wasLoadingControls = loadingControls_;
        loadingControls_ = true;

        const std::wstring documentWeapon = GetWeaponSelection().empty() ? document_.general.weaponUnderTest : GetWeaponSelection();
        if (overwriteFields || GetComboSelectionValue(IDC_GUIDED_WEAPON).empty()) {
            SetComboSelectionValue(IDC_GUIDED_WEAPON, documentWeapon.empty() ? L"glock" : documentWeapon);
        }
        if (overwriteFields || GetComboSelectionValue(IDC_GUIDED_SOURCE_TYPE).empty()) {
            SetComboSelectionValue(IDC_GUIDED_SOURCE_TYPE, L"current editor cfg");
        }
        if (overwriteFields || GetTextValue(IDC_GUIDED_SOURCE_VALUE).empty()) {
            SetTextValue(IDC_GUIDED_SOURCE_VALUE, GetEffectiveCfgFileNameForQuickExport());
        }
        if (overwriteFields || GetComboSelectionValue(IDC_GUIDED_TARGET_PROFILE).empty()) {
            const std::wstring profile = document_.targetDummy.targetProfileName.empty() ? L"unarmored" : document_.targetDummy.targetProfileName;
            SetComboSelectionValue(IDC_GUIDED_TARGET_PROFILE, profile);
        }
        if (overwriteFields || GetTextValue(IDC_GUIDED_TARGET_SPOT).empty()) {
            SetTextValue(IDC_GUIDED_TARGET_SPOT, L"default");
        }
        if (overwriteFields || GetComboSelectionValue(IDC_GUIDED_SCENARIO).empty()) {
            const std::wstring weapon = GetGuidedWeapon();
            if (weapon == L"mp5") {
                SetComboSelectionValue(IDC_GUIDED_SCENARIO, L"burst/spam control");
            } else if (weapon == L"shotgun") {
                SetComboSelectionValue(IDC_GUIDED_SCENARIO, L"shotgun pellet test");
            } else {
                SetComboSelectionValue(IDC_GUIDED_SCENARIO, L"single-shot precision");
            }
        }

        loadingControls_ = wasLoadingControls;

        std::wstring sourceDescription;
        const std::vector<std::wstring> commands = BuildGuidedTestCommands(false, sourceDescription);
        lastGuidedCommandSequence_ = commands;
        lastGuidedInstructions_ = BuildGuidedScenarioInstructions(GetGuidedWeapon(), GetGuidedScenario());

        const bool wasLoadingOutput = loadingControls_;
        loadingControls_ = true;
        SetTextValue(IDC_GUIDED_COMMAND_SEQUENCE, JoinCommandsForClipboard(commands));
        SetTextValue(IDC_GUIDED_INSTRUCTIONS, lastGuidedInstructions_);
        if (!lastGuidedAnalysisText_.empty()) {
            SetTextValue(IDC_GUIDED_ANALYSIS, lastGuidedAnalysisText_);
        }
        loadingControls_ = wasLoadingOutput;

        if (lastGuidedStatus_.empty()) {
            SetGuidedStatus(L"Ready. Review the command sequence, configure RCON on the Live Server tab, then click Start Test.\r\nSource: " + sourceDescription);
        } else {
            SetGuidedStatus(lastGuidedStatus_);
        }
    }

    void RecordGuidedBaselineLog() {
        lastGuidedBaselineLogPath_.clear();
        lastGuidedBaselineLogWriteTime_ = {};

        std::filesystem::path latestPath;
        std::wstring details;
        if (!TryFindLatestWeaponLog(latestPath, details)) {
            return;
        }

        lastGuidedBaselineLogPath_ = latestPath.wstring();
        GetFileWriteTime(latestPath, lastGuidedBaselineLogWriteTime_);
    }

    void StartGuidedWeaponTest() {
        SyncDocumentFromControls();
        RefreshLiveServerCommandPreview(false);
        RecordGuidedBaselineLog();

        std::wstring sourceDescription;
        std::vector<std::wstring> commands = BuildGuidedTestCommands(true, sourceDescription);
        if (commands.empty()) {
            SetGuidedStatus(L"Start Test could not build a command sequence. Check the selected cfg, preset, or match pack.");
            return;
        }

        lastGuidedCommandSequence_ = commands;
        lastGuidedInstructions_ = BuildGuidedScenarioInstructions(GetGuidedWeapon(), GetGuidedScenario());

        const bool wasLoadingControls = loadingControls_;
        loadingControls_ = true;
        SetTextValue(IDC_GUIDED_COMMAND_SEQUENCE, JoinCommandsForClipboard(commands));
        SetTextValue(IDC_GUIDED_INSTRUCTIONS, lastGuidedInstructions_);
        SetTextValue(IDC_GUIDED_ANALYSIS, L"");
        loadingControls_ = wasLoadingControls;
        lastGuidedAnalysisText_.clear();
        lastGuidedAnalyzedLogPath_.clear();

        const bool sent = SendLiveCommandSequence(commands, L"Start guided weapon test");
        std::wstring status = sent ? L"Guided setup commands were sent through RCON.\r\n"
                                   : L"Guided setup was not sent through RCON. Fallback commands were copied for manual HLDS paste.\r\n";
        status += L"Source: " + sourceDescription + L"\r\n";
        status += lastGuidedBaselineLogPath_.empty()
                      ? L"No baseline weapon log was found before start."
                      : L"Baseline log before start:\r\n" + lastGuidedBaselineLogPath_;
        SetGuidedStatus(status);
    }

    void FinishAndAnalyzeGuidedWeaponTest() {
        std::filesystem::path latestPath;
        std::wstring details;
        if (!TryFindLatestWeaponLog(latestPath, details)) {
            SetGuidedStatus(details);
            return;
        }

        std::filesystem::file_time_type latestWriteTime{};
        const bool hasLatestWriteTime = GetFileWriteTime(latestPath, latestWriteTime);
        bool newerThanBaseline = lastGuidedBaselineLogPath_.empty();
        if (!newerThanBaseline && hasLatestWriteTime) {
            newerThanBaseline = latestPath.wstring() != lastGuidedBaselineLogPath_ || latestWriteTime > lastGuidedBaselineLogWriteTime_;
        }

        const std::wstring weaponFilter = GetGuidedWeapon();
        std::wstring output;
        std::wstring errorMessage;
        if (!RunAnalyzerProcess(latestPath, weaponFilter, output, errorMessage)) {
            lastGuidedAnalysisText_ = output.empty() ? errorMessage : output;
            SetTextValue(IDC_GUIDED_ANALYSIS, lastGuidedAnalysisText_);
            SetGuidedStatus(L"Finish & Analyze failed for:\r\n" + latestPath.wstring() + L"\r\n\r\n" + errorMessage);
            return;
        }

        const std::wstring quickSummary = BuildQuickTelemetrySummary(output);
        lastGuidedAnalyzedLogPath_ = latestPath.wstring();
        lastGuidedAnalysisText_ =
            L"Compact guided result\r\n" +
            (quickSummary.empty() ? L"No compact evidence summary was derived.\r\n" : quickSummary) +
            L"\r\nAnalyzer output\r\n" + output;

        const bool wasLoadingControls = loadingControls_;
        loadingControls_ = true;
        SetTextValue(IDC_GUIDED_ANALYSIS, lastGuidedAnalysisText_);
        SetTextValue(IDC_TELEMETRY_SELECTED_LOG, latestPath.wstring());
        SetComboSelectionValue(IDC_TELEMETRY_WEAPON_FILTER, weaponFilter);
        SetTextValue(IDC_TELEMETRY_ANALYSIS_TEXT, lastGuidedAnalysisText_);
        loadingControls_ = wasLoadingControls;
        lastTelemetryAnalysisText_ = lastGuidedAnalysisText_;

        std::wstring status = L"Finish & Analyze used ";
        status += newerThanBaseline ? L"a new or updated weapon log.\r\n" : L"the newest log; no newer timestamp was detected after Start Test.\r\n";
        status += L"Log: " + latestPath.wstring() + L"\r\nWeapon filter: " + weaponFilter;
        SetGuidedStatus(status);
    }

    void CopyGuidedTestCommands() {
        std::wstring sourceDescription;
        std::vector<std::wstring> commands = BuildGuidedTestCommands(false, sourceDescription);
        if (commands.empty()) {
            SetGuidedStatus(L"No guided command sequence is available to copy.");
            return;
        }

        lastGuidedCommandSequence_ = commands;
        if (!CopyTextToClipboard(hwnd_, JoinCommandsForClipboard(commands))) {
            SetGuidedStatus(L"Unable to copy guided command sequence.");
            return;
        }

        SetGuidedStatus(L"Copied guided command sequence.\r\nSource: " + sourceDescription);
    }

    std::filesystem::path GetGuidedReportFolder() const {
        if (!environment_.repoRoot.empty()) {
            return std::filesystem::path(environment_.repoRoot) / L"testbed" / L"logs" / L"reports" / L"guided-tests";
        }
        if (!environment_.liveModRoot.empty()) {
            return std::filesystem::path(environment_.liveModRoot) / L"reports" / L"guided-tests";
        }
        return std::filesystem::path(L".") / L"guided-tests";
    }

    std::filesystem::path GetStockClientPlaytestReportFolder() const {
        if (!environment_.repoRoot.empty()) {
            return std::filesystem::path(environment_.repoRoot) / L"testbed" / L"logs" / L"reports" / L"stock-client-playtests";
        }
        if (!environment_.liveModRoot.empty()) {
            return std::filesystem::path(environment_.liveModRoot) / L"reports" / L"stock-client-playtests";
        }
        return std::filesystem::path(L".") / L"stock-client-playtests";
    }

    std::wstring BuildGuidedReportText() const {
        std::wstring report;
        report += L"Guided weapon test report\r\n";
        report += L"timestamp: " + BuildLocalTimestampText(false) + L"\r\n";
        report += L"weapon: " + GetGuidedWeapon() + L"\r\n";
        report += L"source_type: " + GetGuidedSourceType() + L"\r\n";
        report += L"source_value: " + GetGuidedSourceValue() + L"\r\n";
        report += L"target_profile: " + GetGuidedTargetProfile() + L"\r\n";
        report += L"target_spot: " + GetGuidedTargetSpot() + L"\r\n";
        report += L"scenario: " + GetGuidedScenario() + L"\r\n";
        report += L"baseline_log: " + lastGuidedBaselineLogPath_ + L"\r\n";
        report += L"analyzed_log: " + lastGuidedAnalyzedLogPath_ + L"\r\n\r\n";
        report += L"command_sequence:\r\n" + JoinCommandsForClipboard(lastGuidedCommandSequence_) + L"\r\n\r\n";
        report += L"shooting_instructions:\r\n" + lastGuidedInstructions_ + L"\r\n\r\n";
        report += L"status:\r\n" + lastGuidedStatus_ + L"\r\n\r\n";
        report += L"analysis:\r\n" + (lastGuidedAnalysisText_.empty() ? GetTextValue(IDC_GUIDED_ANALYSIS) : lastGuidedAnalysisText_) + L"\r\n";
        return report;
    }

    void SaveGuidedTestReport() {
        if (lastGuidedCommandSequence_.empty()) {
            std::wstring sourceDescription;
            lastGuidedCommandSequence_ = BuildGuidedTestCommands(false, sourceDescription);
        }
        if (lastGuidedInstructions_.empty()) {
            lastGuidedInstructions_ = BuildGuidedScenarioInstructions(GetGuidedWeapon(), GetGuidedScenario());
        }

        const std::filesystem::path reportFolder = GetGuidedReportFolder();
        const std::wstring fileName =
            L"guided-test-" + BuildLocalTimestampText(true) + L"-" + SanitizeFileNamePart(GetGuidedWeapon()) + L".txt";
        const std::filesystem::path reportPath = reportFolder / fileName;

        std::wstring errorMessage;
        if (!WriteUtf8TextFile(reportPath, BuildGuidedReportText(), errorMessage)) {
            SetGuidedStatus(L"Unable to save guided test report.\r\n\r\n" + errorMessage);
            return;
        }

        const std::filesystem::path jsonPath = reportPath.parent_path() / (reportPath.stem().wstring() + L".json");
        std::wstring jsonWarning;
        if (!WriteUtf8TextFile(jsonPath, BuildGuidedReportJson(reportPath), jsonWarning)) {
            SetGuidedStatus(L"Saved guided test report:\r\n" + reportPath.wstring() + L"\r\n\r\nJSON sidecar was not saved:\r\n" + jsonWarning);
            RefreshReportHistory(true);
            return;
        }

        SetGuidedStatus(L"Saved guided test report:\r\n" + reportPath.wstring() + L"\r\nJSON sidecar:\r\n" + jsonPath.wstring());
        RefreshReportHistory(true);
    }

    std::wstring BuildGuidedReportJson(const std::filesystem::path& reportPath) const {
        const GuidedReportMetrics metrics = BuildMetricsFromAnalyzerText(lastGuidedAnalysisText_);

        hlcfg::JsonValue root = hlcfg::JsonValue::MakeObject();
        auto& object = root.AsObject();
        object[L"timestamp"] = hlcfg::JsonValue(BuildLocalTimestampText(false));
        object[L"weapon"] = hlcfg::JsonValue(GetGuidedWeapon());
        object[L"source_type"] = hlcfg::JsonValue(GetGuidedSourceType());
        object[L"source_value"] = hlcfg::JsonValue(GetGuidedSourceValue());
        object[L"target_profile"] = hlcfg::JsonValue(GetGuidedTargetProfile());
        object[L"target_spot"] = hlcfg::JsonValue(GetGuidedTargetSpot());
        object[L"scenario"] = hlcfg::JsonValue(GetGuidedScenario());
        object[L"baseline_log"] = hlcfg::JsonValue(lastGuidedBaselineLogPath_);
        object[L"analyzed_log"] = hlcfg::JsonValue(lastGuidedAnalyzedLogPath_);
        object[L"report_path"] = hlcfg::JsonValue(reportPath.wstring());
        object[L"command_sequence"] = hlcfg::JsonValue(JoinCommandsForClipboard(lastGuidedCommandSequence_));
        object[L"analyzer_summary_text"] = hlcfg::JsonValue(metrics.analyzerSummaryText);

        hlcfg::JsonValue metricsJson = hlcfg::JsonValue::MakeObject();
        auto& metricsObject = metricsJson.AsObject();
        metricsObject[L"accepted_shots"] = hlcfg::JsonValue(static_cast<double>(metrics.acceptedShots));
        metricsObject[L"hit_count"] = hlcfg::JsonValue(static_cast<double>(metrics.hitEvents));
        metricsObject[L"kill_count"] = hlcfg::JsonValue(static_cast<double>(metrics.killEvents));
        metricsObject[L"headshot_hits"] = hlcfg::JsonValue(static_cast<double>(metrics.headshotHits));
        metricsObject[L"headshot_kills"] = hlcfg::JsonValue(static_cast<double>(metrics.headshotKills));
        metricsObject[L"pattern_evidence"] = hlcfg::JsonValue(static_cast<double>(metrics.patternEvidence));
        metricsObject[L"cadence_evidence"] = hlcfg::JsonValue(static_cast<double>(metrics.cadenceEvidence));
        metricsObject[L"burst_growth_evidence"] = hlcfg::JsonValue(static_cast<double>(metrics.burstGrowthEvidence));
        metricsObject[L"armor_evidence"] = hlcfg::JsonValue(static_cast<double>(metrics.armorEvidence));
        metricsObject[L"spread_summary"] = hlcfg::JsonValue(metrics.spreadSummary);
        metricsObject[L"damage_summary"] = hlcfg::JsonValue(metrics.damageSummary);
        metricsObject[L"consistency_warnings"] = hlcfg::JsonValue(metrics.consistencyWarnings);
        object[L"metrics"] = std::move(metricsJson);

        return hlcfg::SerializeJsonText(root);
    }

    std::vector<std::filesystem::path> GetReportHistoryFolders() const {
        std::vector<std::filesystem::path> folders;
        const std::filesystem::path primary = GetGuidedReportFolder();
        if (!primary.empty()) {
            folders.push_back(primary);
        }
        if (!environment_.liveModRoot.empty()) {
            const std::filesystem::path liveFolder = std::filesystem::path(environment_.liveModRoot) / L"reports" / L"guided-tests";
            if (std::find(folders.begin(), folders.end(), liveFolder) == folders.end()) {
                folders.push_back(liveFolder);
            }
        }
        return folders;
    }

    std::vector<std::filesystem::path> GetStockClientPlaytestHistoryFolders() const {
        std::vector<std::filesystem::path> folders;
        const std::filesystem::path primary = GetStockClientPlaytestReportFolder();
        if (!primary.empty()) {
            folders.push_back(primary);
        }
        if (!environment_.liveModRoot.empty()) {
            const std::filesystem::path liveFolder = std::filesystem::path(environment_.liveModRoot) / L"reports" / L"stock-client-playtests";
            if (std::find(folders.begin(), folders.end(), liveFolder) == folders.end()) {
                folders.push_back(liveFolder);
            }
        }
        return folders;
    }

    std::wstring GetReportTextValue(const std::wstring& text, const std::wstring& key) const {
        std::wstringstream stream(text);
        std::wstring line;
        const std::wstring expected = key + L":";
        while (std::getline(stream, line)) {
            const std::wstring trimmed = hlcfg::Trimmed(line);
            if (trimmed.rfind(expected, 0) == 0) {
                return hlcfg::Trimmed(trimmed.substr(expected.size()));
            }
        }
        return {};
    }

    std::wstring GetReportTextEqualsValue(const std::wstring& text, const std::wstring& key) const {
        std::wstringstream stream(text);
        std::wstring line;
        const std::wstring expected = key + L"=";
        while (std::getline(stream, line)) {
            const std::wstring trimmed = hlcfg::Trimmed(line);
            if (trimmed.rfind(expected, 0) == 0) {
                return hlcfg::Trimmed(trimmed.substr(expected.size()));
            }
        }
        return {};
    }

    std::wstring AnalyzerMetricValue(const std::wstring& analyzerText, const std::wstring& label) const {
        std::wstring labelLower = label;
        std::transform(labelLower.begin(), labelLower.end(), labelLower.begin(), [](wchar_t ch) {
            return static_cast<wchar_t>(std::towlower(ch));
        });

        std::wstringstream stream(analyzerText);
        std::wstring line;
        while (std::getline(stream, line)) {
            std::wstring trimmed = hlcfg::Trimmed(line);
            std::wstring lower = trimmed;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t ch) {
                return static_cast<wchar_t>(std::towlower(ch));
            });

            if (lower.rfind(labelLower, 0) != 0) {
                continue;
            }

            const std::size_t colon = trimmed.find(L':');
            if (colon == std::wstring::npos) {
                continue;
            }
            return hlcfg::Trimmed(trimmed.substr(colon + 1));
        }
        return {};
    }

    int ParseMetricInt(const std::wstring& value) const {
        const std::wstring trimmed = hlcfg::Trimmed(value);
        if (trimmed.empty() || trimmed == L"n/a") {
            return -1;
        }

        std::size_t index = 0;
        while (index < trimmed.size() && !std::iswdigit(trimmed[index]) && trimmed[index] != L'-') {
            ++index;
        }
        if (index >= trimmed.size()) {
            return -1;
        }

        wchar_t* end = nullptr;
        const long parsed = std::wcstol(trimmed.c_str() + index, &end, 10);
        return end != trimmed.c_str() + index ? static_cast<int>(parsed) : -1;
    }

    int AnalyzerMetricInt(const std::wstring& analyzerText, std::initializer_list<const wchar_t*> labels) const {
        for (const wchar_t* label : labels) {
            const int value = ParseMetricInt(AnalyzerMetricValue(analyzerText, label));
            if (value >= 0) {
                return value;
            }
        }
        return -1;
    }

    std::wstring BuildConsistencyWarningSummary(const std::wstring& analyzerText) const {
        std::wstring summary;
        std::wstringstream stream(analyzerText);
        std::wstring line;
        bool inWarnings = false;
        int warningCount = 0;
        while (std::getline(stream, line)) {
            const std::wstring trimmed = hlcfg::Trimmed(line);
            std::wstring lower = trimmed;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t ch) {
                return static_cast<wchar_t>(std::towlower(ch));
            });

            if (lower.rfind(L"consistent ", 0) == 0) {
                summary += trimmed + L"; ";
                continue;
            }
            if (lower == L"warnings") {
                inWarnings = true;
                continue;
            }
            if (inWarnings) {
                if (trimmed.empty()) {
                    continue;
                }
                if (lower == L"evidence only:" || lower.rfind(L"evidence only", 0) == 0) {
                    break;
                }
                if (trimmed.rfind(L"-", 0) == 0 || trimmed.rfind(L"\u2022", 0) == 0) {
                    ++warningCount;
                    if (warningCount <= 3) {
                        summary += trimmed + L"; ";
                    }
                }
            }
        }

        if (summary.empty()) {
            return L"n/a";
        }
        if (summary.size() >= 2 && summary.substr(summary.size() - 2) == L"; ") {
            summary.resize(summary.size() - 2);
        }
        return summary;
    }

    GuidedReportMetrics BuildMetricsFromAnalyzerText(const std::wstring& analyzerText) const {
        GuidedReportMetrics metrics;
        metrics.acceptedShots = AnalyzerMetricInt(analyzerText, {L"accepted shots"});
        metrics.hitEvents = AnalyzerMetricInt(analyzerText, {L"hit events"});
        metrics.killEvents = AnalyzerMetricInt(analyzerText, {L"kill events"});
        metrics.headshotHits = AnalyzerMetricInt(analyzerText, {L"headshot hits", L"target hs hits", L"player hs hits"});
        metrics.headshotKills = AnalyzerMetricInt(analyzerText, {L"headshot kills", L"target hs kills", L"player hs kills"});
        metrics.patternEvidence = AnalyzerMetricInt(analyzerText, {L"pattern evidence"});
        metrics.cadenceEvidence = AnalyzerMetricInt(analyzerText, {L"cadence growth evidence", L"cadence evidence"});
        metrics.burstGrowthEvidence = AnalyzerMetricInt(analyzerText, {L"burst resets", L"cadence growth evidence"});

        const int dummyArmor = AnalyzerMetricInt(analyzerText, {L"direct dummy armor evid."});
        const int playerArmor = AnalyzerMetricInt(analyzerText, {L"direct player armor evid."});
        const int armoredTargetHits = AnalyzerMetricInt(analyzerText, {L"armored target hits"});
        metrics.armorEvidence = 0;
        bool hasArmorMetric = false;
        for (int value : {dummyArmor, playerArmor, armoredTargetHits}) {
            if (value >= 0) {
                metrics.armorEvidence += value;
                hasArmorMetric = true;
            }
        }
        if (!hasArmorMetric) {
            metrics.armorEvidence = -1;
        }

        metrics.spreadSummary = AnalyzerMetricValue(analyzerText, L"spread min/avg/max");
        metrics.damageSummary = AnalyzerMetricValue(analyzerText, L"applied dmg min/avg/max");
        metrics.consistencyWarnings = BuildConsistencyWarningSummary(analyzerText);
        metrics.analyzerSummaryText = BuildQuickTelemetrySummary(analyzerText);
        return metrics;
    }

    GuidedReportMetrics BuildMetricsFromAnalyzerFile(const std::wstring& analyzerPath) const {
        if (analyzerPath.empty()) {
            return {};
        }

        std::wstring analyzerText;
        std::wstring errorMessage;
        if (!ReadUtf8TextFile(std::filesystem::path(analyzerPath), analyzerText, errorMessage)) {
            return {};
        }

        return BuildMetricsFromAnalyzerText(analyzerText);
    }

    std::wstring ScoreText(int value) const {
        return value >= 0 ? std::to_wstring(value) : L"n/a";
    }

    std::wstring NotesPreview(const std::wstring& value) const {
        std::wstring preview = hlcfg::Trimmed(value);
        std::replace(preview.begin(), preview.end(), L'\r', L' ');
        std::replace(preview.begin(), preview.end(), L'\n', L' ');
        if (preview.empty()) {
            return L"n/a";
        }
        constexpr std::size_t kMaxPreview = 72;
        if (preview.size() > kMaxPreview) {
            preview = preview.substr(0, kMaxPreview - 3) + L"...";
        }
        return preview;
    }

    GuidedReportScorecard LoadScorecardFields(
        const hlcfg::JsonValue::Object& rootObject,
        const hlcfg::JsonValue::Object& entryObject,
        const std::filesystem::path& scorecardPath,
        const std::filesystem::path& scorecardTextPath) const {
        GuidedReportScorecard scorecard;
        scorecard.hasScorecard = true;
        scorecard.scorecardPath = scorecardPath.wstring();
        std::error_code existsError;
        if (std::filesystem::exists(scorecardTextPath, existsError) && !existsError) {
            scorecard.scorecardTextPath = scorecardTextPath.wstring();
        }
        scorecard.skipped = ReadJsonBool(entryObject, L"skipped");
        scorecard.skipReason = ReadJsonString(entryObject, L"skip_reason");
        scorecard.telemetryFresh = ReadJsonString(entryObject, L"telemetry_fresh");
        scorecard.telemetryEventCount = ReadJsonInt(entryObject, L"telemetry_event_count");
        scorecard.telemetryReason = ReadJsonString(entryObject, L"telemetry_reason");
        scorecard.analyzerReportPath = ReadJsonString(entryObject, L"analyzer_report");
        scorecard.analyzerStatus = ReadJsonString(entryObject, L"analyzer_status");
        scorecard.analyzerError = ReadJsonString(entryObject, L"analyzer_error");
        scorecard.notes = ReadJsonString(entryObject, L"notes");
        scorecard.globalNotes = ReadJsonString(entryObject, L"global_notes");
        if (scorecard.globalNotes.empty()) {
            scorecard.globalNotes = ReadJsonString(rootObject, L"global_notes");
        }
        scorecard.warnings = ReadJsonString(entryObject, L"warnings");

        if (const hlcfg::JsonValue* ratingsValue = FindJsonMember(entryObject, L"ratings"); ratingsValue != nullptr && ratingsValue->IsObject()) {
            const auto& ratings = ratingsValue->AsObject();
            scorecard.overallFeel = ReadJsonInt(ratings, L"overall_feel");
            scorecard.singleShotAccuracy = ReadJsonInt(ratings, L"single_shot_accuracy");
            scorecard.spamSprayPenalty = ReadJsonInt(ratings, L"spam_spray_penalty");
            scorecard.movementPenaltyFeel = ReadJsonInt(ratings, L"movement_penalty_feel");
            scorecard.headshotFeel = ReadJsonInt(ratings, L"headshot_feel");
            scorecard.clientVisualSync = ReadJsonInt(ratings, L"client_visual_sync");
            scorecard.shortBurstFeel = ReadJsonInt(ratings, L"short_burst_feel");
            scorecard.longSprayPunishment = ReadJsonInt(ratings, L"long_spray_punishment");
            scorecard.pelletConsistencyFeel = ReadJsonInt(ratings, L"pellet_consistency_feel");
            scorecard.closeRangeLethalityFeel = ReadJsonInt(ratings, L"close_range_lethality_feel");
        }

        return scorecard;
    }

    void ApplyStockScorecardRootFields(const hlcfg::JsonValue::Object& rootObject, GuidedReportEntry& entry) const {
        entry.timestamp = ReadJsonString(rootObject, L"timestamp");
        entry.sourceType = L"match_pack";
        entry.sourceValue = ReadJsonString(rootObject, L"match_pack");
        entry.targetProfile = ReadJsonString(rootObject, L"target_profile");
        entry.targetSpot = ReadJsonString(rootObject, L"target_spot");
        entry.scenario = ReadJsonBool(rootObject, L"scorecard_only") ? L"stock-client scorecard-only" : L"stock-client playtest";
        entry.reportKind = L"stock-client";
    }

    bool LoadStockClientScorecardJson(
        const std::filesystem::path& reportDirectory,
        const std::filesystem::path& scorecardPath,
        std::vector<GuidedReportEntry>& entries) const {
        std::wstring text;
        std::wstring errorMessage;
        if (!ReadUtf8TextFile(scorecardPath, text, errorMessage)) {
            return false;
        }

        hlcfg::JsonValue root;
        if (!hlcfg::ParseJsonText(text, root, errorMessage) || !root.IsObject()) {
            return false;
        }

        const auto& rootObject = root.AsObject();
        const std::filesystem::path summaryPath = reportDirectory / L"summary.txt";
        const std::filesystem::path scorecardTextPath = reportDirectory / L"scorecard.txt";
        const std::filesystem::path defaultReportPath = std::filesystem::exists(summaryPath) ? summaryPath : scorecardPath;

        const hlcfg::JsonValue* scoreEntriesValue = FindJsonMember(rootObject, L"entries");
        if (scoreEntriesValue == nullptr || !scoreEntriesValue->IsArray()) {
            return false;
        }

        bool addedAny = false;
        for (const hlcfg::JsonValue& scoreEntryValue : scoreEntriesValue->AsArray()) {
            if (!scoreEntryValue.IsObject()) {
                continue;
            }

            const auto& scoreEntryObject = scoreEntryValue.AsObject();
            GuidedReportEntry entry;
            ApplyStockScorecardRootFields(rootObject, entry);
            entry.weapon = ReadJsonString(scoreEntryObject, L"weapon");
            entry.reportPath = defaultReportPath.wstring();
            entry.jsonPath = scorecardPath.wstring();
            entry.hasJsonSidecar = true;
            entry.scorecard = LoadScorecardFields(rootObject, scoreEntryObject, scorecardPath, scorecardTextPath);
            entry.analyzedLogPath = entry.scorecard.analyzerReportPath;
            entry.metrics = BuildMetricsFromAnalyzerFile(entry.scorecard.analyzerReportPath);
            if (entry.timestamp.empty()) {
                entry.timestamp = reportDirectory.filename().wstring();
            }
            entries.push_back(std::move(entry));
            addedAny = true;
        }

        return addedAny;
    }

    bool LoadStockClientSummaryReport(const std::filesystem::path& reportDirectory, GuidedReportEntry& entry) const {
        const std::filesystem::path summaryPath = reportDirectory / L"summary.txt";
        std::wstring summaryText;
        std::wstring errorMessage;
        if (!ReadUtf8TextFile(summaryPath, summaryText, errorMessage)) {
            return false;
        }

        entry.reportKind = L"stock-client";
        entry.reportPath = summaryPath.wstring();
        entry.timestamp = GetReportTextEqualsValue(summaryText, L"timestamp");
        entry.weapon = GetReportTextEqualsValue(summaryText, L"weapon_selection");
        entry.sourceType = L"match_pack";
        entry.sourceValue = GetReportTextEqualsValue(summaryText, L"match_pack");
        entry.targetProfile = GetReportTextEqualsValue(summaryText, L"target_profile");
        entry.targetSpot = GetReportTextEqualsValue(summaryText, L"target_spot");
        entry.scenario = L"stock-client playtest";
        if (entry.timestamp.empty()) {
            entry.timestamp = reportDirectory.filename().wstring();
        }

        const std::filesystem::path analysisPath = reportDirectory / (entry.weapon + L"-analysis.txt");
        std::error_code existsError;
        if (!entry.weapon.empty() && std::filesystem::exists(analysisPath, existsError) && !existsError) {
            entry.analyzedLogPath = analysisPath.wstring();
            entry.metrics = BuildMetricsFromAnalyzerFile(entry.analyzedLogPath);
        }

        entry.parsedFromText = true;
        return true;
    }

    bool LoadGuidedReportJson(const std::filesystem::path& jsonPath, GuidedReportEntry& entry) const {
        std::wstring text;
        std::wstring errorMessage;
        if (!ReadUtf8TextFile(jsonPath, text, errorMessage)) {
            return false;
        }

        hlcfg::JsonValue root;
        if (!hlcfg::ParseJsonText(text, root, errorMessage) || !root.IsObject()) {
            return false;
        }

        const auto& object = root.AsObject();
        entry.reportKind = L"guided";
        entry.timestamp = ReadJsonString(object, L"timestamp");
        entry.weapon = ReadJsonString(object, L"weapon");
        entry.sourceType = ReadJsonString(object, L"source_type");
        entry.sourceValue = ReadJsonString(object, L"source_value");
        entry.targetProfile = ReadJsonString(object, L"target_profile");
        entry.targetSpot = ReadJsonString(object, L"target_spot");
        entry.scenario = ReadJsonString(object, L"scenario");
        entry.baselineLogPath = ReadJsonString(object, L"baseline_log");
        entry.analyzedLogPath = ReadJsonString(object, L"analyzed_log");
        const std::wstring reportPath = ReadJsonString(object, L"report_path");
        if (!reportPath.empty()) {
            entry.reportPath = reportPath;
        }
        entry.metrics.analyzerSummaryText = ReadJsonString(object, L"analyzer_summary_text");

        if (const hlcfg::JsonValue* metricsValue = FindJsonMember(object, L"metrics"); metricsValue != nullptr && metricsValue->IsObject()) {
            const auto& metricsObject = metricsValue->AsObject();
            entry.metrics.acceptedShots = ReadJsonInt(metricsObject, L"accepted_shots");
            entry.metrics.hitEvents = ReadJsonInt(metricsObject, L"hit_count");
            entry.metrics.killEvents = ReadJsonInt(metricsObject, L"kill_count");
            entry.metrics.headshotHits = ReadJsonInt(metricsObject, L"headshot_hits");
            entry.metrics.headshotKills = ReadJsonInt(metricsObject, L"headshot_kills");
            entry.metrics.patternEvidence = ReadJsonInt(metricsObject, L"pattern_evidence");
            entry.metrics.cadenceEvidence = ReadJsonInt(metricsObject, L"cadence_evidence");
            entry.metrics.burstGrowthEvidence = ReadJsonInt(metricsObject, L"burst_growth_evidence");
            entry.metrics.armorEvidence = ReadJsonInt(metricsObject, L"armor_evidence");
            entry.metrics.spreadSummary = ReadJsonString(metricsObject, L"spread_summary");
            entry.metrics.damageSummary = ReadJsonString(metricsObject, L"damage_summary");
            entry.metrics.consistencyWarnings = ReadJsonString(metricsObject, L"consistency_warnings");
        }

        entry.jsonPath = jsonPath.wstring();
        entry.hasJsonSidecar = true;
        return true;
    }

    bool LoadGuidedReportText(const std::filesystem::path& reportPath, GuidedReportEntry& entry) const {
        std::wstring text;
        std::wstring errorMessage;
        if (!ReadUtf8TextFile(reportPath, text, errorMessage)) {
            return false;
        }

        if (entry.timestamp.empty()) {
            entry.timestamp = GetReportTextValue(text, L"timestamp");
        }
        if (entry.weapon.empty()) {
            entry.weapon = GetReportTextValue(text, L"weapon");
        }
        if (entry.sourceType.empty()) {
            entry.sourceType = GetReportTextValue(text, L"source_type");
        }
        if (entry.sourceValue.empty()) {
            entry.sourceValue = GetReportTextValue(text, L"source_value");
        }
        if (entry.targetProfile.empty()) {
            entry.targetProfile = GetReportTextValue(text, L"target_profile");
        }
        if (entry.targetSpot.empty()) {
            entry.targetSpot = GetReportTextValue(text, L"target_spot");
        }
        if (entry.scenario.empty()) {
            entry.scenario = GetReportTextValue(text, L"scenario");
        }
        if (entry.baselineLogPath.empty()) {
            entry.baselineLogPath = GetReportTextValue(text, L"baseline_log");
        }
        if (entry.analyzedLogPath.empty()) {
            entry.analyzedLogPath = GetReportTextValue(text, L"analyzed_log");
        }

        const std::size_t analysisOffset = text.find(L"analysis:");
        if (analysisOffset != std::wstring::npos) {
            entry.metrics = BuildMetricsFromAnalyzerText(text.substr(analysisOffset));
        }

        entry.parsedFromText = true;
        return true;
    }

    void LoadGuidedReportHistoryFromDisk() {
        reportEntries_.clear();
        std::map<std::wstring, std::filesystem::path> reportsByStem;

        for (const std::filesystem::path& folder : GetReportHistoryFolders()) {
            std::error_code error;
            if (!std::filesystem::exists(folder, error) || error) {
                continue;
            }

            for (const auto& directoryEntry : std::filesystem::directory_iterator(folder, error)) {
                if (error || !directoryEntry.is_regular_file()) {
                    continue;
                }
                if (directoryEntry.path().extension() != L".txt") {
                    continue;
                }
                reportsByStem[directoryEntry.path().stem().wstring()] = directoryEntry.path();
            }
        }

        for (const auto& [stem, reportPath] : reportsByStem) {
            GuidedReportEntry entry;
            entry.reportPath = reportPath.wstring();
            const std::filesystem::path jsonPath = reportPath.parent_path() / (reportPath.stem().wstring() + L".json");
            std::error_code existsError;
            if (std::filesystem::exists(jsonPath, existsError) && !existsError) {
                LoadGuidedReportJson(jsonPath, entry);
            }
            LoadGuidedReportText(reportPath, entry);
            if (entry.timestamp.empty()) {
                entry.timestamp = reportPath.stem().wstring();
            }
            reportEntries_.push_back(std::move(entry));
        }

        for (const std::filesystem::path& folder : GetStockClientPlaytestHistoryFolders()) {
            std::error_code error;
            if (!std::filesystem::exists(folder, error) || error) {
                continue;
            }

            for (const auto& directoryEntry : std::filesystem::directory_iterator(folder, error)) {
                if (error || !directoryEntry.is_directory()) {
                    continue;
                }

                const std::filesystem::path reportDirectory = directoryEntry.path();
                const std::filesystem::path scorecardPath = reportDirectory / L"scorecard.json";
                std::error_code existsError;
                if (std::filesystem::exists(scorecardPath, existsError) && !existsError &&
                    LoadStockClientScorecardJson(reportDirectory, scorecardPath, reportEntries_)) {
                    continue;
                }

                GuidedReportEntry entry;
                if (LoadStockClientSummaryReport(reportDirectory, entry)) {
                    reportEntries_.push_back(std::move(entry));
                }
            }
        }

        std::sort(reportEntries_.begin(), reportEntries_.end(), [](const GuidedReportEntry& left, const GuidedReportEntry& right) {
            return left.timestamp > right.timestamp;
        });
    }

    std::wstring MetricText(int value) const {
        return value >= 0 ? std::to_wstring(value) : L"n/a";
    }

    std::wstring ReportSourceLabel(const GuidedReportEntry& entry) const {
        if (entry.sourceValue.empty()) {
            return entry.sourceType.empty() ? L"n/a" : entry.sourceType;
        }
        if (entry.sourceType.empty()) {
            return entry.sourceValue;
        }
        return entry.sourceType + L": " + entry.sourceValue;
    }

    std::wstring ReportKindLabel(const GuidedReportEntry& entry) const {
        if (!entry.reportKind.empty()) {
            return entry.reportKind;
        }
        return entry.scorecard.hasScorecard ? L"stock-client" : L"guided";
    }

    std::wstring ScorecardNotesText(const GuidedReportEntry& entry) const {
        if (!entry.scorecard.notes.empty()) {
            return entry.scorecard.notes;
        }
        return entry.scorecard.globalNotes;
    }

    std::wstring ReportListLabel(const GuidedReportEntry& entry) const {
        std::wstring label = entry.timestamp.empty() ? L"(unknown time)" : entry.timestamp;
        label += L" | " + ReportKindLabel(entry);
        label += L" | " + (entry.weapon.empty() ? L"weapon?" : entry.weapon);
        label += L" | " + (entry.sourceValue.empty() ? entry.sourceType : entry.sourceValue);
        label += L" | " + (entry.targetProfile.empty() ? L"target?" : entry.targetProfile);
        if (entry.scorecard.hasScorecard) {
            label += L" | scorecard overall=" + ScoreText(entry.scorecard.overallFeel);
            const std::wstring notes = NotesPreview(ScorecardNotesText(entry));
            if (notes != L"n/a") {
                label += L" | " + notes;
            }
        } else {
            label += L" | " + (entry.scenario.empty() ? L"scenario?" : entry.scenario);
        }
        if (!entry.hasJsonSidecar) {
            label += L" | text-only";
        }
        return label;
    }

    void RefreshReportHistory(bool reloadFromDisk) {
        if (reloadFromDisk || reportEntries_.empty()) {
            LoadGuidedReportHistoryFromDisk();
        }

        HWND list = FindControl(IDC_REPORT_LIST);
        if (list != nullptr) {
            const bool wasLoadingControls = loadingControls_;
            loadingControls_ = true;
            ListBox_ResetContent(list);
            for (std::size_t index = 0; index < reportEntries_.size(); ++index) {
                const std::wstring label = ReportListLabel(reportEntries_[index]);
                const int row = static_cast<int>(SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str())));
                if (row >= 0) {
                    SendMessageW(list, LB_SETITEMDATA, row, static_cast<LPARAM>(index));
                }
            }
            if (!reportEntries_.empty()) {
                SendMessageW(list, LB_SETSEL, TRUE, 0);
            }
            loadingControls_ = wasLoadingControls;
        }

        RefreshReportSelectionDetails();
        if (lastReportStatus_.empty()) {
            SetReportStatus(L"Loaded " + std::to_wstring(reportEntries_.size()) + L" report history item(s). Select two or more, then click Compare Selected.");
        } else {
            SetReportStatus(lastReportStatus_);
        }
    }

    std::vector<std::size_t> GetSelectedReportIndices() const {
        std::vector<std::size_t> indices;
        HWND list = FindControl(IDC_REPORT_LIST);
        if (list == nullptr) {
            return indices;
        }

        const int count = static_cast<int>(SendMessageW(list, LB_GETSELCOUNT, 0, 0));
        if (count > 0) {
            std::vector<int> rows(static_cast<std::size_t>(count), 0);
            SendMessageW(list, LB_GETSELITEMS, static_cast<WPARAM>(count), reinterpret_cast<LPARAM>(rows.data()));
            for (int row : rows) {
                const LRESULT itemData = SendMessageW(list, LB_GETITEMDATA, static_cast<WPARAM>(row), 0);
                if (itemData >= 0 && static_cast<std::size_t>(itemData) < reportEntries_.size()) {
                    indices.push_back(static_cast<std::size_t>(itemData));
                }
            }
        } else {
            const int row = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
            if (row != LB_ERR) {
                const LRESULT itemData = SendMessageW(list, LB_GETITEMDATA, static_cast<WPARAM>(row), 0);
                if (itemData >= 0 && static_cast<std::size_t>(itemData) < reportEntries_.size()) {
                    indices.push_back(static_cast<std::size_t>(itemData));
                }
            }
        }

        return indices;
    }

    const GuidedReportEntry* GetPrimarySelectedReport() const {
        const std::vector<std::size_t> selected = GetSelectedReportIndices();
        if (selected.empty()) {
            return nullptr;
        }
        return &reportEntries_[selected.front()];
    }

    std::wstring BuildReportDetailsText(const GuidedReportEntry& entry) const {
        std::wstring text;
        text += L"timestamp: " + entry.timestamp + L"\r\n";
        text += L"kind: " + ReportKindLabel(entry) + L"\r\n";
        text += L"weapon: " + entry.weapon + L"\r\n";
        text += L"source: " + ReportSourceLabel(entry) + L"\r\n";
        text += L"target_profile: " + entry.targetProfile + L"\r\n";
        text += L"target_spot: " + entry.targetSpot + L"\r\n";
        text += L"scenario: " + entry.scenario + L"\r\n";
        text += L"report: " + entry.reportPath + L"\r\n";
        text += L"json_sidecar: " + (entry.hasJsonSidecar ? entry.jsonPath : L"(missing; parsed as text)") + L"\r\n";
        text += L"log: " + entry.analyzedLogPath + L"\r\n";
        if (entry.scorecard.hasScorecard) {
            text += L"\r\nScorecard\r\n";
            text += L"scorecard_json: " + entry.scorecard.scorecardPath + L"\r\n";
            text += L"scorecard_txt: " + (entry.scorecard.scorecardTextPath.empty() ? L"n/a" : entry.scorecard.scorecardTextPath) + L"\r\n";
            text += L"scorecard_skipped: " + std::wstring(entry.scorecard.skipped ? L"yes" : L"no") + L"\r\n";
            if (entry.scorecard.skipped) {
                text += L"skip_reason: " + entry.scorecard.skipReason + L"\r\n";
            }
            text += L"telemetry_fresh: " + (entry.scorecard.telemetryFresh.empty() ? L"n/a" : entry.scorecard.telemetryFresh) + L"\r\n";
            text += L"telemetry_events: " + MetricText(entry.scorecard.telemetryEventCount) + L"\r\n";
            text += L"analyzer_status: " + (entry.scorecard.analyzerStatus.empty() ? L"n/a" : entry.scorecard.analyzerStatus) + L"\r\n";
            text += L"overall/single/spam_or_spray: " + ScoreText(entry.scorecard.overallFeel) + L" / " +
                    ScoreText(entry.scorecard.singleShotAccuracy) + L" / " + ScoreText(entry.scorecard.spamSprayPenalty) + L"\r\n";
            text += L"movement/headshot/visual_sync: " + ScoreText(entry.scorecard.movementPenaltyFeel) + L" / " +
                    ScoreText(entry.scorecard.headshotFeel) + L" / " + ScoreText(entry.scorecard.clientVisualSync) + L"\r\n";
            if (entry.scorecard.shortBurstFeel >= 0 || entry.scorecard.longSprayPunishment >= 0) {
                text += L"mp5 short_burst/long_spray: " + ScoreText(entry.scorecard.shortBurstFeel) + L" / " +
                        ScoreText(entry.scorecard.longSprayPunishment) + L"\r\n";
            }
            if (entry.scorecard.pelletConsistencyFeel >= 0 || entry.scorecard.closeRangeLethalityFeel >= 0) {
                text += L"shotgun pellet_consistency/close_lethality: " + ScoreText(entry.scorecard.pelletConsistencyFeel) + L" / " +
                        ScoreText(entry.scorecard.closeRangeLethalityFeel) + L"\r\n";
            }
            text += L"notes: " + NotesPreview(ScorecardNotesText(entry)) + L"\r\n";
            text += L"scorecard_warnings: " + (entry.scorecard.warnings.empty() ? L"n/a" : entry.scorecard.warnings) + L"\r\n";
        }
        text += L"\r\nAnalyzer metrics\r\n";
        text += L"accepted_shots: " + MetricText(entry.metrics.acceptedShots) + L"\r\n";
        text += L"hits/kills: " + MetricText(entry.metrics.hitEvents) + L" / " + MetricText(entry.metrics.killEvents) + L"\r\n";
        text += L"headshots: " + MetricText(entry.metrics.headshotHits) + L" hits / " + MetricText(entry.metrics.headshotKills) + L" kills\r\n";
        text += L"pattern/cadence/burst: " + MetricText(entry.metrics.patternEvidence) + L" / " +
                MetricText(entry.metrics.cadenceEvidence) + L" / " + MetricText(entry.metrics.burstGrowthEvidence) + L"\r\n";
        text += L"spread: " + (entry.metrics.spreadSummary.empty() ? L"n/a" : entry.metrics.spreadSummary) + L"\r\n";
        text += L"damage: " + (entry.metrics.damageSummary.empty() ? L"n/a" : entry.metrics.damageSummary);
        return text;
    }

    void RefreshReportSelectionDetails() {
        const GuidedReportEntry* entry = GetPrimarySelectedReport();
        SetTextValue(IDC_REPORT_DETAILS, entry == nullptr ? L"Select a report to inspect its metadata and parsed metrics." : BuildReportDetailsText(*entry));
    }

    std::wstring BuildReportComparisonText(const std::vector<std::size_t>& selectedIndices) const {
        if (selectedIndices.size() < 2) {
            return L"Select two or more reports to compare.";
        }

        auto appendRow = [&](std::wstring& text, const std::wstring& label, auto getter) {
            text += label;
            for (std::size_t index : selectedIndices) {
                text += L"\t" + getter(reportEntries_[index]);
            }
            text += L"\r\n";
        };

        std::wstring text = L"metric";
        for (std::size_t ordinal = 0; ordinal < selectedIndices.size(); ++ordinal) {
            text += L"\treport_" + std::to_wstring(ordinal + 1);
        }
        text += L"\r\n";

        appendRow(text, L"file", [](const GuidedReportEntry& entry) { return std::filesystem::path(entry.reportPath).filename().wstring(); });
        appendRow(text, L"timestamp", [](const GuidedReportEntry& entry) { return entry.timestamp; });
        appendRow(text, L"kind", [&](const GuidedReportEntry& entry) { return ReportKindLabel(entry); });
        appendRow(text, L"weapon", [](const GuidedReportEntry& entry) { return entry.weapon; });
        appendRow(text, L"config/preset/pack", [&](const GuidedReportEntry& entry) { return ReportSourceLabel(entry); });
        appendRow(text, L"target", [](const GuidedReportEntry& entry) { return entry.targetProfile; });
        appendRow(text, L"scenario", [](const GuidedReportEntry& entry) { return entry.scenario; });
        appendRow(text, L"accepted shots", [&](const GuidedReportEntry& entry) { return MetricText(entry.metrics.acceptedShots); });
        appendRow(text, L"hits", [&](const GuidedReportEntry& entry) { return MetricText(entry.metrics.hitEvents); });
        appendRow(text, L"kills", [&](const GuidedReportEntry& entry) { return MetricText(entry.metrics.killEvents); });
        appendRow(text, L"headshot hits", [&](const GuidedReportEntry& entry) { return MetricText(entry.metrics.headshotHits); });
        appendRow(text, L"headshot kills", [&](const GuidedReportEntry& entry) { return MetricText(entry.metrics.headshotKills); });
        appendRow(text, L"pattern evidence", [&](const GuidedReportEntry& entry) { return MetricText(entry.metrics.patternEvidence); });
        appendRow(text, L"cadence evidence", [&](const GuidedReportEntry& entry) { return MetricText(entry.metrics.cadenceEvidence); });
        appendRow(text, L"burst growth evidence", [&](const GuidedReportEntry& entry) { return MetricText(entry.metrics.burstGrowthEvidence); });
        appendRow(text, L"armor evidence", [&](const GuidedReportEntry& entry) { return MetricText(entry.metrics.armorEvidence); });
        appendRow(text, L"spread summary", [](const GuidedReportEntry& entry) { return entry.metrics.spreadSummary.empty() ? L"n/a" : entry.metrics.spreadSummary; });
        appendRow(text, L"damage summary", [](const GuidedReportEntry& entry) { return entry.metrics.damageSummary.empty() ? L"n/a" : entry.metrics.damageSummary; });
        appendRow(text, L"consistency warnings", [](const GuidedReportEntry& entry) { return entry.metrics.consistencyWarnings.empty() ? L"n/a" : entry.metrics.consistencyWarnings; });
        appendRow(text, L"scorecard", [](const GuidedReportEntry& entry) {
            return std::wstring(entry.scorecard.hasScorecard ? (entry.scorecard.skipped ? L"skipped" : L"yes") : L"no");
        });
        appendRow(text, L"telemetry freshness", [](const GuidedReportEntry& entry) { return entry.scorecard.telemetryFresh.empty() ? L"n/a" : entry.scorecard.telemetryFresh; });
        appendRow(text, L"overall feel", [&](const GuidedReportEntry& entry) { return ScoreText(entry.scorecard.overallFeel); });
        appendRow(text, L"single-shot accuracy", [&](const GuidedReportEntry& entry) { return ScoreText(entry.scorecard.singleShotAccuracy); });
        appendRow(text, L"spam/spray penalty", [&](const GuidedReportEntry& entry) { return ScoreText(entry.scorecard.spamSprayPenalty); });
        appendRow(text, L"movement penalty feel", [&](const GuidedReportEntry& entry) { return ScoreText(entry.scorecard.movementPenaltyFeel); });
        appendRow(text, L"headshot feel", [&](const GuidedReportEntry& entry) { return ScoreText(entry.scorecard.headshotFeel); });
        appendRow(text, L"visual/desync score", [&](const GuidedReportEntry& entry) { return ScoreText(entry.scorecard.clientVisualSync); });
        appendRow(text, L"short burst feel", [&](const GuidedReportEntry& entry) { return ScoreText(entry.scorecard.shortBurstFeel); });
        appendRow(text, L"long spray punishment", [&](const GuidedReportEntry& entry) { return ScoreText(entry.scorecard.longSprayPunishment); });
        appendRow(text, L"pellet consistency", [&](const GuidedReportEntry& entry) { return ScoreText(entry.scorecard.pelletConsistencyFeel); });
        appendRow(text, L"close-range lethality", [&](const GuidedReportEntry& entry) { return ScoreText(entry.scorecard.closeRangeLethalityFeel); });
        appendRow(text, L"notes preview", [&](const GuidedReportEntry& entry) { return NotesPreview(ScorecardNotesText(entry)); });
        return text;
    }

    void CompareSelectedReports() {
        const std::vector<std::size_t> selected = GetSelectedReportIndices();
        lastReportComparisonText_ = BuildReportComparisonText(selected);
        SetTextValue(IDC_REPORT_COMPARISON, lastReportComparisonText_);
        SetReportStatus(selected.size() < 2 ? L"Select at least two reports before comparing." : L"Comparison refreshed for " + std::to_wstring(selected.size()) + L" report(s).");
    }

    void SetReportStatus(const std::wstring& value) {
        lastReportStatus_ = value;
        const bool wasLoadingControls = loadingControls_;
        loadingControls_ = true;
        SetTextValue(IDC_REPORT_STATUS, value);
        loadingControls_ = wasLoadingControls;
    }

    void OpenFilePath(const std::wstring& path, const wchar_t* emptyMessage) {
        if (path.empty()) {
            SetReportStatus(emptyMessage);
            return;
        }
        const HINSTANCE openResult = ShellExecuteW(hwnd_, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(openResult) <= 32) {
            SetReportStatus(L"Windows could not open:\r\n" + path);
        } else {
            SetReportStatus(L"Opened:\r\n" + path);
        }
    }

    void OpenSelectedReportFile() {
        const GuidedReportEntry* entry = GetPrimarySelectedReport();
        OpenFilePath(entry == nullptr ? L"" : entry->reportPath, L"Select a report before opening it.");
    }

    void OpenSelectedReportLog() {
        const GuidedReportEntry* entry = GetPrimarySelectedReport();
        OpenFilePath(entry == nullptr ? L"" : entry->analyzedLogPath, L"Select a report with an analyzed log before opening the log.");
    }

    void CopySelectedReportPath() {
        const GuidedReportEntry* entry = GetPrimarySelectedReport();
        if (entry == nullptr || entry->reportPath.empty()) {
            SetReportStatus(L"Select a report before copying its path.");
            return;
        }
        SetReportStatus(CopyTextToClipboard(hwnd_, entry->reportPath) ? L"Copied report path." : L"Unable to copy report path.");
    }

    void CopySelectedReportLogPath() {
        const GuidedReportEntry* entry = GetPrimarySelectedReport();
        if (entry == nullptr || entry->analyzedLogPath.empty()) {
            SetReportStatus(L"Select a report with an analyzed log before copying its log path.");
            return;
        }
        SetReportStatus(CopyTextToClipboard(hwnd_, entry->analyzedLogPath) ? L"Copied log path." : L"Unable to copy log path.");
    }

    void CopyReportComparisonSummary() {
        if (lastReportComparisonText_.empty()) {
            CompareSelectedReports();
        }
        if (lastReportComparisonText_.empty()) {
            SetReportStatus(L"No comparison text is available to copy.");
            return;
        }
        SetReportStatus(CopyTextToClipboard(hwnd_, lastReportComparisonText_) ? L"Copied comparison summary." : L"Unable to copy comparison summary.");
    }

    std::filesystem::path GetReportComparisonFolder() const {
        return GetGuidedReportFolder() / L"comparisons";
    }

    void ExportReportComparison() {
        const std::vector<std::size_t> selected = GetSelectedReportIndices();
        lastReportComparisonText_ = BuildReportComparisonText(selected);
        SetTextValue(IDC_REPORT_COMPARISON, lastReportComparisonText_);
        if (selected.size() < 2) {
            SetReportStatus(L"Select at least two reports before exporting a comparison.");
            return;
        }

        std::wstring output;
        output += L"Editor report comparison\r\n";
        output += L"timestamp: " + BuildLocalTimestampText(false) + L"\r\n\r\n";
        output += L"selected_reports:\r\n";
        for (std::size_t index : selected) {
            output += L"- " + reportEntries_[index].reportPath + L"\r\n";
        }
        output += L"\r\ncomparison:\r\n" + lastReportComparisonText_;

        const std::filesystem::path outputPath = GetReportComparisonFolder() / (L"report-comparison-" + BuildLocalTimestampText(true) + L".txt");
        std::wstring errorMessage;
        if (!WriteUtf8TextFile(outputPath, output, errorMessage)) {
            SetReportStatus(L"Unable to export comparison.\r\n\r\n" + errorMessage);
            return;
        }

        SetReportStatus(L"Exported comparison:\r\n" + outputPath.wstring());
    }

    bool IsReportControl(int controlId) const {
        return controlId >= IDC_REPORT_LIST && controlId <= IDC_REPORT_EXPORT_COMPARISON;
    }

    bool IsTelemetryControl(int controlId) const {
        return controlId >= IDC_TELEMETRY_LATEST_LOG && controlId <= IDC_TELEMETRY_COPY_ANALYSIS;
    }

    void SetTelemetryStatus(const std::wstring& value) {
        lastTelemetryStatus_ = value;
        const bool wasLoadingControls = loadingControls_;
        loadingControls_ = true;
        SetTextValue(IDC_TELEMETRY_STATUS, value);
        loadingControls_ = wasLoadingControls;
    }

    std::wstring GetTelemetryWeaponFilter() const {
        std::wstring filter = GetComboSelectionValue(IDC_TELEMETRY_WEAPON_FILTER);
        return filter.empty() ? L"all" : filter;
    }

    std::vector<std::filesystem::path> GetTelemetrySearchFolders() const {
        std::vector<std::filesystem::path> folders;
        auto addFolder = [&](const std::filesystem::path& folder) {
            if (folder.empty()) {
                return;
            }

            for (const std::filesystem::path& existing : folders) {
                if (_wcsicmp(existing.wstring().c_str(), folder.wstring().c_str()) == 0) {
                    return;
                }
            }
            folders.push_back(folder);
        };

        const std::wstring halfLifeRoot = GetHalfLifeRootFolder();
        if (!halfLifeRoot.empty()) {
            addFolder(std::filesystem::path(halfLifeRoot) / L"logs");
        }
        if (!environment_.liveModRoot.empty()) {
            addFolder(std::filesystem::path(environment_.liveModRoot) / L"logs");
        }
        if (!environment_.logsRoot.empty()) {
            addFolder(std::filesystem::path(environment_.logsRoot));
        }
        if (!environment_.stagedLiveModRoot.empty()) {
            addFolder(std::filesystem::path(environment_.stagedLiveModRoot) / L"logs");
        }
        if (!environment_.repoRoot.empty()) {
            addFolder(std::filesystem::path(environment_.repoRoot) / L"logs");
        }

        return folders;
    }

    bool TryFindLatestWeaponLog(std::filesystem::path& latestPath, std::wstring& details) const {
        latestPath.clear();
        std::filesystem::file_time_type latestWriteTime{};
        bool found = false;

        std::wstring searched;
        for (const std::filesystem::path& folder : GetTelemetrySearchFolders()) {
            searched += folder.wstring() + L"\r\n";

            std::error_code error;
            if (!std::filesystem::exists(folder, error) || error) {
                continue;
            }

            for (const auto& entry : std::filesystem::directory_iterator(folder, error)) {
                if (error || !entry.is_regular_file()) {
                    continue;
                }

                const std::filesystem::path path = entry.path();
                const std::wstring fileName = path.filename().wstring();
                if (path.extension() != L".log" || fileName.rfind(L"weapon-debug-", 0) != 0) {
                    continue;
                }

                const std::filesystem::file_time_type writeTime = entry.last_write_time(error);
                if (error) {
                    continue;
                }

                if (!found || writeTime > latestWriteTime) {
                    found = true;
                    latestWriteTime = writeTime;
                    latestPath = path;
                }
            }
        }

        if (!found) {
            details = L"No weapon-debug log was found. Searched:\r\n" + searched;
            return false;
        }

        details = L"Latest weapon log:\r\n" + latestPath.wstring();
        return true;
    }

    bool FindLatestTelemetryLog(bool showStatus) {
        std::filesystem::path latestPath;
        std::wstring details;
        if (!TryFindLatestWeaponLog(latestPath, details)) {
            const bool wasLoadingControls = loadingControls_;
            loadingControls_ = true;
            SetTextValue(IDC_TELEMETRY_LATEST_LOG, L"");
            loadingControls_ = wasLoadingControls;
            if (showStatus) {
                SetTelemetryStatus(details);
            }
            return false;
        }

        const bool wasLoadingControls = loadingControls_;
        loadingControls_ = true;
        SetTextValue(IDC_TELEMETRY_LATEST_LOG, latestPath.wstring());
        SetTextValue(IDC_TELEMETRY_SELECTED_LOG, latestPath.wstring());
        loadingControls_ = wasLoadingControls;

        if (showStatus) {
            SetTelemetryStatus(details);
        }
        return true;
    }

    std::wstring GetSelectedTelemetryLogPath() const {
        std::wstring selected = hlcfg::Trimmed(GetTextValue(IDC_TELEMETRY_SELECTED_LOG));
        if (!selected.empty()) {
            return selected;
        }
        return hlcfg::Trimmed(GetTextValue(IDC_TELEMETRY_LATEST_LOG));
    }

    void RefreshTelemetryStatusPreview(bool overwriteFields) {
        const bool wasLoadingControls = loadingControls_;
        loadingControls_ = true;

        if (overwriteFields || GetComboSelectionValue(IDC_TELEMETRY_WEAPON_FILTER).empty()) {
            SetComboSelectionValue(IDC_TELEMETRY_WEAPON_FILTER, L"all");
        }

        loadingControls_ = wasLoadingControls;

        if (overwriteFields || GetTextValue(IDC_TELEMETRY_LATEST_LOG).empty()) {
            FindLatestTelemetryLog(false);
        }

        if (lastTelemetryStatus_.empty()) {
            std::wstring status = L"Ready. Click Find Latest Log or Analyze Latest after shooting in a live session.\r\n";
            status += L"Analyzer script: ";
            status += GetAnalyzerScriptPath().wstring();
            SetTelemetryStatus(status);
        } else {
            SetTelemetryStatus(lastTelemetryStatus_);
        }

        if (!lastTelemetryAnalysisText_.empty()) {
            SetTextValue(IDC_TELEMETRY_ANALYSIS_TEXT, lastTelemetryAnalysisText_);
        }
    }

    std::filesystem::path GetAnalyzerScriptPath() const {
        if (environment_.repoRoot.empty()) {
            return {};
        }
        return std::filesystem::path(environment_.repoRoot) / L"scripts" / L"analyze-weapon-log.ps1";
    }

    bool RunAnalyzerProcess(const std::filesystem::path& logPath, const std::wstring& weaponFilter, std::wstring& output, std::wstring& errorMessage) const {
        const std::filesystem::path analyzerPath = GetAnalyzerScriptPath();
        std::error_code filesystemError;
        if (analyzerPath.empty() || !std::filesystem::exists(analyzerPath, filesystemError) || filesystemError) {
            errorMessage = L"Analyzer script could not be resolved: " + analyzerPath.wstring();
            return false;
        }
        if (!std::filesystem::exists(logPath, filesystemError) || filesystemError) {
            errorMessage = L"Selected weapon log was not found: " + logPath.wstring();
            return false;
        }

        std::wstring outputPath;
        if (!CreateTemporaryTextFilePath(outputPath, errorMessage)) {
            return false;
        }

        const std::wstring script =
            L"& { try { & " + QuotePowerShellLiteral(analyzerPath.wstring()) +
            L" -Path " + QuotePowerShellLiteral(logPath.wstring()) +
            L" -Weapon " + QuotePowerShellLiteral(weaponFilter.empty() ? L"all" : weaponFilter) +
            L" *>&1 | Out-String -Width 4096 | Set-Content -LiteralPath " + QuotePowerShellLiteral(outputPath) +
            L" -Encoding UTF8; exit $LASTEXITCODE } catch { $_ | Out-String -Width 4096 | Set-Content -LiteralPath " +
            QuotePowerShellLiteral(outputPath) + L" -Encoding UTF8; exit 1 } }";

        std::wstring commandLine = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command " + QuoteWindowsCommandLineArgument(script);

        STARTUPINFOW startupInfo{};
        startupInfo.cb = sizeof(startupInfo);
        PROCESS_INFORMATION processInfo{};
        std::wstring mutableCommandLine = commandLine;
        if (!CreateProcessW(
                nullptr,
                mutableCommandLine.data(),
                nullptr,
                nullptr,
                FALSE,
                CREATE_NO_WINDOW,
                nullptr,
                environment_.repoRoot.empty() ? nullptr : environment_.repoRoot.c_str(),
                &startupInfo,
                &processInfo)) {
            DeleteFileW(outputPath.c_str());
            errorMessage = L"Unable to launch powershell.exe. Win32 error " + std::to_wstring(GetLastError()) + L".";
            return false;
        }

        const DWORD waitResult = WaitForSingleObject(processInfo.hProcess, 60000);
        if (waitResult == WAIT_TIMEOUT) {
            TerminateProcess(processInfo.hProcess, 1);
            CloseHandle(processInfo.hThread);
            CloseHandle(processInfo.hProcess);
            DeleteFileW(outputPath.c_str());
            errorMessage = L"Analyzer timed out after 60 seconds.";
            return false;
        }

        DWORD exitCode = 1;
        GetExitCodeProcess(processInfo.hProcess, &exitCode);
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);

        std::wstring readError;
        if (!ReadUtf8TextFile(outputPath, output, readError)) {
            DeleteFileW(outputPath.c_str());
            errorMessage = L"Analyzer finished, but its output could not be read. " + readError;
            return false;
        }

        DeleteFileW(outputPath.c_str());
        if (exitCode != 0) {
            errorMessage = L"Analyzer exited with code " + std::to_wstring(exitCode) + L".";
            if (!output.empty()) {
                errorMessage += L"\r\n\r\n" + output;
            }
            return false;
        }

        return true;
    }

    std::wstring BuildQuickTelemetrySummary(const std::wstring& analyzerOutput) const {
        std::wstring summary;
        std::wstringstream stream(analyzerOutput);
        std::wstring line;
        int matched = 0;
        const wchar_t* interesting[] = {
            L"accepted shots",
            L"hit events",
            L"kill events",
            L"headshot",
            L"pattern evidence",
            L"cadence evidence",
            L"consistent dummy",
            L"warnings",
        };

        while (std::getline(stream, line) && matched < 8) {
            std::wstring lower = line;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t ch) {
                return static_cast<wchar_t>(std::towlower(ch));
            });
            for (const wchar_t* marker : interesting) {
                if (lower.find(marker) != std::wstring::npos) {
                    summary += hlcfg::Trimmed(line) + L"\r\n";
                    ++matched;
                    break;
                }
            }
        }

        return summary;
    }

    void AnalyzeTelemetryLog(const std::filesystem::path& logPath) {
        const std::wstring weaponFilter = GetTelemetryWeaponFilter();
        std::wstring output;
        std::wstring errorMessage;
        if (!RunAnalyzerProcess(logPath, weaponFilter, output, errorMessage)) {
            lastTelemetryAnalysisText_ = output.empty() ? errorMessage : output;
            SetTextValue(IDC_TELEMETRY_ANALYSIS_TEXT, lastTelemetryAnalysisText_);
            SetTelemetryStatus(L"Analyzer failed for:\r\n" + logPath.wstring() + L"\r\n\r\n" + errorMessage);
            return;
        }

        const std::wstring quickSummary = BuildQuickTelemetrySummary(output);
        lastTelemetryAnalysisText_ = quickSummary.empty() ? output : (L"Quick evidence summary\r\n" + quickSummary + L"\r\nAnalyzer output\r\n" + output);
        SetTextValue(IDC_TELEMETRY_ANALYSIS_TEXT, lastTelemetryAnalysisText_);
        SetTelemetryStatus(L"Analyzer succeeded.\r\nLog: " + logPath.wstring() + L"\r\nWeapon filter: " + weaponFilter);
    }

    void BrowseTelemetryLog() {
        std::wstring initialFolder;
        const std::wstring selected = GetSelectedTelemetryLogPath();
        if (!selected.empty()) {
            initialFolder = std::filesystem::path(selected).parent_path().wstring();
        } else if (!environment_.logsRoot.empty()) {
            initialFolder = environment_.logsRoot;
        }

        const std::wstring path = ShowOpenLogDialog(hwnd_, initialFolder);
        if (path.empty()) {
            return;
        }

        const bool wasLoadingControls = loadingControls_;
        loadingControls_ = true;
        SetTextValue(IDC_TELEMETRY_SELECTED_LOG, path);
        loadingControls_ = wasLoadingControls;
        SetTelemetryStatus(L"Selected weapon log:\r\n" + path);
    }

    void AnalyzeLatestTelemetryLog() {
        if (!FindLatestTelemetryLog(true)) {
            return;
        }
        AnalyzeTelemetryLog(std::filesystem::path(GetSelectedTelemetryLogPath()));
    }

    void AnalyzeSelectedTelemetryLog() {
        const std::wstring selected = GetSelectedTelemetryLogPath();
        if (selected.empty()) {
            SetTelemetryStatus(L"No selected weapon log. Click Find Latest Log or Browse first.");
            return;
        }
        AnalyzeTelemetryLog(std::filesystem::path(selected));
    }

    void OpenTelemetryLogFolder() {
        const std::wstring selected = GetSelectedTelemetryLogPath();
        if (!selected.empty()) {
            OpenResolvedFolder(std::filesystem::path(selected).parent_path().wstring(), L"The log folder could not be resolved.", L"Opened telemetry log folder");
            return;
        }
        OpenResolvedFolder(environment_.logsRoot, L"The repo logs folder could not be resolved.", L"Opened telemetry log folder");
    }

    void CopyTelemetryLogPath() {
        const std::wstring selected = GetSelectedTelemetryLogPath();
        if (selected.empty()) {
            SetTelemetryStatus(L"No weapon log path is selected.");
            return;
        }

        if (!CopyTextToClipboard(hwnd_, selected)) {
            SetTelemetryStatus(L"Unable to copy weapon log path.");
            return;
        }

        SetTelemetryStatus(L"Copied weapon log path:\r\n" + selected);
    }

    void CopyTelemetryAnalysisText() {
        const std::wstring text = GetTextValue(IDC_TELEMETRY_ANALYSIS_TEXT);
        if (hlcfg::Trimmed(text).empty()) {
            SetTelemetryStatus(L"No analyzer output is available to copy.");
            return;
        }

        if (!CopyTextToClipboard(hwnd_, text)) {
            SetTelemetryStatus(L"Unable to copy analyzer output.");
            return;
        }

        SetTelemetryStatus(L"Copied analyzer output.");
    }

    HINSTANCE instance_ = nullptr;
    std::wstring moduleFilePath_;
    hlcfg::EnvironmentPaths environment_;
    HWND hwnd_ = nullptr;
    HWND tab_ = nullptr;
    HWND pages_[kPageCount]{};
    hlcfg::ProjectDocument document_;
    std::wstring currentProjectPath_;
    std::wstring lastSuggestedCfgFileName_;
    std::wstring lastActionStatus_;
    std::wstring lastBrowserStatus_;
    std::wstring lastLiveStatus_;
    std::wstring lastGuidedStatus_;
    std::wstring lastGuidedAnalysisText_;
    std::wstring lastGuidedInstructions_;
    std::wstring lastGuidedBaselineLogPath_;
    std::wstring lastGuidedAnalyzedLogPath_;
    std::filesystem::file_time_type lastGuidedBaselineLogWriteTime_{};
    std::vector<std::wstring> lastGuidedCommandSequence_;
    std::wstring lastReportStatus_;
    std::wstring lastReportComparisonText_;
    std::wstring lastTelemetryStatus_;
    std::wstring lastTelemetryAnalysisText_;
    std::vector<std::wstring> lastLiveFallbackCommands_;
    std::vector<GuidedReportEntry> reportEntries_;
    std::vector<BrowserEntry> presetBrowserEntries_;
    std::vector<BrowserEntry> matchPackBrowserEntries_;
    std::vector<std::size_t> presetBrowserVisibleIndices_;
    BrowserEntryKind activeBrowserEntryKind_ = BrowserEntryKind::None;
    std::size_t activeBrowserEntryIndex_ = 0;
    bool dirty_ = false;
    bool loadingControls_ = false;
};

bool ValidateContains(const std::wstring& text, const std::wstring& expected) {
    return text.find(expected) != std::wstring::npos;
}

bool WriteSummaryFile(const std::filesystem::path& path, const std::wstring& text) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        return false;
    }

    const int required = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), utf8.data(), required, nullptr, nullptr);
    stream.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
    return stream.good();
}

int RunSelfTestInternal(const std::wstring& moduleFilePath) {
    hlcfg::EnvironmentPaths environment = hlcfg::ResolveEnvironmentPaths(moduleFilePath);
    const std::filesystem::path root = environment.repoRoot.empty()
                                           ? std::filesystem::temp_directory_path() / L"HlConfigEditorCppSelfTest"
                                           : std::filesystem::path(environment.repoRoot) / L"artifacts" / L"HlConfigEditorCppSelfTest";

    std::error_code error;
    std::filesystem::create_directories(root, error);
    if (error) {
        return 1;
    }

    const std::filesystem::path exportRoot = environment.liveModRoot.empty()
                                                 ? (root / L"hlserver_testbed")
                                                 : std::filesystem::path(environment.liveModRoot);

    hlcfg::ProjectDocument glock = hlcfg::CreateDefaultProject();
    glock.metadata.projectName = L"SelfTest Glock";
    glock.general.sessionTag = L"editor_cfg_test";
    glock.general.debugWeaponLog = true;
    glock.general.debugWeaponLogRejections = true;
    glock.exportSettings.exportFolder = exportRoot.wstring();
    glock.exportSettings.cfgFileName = L"editor_glock_simple.cfg";
    hlcfg::ApplyGlockPreset(glock, L"glock_cadence_tight");
    glock.glock.profileName = L"editor_glock_simple";
    hlcfg::ApplyDummyPreset(glock, L"vest_headprotected");

    const std::filesystem::path glockProjectPath = root / L"editor_glock_simple.hlcfg.json";
    std::wstring errorMessage;
    if (!hlcfg::SaveProjectDocumentToFile(glock, glockProjectPath.wstring(), errorMessage)) {
        return 1;
    }

    hlcfg::ProjectDocument loadedGlock;
    if (!hlcfg::LoadProjectDocumentFromFile(glockProjectPath.wstring(), loadedGlock, errorMessage)) {
        return 1;
    }

    hlcfg::ExportResult glockExport;
    if (!hlcfg::ExportCfgToFile(loadedGlock, environment, glockExport, errorMessage)) {
        return 1;
    }

    if (!ValidateContains(glockExport.cfgText, L"sv_exp_weapon_under_test \"glock\"") ||
        !ValidateContains(glockExport.cfgText, L"sv_exp_session_tag \"editor_cfg_test\"") ||
        !ValidateContains(glockExport.cfgText, L"sv_exp_glock_profile_name \"editor_glock_simple\"") ||
        !ValidateContains(glockExport.cfgText, L"sv_exp_glock_primary_shot_growth 0.05") ||
        !ValidateContains(glockExport.cfgText, L"sv_exp_glock_primary_cadence_mode 1") ||
        !ValidateContains(glockExport.cfgText, L"sv_exp_glock_primary_cycle_time 0.47") ||
        !ValidateContains(glockExport.cfgText, L"sv_exp_glock_primary_click_penalty 0.04") ||
        !ValidateContains(glockExport.cfgText, L"sv_exp_glock_primary_click_penalty_scale 1.6") ||
        !ValidateContains(glockExport.cfgText, L"sv_exp_glock_primary_click_reset_time 0.6") ||
        !ValidateContains(glockExport.cfgText, L"sv_exp_glock_primary_hold_penalty_scale 0.6") ||
        !ValidateContains(glockExport.cfgText, L"sv_exp_glock_pattern_mode 1") ||
        !ValidateContains(glockExport.cfgText, L"sv_exp_glock_pattern_scale_y 0.42") ||
        !ValidateContains(glockExport.cfgText, L"sv_exp_glock_pattern_reset_time 0.32") ||
        !ValidateContains(glockExport.cfgText, L"sv_exp_glock_primary_headshot_lethal 1") ||
        !ValidateContains(glockExport.cfgText, L"sv_exp_glock_lab_target_profile_name \"vest_headprotected\"") ||
        glockExport.execCommand != L"exec editor_glock_simple.cfg" ||
        glockExport.launcherCommand != L"scripts\\play-hlserver-testbed-direct.bat -CfgProfile \"editor_glock_simple.cfg\"") {
        return 1;
    }

    hlcfg::ProjectDocument mp5 = hlcfg::CreateDefaultProject();
    mp5.metadata.projectName = L"SelfTest MP5";
    mp5.general.sessionTag = L"editor_cfg_test";
    mp5.general.debugWeaponLog = true;
    mp5.general.debugWeaponLogRejections = true;
    mp5.exportSettings.exportFolder = exportRoot.wstring();
    mp5.exportSettings.cfgFileName = L"editor_mp5_simple.cfg";
    hlcfg::ApplyMp5Preset(mp5, L"mp5_pattern_burst");
    mp5.mp5.profileName = L"editor_mp5_simple";
    hlcfg::ApplyDummyPreset(mp5, L"vest");
    mp5.mp5.primaryBurstGrowth = L"0.0180";
    mp5.mp5.primaryBurstMaxAdditionalSpread = L"0.0950";
    mp5.mp5.primarySpreadRecovery = L"0.9500";
    mp5.mp5.primaryBurstResetTime = L"0.3000";
    mp5.mp5.primaryHoldPenaltyScale = L"1.0500";
    mp5.general.debugWeaponLog = true;
    mp5.general.debugWeaponLogRejections = true;

    const std::filesystem::path mp5ProjectPath = root / L"editor_mp5_simple.hlcfg.json";
    if (!hlcfg::SaveProjectDocumentToFile(mp5, mp5ProjectPath.wstring(), errorMessage)) {
        return 1;
    }

    hlcfg::ProjectDocument loadedMp5;
    if (!hlcfg::LoadProjectDocumentFromFile(mp5ProjectPath.wstring(), loadedMp5, errorMessage)) {
        return 1;
    }

    hlcfg::ExportResult mp5Export;
    if (!hlcfg::ExportCfgToFile(loadedMp5, environment, mp5Export, errorMessage)) {
        return 1;
    }

    if (!ValidateContains(mp5Export.cfgText, L"sv_exp_weapon_under_test \"mp5\"") ||
        !ValidateContains(mp5Export.cfgText, L"sv_exp_session_tag \"editor_cfg_test\"") ||
        !ValidateContains(mp5Export.cfgText, L"sv_exp_mp5_primary_enabled 1") ||
        !ValidateContains(mp5Export.cfgText, L"sv_exp_mp5_profile_name \"editor_mp5_simple\"") ||
        !ValidateContains(mp5Export.cfgText, L"sv_exp_mp5_primary_burst_growth 0.018") ||
        !ValidateContains(mp5Export.cfgText, L"sv_exp_mp5_primary_burst_reset_time 0.3") ||
        !ValidateContains(mp5Export.cfgText, L"sv_exp_mp5_primary_hold_penalty_scale 1.05") ||
        !ValidateContains(mp5Export.cfgText, L"sv_exp_mp5_pattern_mode 1") ||
        !ValidateContains(mp5Export.cfgText, L"sv_exp_mp5_pattern_scale_y 0.48") ||
        !ValidateContains(mp5Export.cfgText, L"sv_exp_mp5_pattern_reset_time 0.3") ||
        !ValidateContains(mp5Export.cfgText, L"sv_exp_mp5_lab_loadout 1") ||
        !ValidateContains(mp5Export.cfgText, L"sv_exp_glock_lab_target_profile_name \"vest\"") ||
        mp5Export.execCommand != L"exec editor_mp5_simple.cfg" ||
        mp5Export.launcherCommand != L"scripts\\play-hlserver-testbed-direct.bat -CfgProfile \"editor_mp5_simple.cfg\"") {
        return 1;
    }

    hlcfg::ProjectDocument weapon357 = hlcfg::CreateDefaultProject();
    weapon357.metadata.projectName = L"SelfTest 357";
    weapon357.general.sessionTag = L"editor_cfg_test";
    weapon357.exportSettings.exportFolder = exportRoot.wstring();
    weapon357.exportSettings.cfgFileName = L"editor_357_test.cfg";
    hlcfg::Apply357Preset(weapon357, L"357_cadence_headshot");
    weapon357.weapon357.profileName = L"editor_357_test";
    hlcfg::ApplyDummyPreset(weapon357, L"vest_headprotected");

    const std::filesystem::path weapon357ProjectPath = root / L"editor_357_test.hlcfg.json";
    if (!hlcfg::SaveProjectDocumentToFile(weapon357, weapon357ProjectPath.wstring(), errorMessage)) {
        return 1;
    }

    hlcfg::ProjectDocument loaded357;
    if (!hlcfg::LoadProjectDocumentFromFile(weapon357ProjectPath.wstring(), loaded357, errorMessage)) {
        return 1;
    }

    hlcfg::ExportResult weapon357Export;
    if (!hlcfg::ExportCfgToFile(loaded357, environment, weapon357Export, errorMessage)) {
        return 1;
    }

    if (!ValidateContains(weapon357Export.cfgText, L"sv_exp_weapon_under_test \"357\"") ||
        !ValidateContains(weapon357Export.cfgText, L"sv_exp_session_tag \"editor_cfg_test\"") ||
        !ValidateContains(weapon357Export.cfgText, L"sv_exp_357_primary_enabled 1") ||
        !ValidateContains(weapon357Export.cfgText, L"sv_exp_357_profile_name \"editor_357_test\"") ||
        !ValidateContains(weapon357Export.cfgText, L"sv_exp_357_primary_cadence_mode 1") ||
            !ValidateContains(weapon357Export.cfgText, L"sv_exp_357_primary_cycle_time 1.1") ||
            !ValidateContains(weapon357Export.cfgText, L"sv_exp_357_primary_click_penalty 0.042") ||
            !ValidateContains(weapon357Export.cfgText, L"sv_exp_357_primary_click_penalty_scale 1.4") ||
            !ValidateContains(weapon357Export.cfgText, L"sv_exp_357_primary_click_reset_time 1.35") ||
            !ValidateContains(weapon357Export.cfgText, L"sv_exp_357_primary_hold_penalty_scale 0.4") ||
            !ValidateContains(weapon357Export.cfgText, L"sv_exp_357_pattern_mode 1") ||
            !ValidateContains(weapon357Export.cfgText, L"sv_exp_357_pattern_scale_x 0.21") ||
            !ValidateContains(weapon357Export.cfgText, L"sv_exp_357_pattern_scale_y 0.34") ||
            !ValidateContains(weapon357Export.cfgText, L"sv_exp_357_pattern_reset_time 1.15") ||
            !ValidateContains(weapon357Export.cfgText, L"sv_exp_357_pattern_max_index 4") ||
            !ValidateContains(weapon357Export.cfgText, L"sv_exp_357_lab_loadout 1") ||
            !ValidateContains(weapon357Export.cfgText, L"sv_exp_glock_lab_target_profile_name \"vest_headprotected\"") ||
            weapon357Export.execCommand != L"exec editor_357_test.cfg" ||
        weapon357Export.launcherCommand != L"scripts\\play-hlserver-testbed-direct.bat -CfgProfile \"editor_357_test.cfg\"") {
        return 1;
    }

    hlcfg::ProjectDocument shotgun = hlcfg::CreateDefaultProject();
    shotgun.metadata.projectName = L"SelfTest Shotgun";
    shotgun.general.sessionTag = L"editor_cfg_test";
    shotgun.exportSettings.exportFolder = exportRoot.wstring();
    shotgun.exportSettings.cfgFileName = L"editor_shotgun_test.cfg";
    hlcfg::ApplyShotgunPreset(shotgun, L"shotgun_pattern_tight");
    shotgun.shotgun.profileName = L"editor_shotgun_test";
    hlcfg::ApplyDummyPreset(shotgun, L"vest");

    const std::filesystem::path shotgunProjectPath = root / L"editor_shotgun_test.hlcfg.json";
    if (!hlcfg::SaveProjectDocumentToFile(shotgun, shotgunProjectPath.wstring(), errorMessage)) {
        return 1;
    }

    hlcfg::ProjectDocument loadedShotgun;
    if (!hlcfg::LoadProjectDocumentFromFile(shotgunProjectPath.wstring(), loadedShotgun, errorMessage)) {
        return 1;
    }

    hlcfg::ExportResult shotgunExport;
    if (!hlcfg::ExportCfgToFile(loadedShotgun, environment, shotgunExport, errorMessage)) {
        return 1;
    }

    if (!ValidateContains(shotgunExport.cfgText, L"sv_exp_weapon_under_test \"shotgun\"") ||
        !ValidateContains(shotgunExport.cfgText, L"sv_exp_session_tag \"editor_cfg_test\"") ||
        !ValidateContains(shotgunExport.cfgText, L"sv_exp_shotgun_primary_enabled 1") ||
        !ValidateContains(shotgunExport.cfgText, L"sv_exp_shotgun_profile_name \"editor_shotgun_test\"") ||
        !ValidateContains(shotgunExport.cfgText, L"sv_exp_shotgun_pattern_mode 1") ||
        !ValidateContains(shotgunExport.cfgText, L"sv_exp_shotgun_primary_pellet_spread_mode 1") ||
        !ValidateContains(shotgunExport.cfgText, L"sv_exp_shotgun_primary_shot_growth 0.01") ||
        !ValidateContains(shotgunExport.cfgText, L"sv_exp_shotgun_lab_loadout 1") ||
        !ValidateContains(shotgunExport.cfgText, L"sv_exp_shotgun_primary_pellet_count") ||
        !ValidateContains(shotgunExport.cfgText, L"sv_exp_glock_lab_target_profile_name \"vest\"") ||
        shotgunExport.execCommand != L"exec editor_shotgun_test.cfg" ||
        shotgunExport.launcherCommand != L"scripts\\play-hlserver-testbed-direct.bat -CfgProfile \"editor_shotgun_test.cfg\"") {
        return 1;
    }

    hlcfg::ProjectDocument duel357 = hlcfg::CreateDefaultProject();
    duel357.metadata.projectName = L"SelfTest Duel 357";
    duel357.general.sessionTag = L"editor_match_test";
    duel357.exportSettings.exportFolder = exportRoot.wstring();
    duel357.exportSettings.cfgFileName = L"editor_duel_357.cfg";
    hlcfg::ApplyMatchPreset(duel357, L"duel_357");
    duel357.matchPack.name = L"duel_357";
    duel357.matchPack.description = L"Editor self-test duel 357 pack";
    duel357.matchPack.tags = L"duel,357";

    const std::filesystem::path duel357ProjectPath = root / L"editor_duel_357.hlcfg.json";
    if (!hlcfg::SaveProjectDocumentToFile(duel357, duel357ProjectPath.wstring(), errorMessage)) {
        return 1;
    }

    hlcfg::ProjectDocument loadedDuel357;
    if (!hlcfg::LoadProjectDocumentFromFile(duel357ProjectPath.wstring(), loadedDuel357, errorMessage)) {
        return 1;
    }

    hlcfg::ExportResult duel357Export;
    if (!hlcfg::ExportCfgToFile(loadedDuel357, environment, duel357Export, errorMessage)) {
        return 1;
    }

    if (!ValidateContains(duel357Export.cfgText, L"sv_exp_round_mode 1") ||
        !ValidateContains(duel357Export.cfgText, L"sv_exp_round_loadout_mode \"357\"") ||
        !ValidateContains(duel357Export.cfgText, L"sv_exp_round_weapon_profile \"duel_357\"") ||
        duel357Export.matchPackPath.empty() ||
        !ValidateContains(duel357Export.matchPackText, L"\"name\": \"duel_357\"") ||
        !ValidateContains(duel357Export.matchPackText, L"\"cfg\": \"editor_duel_357.cfg\"") ||
        !std::filesystem::exists(std::filesystem::path(duel357Export.matchPackPath)) ||
        duel357Export.execCommand != L"exec editor_duel_357.cfg" ||
        duel357Export.launcherCommand != L"scripts\\play-hlserver-testbed-direct.bat -CfgProfile \"editor_duel_357.cfg\"") {
        return 1;
    }

    hlcfg::ProjectDocument teamMp5 = hlcfg::CreateDefaultProject();
    teamMp5.metadata.projectName = L"SelfTest Team MP5";
    teamMp5.general.sessionTag = L"editor_match_test";
    teamMp5.exportSettings.exportFolder = exportRoot.wstring();
    teamMp5.exportSettings.cfgFileName = L"editor_team_mp5.cfg";
    hlcfg::ApplyMatchPreset(teamMp5, L"team_mp5");
    teamMp5.matchPack.name = L"team_mp5";
    teamMp5.matchPack.description = L"Editor self-test team MP5 pack";
    teamMp5.matchPack.tags = L"team,mp5";

    const std::filesystem::path teamMp5ProjectPath = root / L"editor_team_mp5.hlcfg.json";
    if (!hlcfg::SaveProjectDocumentToFile(teamMp5, teamMp5ProjectPath.wstring(), errorMessage)) {
        return 1;
    }

    hlcfg::ProjectDocument loadedTeamMp5;
    if (!hlcfg::LoadProjectDocumentFromFile(teamMp5ProjectPath.wstring(), loadedTeamMp5, errorMessage)) {
        return 1;
    }

    hlcfg::ExportResult teamMp5Export;
    if (!hlcfg::ExportCfgToFile(loadedTeamMp5, environment, teamMp5Export, errorMessage)) {
        return 1;
    }

    if (!ValidateContains(teamMp5Export.cfgText, L"sv_exp_round_mode 1") ||
        !ValidateContains(teamMp5Export.cfgText, L"sv_exp_team_round_mode 1") ||
        !ValidateContains(teamMp5Export.cfgText, L"sv_exp_team_round_team1_loadout \"mp5\"") ||
        !ValidateContains(teamMp5Export.cfgText, L"sv_exp_team_round_team2_loadout \"mp5\"") ||
        teamMp5Export.execCommand != L"exec editor_team_mp5.cfg" ||
        teamMp5Export.launcherCommand != L"scripts\\play-hlserver-testbed-direct.bat -CfgProfile \"editor_team_mp5.cfg\"") {
        return 1;
    }

    hlcfg::ProjectDocument buyArmor = hlcfg::CreateDefaultProject();
    buyArmor.metadata.projectName = L"SelfTest Buy Armor";
    buyArmor.general.sessionTag = L"editor_match_test";
    buyArmor.exportSettings.exportFolder = exportRoot.wstring();
    buyArmor.exportSettings.cfgFileName = L"editor_buy_armor.cfg";
    hlcfg::ApplyMatchPreset(buyArmor, L"buy_test");
    buyArmor.matchPack.name = L"buy_test";
    buyArmor.matchPack.description = L"Editor self-test buy and armor pack";
    buyArmor.matchPack.tags = L"buy,armor";

    const std::filesystem::path buyArmorProjectPath = root / L"editor_buy_armor.hlcfg.json";
    if (!hlcfg::SaveProjectDocumentToFile(buyArmor, buyArmorProjectPath.wstring(), errorMessage)) {
        return 1;
    }

    hlcfg::ProjectDocument loadedBuyArmor;
    if (!hlcfg::LoadProjectDocumentFromFile(buyArmorProjectPath.wstring(), loadedBuyArmor, errorMessage)) {
        return 1;
    }

    hlcfg::ExportResult buyArmorExport;
    if (!hlcfg::ExportCfgToFile(loadedBuyArmor, environment, buyArmorExport, errorMessage)) {
        return 1;
    }

    if (!ValidateContains(buyArmorExport.cfgText, L"sv_exp_buy_mode 1") ||
        !ValidateContains(buyArmorExport.cfgText, L"sv_exp_armor_mode 1") ||
        !ValidateContains(buyArmorExport.cfgText, L"sv_exp_helmet_mode 1") ||
        !ValidateContains(buyArmorExport.cfgText, L"sv_exp_buy_cost_handgrenade") ||
        buyArmorExport.execCommand != L"exec editor_buy_armor.cfg" ||
        buyArmorExport.launcherCommand != L"scripts\\play-hlserver-testbed-direct.bat -CfgProfile \"editor_buy_armor.cfg\"") {
        return 1;
    }

    std::wostringstream summary;
    summary << L"glock_project=" << glockProjectPath.wstring() << L"\n";
    summary << L"glock_cfg=" << glockExport.exportPath << L"\n";
    summary << L"glock_exec=" << glockExport.execCommand << L"\n";
    summary << L"mp5_project=" << mp5ProjectPath.wstring() << L"\n";
    summary << L"mp5_cfg=" << mp5Export.exportPath << L"\n";
    summary << L"mp5_exec=" << mp5Export.execCommand << L"\n";
    summary << L"357_project=" << weapon357ProjectPath.wstring() << L"\n";
    summary << L"357_cfg=" << weapon357Export.exportPath << L"\n";
    summary << L"357_exec=" << weapon357Export.execCommand << L"\n";
    summary << L"shotgun_project=" << shotgunProjectPath.wstring() << L"\n";
    summary << L"shotgun_cfg=" << shotgunExport.exportPath << L"\n";
    summary << L"shotgun_exec=" << shotgunExport.execCommand << L"\n";
    summary << L"duel357_project=" << duel357ProjectPath.wstring() << L"\n";
    summary << L"duel357_cfg=" << duel357Export.exportPath << L"\n";
    summary << L"duel357_exec=" << duel357Export.execCommand << L"\n";
    summary << L"duel357_pack=" << duel357Export.matchPackPath << L"\n";
    summary << L"team_mp5_project=" << teamMp5ProjectPath.wstring() << L"\n";
    summary << L"team_mp5_cfg=" << teamMp5Export.exportPath << L"\n";
    summary << L"team_mp5_exec=" << teamMp5Export.execCommand << L"\n";
    summary << L"team_mp5_pack=" << teamMp5Export.matchPackPath << L"\n";
    summary << L"buy_armor_project=" << buyArmorProjectPath.wstring() << L"\n";
    summary << L"buy_armor_cfg=" << buyArmorExport.exportPath << L"\n";
    summary << L"buy_armor_exec=" << buyArmorExport.execCommand << L"\n";
    summary << L"buy_armor_pack=" << buyArmorExport.matchPackPath << L"\n";

    if (!WriteSummaryFile(root / L"selftest-summary.txt", summary.str())) {
        return 1;
    }

    return 0;
}

}  // namespace

int RunEditorApplication(HINSTANCE instance, int commandShow, const std::wstring& moduleFilePath) {
    HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool comInitialized = SUCCEEDED(result);

    EditorWindow window(instance, moduleFilePath);
    const int exitCode = window.Run(commandShow);

    if (comInitialized) {
        CoUninitialize();
    }

    return exitCode;
}

int RunSelfTest(const std::wstring& moduleFilePath) {
    HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool comInitialized = SUCCEEDED(result);
    const int exitCode = RunSelfTestInternal(moduleFilePath);
    if (comInitialized) {
        CoUninitialize();
    }

    return exitCode;
}
