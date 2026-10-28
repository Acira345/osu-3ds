#include "ModeCatch.hpp"
#include "AppContext.hpp"
#include "backend/i18n/Localization.hpp"

void ModeCatch::drawCenteredText(C2D_TextBuf buf, float cx, float cy, float scale, u32 color, const char* str, float z) {
    C2D_Text text;
    C2D_TextParse(&text, buf, str);
    C2D_TextOptimize(&text);
    float w = 0.0f, h = 0.0f;
    C2D_TextGetDimensions(&text, scale, scale, &w, &h);
    C2D_DrawText(&text, C2D_WithColor, cx - (w * 0.5f), cy - (h * 0.5f), z, scale, scale, color);
}

// Renderizado en Pantalla Superior (400x240) con soporte Estereoscópico 3D
void ModeCatch::renderTopScreen(C2D_TextBuf textBuf, int songTime, float speedRate, float eyeSign, float slider3DVal, bool inKiai, const AppContext* pCtx) {
    auto depthShift = [&](float px) { return eyeSign * slider3DVal * px; };
    float dPlayfield = depthShift(-1.0f);
    float dCatcher   = depthShift(3.2f);
    float approachTime = BASE_APPROACH / std::max(0.5f, speedRate);

    // 1. Paredes y Fondo del Playfield de Catch
    float pfLeft = PLAYFIELD_X + dPlayfield;
    float pfRight = pfLeft + PLAYFIELD_W;

    u32 bgClrTop = inKiai ? Ui::rgba(30, 16, 42, 230) : Ui::rgba(14, 12, 24, 230);
    u32 bgClrBot = inKiai ? Ui::rgba(42, 20, 58, 245) : Ui::rgba(20, 16, 32, 245);
    Ui::gradV(pfLeft, 0.0f, PLAYFIELD_W, 240.0f, 0.15f, bgClrTop, bgClrBot);

    // Paredes laterales de bambú / neón
    u32 wallBorder = inKiai ? Ui::rgba(255, 120, 190) : Ui::rgba(95, 80, 140);
    C2D_DrawLine(pfLeft, 0.0f, wallBorder, pfLeft, 240.0f, wallBorder, 2.5f, 0.20f);
    C2D_DrawLine(pfRight, 0.0f, wallBorder, pfRight, 240.0f, wallBorder, 2.5f, 0.20f);

    // 2. Línea guía de captura (Platter Line)
    u32 guideClr = inKiai ? Ui::rgba(255, 140, 200, 90) : Ui::rgba(160, 150, 200, 55);
    C2D_DrawLine(pfLeft, PLATE_Y, guideClr, pfRight, PLATE_Y, guideClr, 1.0f, 0.22f);

    // 3. Estelas fantasma del Catcher al acelerar (Dash / Hyperdash)
    float dGhost = depthShift(2.8f);
    for (const auto& g : ghosts) {
        float gx = toScreenX(g.x) + dGhost;
        u32 gClr = g.isHyper ? Ui::rgba(255, 40, 50, static_cast<u8>(g.alpha * 220.0f))
                             : Ui::rgba(80, 180, 255, static_cast<u8>(g.alpha * 130.0f));
        float pW = catcherPlateWidth * SCALE_X;
        Ui::pill(gx - (pW * 0.5f), PLATE_Y - 2.0f, pW, 5.0f, 0.25f, gClr);
    }

    // Líneas de velocidad cósmica durante Hyperdash (Warp Speed Lines)
    if (isHyperDashing) {
        for (int w = 0; w < 7; ++w) {
            float wy = PLATE_Y - 28.0f + (w * 8.5f);
            float wSpeed = std::fmod((songTime * 2.2f + w * 57.0f), 380.0f);
            float dirSign = (facingDir > 0) ? -1.0f : 1.0f;
            float wx0 = toScreenX(catcherX) + dCatcher + dirSign * wSpeed;
            float wx1 = wx0 + dirSign * 55.0f;
            u32 wClr = Ui::rgba(255, 35, 45, 175);
            C2D_DrawLine(wx0, wy, Ui::withAlpha(wClr, 0), wx1, wy, wClr, 1.8f, 0.26f);
        }
    }

    // 4. Renderizado de Frutas que caen con caída tridimensional hacia el frente
    for (size_t i = nextFruitIndex; i < fruits.size(); ++i) {
        auto& f = fruits[i];
        if (f.state != NOT_HIT) continue;

        if (f.time - songTime > static_cast<int>(approachTime)) {
            break; // Aún no visible
        }

        float dt = static_cast<float>(f.time - songTime);
        float fruitY = PLATE_Y - (dt / approachTime) * PLATE_Y;

        // Oscilación fluida suave para gotas de jugo (sensación líquida suspendida)
        float sway = 0.0f;
        if (f.type == FRUIT_DROPLET || f.type == FRUIT_TINY_DROPLET) {
            sway = std::sin((songTime * 0.007f) + (f.x * 0.045f)) * (f.type == FRUIT_DROPLET ? 3.0f : 1.8f);
        }

        float yRatio = std::max(0.0f, std::min(1.0f, fruitY / PLATE_Y));
        float fDepth = -2.2f + yRatio * 5.4f; // Cae desde el fondo (-2.2f) hacia el plato (+3.2f)
        float fruitX = toScreenX(f.x) + depthShift(fDepth) + sway;

        if (fruitY >= -20.0f && fruitY <= PLATE_Y + 15.0f) {
            // Línea guía / estela láser conectora para Hyperfruits hacia la siguiente fruta
            if (f.isHyper && fruitY >= -10.0f && fruitY <= PLATE_Y) {
                float destX = toScreenX(f.hyperTargetX) + dCatcher;
                float hPulse = 0.5f + 0.5f * std::sin((songTime * 0.02f) + f.time * 0.01f);
                u32 guideLaser = Ui::rgba(255, 30, 40, static_cast<u8>(85 + 110 * hPulse));
                C2D_DrawLine(fruitX, fruitY, guideLaser, destX, PLATE_Y, guideLaser, 1.6f, 0.24f);

                // Flecha indicadora en el plato
                C2D_DrawTriangle(destX - 4.5f, PLATE_Y - 8.0f, guideLaser, destX + 4.5f, PLATE_Y - 8.0f, guideLaser, destX, PLATE_Y - 1.5f, guideLaser, 0.25f);
            }

            renderFruit(fruitX, fruitY, f.type, f.isHyper, 0.45f, pCtx);
        }
    }

    // 5. Renderizado del Catcher y su Plato en primer plano
    float pBounceY = std::sin(plateBounce * static_cast<float>(M_PI)) * 4.0f;
    float currentPlateY = PLATE_Y + pBounceY;
    float screenCatcherX = toScreenX(catcherX) + dCatcher;
    float plateW = catcherPlateWidth * SCALE_X;
    float halfW = plateW * 0.5f;

    // Aura de Hyperdash alrededor del Catcher si está activa
    if (isHyperDashing) {
        float pulse = 0.5f + 0.5f * std::sin(songTime * 0.05f);
        u32 auraClr = Ui::rgba(255, 30, 50, static_cast<u8>(150 + 90 * pulse));
        C2D_DrawCircleSolid(screenCatcherX, currentPlateY + 12.0f, 0.30f, halfW * 1.4f, auraClr);
        // Destellos de rayos alrededor
        for (int r = 0; r < 4; ++r) {
            float rAng = r * 1.57f + songTime * 0.01f;
            float rx = screenCatcherX + std::cos(rAng) * (halfW * 0.8f);
            float ry = currentPlateY + std::sin(rAng) * 12.0f;
            C2D_DrawCircleSolid(rx, ry, 0.32f, 2.2f, Ui::rgba(255, 230, 100));
        }
    } else if (isDashing) {
        u32 dashAura = Ui::rgba(70, 190, 255, 110);
        C2D_DrawCircleSolid(screenCatcherX, currentPlateY + 12.0f, 0.30f, halfW * 1.18f, dashAura);
    }

    // Plato metálico plateado con asas y rebote elástico (o sprite del skin)
    if (pCtx && pCtx->texCatchPlate.loaded) {
        pCtx->texCatchPlate.drawCentered(screenCatcherX, currentPlateY, plateW, 10.0f, 0.50f);
    } else {
        u32 plateTop = Ui::rgba(240, 242, 255);
        u32 plateBot = Ui::rgba(130, 135, 160);
        Ui::gradV(screenCatcherX - halfW, currentPlateY - 2.0f, plateW, 5.0f, 0.50f, plateTop, plateBot);
        Ui::pill(screenCatcherX - halfW - 2.0f, currentPlateY - 3.0f, 4.0f, 7.0f, 0.52f, Ui::rgba(200, 205, 230));
        Ui::pill(screenCatcherX + halfW - 2.0f, currentPlateY - 3.0f, 4.0f, 7.0f, 0.52f, Ui::rgba(200, 205, 230));
    }

    // Partículas de salpicadura de jugo (Juice Splashes proyectadas hacia el jugador)
    float dSplash = depthShift(4.2f);
    for (const auto& sp : splashes) {
        u8 spAlpha = static_cast<u8>(std::max(0.0f, std::min(1.0f, sp.life)) * 255.0f);
        C2D_DrawCircleSolid(sp.x + dSplash, sp.y, 0.54f, sp.size, Ui::withAlpha(sp.color, spAlpha));
        C2D_DrawCircleSolid(sp.x + dSplash - 0.6f, sp.y - 0.6f, 0.55f, sp.size * 0.4f, Ui::withAlpha(CLR_WHITE, spAlpha));
    }

    // Indicadores flotantes de puntos / combo de frutas (+300 / +100 / +10 / +1100)
    float dPopup = depthShift(4.6f);
    for (const auto& pp : popups) {
        u8 pAlpha = static_cast<u8>(std::max(0.0f, std::min(1.0f, pp.life)) * 255.0f);
        drawCenteredText(textBuf, pp.x + dPopup, pp.y, 0.36f, Ui::withAlpha(pp.color, pAlpha), pp.text, 0.60f);
    }

    // Frutas acumuladas en el plato
    for (const auto& pf : plateFruits) {
        float pfx = screenCatcherX + (pf.relX * SCALE_X);
        float pfy = currentPlateY - 6.0f - (pf.relY * 0.6f);
        if (pfy >= currentPlateY - 35.0f) {
            renderFruitMini(pfx, pfy, pf.type, 0.55f);
        }
    }

    // Personaje Catcher Chibi debajo del plato (o sprite del skin)
    if (pCtx && pCtx->texCatchCatcher.loaded) {
        pCtx->texCatchCatcher.drawCentered(screenCatcherX, currentPlateY + 16.0f, 38.0f, 42.0f, 0.48f);
    } else {
        renderCatcherChibi(screenCatcherX, currentPlateY + 3.0f, facingDir, isDashing || isHyperDashing, 0.48f);
    }
}

