#pragma once

#include <string>

#include "ConfigProject.h"

namespace hlcfg {

void ApplyGlockPreset(ProjectDocument& document, const std::wstring& presetName);
void ApplyMp5Preset(ProjectDocument& document, const std::wstring& presetName);
void Apply357Preset(ProjectDocument& document, const std::wstring& presetName);
void ApplyShotgunPreset(ProjectDocument& document, const std::wstring& presetName);
void ApplyDummyPreset(ProjectDocument& document, const std::wstring& presetName);

}  // namespace hlcfg
