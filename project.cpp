#include "raylib.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>

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
    col = (choice - 1) / 3;
    row = (choice - 1) % 3;
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

int main()
{

    const int screenWidth = 1440;
    const int screenHeight = 900;
    InitWindow(screenWidth, screenHeight, "Bitwise Dice Duel - 3x3 Grid, Anchored Tokens");
    SetTargetFPS(60);

    Font ui = LoadUIFont();

    Texture2D texPlayer = LoadTexture("player.png");
    Texture2D texAI = LoadTexture("ai.png");

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

    UIState uiState = UIState::EnterName;
    std::string typedName;
    std::string playerName = "Player 1";
    std::string aiName = "A.I.";

    int playerIdx = 0;
    int aiIdx = 0;

    int d0 = 0, d1 = 0, d2 = 0;
    auto roll3 = [&]()
    { d0 = GetRandomValue(1,8); d1 = GetRandomValue(1,8); d2 = GetRandomValue(1,8); };

    MoveAnim anim;
    double prev = GetTime();

    const float panelW = 900.0f;
    const float panelH = 190.0f;
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
                roll3();
                uiState = UIState::Playing;
            }
        }
        else
        {
            if (!anim.active && IsKeyPressed(KEY_SPACE))
            {
                roll3();
            }
            if (!anim.active)
            {
                for (int key = KEY_ONE; key <= KEY_NINE; ++key)
                {
                    if (IsKeyPressed((KeyboardKey)key))
                    {
                        int choice = key - KEY_ZERO;
                        int row, col;
                        if (ChoiceToRowCol(choice, row, col))
                        {
                            int grid[3][3];
                            BuildGrid(d0, d1, d2, grid);
                            int move = grid[row][col];

                            int maxAdvance = (totalCells - 1) - playerIdx;
                            MoveAnim m;
                            anim.stepsRemaining = (move < maxAdvance ? move : maxAdvance);
                            anim.timer = 0.0;
                            anim.active = (anim.stepsRemaining > 0);

                            roll3();
                        }
                    }
                }
            }
        }

        if (anim.active)
        {
            anim.timer += dt;
            while (anim.active && anim.timer >= anim.stepInterval)
            {
                anim.timer -= anim.stepInterval;
                if (anim.stepsRemaining > 0 && playerIdx < totalCells - 1)
                {
                    playerIdx++;
                    anim.stepsRemaining--;
                }
                if (anim.stepsRemaining <= 0)
                    anim.active = false;
            }
        }

        BeginDrawing();
        ClearBackground(Color{20, 20, 24, 255});

        DrawTextEx(ui, "Game", {24, 16}, 32, 2, RAYWHITE);
        DrawTextEx(ui, (playerName + "  vs  " + aiName).c_str(), {24, 56}, 22, 1, RAYWHITE);

        DrawRectangleRec(panel, Color{30, 30, 36, 255});
        DrawRectangleLinesEx(panel, 2.0f, Color{110, 110, 120, 255});
        DrawTextEx(ui, "Press SPACE to roll   |   1–9 to choose an option",
                   {panel.x + 16, panel.y + 10}, 22, 1, RAYWHITE);

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
                Rectangle cellRect = {x, y - 4.0f, colW, cellHtxt + 8.0f};

                DrawRectangleRec(cellRect, Color{40, 40, 48, 255});
                DrawRectangleLinesEx(cellRect, 2.0f, Color{120, 120, 130, 255});

                char line[80];
                std::snprintf(line, sizeof(line), "[%d] (%s) %s -> %d", choiceNum, pair, opLbl, val);
                Vector2 ms = MeasureTextEx(ui, line, 20.0f, 1);
                DrawTextEx(ui, line, {x + (colW - ms.x) / 2.0f, y}, 20.0f, 1, RAYWHITE);
            }
        }

        for (int i = 0; i < totalCells - 1; ++i)
            DrawLineV({cells[i].cx, cells[i].cy}, {cells[i + 1].cx, cells[i + 1].cy}, Color{90, 90, 100, 255});

        for (int i = 0; i < totalCells; ++i)
        {
            Rectangle r = {cells[i].cx - cellW / 2.0f, cells[i].cy - cellH / 2.0f, cellW, cellH};
            Color fill = (i == 0)                ? Color{10, 90, 10, 255}
                         : (i == totalCells - 1) ? Color{120, 90, 10, 255}
                                                 : Color{40, 40, 48, 255};
            DrawRectangleRec(r, fill);
            DrawRectangleLinesEx(r, 2.0f, Color{160, 160, 170, 255});
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%d", i + 1);
            DrawTextCenteredEx(ui, buf, {r.x + r.width / 2, r.y + r.height / 2}, cellH * 0.50f, 1, RAYWHITE);
        }

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

        EndDrawing();
    }

    if (texPlayer.id != 0)
        UnloadTexture(texPlayer);
    if (texAI.id != 0)
        UnloadTexture(texAI);
    if (ui.texture.id != GetFontDefault().texture.id)
        UnloadFont(ui);
    CloseWindow();
    return 0;
}
