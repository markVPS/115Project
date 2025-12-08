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
    Vector2 pos = {center.x - m.x * 0.5f, center.y - m.y * 0.5f};
    DrawTextEx(font, text, pos, size, spacing, tint);
}

enum class AnchorSide
{
    Left,
    Center,
    Right
};

static void DrawTokenAnchored(Texture2D tex,
                              Rectangle tile,
                              float maxFracW, float maxFracH,
                              float pad,
                              AnchorSide side,
                              Color tint)
{
    float availW = tile.width - 2.0f * pad;
    float availH = tile.height - 2.0f * pad;
    float wantedW = availW * maxFracW;
    float wantedH = availH * maxFracH;
    float scale = 1.0f;

    if (tex.id != 0)
    {
        float sx = wantedW / (float)tex.width;
        float sy = wantedH / (float)tex.height;
        scale = (sx < sy) ? sx : sy;
    }

    if (tex.id == 0)
    {
        float radius = (wantedH < wantedW ? wantedH : wantedW) * 0.4f;
        float cx = tile.x + tile.width * 0.5f;
        if (side == AnchorSide::Left)
            cx = tile.x + tile.width * 0.3f;
        else if (side == AnchorSide::Right)
            cx = tile.x + tile.width * 0.7f;
        float cy = tile.y + tile.height * 0.5f;
        DrawCircleV(Vector2{cx, cy}, radius, tint);
        return;
    }

    float w = tex.width * scale;
    float h = tex.height * scale;
    float cx = tile.x + tile.width * 0.5f;
    if (side == AnchorSide::Left)
        cx = tile.x + tile.width * 0.3f;
    else if (side == AnchorSide::Right)
        cx = tile.x + tile.width * 0.7f;
    float cy = tile.y + tile.height * 0.5f;

    Rectangle src = {0, 0, (float)tex.width, (float)tex.height};
    Rectangle dst = {cx - w * 0.5f, cy - h * 0.5f, w, h};
    Vector2 origin = {0, 0};
    DrawTexturePro(tex, src, dst, origin, 0.0f, tint);
}

static void BuildGrid(int d0, int d1, int d2, int grid[3][3])
{
    int A0 = d0, A1 = d0, A2 = d1;
    int B0 = d1, B1 = d2, B2 = d2;

    grid[0][0] = A0 & B0; // AND
    grid[1][0] = A1 & B1;
    grid[2][0] = A2 & B2;

    grid[0][1] = A0 | B0; // OR
    grid[1][1] = A1 | B1;
    grid[2][1] = A2 | B2; // OR
    grid[0][2] = A0 ^ B0; // XOR
    grid[1][2] = A1 ^ B1;
    grid[2][2] = A2 ^ B2; // XOR
}

