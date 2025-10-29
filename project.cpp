#include "raylib.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>

static const int DESIGN_W = 1440;
static const int DESIGN_H = 900;

struct Cell
{
    float cx, cy;
};

static void DrawTextCentered(Font font, const char *text, Vector2 center, float size, float spacing, Color tint)
{
    Vector2 m = MeasureTextEx(font, text, size, spacing);
    Vector2 pos = {center.x - m.x * 0.5f, center.y - m.y * 0.55f};
    DrawTextEx(font, text, pos, size, spacing, tint);
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
        float scale = fminf(maxW / tw, maxH / th);
        dw = tw * scale;
        dh = th * scale;
    }
    else
    {
        dw = maxW;
        dh = maxH;
    }

    float cx = (side == AnchorSide::Left)    ? (tile.x + pad + dw * 0.5f)
               : (side == AnchorSide::Right) ? (tile.x + tile.width - pad - dw * 0.5f)
                                             : (tile.x + tile.width * 0.5f);
    float cy = tile.y + tile.height * 0.5f;

    if (tex.id == 0)
    {
        DrawCircleV({cx, cy}, fminf(dw, dh) * 0.45f, tint);
        return;
    }

    Rectangle src = {0, 0, (float)tex.width, (float)tex.height};
    Rectangle dst = {cx - dw / 2.0f, cy - dh / 2.0f, dw, dh};
    DrawTexturePro(tex, src, dst, {0, 0}, 0.0f, tint);
}

// Build AND/OR/XOR grid once into the provided buffer
static void BuildGrid(int d0, int d1, int d2, int grid[3][3])
{
    int A0 = d0, A1 = d0, A2 = d1;
    int B0 = d1, B1 = d2, B2 = d2;
    grid[0][0] = A0 & B0;
    grid[1][0] = A1 & B1;
    grid[2][0] = A2 & B2; // AND
    grid[0][1] = A0 | B0;
    grid[1][1] = A1 | B1;
    grid[2][1] = A2 | B2; // OR
    grid[0][2] = A0 ^ B0;
    grid[1][2] = A1 ^ B1;
    grid[2][2] = A2 ^ B2; // XOR
}

static bool ChoiceToRowCol(int choice, int &row, int &col)
{
    if (choice < 1 || choice > 9)
        return false;
    row = (choice - 1) % 3; // 1..9 mapped column-major
    col = (choice - 1) / 3;
    return true;
}

// Strong key-capture for first keystroke: only IsKeyPressed() checks.
static int GetDigitPressedStrong()
{
    if (IsKeyPressed(KEY_ONE))
        return 1;
    if (IsKeyPressed(KEY_TWO))
        return 2;
    if (IsKeyPressed(KEY_THREE))
        return 3;
    if (IsKeyPressed(KEY_FOUR))
        return 4;
    if (IsKeyPressed(KEY_FIVE))
        return 5;
    if (IsKeyPressed(KEY_SIX))
        return 6;
    if (IsKeyPressed(KEY_SEVEN))
        return 7;
    if (IsKeyPressed(KEY_EIGHT))
        return 8;
    if (IsKeyPressed(KEY_NINE))
        return 9;
    return 0;
}

