#pragma once
#include <3ds.h>
#include <citro2d.h>
#include <string>
#include "UiKit.hpp"

void drawRingZ(float x, float y, float z, float radius, float thickness, u32 color, int segments = 24);
void drawRing(float x, float y, float radius, float thickness, u32 color, int segments = 24, float z = 0.5f);
void drawRectOutline(float x, float y, float w, float h, float thickness, u32 color, float z = 0.5f);
void drawText(C2D_TextBuf buf, float x, float y, float scale, u32 color, const char* str, float z = 0.85f);
void drawCenteredText(C2D_TextBuf buf, float cx, float cy, float scale, u32 color, const char* str, float z = 0.85f);
void drawTextWithShadow(C2D_TextBuf buf, float x, float y, float scale, u32 color, const char* str, float z = 0.85f, u32 shadowColor = Ui::rgba(0, 0, 0, 220));
void drawCenteredTextWithShadow(C2D_TextBuf buf, float cx, float cy, float scale, u32 color, const char* str, float z = 0.85f, u32 shadowColor = Ui::rgba(0, 0, 0, 220));
void drawKeyBadge(C2D_TextBuf buf, float x, float y, const char* key, const char* label, u32 keyClr, u32 textClr = Ui::rgba(255, 255, 255), float z = 0.85f);
void drawReverseArrow(float cx, float cy, float targetX, float targetY, float size, u32 color, float z = 0.42f);
std::string truncateString(const std::string& str, size_t maxLen);
std::string formatScoreThousands(int score);
u32 getScoreGradeColor(const std::string& grade, u32 cursorClr);
void drawStatBar(C2D_TextBuf buf, float x, float y, const char* label, float val, u32 fillClr, u32 bgClr, u32 txtClr);
