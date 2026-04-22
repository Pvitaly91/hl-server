#include "MainWindow.h"

#include <commctrl.h>
#include <commdlg.h>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <windowsx.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>

#include "CfgExport.h"
#include "ConfigProject.h"
#include "Presets.h"

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

constexpr wchar_t kWindowClassName[] = L"HlConfigEditorCppWindow";
constexpr wchar_t kPageWindowClassName[] = L"HlConfigEditorCppPage";
constexpr wchar_t kWindowTitle[] = L"HL Weapon Config Editor (C++)";
constexpr int kWindowWidth = 1160;
constexpr int kWindowHeight = 860;

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
    IDC_GLOCK_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD,
    IDC_GLOCK_PRIMARY_MAX_SPREAD,
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
    IDC_MP5_PRIMARY_FIRST_SHOT_ACCURACY,
    IDC_MP5_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD,
    IDC_MP5_PRIMARY_MAX_SPREAD,
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
    IDC_357_PRIMARY_DAMAGE,
    IDC_357_PRIMARY_HEADSHOT_SCALE,
    IDC_357_PRIMARY_HEADSHOT_LETHAL,
    IDC_357_LAB_LOADOUT,
    IDC_357_LAB_AMMO,
    IDC_357_LAB_AUTOSWITCH,
    IDC_357_PRESET_DEFAULT,
    IDC_357_PRESET_PRECISION_TEST,
    IDC_357_PRESET_HEADSHOT_TEST,

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

    IDC_DUMMY_ENABLED = 1300,
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

