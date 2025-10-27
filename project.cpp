// We do not have to show the bitwise operations, just the sum
/*33 34 35 36 37 38 39 40
32 31 30 29 28 27 26 25
17 18 19 20 21 22 23 24
16 15 14 13 12 11 10 9
1 2 3 4 5 6 7 8
*/

#include "raylib.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>

int DESIGN_W = 1440;
int DESIGN_H = 900;

struct Cell
{
    float cx, cy;
};

static void DrawTextCenteredEx(Font font, const char *text, Vector2 center, float fontSize, float spacing, Color tint)
{
    Vector2 size = MeasureTextEx(font, text, fontSize, spacing);
    Vector2 pos = {center.x - size.x * 0.5f, center.y - size.y * 0.55f};
    DrawTextEx(font, text, pos, fontSize, spacing, tint);
}

enum class AnchorSide
{
    Left,
    Center,
    Right
};

static void DrawTokenAnchored(Texture2D tex, Rectangle tile, float maxFracW, float maxFracH, float pad, AnchorSide side, Color tint)
{
    float maxW = tile.width * maxFracW;
    float maxH = tile.height * maxFracH;

    float dw, dh;
    if (tex.id != 0)
    {
        float tw = (float)tex.width, th = (float)tex.height;
        float scale = std::min(maxW / tw, maxH / th);
        dw = tw * scale;
        dh = th * scale;
    }
    else
    {
        dw = maxW;
        dh = maxH;
    }

    float cx;
    switch (side)
    {
    case AnchorSide::Left:
        cx = tile.x + pad + dw * 0.5f;
        break;
    case AnchorSide::Right:
        cx = tile.x + tile.width - pad - dw * 0.5f;
        break;
    default:
        cx = tile.x + tile.width * 0.5f;
        break;
    }
    float cy = tile.y + tile.height * 0.5f;

    if (tex.id == 0)
    {
        DrawCircleV({cx, cy}, std::min(dw, dh) * 0.45f, tint);
        return;
    }

    Rectangle src = {0, 0, (float)tex.width, (float)tex.height};
    Rectangle dst = {cx - dw / 2.0f, cy - dh / 2.0f, dw, dh};
    DrawTexturePro(tex, src, dst, {0, 0}, 0.0f, tint);
}

enum class UIState
{
    EnterName,
    Playing
};

struct MoveAnim
{
    bool active = false;
    int stepsRemaining = 0;
    double timer = 0.0;
    double stepInterval = 0.085;
};

static void BuildGrid(int d0, int d1, int d2, int grid[3][3])
{
    int A[3] = {d0, d0, d1};
    int B[3] = {d1, d2, d2};

    grid[0][0] = A[0] & B[0];
    grid[1][0] = A[1] & B[1];
    grid[2][0] = A[2] & B[2];

    grid[0][1] = A[0] | B[0];
    grid[1][1] = A[1] | B[1];
    grid[2][1] = A[2] | B[2];

    grid[0][2] = A[0] ^ B[0];
    grid[1][2] = A[1] ^ B[1];
    grid[2][2] = A[2] ^ B[2];
}

static bool ChoiceToRowCol(int choice, int &row, int &col)
{
    if (choice < 1 || choice > 9)
        return false;
    row = (choice - 1) / 3; // 0..2 (top, middle, bottom)
    col = (choice - 1) % 3; // 0..2 (left, mid, right)
    return true;
}

static Font LoadUIFont()
{
    const char *needed = u8"0123456789() -> ^∨⊕ Game Press SPACE to roll  1-9 to choose  Enter your name and press Enter: d0 d1 d2";
    int cpCount = 0;
    int *cps = LoadCodepoints(needed, &cpCount);

    Font f = LoadFontEx("fonts/DejaVuSans.ttf", 30, cps, cpCount);
    if (f.texture.id == 0)
    {
        f = LoadFontEx("DejaVuSans.ttf", 30, cps, cpCount);
    }
    UnloadCodepoints(cps);

    if (f.texture.id == 0)
    {
        f = GetFontDefault();
    }
    else
    {
        SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
    }
    return f;
}
static int ReadChoiceKeyPressed()
{
    // Top row 1..9
    for (int k = KEY_ONE; k <= KEY_NINE; ++k)
        if (IsKeyPressed((KeyboardKey)k))
            return k - KEY_ZERO;

    // Numpad 1..9
    if (IsKeyPressed(KEY_KP_1))
        return 1;
    if (IsKeyPressed(KEY_KP_2))
        return 2;
    if (IsKeyPressed(KEY_KP_3))
        return 3;
    if (IsKeyPressed(KEY_KP_4))
        return 4;
    if (IsKeyPressed(KEY_KP_5))
        return 5;
    if (IsKeyPressed(KEY_KP_6))
        return 6;
    if (IsKeyPressed(KEY_KP_7))
        return 7;
    if (IsKeyPressed(KEY_KP_8))
        return 8;
    if (IsKeyPressed(KEY_KP_9))
        return 9;

    return 0; // none
}

