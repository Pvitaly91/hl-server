#pragma once

#include <string>

#include "ConfigProject.h"

namespace hlcfg {

struct EnvironmentPaths {
    std::wstring repoRoot;
    std::wstring liveModRoot;
    std::wstring stagedLiveModRoot;
    std::wstring logsRoot;
    std::wstring defaultExportFolder;
};

struct ExportResult {
    std::wstring cfgText;
    std::wstring execCommand;
    std::wstring cfgProfile;
    std::wstring launcherCommand;
    std::wstring exportPath;
};

EnvironmentPaths ResolveEnvironmentPaths(const std::wstring& moduleFilePath);
bool BuildExportResult(const ProjectDocument& document, const EnvironmentPaths& environment, ExportResult& result, std::wstring& errorMessage);
bool ExportCfgToFile(const ProjectDocument& document, const EnvironmentPaths& environment, ExportResult& result, std::wstring& errorMessage);

}  // namespace hlcfg
