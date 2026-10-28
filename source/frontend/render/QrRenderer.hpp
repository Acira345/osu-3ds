#pragma once
#include <3ds.h>

void renderQrCode(const char* text, float startX, float startY, float maxBoxSize);
void renderWifiBars(float x, float y, u8 strength, u32 clrActive, u32 clrInactive);