// Renderizado en Pantalla Inferior (320x240): Deck de Control Táctil y Radar
void ModeCatch::renderBottomDeck(C2D_TextBuf textBuf, float animTick, int songTime, const AppContext* pCtx) {
    // 1. Radar de frutas que se aproximan en la parte superior
    Ui::panel(8.0f, 10.0f, 304.0f, 85.0f, 0.15f, Ui::rgba(20, 16, 32, 230), Ui::rgba(75, 60, 110));
    drawCenteredText(textBuf, 160.0f, 18.0f, 0.28f, Ui::rgba(180, 170, 215), LOC(STR_CATCH_RADAR_DECK), 0.25f);

    // Representación de frutas en el radar
    float radarX = 14.0f;
    float radarW = 292.0f;
    for (size_t i = nextFruitIndex; i < fruits.size(); ++i) {
        const auto& f = fruits[i];
        if (f.state != NOT_HIT) continue;
        int dt = f.time - songTime;
        if (dt < 0) continue;
        if (dt > 1600) break;

        float rx = radarX + (f.x / 512.0f) * radarW;
        float ry = 85.0f - (static_cast<float>(dt) / 1600.0f) * 60.0f;
        u32 fClr = (f.type == FRUIT_BANANA) ? CLR_BANANA : ((f.type == FRUIT_DROPLET) ? CLR_DROPLET : CLR_APPLE);
        C2D_DrawCircleSolid(rx, ry, 0.30f, f.isHyper ? 3.5f : 2.0f, fClr);
    }

    // Indicador del Catcher en el radar
    float cRadarX = radarX + (catcherX / 512.0f) * radarW;
    Ui::pill(cRadarX - 10.0f, 82.0f, 20.0f, 4.0f, 0.35f, CLR_WHITE);

    // 2. Barra táctil de posicionamiento directo (Stylus Touch Slider)
    float barY = 105.0f;
    Ui::panel(8.0f, barY, 304.0f, 36.0f, 0.20f, Ui::rgba(26, 22, 42, 230), Ui::rgba(95, 75, 140));
    drawCenteredText(textBuf, 160.0f, barY + 9.0f, 0.30f, Ui::rgba(200, 190, 230), LOC(STR_CATCH_TOUCH_OR_DRAG), 0.32f);

    // Cursor del Catcher en la barra táctil
    float touchBarX = 14.0f + (catcherX / 512.0f) * 292.0f;
    Ui::pill(touchBarX - 18.0f, barY + 18.0f, 36.0f, 12.0f, 0.38f, isDashing ? Ui::rgba(70, 190, 255) : Ui::rgba(255, 102, 171));
    drawCenteredText(textBuf, touchBarX, barY + 24.0f, 0.26f, CLR_WHITE, "CATCH", 0.42f);

    // 3. Botones táctiles inferiores: [ < IZQ ], [ DER > ] y [ DASH / ACELERAR ]
    // Botón Izquierda
    Ui::panel(10.0f, 150.0f, 80.0f, 38.0f, 0.25f, Ui::rgba(32, 26, 52, 230), Ui::rgba(95, 80, 140));
    drawCenteredText(textBuf, 50.0f, 162.0f, 0.46f, CLR_WHITE, LOC(STR_CATCH_LEFT_ARROW), 0.35f);
    drawCenteredText(textBuf, 50.0f, 178.0f, 0.22f, Ui::rgba(180, 170, 200), "D-PAD < / Y", 0.35f);

    // Botón Derecha
    Ui::panel(98.0f, 150.0f, 80.0f, 38.0f, 0.25f, Ui::rgba(32, 26, 52, 230), Ui::rgba(95, 80, 140));
    drawCenteredText(textBuf, 138.0f, 162.0f, 0.46f, CLR_WHITE, LOC(STR_CATCH_RIGHT_ARROW), 0.35f);
    drawCenteredText(textBuf, 138.0f, 178.0f, 0.22f, Ui::rgba(180, 170, 200), "D-PAD > / A", 0.35f);

    // Botón DASH
    u32 dashBg = isDashing ? Ui::rgba(50, 160, 240, 240) : Ui::rgba(35, 75, 130, 230);
    Ui::panel(186.0f, 150.0f, 124.0f, 38.0f, 0.25f, dashBg, CLR_WHITE);
    drawCenteredText(textBuf, 248.0f, 162.0f, 0.44f, CLR_WHITE, "DASH >>", 0.35f);
    drawCenteredText(textBuf, 248.0f, 178.0f, 0.22f, Ui::rgba(210, 235, 255), LOC(STR_CATCH_HOLD_DASH), 0.35f);
}