constexpr int kPageCount = 10;
constexpr int kRoundPageIndex = 6;
constexpr int kTeamRoundPageIndex = 7;
constexpr int kBuyEquipmentPageIndex = 8;
constexpr int kExportPageIndex = 9;

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
            ApplyPreset([&] { hlcfg::ApplyGlockPreset(document_, L"cs_like_soft"); });
            return 0;
        case IDC_GLOCK_PRESET_CS_TIGHT:
            ApplyPreset([&] { hlcfg::ApplyGlockPreset(document_, L"cs_tight"); });
            return 0;
        case IDC_GLOCK_PRESET_HEADSHOT_TEST:
            ApplyPreset([&] { hlcfg::ApplyGlockPreset(document_, L"headshot_test"); });
            return 0;
        case IDC_MP5_PRESET_DEFAULT:
            ApplyPreset([&] { hlcfg::ApplyMp5Preset(document_, L"default"); });
            return 0;
        case IDC_MP5_PRESET_CS_BURST:
            ApplyPreset([&] { hlcfg::ApplyMp5Preset(document_, L"cs_burst"); });
            return 0;
        case IDC_MP5_PRESET_CS_MOBILE:
            ApplyPreset([&] { hlcfg::ApplyMp5Preset(document_, L"cs_mobile"); });
            return 0;
        case IDC_MP5_PRESET_SPRAY_TEST:
            ApplyPreset([&] { hlcfg::ApplyMp5Preset(document_, L"spray_test"); });
            return 0;
        case IDC_357_PRESET_DEFAULT:
            ApplyPreset([&] { hlcfg::Apply357Preset(document_, L"default"); });
            return 0;
        case IDC_357_PRESET_PRECISION_TEST:
            ApplyPreset([&] { hlcfg::Apply357Preset(document_, L"precision_test"); });
            return 0;
        case IDC_357_PRESET_HEADSHOT_TEST:
            ApplyPreset([&] { hlcfg::Apply357Preset(document_, L"headshot_test"); });
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

        if (notifyCode == EN_CHANGE || notifyCode == BN_CLICKED || notifyCode == CBN_SELCHANGE) {
            if (controlId != IDC_LIVE_MOD_FOLDER_PREVIEW && controlId != IDC_LAUNCHER_PREVIEW &&
                controlId != IDC_EXEC_PREVIEW && controlId != IDC_CFG_PREVIEW &&
                controlId != IDC_HALF_LIFE_ROOT_PREVIEW && controlId != IDC_QUICK_EXPORT_TARGET_PREVIEW &&
                controlId != IDC_EDITOR_EXE_PATH_PREVIEW && controlId != IDC_EXPORT_STATUS) {
                MaybeRefreshSuggestedCfgFileName(controlId);
                dirty_ = true;
                UpdateWindowTitle();
                RefreshMatchSummary();
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
        CreateButton(page, L"CS-Like Soft", IDC_GLOCK_PRESET_CS_LIKE_SOFT, 175, 45, 150, 24);
        CreateButton(page, L"CS Tight", IDC_GLOCK_PRESET_CS_TIGHT, 340, 45, 120, 24);
        CreateButton(page, L"Headshot Test", IDC_GLOCK_PRESET_HEADSHOT_TEST, 475, 45, 140, 24);

        CreateGroupBox(page, L"General Glock Settings", 20, 110, 500, 185);
        CreateLabel(page, L"Profile name", 40, 145, 120, 20);
        CreateEdit(page, IDC_GLOCK_PROFILE_NAME, 220, 140, 260, 24);
        CreateCheckBox(page, L"Tap-fire only", IDC_GLOCK_TAP_FIRE, 40, 180, 160, 20);
        CreateCheckBox(page, L"First-shot accuracy", IDC_GLOCK_FIRST_SHOT_ACCURACY, 220, 180, 180, 20);
        CreateLabel(page, L"Spread recovery", 40, 220, 120, 20);
        CreateEdit(page, IDC_GLOCK_SPREAD_RECOVERY, 220, 215, 120, 24);
        CreateLabel(page, L"Move spread scale", 40, 255, 120, 20);
        CreateEdit(page, IDC_GLOCK_MOVE_SPREAD_SCALE, 220, 250, 120, 24);

        CreateGroupBox(page, L"Primary Spread Model", 540, 110, 520, 255);
        CreateLabel(page, L"Base spread", 560, 145, 180, 20);
        CreateEdit(page, IDC_GLOCK_PRIMARY_BASE_SPREAD, 790, 140, 220, 24);
        CreateLabel(page, L"Ground move penalty", 560, 180, 180, 20);
        CreateEdit(page, IDC_GLOCK_PRIMARY_GROUND_MOVE_PENALTY, 790, 175, 220, 24);
        CreateLabel(page, L"Air move penalty", 560, 215, 180, 20);
        CreateEdit(page, IDC_GLOCK_PRIMARY_AIR_MOVE_PENALTY, 790, 210, 220, 24);
        CreateLabel(page, L"Duck penalty scale", 560, 250, 180, 20);
        CreateEdit(page, IDC_GLOCK_PRIMARY_DUCK_PENALTY_SCALE, 790, 245, 220, 24);
        CreateLabel(page, L"First-shot speed threshold", 560, 285, 180, 20);
        CreateEdit(page, IDC_GLOCK_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD, 790, 280, 220, 24);
        CreateLabel(page, L"Max spread", 560, 320, 180, 20);
        CreateEdit(page, IDC_GLOCK_PRIMARY_MAX_SPREAD, 790, 315, 220, 24);

        CreateGroupBox(page, L"Damage Model", 20, 315, 500, 130);
        CreateLabel(page, L"Damage", 40, 350, 120, 20);
        CreateEdit(page, IDC_GLOCK_PRIMARY_DAMAGE, 220, 345, 120, 24);
        CreateLabel(page, L"Headshot scale", 40, 385, 120, 20);
        CreateEdit(page, IDC_GLOCK_PRIMARY_HEADSHOT_SCALE, 220, 380, 120, 24);
        CreateCheckBox(page, L"Lethal headshot", IDC_GLOCK_PRIMARY_HEADSHOT_LETHAL, 40, 415, 160, 20);
    }

    void CreateMp5Page() {
        HWND page = pages_[2];
        CreateGroupBox(page, L"Editor Templates", 20, 20, 1040, 70);
        CreateButton(page, L"Default", IDC_MP5_PRESET_DEFAULT, 40, 45, 120, 24);
        CreateButton(page, L"CS Burst", IDC_MP5_PRESET_CS_BURST, 175, 45, 120, 24);
        CreateButton(page, L"CS Mobile", IDC_MP5_PRESET_CS_MOBILE, 310, 45, 130, 24);
        CreateButton(page, L"Spray Test", IDC_MP5_PRESET_SPRAY_TEST, 455, 45, 130, 24);

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
        CreateLabel(page, L"Duck penalty scale", 560, 250, 180, 20);
        CreateEdit(page, IDC_MP5_PRIMARY_DUCK_PENALTY_SCALE, 790, 245, 220, 24);
        CreateLabel(page, L"Burst growth", 560, 285, 180, 20);
        CreateEdit(page, IDC_MP5_PRIMARY_BURST_GROWTH, 790, 280, 220, 24);
        CreateLabel(page, L"Burst max additional spread", 560, 320, 180, 20);
        CreateEdit(page, IDC_MP5_PRIMARY_BURST_MAX_ADDITIONAL_SPREAD, 790, 315, 220, 24);
        CreateLabel(page, L"Spread recovery", 560, 355, 180, 20);
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
    }

    void Create357Page() {
        HWND page = pages_[3];
        CreateGroupBox(page, L"Editor Templates", 20, 20, 1040, 70);
        CreateButton(page, L"Default", IDC_357_PRESET_DEFAULT, 40, 45, 120, 24);
        CreateButton(page, L"Precision Test", IDC_357_PRESET_PRECISION_TEST, 175, 45, 145, 24);
        CreateButton(page, L"Headshot Test", IDC_357_PRESET_HEADSHOT_TEST, 335, 45, 145, 24);

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
    }

    void CreateShotgunPage() {
        HWND page = pages_[4];
        CreateGroupBox(page, L"Editor Templates", 20, 20, 1040, 70);
        CreateButton(page, L"Default", IDC_SHOTGUN_PRESET_DEFAULT, 40, 45, 120, 24);
        CreateButton(page, L"Close Quickkill", IDC_SHOTGUN_PRESET_CLOSE_QUICKKILL, 175, 45, 145, 24);
        CreateButton(page, L"Precision Test", IDC_SHOTGUN_PRESET_PRECISION_TEST, 335, 45, 145, 24);

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

        CreateGroupBox(page, L"Pellet And Damage Model", 20, 365, 500, 145);
        CreateLabel(page, L"Damage per pellet", 40, 400, 140, 20);
        CreateEdit(page, IDC_SHOTGUN_PRIMARY_DAMAGE_PER_PELLET, 220, 395, 120, 24);
        CreateLabel(page, L"Pellet count", 40, 435, 140, 20);
        CreateEdit(page, IDC_SHOTGUN_PRIMARY_PELLET_COUNT, 220, 430, 120, 24);
        CreateLabel(page, L"Headshot scale", 40, 470, 140, 20);
        CreateEdit(page, IDC_SHOTGUN_PRIMARY_HEADSHOT_SCALE, 220, 465, 120, 24);
        CreateCheckBox(page, L"Lethal headshot", IDC_SHOTGUN_PRIMARY_HEADSHOT_LETHAL, 360, 465, 120, 20);
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
        }
    }

    void ResetToNewDocument() {
        document_ = hlcfg::CreateDefaultProject();
        document_.exportSettings.exportFolder = environment_.defaultExportFolder;
        document_.exportSettings.cfgFileName = BuildSuggestedCfgFileName(document_);
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

    void RunExportWorkflow(const std::wstring* forcedExportFolder, bool quickExport) {
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
            return;
        }

        if (!ConfirmOverwrite(preview.exportPath, actionLabel)) {
            SetActionStatus(std::wstring(actionLabel) + L" was cancelled.\n\nTarget path:\n" + preview.exportPath);
            return;
        }

        hlcfg::ExportResult result;
        if (!hlcfg::ExportCfgToFile(exportDocument, environment_, result, errorMessage)) {
            RefreshExportPreview(true);
            ShowActionError(
                actionLabel,
                errorMessage,
                preview.exportPath,
                L"Check that the target folder exists and is writable, then try again.");
            return;
        }

        std::error_code verifyError;
        if (!std::filesystem::exists(result.exportPath, verifyError) || verifyError) {
            RefreshExportPreview(true);
            ShowActionError(
                actionLabel,
                L"The cfg export completed, but the written file could not be confirmed on disk.",
                result.exportPath,
                L"Check file permissions, confirm the target folder, and try again.");
            return;
        }

        document_ = std::move(exportDocument);
        document_.exportSettings.exportFolder = std::filesystem::path(result.exportPath).parent_path().wstring();
        document_.exportSettings.cfgFileName = std::filesystem::path(result.exportPath).filename().wstring();
        LoadDocumentToControls();
        dirty_ = true;
        UpdateWindowTitle();

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
        summary += hlcfg::Trimmed(preview.general.weaponUnderTest).empty() ? L"(unset)" : hlcfg::Trimmed(preview.general.weaponUnderTest);
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
        SetTextValue(IDC_GLOCK_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD, document_.glock.primaryFirstShotSpeedThreshold);
        SetTextValue(IDC_GLOCK_PRIMARY_MAX_SPREAD, document_.glock.primaryMaxSpread);
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
        Button_SetCheck(FindControl(IDC_MP5_PRIMARY_FIRST_SHOT_ACCURACY), document_.mp5.primaryFirstShotAccuracy ? BST_CHECKED : BST_UNCHECKED);
        SetTextValue(IDC_MP5_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD, document_.mp5.primaryFirstShotSpeedThreshold);
        SetTextValue(IDC_MP5_PRIMARY_MAX_SPREAD, document_.mp5.primaryMaxSpread);
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
        document_.glock.primaryFirstShotSpeedThreshold = GetTextValue(IDC_GLOCK_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD);
        document_.glock.primaryMaxSpread = GetTextValue(IDC_GLOCK_PRIMARY_MAX_SPREAD);
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
        document_.mp5.primaryFirstShotAccuracy = GetCheckValue(IDC_MP5_PRIMARY_FIRST_SHOT_ACCURACY);
        document_.mp5.primaryFirstShotSpeedThreshold = GetTextValue(IDC_MP5_PRIMARY_FIRST_SHOT_SPEED_THRESHOLD);
        document_.mp5.primaryMaxSpread = GetTextValue(IDC_MP5_PRIMARY_MAX_SPREAD);
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
    hlcfg::ApplyGlockPreset(glock, L"cs_tight");
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
        !ValidateContains(glockExport.cfgText, L"sv_exp_glock_primary_headshot_lethal 1") ||
        !ValidateContains(glockExport.cfgText, L"sv_exp_glock_lab_target_profile_name \"vest_headprotected\"") ||
        glockExport.execCommand != L"exec editor_glock_simple.cfg" ||
        glockExport.launcherCommand != L"scripts\\play-hlserver-testbed-direct.bat -CfgProfile \"editor_glock_simple.cfg\"") {
        return 1;
    }

    hlcfg::ProjectDocument mp5 = hlcfg::CreateDefaultProject();
    mp5.metadata.projectName = L"SelfTest MP5";
    mp5.general.sessionTag = L"editor_cfg_test";
    mp5.exportSettings.exportFolder = exportRoot.wstring();
    mp5.exportSettings.cfgFileName = L"editor_mp5_simple.cfg";
    hlcfg::ApplyMp5Preset(mp5, L"cs_burst");
    mp5.mp5.profileName = L"editor_mp5_simple";
    hlcfg::ApplyDummyPreset(mp5, L"vest");

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
    hlcfg::Apply357Preset(weapon357, L"headshot_test");
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
    hlcfg::ApplyShotgunPreset(shotgun, L"precision_test");
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
