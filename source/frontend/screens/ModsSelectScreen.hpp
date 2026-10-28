#pragma once
#include "AppContext.hpp"

void runModsSelectScreen(AppContext& ctx);
void renderModsTop(AppContext& ctx, float eyeSign, float baseCS, float baseAR, float baseOD, float baseHP, const std::string& title, const std::string& version);
void renderModsBottom(AppContext& ctx, bool isMultiplayer);
void handleModsInput(AppContext& ctx, bool isMultiplayer);