static bool ChoiceToRowCol(int choice, int &row, int &col)
{
    if (choice < 1 || choice > 9)
        return false;
    row = (choice - 1) % 3;
    col = (choice - 1) / 3;
    return true;
}

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
    // Fixed-size window: no dynamic scaling or camera. Designed for 1440x900.
    InitWindow(DESIGN_W, DESIGN_H, "Bitwise Dice Duel - project.cpp");
    SetTargetFPS(60);

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
        DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height, Color{90, 90, 100, 255});
        if (value > 0)
        {
            char t[8];
            std::snprintf(t, sizeof(t), "%d", value);
            DrawTextCentered(f, t, {x + dieW / 2, y + dieH / 2}, 24, 1, RAYWHITE);
        }
    };

    // Die combinations layout
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
    const float centerYBoard = innerTop + innerH * 0.5f;

    float rowStep = (innerH - 80.0f) / (rows + 1);
    float colStep = (boardW - 280.0f) / (cols - 1);

    float leftCenterX = 140.0f;
    float bottomCenterY = innerBottom - 30.0f;

    std::vector<Cell> cells(totalCells);
    cells[0].cx = leftCenterX;
    cells[0].cy = bottomCenterY;

    {
        int idx = 1;
        for (int r = 0; r < rows; ++r)
        {
            float cy = bottomCenterY - (r + 1) * rowStep;
            bool leftToRight = (r % 2 == 0);
            if (leftToRight)
            {
                for (int c = 0; c < cols && idx < totalCells; ++c)
                {
                    cells[idx].cx = leftCenterX + c * colStep;
                    cells[idx].cy = cy;
                    ++idx;
                }
            }
            else
            {
                for (int c = cols - 1; c >= 0 && idx < totalCells; --c)
                {
                    cells[idx].cx = leftCenterX + c * colStep;
                    cells[idx].cy = cy;
                    ++idx;
                }
            }
        }
    }

    // Lambda instead of nested function
    auto GetTileRect = [&](int index) -> Rectangle
    {
        const float tileH = 64.0f;
        const float tileW = 85.0f;
        const float startW = 120.0f;

        if (index == 0)
        {
            float x = cells[0].cx - startW * 0.5f;
            float y = cells[0].cy - tileH * 0.5f;
            return Rectangle{x, y, startW, tileH};
        }
        else
        {
            Cell c = cells[index];
            float x = c.cx - tileW * 0.5f;
            float y = c.cy - tileH * 0.5f;
            return Rectangle{x, y, tileW, tileH};
        }
    };

    int playerIdx = 0;
    int aiIdx = 0;
    bool playerWon = false;
    bool aiWon = false;

    int d0 = 0, d1 = 0, d2 = 0;
    auto roll3 = [&]()
    {
        d0 = GetRandomValue(1, 8);
        d1 = GetRandomValue(1, 8);
        d2 = GetRandomValue(1, 8);
    };
    roll3();

    int gridVals[3][3];
    Rectangle optCell[3][3];

    while (!WindowShouldClose())
    {
        if (playerWon || aiWon)
        {
            BeginDrawing();
            ClearBackground(Color{20, 20, 24, 255});

            bool pWon = playerWon;
            Texture2D leftTex = pWon ? texPlayer : texPlayerSad;
            Texture2D rightTex = pWon ? texAISad : texAI;

            int sw = DESIGN_W, sh = DESIGN_H;
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

        BuildGrid(d0, d1, d2, gridVals);

        for (int c = 0; c < gridCols; ++c)
        {
            float colX = optionsOuterX + c * (colW + colGap);
            for (int r = 0; r < gridRows; ++r)
            {
                float rowY = optionsTopY + 22.0f + r * (34.0f + rowGap);
                float cellH = 34.0f;
                Rectangle rc = {colX, rowY, colW, cellH};
                optCell[r][c] = rc;
            }
        }

        int chosen = 0;
        int digit = GetDigitPressedStrong();
        if (digit >= 1 && digit <= 9)
            chosen = digit;

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            Vector2 m = GetMousePosition();
            for (int c = 0; c < gridCols && chosen == 0; ++c)
            {
                for (int r = 0; r < gridRows; ++r)
                {
                    if (CheckCollisionPointRec(m, optCell[r][c]))
                    {
                        chosen = c * 3 + r + 1;
                        break;
                    }
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
                else
                {
                    roll3();
                    int aiGrid[3][3];
                    BuildGrid(d0, d1, d2, aiGrid);
                    int bestAI = 0;
                    for (int rr = 0; rr < 3; ++rr)
                    {
                        for (int cc = 0; cc < 3; ++cc)
                        {
                            bestAI = std::max(bestAI, aiGrid[rr][cc]);
                        }
                    }
                    aiIdx += bestAI;
                    if (aiIdx >= LAST_INDEX)
                    {
                        aiIdx = LAST_INDEX;
                        aiWon = true;
                    }
                    else
                    {
                        roll3();
                    }
                }
            }
        }

        // DRAW (normal frame)
        BeginDrawing();
        ClearBackground(Color{20, 20, 24, 255});

        // Title & names
        DrawTextEx(ui, "Bitwise Dice Duel", {24, 16}, 32, 2, RAYWHITE);
        DrawTextEx(ui, "Player", {24, 56}, 22, 1, PLAYER_COLOR);
        float playerW = MeasureTextEx(ui, "Player", 22, 1).x;
        DrawTextEx(ui, "A.I.", {24 + 20 + playerW, 56}, 22, 1, AI_COLOR);

        // Instructions + dice beneath
        DrawTextEx(ui, instructions, {instructionX, instructionY}, instrSize, 1, RAYWHITE);
        drawDie(ui, d0, diceInnerX + 0 * (dieW + dieGap), diceBaseY);
        drawDie(ui, d1, diceInnerX + 1 * (dieW + dieGap), diceBaseY);
        drawDie(ui, d2, diceInnerX + 2 * (dieW + dieGap), diceBaseY);

        // Combinations headers + options
        for (int c = 0; c < 3; ++c)
        {
            float cxh = optionsOuterX + c * (colW + colGap) + colW * 0.5f;
            DrawTextCentered(ui, colHdr[c], {cxh, optionsTopY - 8.0f}, hdrSize, 1, RAYWHITE);
        }

        for (int c = 0; c < gridCols; ++c)
        {
            for (int r = 0; r < gridRows; ++r)
            {
                Rectangle cellRect = optCell[r][c];
                DrawRectangleRec(cellRect, Color{40, 42, 50, 255});
                DrawRectangleLines((int)cellRect.x, (int)cellRect.y, (int)cellRect.width, (int)cellRect.height, Color{90, 90, 100, 255});

                int val = gridVals[r][c];
                int choiceID = c * 3 + r + 1;
                char buf[32];
                std::snprintf(buf, sizeof(buf), "[%d] %d", choiceID, val);

                float textX = cellRect.x + 10.0f;
                float textY = cellRect.y + (cellRect.height - cellHtxt) * 0.5f;
                DrawTextEx(ui, buf, {textX, textY}, cellHtxt, 1, RAYWHITE);
            }
        }

        float boardAreaTop = innerTop;
        float boardAreaBottom = innerBottom;
        float centerLineY = boardAreaTop + (boardAreaBottom - boardAreaTop) * 0.55f;
        DrawLine(0, (int)centerLineY, DESIGN_W, (int)centerLineY, Color{30, 30, 36, 255});

        for (int i = 0; i < totalCells; ++i)
        {
            Rectangle tr = GetTileRect(i);
            Color fill = Color{35, 35, 42, 255};
            if (i == 0)
            {
                fill = Color{45, 45, 58, 255};
            }
            DrawRectangleRec(tr, fill);
            DrawRectangleLines((int)tr.x, (int)tr.y, (int)tr.width, (int)tr.height, Color{90, 90, 100, 255});

            if (i == 0)
            {
                const char *label = "Start";
                Vector2 tm = MeasureTextEx(ui, label, 22, 1);
                float tx = tr.x + tr.width * 0.5f - tm.x * 0.5f;
                float ty = tr.y + tr.height * 0.5f - tm.y * 0.5f;
                DrawTextEx(ui, label, {tx, ty}, 22, 1, RAYWHITE);
            }
            else
            {
                char lbl[8];
                std::snprintf(lbl, sizeof(lbl), "%02d", i);
                DrawTextCentered(ui, lbl, {tr.x + tr.width * 0.5f, tr.y + tr.height * 0.5f}, 20, 1, Color{220, 220, 230, 255});
            }
        }

        Rectangle tileP = GetTileRect(playerIdx);
        Rectangle tileA = GetTileRect(aiIdx);
        DrawTokenAnchored(texPlayer, tileP, 0.88f, 0.88f, 4.0f, AnchorSide::Left, WHITE);
        DrawTokenAnchored(texAI, tileA, 0.88f, 0.88f, 4.0f, AnchorSide::Right, WHITE);

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
