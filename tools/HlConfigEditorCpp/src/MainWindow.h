#pragma once

#include <string>
#include <windows.h>

int RunEditorApplication(HINSTANCE instance, int commandShow, const std::wstring& moduleFilePath);
int RunSelfTest(const std::wstring& moduleFilePath);
