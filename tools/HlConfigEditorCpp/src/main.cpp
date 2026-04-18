#include <string>
#include <vector>
#include <windows.h>
#include <shellapi.h>

#include "MainWindow.h"

namespace {

std::wstring GetModulePath() {
    std::wstring path(MAX_PATH, L'\0');
    DWORD length = 0;

    while (true) {
        length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0) {
            return {};
        }

        if (length < path.size() - 1) {
            path.resize(length);
            return path;
        }

        path.resize(path.size() * 2);
    }
}

std::vector<std::wstring> GetArguments() {
    int argumentCount = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (arguments == nullptr) {
        return {};
    }

    std::vector<std::wstring> values;
    values.reserve(static_cast<std::size_t>(argumentCount));
    for (int index = 0; index < argumentCount; ++index) {
        values.emplace_back(arguments[index]);
    }

    LocalFree(arguments);
    return values;
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int commandShow) {
    const std::wstring modulePath = GetModulePath();
    const std::vector<std::wstring> arguments = GetArguments();

    for (std::size_t index = 1; index < arguments.size(); ++index) {
        if (arguments[index] == L"--self-test") {
            return RunSelfTest(modulePath);
        }
    }

    return RunEditorApplication(instance, commandShow, modulePath);
}