// Renderizado detallado de cada fruta con sombras y brillos
void ModeCatch::renderFruit(float x, float y, CatchFruitType type, bool isHyper, float z, const AppContext* pCtx) {
    // Aura radiante de Hyperfruit pulsante
    if (isHyper) {
        float pulse = 0.5f + 0.5f * std::sin((x + y) * 0.14f);
        C2D_DrawCircleSolid(x, y, z - 0.05f, 18.0f + 4.0f * pulse, Ui::rgba(255, 30, 45, 130));
        C2D_DrawCircleSolid(x, y, z - 0.03f, 14.0f + 2.0f * pulse, Ui::rgba(255, 70, 75, 190));
        C2D_DrawCircleSolid(x, y, z - 0.01f, 10.5f, Ui::rgba(255, 230, 230, 220));
    }

    if (pCtx) {
        if (type == FRUIT_APPLE && pCtx->texCatchFruitApple.loaded) {
            pCtx->texCatchFruitApple.drawCenteredSprite(x, y, 22.0f, z, CLR_WHITE, 0.0f);
            return;
        } else if (type == FRUIT_ORANGE && pCtx->texCatchFruitOrange.loaded) {
            pCtx->texCatchFruitOrange.drawCenteredSprite(x, y, 22.0f, z, CLR_WHITE, 0.0f);
            return;
        } else if (type == FRUIT_GRAPE && pCtx->texCatchFruitGrape.loaded) {
            pCtx->texCatchFruitGrape.drawCenteredSprite(x, y, 22.0f, z, CLR_WHITE, 0.0f);
            return;
        } else if (type == FRUIT_STRAWBERRY && pCtx->texCatchFruitStrawberry.loaded) {
            pCtx->texCatchFruitStrawberry.drawCenteredSprite(x, y, 22.0f, z, CLR_WHITE, 0.0f);
            return;
        } else if (type == FRUIT_BANANA && pCtx->texCatchFruitBanana.loaded) {
            pCtx->texCatchFruitBanana.drawCenteredSprite(x, y, 24.0f, z, CLR_WHITE, 0.0f);
            return;
        } else if ((type == FRUIT_DROPLET || type == FRUIT_TINY_DROPLET) && pCtx->texCatchFruitDrop.loaded) {
            float dropSize = (type == FRUIT_DROPLET) ? 14.0f : 8.5f;
            pCtx->texCatchFruitDrop.drawCenteredSprite(x, y, dropSize, z, CLR_WHITE, 0.0f);
            return;
        }
    }

    if (type == FRUIT_APPLE) {
        // Manzana roja
        C2D_DrawCircleSolid(x - 3.5f, y, z, 7.5f, CLR_APPLE);
        C2D_DrawCircleSolid(x + 3.5f, y, z, 7.5f, CLR_APPLE);
        C2D_DrawCircleSolid(x - 2.0f, y - 2.0f, z + 0.01f, 3.0f, CLR_WHITE); // Brillo
        // Tallo y hoja
        C2D_DrawLine(x, y - 6.0f, Ui::rgba(90, 50, 20), x + 1.0f, y - 10.0f, Ui::rgba(90, 50, 20), 1.8f, z + 0.02f);
        C2D_DrawCircleSolid(x + 3.5f, y - 9.0f, z + 0.02f, 2.5f, Ui::rgba(60, 200, 70));
    } else if (type == FRUIT_ORANGE) {
        // Naranja
        C2D_DrawCircleSolid(x, y, z, 8.5f, CLR_ORANGE);
        C2D_DrawCircleSolid(x - 2.5f, y - 2.5f, z + 0.01f, 3.2f, Ui::rgba(255, 230, 160));
        C2D_DrawCircleSolid(x, y - 8.0f, z + 0.02f, 2.0f, Ui::rgba(50, 180, 60)); // Hoja
    } else if (type == FRUIT_GRAPE) {
        // Racimo de uvas
        C2D_DrawCircleSolid(x - 3.5f, y - 3.0f, z, 4.5f, CLR_GRAPE);
        C2D_DrawCircleSolid(x + 3.5f, y - 3.0f, z, 4.5f, CLR_GRAPE);
        C2D_DrawCircleSolid(x, y + 2.0f, z, 4.5f, CLR_GRAPE);
        C2D_DrawCircleSolid(x - 2.0f, y - 4.0f, z + 0.01f, 1.8f, CLR_WHITE);
    } else if (type == FRUIT_STRAWBERRY) {
        // Fresa
        C2D_DrawCircleSolid(x, y, z, 7.8f, CLR_STRAWBERRY);
        C2D_DrawCircleSolid(x - 2.0f, y - 2.0f, z + 0.01f, 2.4f, CLR_WHITE);
        // Corona verde
        C2D_DrawCircleSolid(x - 3.0f, y - 7.0f, z + 0.02f, 2.2f, Ui::rgba(50, 200, 60));
        C2D_DrawCircleSolid(x + 3.0f, y - 7.0f, z + 0.02f, 2.2f, Ui::rgba(50, 200, 60));
    } else if (type == FRUIT_BANANA) {
        // Plátano curvado
        C2D_DrawCircleSolid(x, y, z, 8.0f, CLR_BANANA);
        C2D_DrawCircleSolid(x - 2.0f, y - 2.0f, z + 0.01f, 3.0f, CLR_WHITE);
        C2D_DrawCircleSolid(x + 6.0f, y - 5.0f, z + 0.02f, 2.2f, Ui::rgba(120, 90, 40)); // Punta
    } else if (type == FRUIT_DROPLET) {
        // Gota mediana fluida de jugo en forma de lágrima
        C2D_DrawCircleSolid(x, y + 1.0f, z, 4.6f, CLR_DROPLET);
        C2D_DrawTriangle(x - 4.2f, y + 1.0f, CLR_DROPLET, x + 4.2f, y + 1.0f, CLR_DROPLET, x, y - 7.5f, CLR_DROPLET, z);
        C2D_DrawCircleSolid(x - 1.2f, y - 0.5f, z + 0.01f, 1.6f, CLR_WHITE);
    } else {
        // Gota diminuta fluida
        C2D_DrawCircleSolid(x, y + 0.5f, z, 2.6f, CLR_DROPLET);
        C2D_DrawTriangle(x - 2.4f, y + 0.5f, CLR_DROPLET, x + 2.4f, y + 0.5f, CLR_DROPLET, x, y - 4.5f, CLR_DROPLET, z);
    }
}