int main()
{
    InitWindow(DESIGN_W, DESIGN_H, "Bitwise Dice Duel - project2.cpp");
    SetTargetFPS(60);
    int mon = GetCurrentMonitor();
    SetWindowSize(GetMonitorWidth(mon), GetMonitorHeight(mon));

    float uiScale = fminf(GetScreenWidth() / (float)DESIGN_W, GetScreenHeight() / (float)DESIGN_H);
    float offX = (GetScreenWidth() - DESIGN_W * uiScale) * 0.5f;
    float offY = (GetScreenHeight() - DESIGN_H * uiScale) * 0.5f;

    Camera2D cam{};
    cam.target = {0, 0};
    cam.offset = {offX, offY};
    cam.zoom = uiScale;

    Font ui = GetFontDefault();
    Texture2D texPlayer = LoadTexture("player.png");
    Texture2D texAI = LoadTexture("ai.png");
    Texture2D texPlayerSad = LoadTexture("player_sad.png");
    Texture2D texAISad = LoadTexture("ai_sad.png");

    Color PLAYER_COLOR = {250, 45, 102, 255};
    Color AI_COLOR = {0, 243, 125, 255};

    // ----- Title & names -----
    const float centerX = DESIGN_W * 0.5f;

    // Shared baseline under title+names
    float baseY = 85.0f;

    // Shift the centered block (instructions + dice) left by an offset
    const float blockOffsetX = -180.0f; // negative = left shift
    // Updated instruction text (use hyphen '-')
    const char *instructions = "Press 1-9 to choose a cell.";
    float instrSize = 20.0f;
    Vector2 instrM = MeasureTextEx(ui, instructions, instrSize, 1);
    float instructionX = (centerX + blockOffsetX) - instrM.x * 0.5f;
    float instructionY = baseY;

    const float dieW = 48.0f, dieH = 48.0f, dieGap = 16.0f;
    const float diceGroupW = 3 * dieW + 2 * dieGap;
    float diceBaseY = instructionY + instrM.y + 4.0f;
    float diceInnerX = (centerX + blockOffsetX) - diceGroupW * 0.5f;

    auto drawDie = [&](Font f, int value, float x, float y)
    {
        Rectangle r = {x, y, dieW, dieH};
        DrawRectangleRec(r, Color{45, 45, 52, 255});
        DrawRectangleLinesEx(r, 2.0f, Color{140, 140, 150, 255});
        if (value > 0)
        {
            char t[4];
            std::snprintf(t, sizeof(t), "%d", value);
            DrawTextCentered(f, t, {x + dieW / 2, y + dieH / 2}, 24, 1, RAYWHITE);
        }
    };

    // Die combinations layout (no background window now)
    const char *colHdr[3] = {"AND", "OR", "XOR"};
    float optionsOuterW = 560.0f; // layout width
    float optionsOuterX = DESIGN_W - 160.0f - optionsOuterW;
    float optionsOuterY = baseY;
    // Taller spacing + skew up a bit
    const float contentShiftUp = -6.0f;
    const float optionsTopY = optionsOuterY + 14.0f + contentShiftUp;
    const int gridCols = 3, gridRows = 3;
    const float colGap = 16.0f, rowGap = 12.0f;
    const float colW = (optionsOuterW - colGap * (gridCols - 1)) / gridCols;
    const float hdrSize = 22.0f, cellHtxt = 22.0f;

    // Board layout (bottom 1440x700 area)
    const int cols = 8;
    const int rows = 5;        // 5 rows of 8
    const int totalCells = 41; // 0..40
    const int LAST_INDEX = 40;

    const float boardW = 1440.0f;
    const float boardH = 700.0f;
    const float bottomMargin = 24.0f;
    const float innerTop = DESIGN_H - boardH;
    const float innerBottom = DESIGN_H - bottomMargin;
    const float innerH = innerBottom - innerTop;

    const float cellW = 150.0f;
    const float cellH = 86.0f;
    const float tokenPad = cellW * 0.06f;

    const float bottomCenterY = innerBottom - cellH * 0.5f;
    const float rowStep = (innerH - cellH) / 5;
    const float leftCenterX = (DESIGN_W - boardW) * 0.5f + cellW * 0.5f;
    const float firstColLeft = leftCenterX - cellW * 0.5f; // align Start left edge
    const float colStep = (boardW - cellW) / (cols - 1);

    std::vector<Cell> cells(totalCells);
    cells[0] = {leftCenterX, bottomCenterY};
    int idx = 1;
    for (int r = 0; r < rows; ++r)
    {
        float cy = bottomCenterY - (r + 1) * rowStep;
        bool leftToRight = (r % 2 == 0);
        if (leftToRight)
        {
            for (int c = 0; c < cols && idx < totalCells; ++c)
                cells[idx++] = {leftCenterX + c * colStep, cy};
        }
        else
        {
            for (int c = cols - 1; c >= 0 && idx < totalCells; --c)
                cells[idx++] = {leftCenterX + c * colStep, cy};
        }
    }

    // State
    int playerIdx = 0, aiIdx = 0;
    bool playerWon = false, aiWon = false;

    int d0 = 0, d1 = 0, d2 = 0;
    auto roll3 = [&]()
    { d0 = GetRandomValue(1,8); d1 = GetRandomValue(1,8); d2 = GetRandomValue(1,8); };
    roll3();

    // Per-frame buffers
    int gridVals[3][3];
    Rectangle optCell[3][3];

    while (!WindowShouldClose())
    {
        // If someone has already won, skip all input/build and show winner screen
        if (playerWon || aiWon)
        {
            BeginDrawing();
            ClearBackground(Color{20, 20, 24, 255});

            bool pWon = playerWon;
            Texture2D leftTex = pWon ? texPlayer : texPlayerSad;
            Texture2D rightTex = pWon ? texAISad : texAI;

            int sw = GetScreenWidth(), sh = GetScreenHeight();
            float gap = 40.0f;
            float totalW = leftTex.width + gap + rightTex.width;
            float x0 = sw * 0.5f - totalW * 0.5f;
            float yMid = sh * 0.5f;
            float leftX = x0;
            float rightX = x0 + leftTex.width + gap;
            float leftY = yMid - leftTex.height * 0.5f;
            float rightY = yMid - rightTex.height * 0.5f;
            DrawTexture(leftTex, (int)leftX, (int)leftY, WHITE);
            DrawTexture(rightTex, (int)rightX, (int)rightY, WHITE);

            const char *msg = pWon ? "PLAYER WINS!" : "A.I. WINS!";
            int fontSize = 48;
            Vector2 size = MeasureTextEx(ui, msg, fontSize, 2);
            Color msgCol = pWon ? (Color){250, 45, 102, 255} : (Color){0, 243, 125, 255};
            DrawTextEx(ui, msg, {(float)sw * 0.5f - size.x * 0.5f, leftY - size.y - 24.0f}, fontSize, 2, msgCol);

            EndDrawing();
            continue;
        }

        // Build current grid & option rects once per frame
        BuildGrid(d0, d1, d2, gridVals);
        for (int c = 0; c < gridCols; ++c)
            for (int r = 0; r < gridRows; ++r)
            {
                float x = optionsOuterX + c * (colW + colGap);
                float y = optionsTopY + r * (cellHtxt + rowGap);
                float optW = colW * 0.86f;
                float optH = (cellHtxt + 12.0f) * 1.05f;
                optCell[r][c] = {x + (colW - optW) * 0.5f, y, optW, optH};
            }

        // INPUT (only when no one has won)
        int chosen = 0;
        int digit = GetDigitPressedStrong();
        if (digit >= 1 && digit <= 9)
            chosen = digit;

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            Vector2 m = GetMousePosition();
            m.x = (m.x - cam.offset.x) / cam.zoom;
            m.y = (m.y - cam.offset.y) / cam.zoom;
            for (int c = 0; c < gridCols && chosen == 0; ++c)
                for (int r = 0; r < gridRows; ++r)
                {
                    if (CheckCollisionPointRec(m, optCell[r][c]))
                    {
                        chosen = c * 3 + r + 1;
                        break;
                    }
                }
        }

        if (chosen >= 1 && chosen <= 9)
        {
            int row, col;
            if (ChoiceToRowCol(chosen, row, col))
            {
                int move = gridVals[row][col];
                playerIdx += move;
                if (playerIdx >= LAST_INDEX)
                {
                    playerIdx = LAST_INDEX;
                    playerWon = true;
                }
                if (!playerWon)
                {
                    roll3(); // AI dice
                    int aiGrid[3][3];
                    BuildGrid(d0, d1, d2, aiGrid);
                    int bestAI = 0;
                    for (int rr = 0; rr < 3; ++rr)
                        for (int cc = 0; cc < 3; ++cc)
                            bestAI = std::max(bestAI, aiGrid[rr][cc]);
                    aiIdx += bestAI;
                    if (aiIdx >= LAST_INDEX)
                    {
                        aiIdx = LAST_INDEX;
                        aiWon = true;
                    }
                    if (!aiWon)
                        roll3(); // next turn
                }
            }
        }

        // DRAW (normal frame)
        BeginDrawing();
        ClearBackground(Color{20, 20, 24, 255});

        BeginMode2D(cam);

        // Title & names
        DrawTextEx(ui, "Bitwise Dice Duel", {24, 16}, 32, 2, RAYWHITE);
        DrawTextEx(ui, "Player", {24, 56}, 22, 1, PLAYER_COLOR);
        float playerW = MeasureTextEx(ui, "Player", 22, 1).x;
        DrawTextEx(ui, "A.I.", {24 + 20 + playerW, 56}, 22, 1, AI_COLOR);

        // Instructions (left-shifted) + dice beneath
        DrawTextEx(ui, instructions, {instructionX, instructionY}, instrSize, 1, RAYWHITE);
        drawDie(ui, d0, diceInnerX + 0 * (dieW + dieGap), diceBaseY);
        drawDie(ui, d1, diceInnerX + 1 * (dieW + dieGap), diceBaseY);
        drawDie(ui, d2, diceInnerX + 2 * (dieW + dieGap), diceBaseY);

        // Combinations headers + options only (no background window)
        for (int c = 0; c < 3; ++c)
        {
            float cxh = optionsOuterX + c * (colW + colGap) + colW * 0.5f;
            DrawTextCentered(ui, colHdr[c], {cxh, optionsTopY - 8.0f}, hdrSize, 1, RAYWHITE);
        }

        for (int c = 0; c < gridCols; ++c)
            for (int r = 0; r < gridRows; ++r)
            {
                Rectangle cellRect = optCell[r][c];
                DrawRectangleRec(cellRect, Color{40, 40, 48, 255});
                DrawRectangleLinesEx(cellRect, 2.0f, Color{120, 120, 130, 255});
                int choiceNum = c * 3 + r + 1;
                int val = gridVals[r][c];
                char line[64];
                std::snprintf(line, sizeof(line), "[%d] %d", choiceNum, val);
                Vector2 ms = MeasureTextEx(ui, line, 20.0f, 1);
                DrawTextEx(ui, line, {cellRect.x + (cellRect.width - ms.x) / 2.0f, cellRect.y + (cellRect.height - 20.0f) / 2.0f}, 20.0f, 1, RAYWHITE);
            }

        // Board tiles
        auto GetTileRect = [&](int i) -> Rectangle
        {
            if (i == 0)
                return Rectangle{firstColLeft, bottomCenterY - cellH / 2, cellW * 2.0f, cellH};
            return Rectangle{cells[i].cx - cellW / 2, cells[i].cy - cellH / 2, cellW, cellH};
        };

        for (int i = 0; i < totalCells; ++i)
        {
            Rectangle r = GetTileRect(i);
            DrawRectangleRec(r, Color{40, 40, 48, 255});
            DrawRectangleLinesEx(r, 2.0f, Color{160, 160, 170, 255});

            if (i == 0)
                DrawTextCentered(ui, "Start", {r.x + r.width / 2, r.y + r.height / 2}, cellH * 0.40f, 1, YELLOW);
            else
            {
                char buf[16];
                if (i < 10)
                    std::snprintf(buf, sizeof(buf), "0%d", i);
                else
                    std::snprintf(buf, sizeof(buf), "%d", i);
                DrawTextCentered(ui, buf, {r.x + r.width / 2, r.y + r.height / 2}, cellH * 0.40f, 1, RAYWHITE);
            }
        }

        // Draw tokens once
        Rectangle tileP = GetTileRect(playerIdx);
        Rectangle tileA = GetTileRect(aiIdx);
        DrawTokenAnchored(texPlayer, tileP, 0.48f, 0.76f, tokenPad, AnchorSide::Left, WHITE);
        DrawTokenAnchored(texAI, tileA, 0.48f, 0.76f, tokenPad, AnchorSide::Right, WHITE);

        EndMode2D();
        EndDrawing();
    }

    if (texPlayer.id != 0)
        UnloadTexture(texPlayer);
    if (texAI.id != 0)
        UnloadTexture(texAI);
    if (texPlayerSad.id != 0)
        UnloadTexture(texPlayerSad);
    if (texAISad.id != 0)
        UnloadTexture(texAISad);
    CloseWindow();
    return 0;
}