int main()
{
    // --- Window & scaling (draw in DESIGN_W x DESIGN_H; scale to monitor) ---
    int screenWidth = DESIGN_W;
    int screenHeight = DESIGN_H;
    InitWindow(screenWidth, screenHeight, "Bitwise Dice Duel - 3x3 Grid, Anchored Tokens");
    SetTargetFPS(60);

    int mon = GetCurrentMonitor();
    int monW = GetMonitorWidth(mon);
    int monH = GetMonitorHeight(mon);
    SetWindowSize(monW, monH);

    float uiScale = std::min(monW / (float)DESIGN_W, monH / (float)DESIGN_H);
    float offX = (monW - DESIGN_W * uiScale) * 0.5f;
    float offY = (monH - DESIGN_H * uiScale) * 0.5f;

    Camera2D cam{};
    cam.target = {0.0f, 0.0f};
    cam.offset = {offX, offY};
    cam.rotation = 0.0f;
    cam.zoom = uiScale;

    // --- Assets ---
    Font ui = LoadUIFont();
    Texture2D texPlayer = LoadTexture("player.png");
    Texture2D texAI = LoadTexture("ai.png");

    InitAudioDevice();
    Sound sMove = LoadSound("move.wav"); // provide a short click/beep WAV next to the exe
    SetSoundVolume(sMove, 0.6f);

    // --- Board geometry (design space) ---
    const int totalCells = 40;
    const int cellsPerRow = 7;
    const int groups = 5;
    const int verticalGaps = 2 * groups - 1;
    const float margin = 70.0f;

    const float usableW = screenWidth - 2 * margin;
    const float dx = usableW / (cellsPerRow - 1);

    const float hScale = 0.60f;
    const float dy = (screenHeight - 2 * margin) / (verticalGaps + hScale);
    const float cellH = hScale * dy;
    const float cellW = dx * 0.82f;

    std::vector<Cell> cells(totalCells);
    float cy = screenHeight - margin - cellH / 2.0f;
    const float startX = margin;

    int idx = 0;
    bool ltr = true;
    while (idx < totalCells)
    {
        for (int i = 0; i < cellsPerRow && idx < totalCells; ++i)
        {
            int col = ltr ? i : (cellsPerRow - 1 - i);
            float cx = startX + col * dx;
            cells[idx++] = {cx, cy};
        }
        if (idx < totalCells)
        {
            cy -= dy;
            float cx = ltr ? (startX + (cellsPerRow - 1) * dx) : startX;
            cells[idx++] = {cx, cy};
            ltr = !ltr;
        }
        if (idx < totalCells)
            cy -= dy;
    }

    const float tokenFracW = 0.78f;
    const float tokenFracH = 0.78f;
    const float tokenPad = cellW * 0.06f;

    // --- Panel & grid UI ---
    const float panelW = 900.0f, panelH = 190.0f;
    Rectangle panel = {(screenWidth - panelW) / 2.0f, margin * 0.25f, panelW, panelH};

    const float dieW = 50.0f, dieH = 50.0f, dieGap = 18.0f;
    const float diceBaseX = panel.x + 16.0f;
    const float diceBaseY = panel.y + 50.0f;

    auto drawDie = [&](Font f, int value, float x, float y)
    {
        Rectangle r = {x, y, dieW, dieH};
        DrawRectangleRec(r, Color{45, 45, 52, 255});
        DrawRectangleLinesEx(r, 2.0f, Color{140, 140, 150, 255});
        char t[4];
        std::snprintf(t, sizeof(t), "%d", value);
        DrawTextCenteredEx(f, t, {x + dieW / 2, y + dieH / 2}, 24, 1, RAYWHITE);
    };

    const float rightPad = 24.0f;
    const float gridLeftX = diceBaseX + 3 * dieW + 2 * dieGap + rightPad;
    const float gridTopY = panel.y + 24.0f;
    const float gridRightX = panel.x + panel.width - 16.0f;
    const float gridWidth = gridRightX - gridLeftX;

    const int gridCols = 3, gridRows = 3;
    const float colGap = 18.0f, rowGap = 8.0f;
    const float colW = (gridWidth - colGap * (gridCols - 1)) / gridCols;
    const float hdrSize = 26.0f, cellFont = 20.0f, cellHtxt = 24.0f;
    const char *colHdr[3] = {"^", "∨", "⊕"};
    const char *rowHdr[3] = {"d0,d1", "d0,d2", "d1,d2"};

    // --- State ---
    UIState uiState = UIState::EnterName;
    std::string typedName;
    std::string playerName = "Player 1";
    std::string aiName = "A.I.";

    int playerIdx = 0, aiIdx = 0;

    int d0 = 0, d1 = 0, d2 = 0;
    auto roll3 = [&]()
    { d0 = GetRandomValue(1,8); d1 = GetRandomValue(1,8); d2 = GetRandomValue(1,8); };

    MoveAnim pAnim, aiAnim;
    int aiQueuedSteps = 0; // >0 => AI will move after player
    // Turn state machine
    enum class Phase
    {
        NeedPlayerRoll,
        NeedPlayerConfirm,
        AnimatingPlayer,
        AnimatingAI
    };
    Phase phase = Phase::NeedPlayerRoll;

    double prev = GetTime();

    // --- Main loop ---
    while (!WindowShouldClose())
    {
        double now = GetTime();
        double dt = now - prev;
        prev = now;

        if (uiState == UIState::EnterName)
        {
            int ch = GetCharPressed();
            while (ch > 0)
            {
                if (ch >= 32 && ch <= 126)
                    typedName.push_back((char)ch);
                ch = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && !typedName.empty())
                typedName.pop_back();

            if (IsKeyPressed(KEY_ENTER))
            {
                if (!typedName.empty())
                    playerName = typedName;

                roll3(); // Player gets the first roll immediately
                phase = Phase::NeedPlayerConfirm;

                uiState = UIState::Playing;
            }

            else // Playing
            {
                // SPACE: player roll (once per turn, only when no animations are running)
                // Handle input only when no animations are running
                if (!pAnim.active && !aiAnim.active)
                {
                    // Not rolled yet? Only allow SPACE to roll.
                    if (!rolledThisTurn)
                    {
                        if (IsKeyPressed(KEY_SPACE))
                        {
                            roll3();
                            rolledThisTurn = true; // lock until player confirms with 1–9
                        }
                    }
                    else
                    {
                        // Already rolled this turn? Only allow 1–9 to confirm.
                        for (int key = KEY_ONE; key <= KEY_NINE; ++key)
                        {
                            if (IsKeyPressed((KeyboardKey)key))
                            {
                                // We auto-pick max; the key just acts as "confirm"
                                // (You can keep/remove the mapping lines as you wish.)
                                int choice = key - KEY_ZERO;
                                (void)choice;
                                int row, col;
                                ChoiceToRowCol(choice, row, col); // optional; not used

                                // Player best from current dice
                                int grid[3][3];
                                BuildGrid(d0, d1, d2, grid);
                                int bestPlayer = 0;
                                for (int rr = 0; rr < 3; ++rr)
                                    for (int cc = 0; cc < 3; ++cc)
                                        if (grid[rr][cc] > bestPlayer)
                                            bestPlayer = grid[rr][cc];

                                int maxAdvance = (totalCells - 1) - playerIdx;
                                int move = (bestPlayer < maxAdvance ? bestPlayer : maxAdvance);
                                pAnim.stepsRemaining = move;
                                pAnim.timer = 0.0;
                                pAnim.active = (pAnim.stepsRemaining > 0);

                                // AI gets a NEW roll for its move this turn
                                roll3();
                                // Keep rolledThisTurn = true; (it will be reset after AI finishes)

                                // Queue AI best from its NEW dice
                                int gridAI[3][3];
                                BuildGrid(d0, d1, d2, gridAI);
                                int bestAI = 0;
                                for (int rr = 0; rr < 3; ++rr)
                                    for (int cc = 0; cc < 3; ++cc)
                                        if (gridAI[rr][cc] > bestAI)
                                            bestAI = gridAI[rr][cc];

                                int aiMaxAdvance = (totalCells - 1) - aiIdx;
                                aiQueuedSteps = (bestAI < aiMaxAdvance ? bestAI : aiMaxAdvance);

                                break; // handled one confirm this frame
                            }
                        }

                        // (Optional) also accept numpad 1–9:
                        if (IsKeyPressed(KEY_KP_1) || IsKeyPressed(KEY_KP_2) || IsKeyPressed(KEY_KP_3) ||
                            IsKeyPressed(KEY_KP_4) || IsKeyPressed(KEY_KP_5) || IsKeyPressed(KEY_KP_6) ||
                            IsKeyPressed(KEY_KP_7) || IsKeyPressed(KEY_KP_8) || IsKeyPressed(KEY_KP_9))
                        {
                            // duplicate the same confirm body here if you want numpad support too,
                            // or refactor to a small helper that triggers the same code path.
                        }
                    }
                }

                // --- Player animation ---
                if (pAnim.active)
                {
                    pAnim.timer += dt;
                    while (pAnim.active && pAnim.timer >= pAnim.stepInterval)
                    {
                        pAnim.timer -= pAnim.stepInterval;
                        if (pAnim.stepsRemaining > 0 && playerIdx < totalCells - 1)
                        {
                            playerIdx++;
                            pAnim.stepsRemaining--;
                            PlaySound(sMove);
                        }
                        if (pAnim.stepsRemaining <= 0)
                            pAnim.active = false;
                    }
                }

                // Start AI animation when player is done and steps are queued
                if (!pAnim.active && !aiAnim.active && aiQueuedSteps > 0)
                {
                    aiAnim.stepsRemaining = aiQueuedSteps;
                    aiAnim.timer = 0.0;
                    aiAnim.active = (aiAnim.stepsRemaining > 0);
                    aiQueuedSteps = 0;
                }

                // --- AI animation ---
                if (aiAnim.active)
                {
                    aiAnim.timer += dt;
                    while (aiAnim.active && aiAnim.timer >= aiAnim.stepInterval)
                    {
                        aiAnim.timer -= aiAnim.stepInterval;
                        if (aiAnim.stepsRemaining > 0 && aiIdx < totalCells - 1)
                        {
                            aiIdx++;
                            aiAnim.stepsRemaining--;
                            PlaySound(sMove);
                        }
                        if (aiAnim.stepsRemaining <= 0)
                            aiAnim.active = false;
                    }
                }

                // Turn boundary: when both anims are idle, next player roll is allowed
                if (!pAnim.active && !aiAnim.active)
                {
                    rolledThisTurn = false;
                }
            }

            // --- Drawing ---
            BeginDrawing();
            ClearBackground(Color{20, 20, 24, 255});
            BeginMode2D(cam);

            DrawTextEx(ui, "Game", {24, 16}, 32, 2, RAYWHITE);
            DrawTextEx(ui, (playerName + "  vs  " + aiName).c_str(), {24, 56}, 22, 1, RAYWHITE);

            DrawRectangleRec(panel, Color{30, 30, 36, 255});
            DrawRectangleLinesEx(panel, 2.0f, Color{110, 110, 120, 255});
            DrawTextEx(ui, "SPACE: roll (once per turn)   |   1–9: confirm — best move is auto-picked",
                       {panel.x + 16, panel.y + 10}, 20, 1, RAYWHITE);

            drawDie(ui, d0, diceBaseX + 0 * (dieW + dieGap), diceBaseY);
            drawDie(ui, d1, diceBaseX + 1 * (dieW + dieGap), diceBaseY);
            drawDie(ui, d2, diceBaseX + 2 * (dieW + dieGap), diceBaseY);

            int gridVals[3][3];
            BuildGrid(d0, d1, d2, gridVals);

            for (int c = 0; c < 3; ++c)
            {
                float cxh = gridLeftX + c * (colW + colGap) + colW * 0.5f;
                DrawTextCenteredEx(ui, colHdr[c], {cxh, gridTopY - 16.0f}, hdrSize, 1, RAYWHITE);
            }
            for (int r = 0; r < 3; ++r)
            {
                float y = gridTopY + r * (cellHtxt + rowGap) + 2.0f;
                DrawTextEx(ui, rowHdr[r], {gridLeftX - 80.0f, y}, 20.0f, 1, Color{210, 210, 210, 255});
            }
            for (int c = 0; c < 3; ++c)
            {
                for (int r = 0; r < 3; ++r)
                {
                    int choiceNum = c * 3 + r + 1;
                    int val = gridVals[r][c];
                    const char *opLbl = (c == 0 ? "^" : c == 1 ? "∨"
                                                               : "⊕");
                    const char *pair = (r == 0 ? "d0,d1" : r == 1 ? "d0,d2"
                                                                  : "d1,d2");

                    float x = gridLeftX + c * (colW + colGap);
                    float y = gridTopY + r * (cellHtxt + rowGap);
                    float optW = colW * 0.78f;
                    float optH = (cellHtxt + 8.0f) * 0.70f; // smaller, centered
                    Rectangle cellRect = {
                        x + (colW - optW) * 0.5f,
                        (y - 4.0f) + ((cellHtxt + 8.0f) - optH) * 0.5f,
                        optW, optH};
                    DrawRectangleRec(cellRect, Color{40, 40, 48, 255});
                    DrawRectangleLinesEx(cellRect, 2.0f, Color{120, 120, 130, 255});

                    char line[80];
                    std::snprintf(line, sizeof(line), "[%d] (%s) %s -> %d", choiceNum, pair, opLbl, val);
                    Vector2 ms = MeasureTextEx(ui, line, 20.0f, 1);
                    DrawTextEx(ui, line, {x + (colW - ms.x) / 2.0f, y}, 20.0f, 1, RAYWHITE);
                }
            }

            // Path lines
            for (int i = 0; i < totalCells - 1; ++i)
                DrawLineV({cells[i].cx, cells[i].cy}, {cells[i + 1].cx, cells[i + 1].cy}, Color{90, 90, 100, 255});

            // Tiles (uniform color)
            for (int i = 0; i < totalCells; ++i)
            {
                Rectangle r = {cells[i].cx - cellW / 2.0f, cells[i].cy - cellH / 2.0f, cellW, cellH};
                DrawRectangleRec(r, Color{40, 40, 48, 255});
                DrawRectangleLinesEx(r, 2.0f, Color{160, 160, 170, 255});
                char buf[8];
                std::snprintf(buf, sizeof(buf), "%d", i + 1);
                DrawTextCenteredEx(ui, buf, {r.x + r.width / 2, r.y + r.height / 2}, cellH * 0.50f, 1, RAYWHITE);
            }

            // Tokens
            if (playerIdx >= 0 && playerIdx < totalCells)
            {
                Rectangle tile = {cells[playerIdx].cx - cellW / 2.0f, cells[playerIdx].cy - cellH / 2.0f, cellW, cellH};
                DrawTokenAnchored(texPlayer, tile, tokenFracW, tokenFracH, tokenPad, AnchorSide::Left, WHITE);
            }
            if (aiIdx >= 0 && aiIdx < totalCells)
            {
                Rectangle tile = {cells[aiIdx].cx - cellW / 2.0f, cells[aiIdx].cy - cellH / 2.0f, cellW, cellH};
                DrawTokenAnchored(texAI, tile, tokenFracW, tokenFracH, tokenPad, AnchorSide::Right, WHITE);
            }

            // Win banner
            bool playerWon = (playerIdx >= totalCells - 1);
            bool aiWon = (aiIdx >= totalCells - 1);
            if (playerWon || aiWon)
            {
                const char *msg = playerWon ? "PLAYER WINS!!!" : "A.I. WINS!!!";
                int fontSize = 64;
                Vector2 size = MeasureTextEx(ui, msg, fontSize, 2);
                DrawTextEx(ui, msg,
                           {(screenWidth - size.x) / 2.0f, (screenHeight - size.y) / 2.0f},
                           fontSize, 2, YELLOW);
            }

            // Name entry overlay
            if (uiState == UIState::EnterName)
            {
                DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 140});
                const char *prompt = "Enter your name and press Enter:";
                Vector2 ps = MeasureTextEx(ui, prompt, 32, 1);
                Vector2 pp = {(screenWidth - ps.x) / 2.0f, screenHeight / 2.0f - 64.0f};
                DrawTextEx(ui, prompt, pp, 32, 1, RAYWHITE);
                Vector2 ns = MeasureTextEx(ui, typedName.c_str(), 32, 1);
                Vector2 np = {(screenWidth - ns.x) / 2.0f, screenHeight / 2.0f - 16.0f};
                DrawTextEx(ui, typedName.c_str(), np, 32, 1, SKYBLUE);
            }

            EndMode2D();
            EndDrawing();
        }

        // --- Shutdown ---
        UnloadSound(sMove);
        CloseAudioDevice();

        if (texPlayer.id != 0)
            UnloadTexture(texPlayer);
        if (texAI.id != 0)
            UnloadTexture(texAI);
        if (ui.texture.id != GetFontDefault().texture.id)
            UnloadFont(ui);

        CloseWindow();
        return 0;
    }