void ModeCatch::renderFruitMini(float x, float y, CatchFruitType type, float z) {
    u32 clr = CLR_APPLE;
    if (type == FRUIT_ORANGE) clr = CLR_ORANGE;
    else if (type == FRUIT_GRAPE) clr = CLR_GRAPE;
    else if (type == FRUIT_STRAWBERRY) clr = CLR_STRAWBERRY;
    else if (type == FRUIT_BANANA) clr = CLR_BANANA;
    else if (type == FRUIT_DROPLET) clr = CLR_DROPLET;

    C2D_DrawCircleSolid(x, y, z, 3.8f, clr);
    C2D_DrawCircleSolid(x - 1.0f, y - 1.0f, z + 0.01f, 1.4f, CLR_WHITE);
}

// Mascota Catcher Chibi
void ModeCatch::renderCatcherChibi(float cx, float cy, int dir, bool dashing, float z) {
    // Cabeza Chibi
    C2D_DrawCircleSolid(cx, cy + 9.0f, z, 8.5f, Ui::rgba(255, 224, 195));

    // Gorro de cocinero / catcher
    Ui::pill(cx - 7.0f, cy + 1.0f, 14.0f, 5.0f, z + 0.02f, CLR_WHITE);
    C2D_DrawCircleSolid(cx, cy - 1.5f, z + 0.01f, 6.0f, CLR_WHITE);

    // Ojos orientados según movimiento
    float eyeOff = (dir > 0) ? 2.5f : -2.5f;
    C2D_DrawCircleSolid(cx + eyeOff - 2.5f, cy + 8.5f, z + 0.03f, 1.4f, Ui::rgba(30, 25, 45));
    C2D_DrawCircleSolid(cx + eyeOff + 2.5f, cy + 8.5f, z + 0.03f, 1.4f, Ui::rgba(30, 25, 45));

    // Sonrisa
    C2D_DrawCircleSolid(cx + eyeOff, cy + 12.0f, z + 0.03f, 1.2f, Ui::rgba(255, 100, 130));

    // Cuerpo / delantal
    Ui::pill(cx - 6.0f, cy + 17.0f, 12.0f, 11.0f, z - 0.01f, dashing ? Ui::rgba(255, 80, 140) : Ui::rgba(70, 160, 240));

    // Manos sosteniendo el plato hacia arriba
    C2D_DrawLine(cx - 5.0f, cy + 16.0f, CLR_WHITE, cx - 7.0f, cy + 2.0f, CLR_WHITE, 2.0f, z + 0.04f);
    C2D_DrawLine(cx + 5.0f, cy + 16.0f, CLR_WHITE, cx + 7.0f, cy + 2.0f, CLR_WHITE, 2.0f, z + 0.04f);
}
