#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>

using namespace std;
// -----------------------------------------------------------------------------
// Game state & evaluation (used by Minimax and Expectiminimax)
// -----------------------------------------------------------------------------
struct GameState
{
    int playerIndex, aiIndex;
};

// Evaluation function: positive is good for the AI, negative for the player
int EvaluateState(const GameState &s)
{
    // Terminal: AI win / Player win
    if (s.aiIndex >= 40)
        return 10000;
    if (s.playerIndex >= 40)
        return -10000;

    int aiDist = 40 - s.aiIndex;
    int plDist = 40 - s.playerIndex;

    // Positive score if AI is closer than the player
    int score = (plDist - aiDist);

    // Bonus for having bumped the player back to 0 (and AI not at 0).
    if (s.playerIndex == 0 && s.aiIndex != 0)
        score += 500;

    return score;
}

// Apply a move for either AI (aiMoving = true) or player (false).
// Includes collision logic (landing on the opponent sends them to 0).
GameState ApplyMove(const GameState &s, int moveVal, bool aiMoving)
{
    GameState ns = s;

    if (aiMoving)
    {
        ns.aiIndex += moveVal;
        if (ns.aiIndex >= 40)
            ns.aiIndex = 40;

        // AI lands on player -> player back to start
        if (ns.aiIndex == ns.playerIndex)
            ns.playerIndex = 0;
    }
    else
    {
        ns.playerIndex += moveVal;
        if (ns.playerIndex >= 40)
            ns.playerIndex = 40;

        // Player lands on AI -> AI back to start
        if (ns.playerIndex == ns.aiIndex)
            ns.aiIndex = 0;
    }

    return ns;
}

// -----------------------------------------------------------------------------
// Bitwise grid helper (uses ∧ ∨ ⊕ conceptually, but ops are &, |, ^)
// -----------------------------------------------------------------------------
void BuildGrid(int d0, int d1, int d2, int grid[3][3])
{
    // Row 0: d0, d1
    grid[0][0] = d0 & d1; // ∧
    grid[0][1] = d0 | d1; // ∨
    grid[0][2] = d0 ^ d1; // ⊕

    // Row 1: d0, d2
    grid[1][0] = d0 & d2; // ∧
    grid[1][1] = d0 | d2; // ∨
    grid[1][2] = d0 ^ d2; // ⊕

    // Row 2: d1, d2
    grid[2][0] = d1 & d2; // ∧
    grid[2][1] = d1 | d2; // ∨
    grid[2][2] = d1 ^ d2; // ⊕
}

// Map choice 1-9 to (row, col) in row-major order:
//  1 2 3
//  4 5 6
//  7 8 9
bool ChoiceToRowCol(int choice, int &row, int &col)
{
    if (choice < 1 || choice > 9)
        return false;
    int index = choice - 1;
    row = index / 3; // 0..2
    col = index % 3; // 0..2
    return true;
}

// -----------------------------------------------------------------------------
// MINIMAX (deterministic, no dice randomness)
// -----------------------------------------------------------------------------
int MinimaxRecursive(const GameState &state, int depth, bool maximizing,
                     const int grid[3][3], long long &nodeCount)
{
    nodeCount++;

    bool terminal = (state.aiIndex >= 40 || state.playerIndex >= 40);
    if (depth == 0 || terminal)
        return EvaluateState(state);

    if (maximizing)
    {
        int best = -1000000000;
        for (int r = 0; r < 3; r++)
        {
            for (int c = 0; c < 3; c++)
            {
                int moveVal = grid[r][c];
                if (moveVal <= 0)
                    continue;

                GameState child = ApplyMove(state, moveVal, true);
                int v = MinimaxRecursive(child, depth - 1, false, grid, nodeCount);
                if (v > best)
                    best = v;
            }
        }
        if (best == -1000000000)
            best = EvaluateState(state);
        return best;
    }
    else
    {
        int best = 1000000000;
        for (int r = 0; r < 3; r++)
        {
            for (int c = 0; c < 3; c++)
            {
                int moveVal = grid[r][c];
                if (moveVal <= 0)
                    continue;

                GameState child = ApplyMove(state, moveVal, false);
                int v = MinimaxRecursive(child, depth - 1, true, grid, nodeCount);
                if (v < best)
                    best = v;
            }
        }
        if (best == 1000000000)
            best = EvaluateState(state);
        return best;
    }
}

// Choose best A.I. move using Minimax at several depths.
// Also prints node counts for each depth for rubric instrumentation.
int ChooseBestAIMove_Minimax(int currentPlayerIndex, int currentAIIndex, const int grid[3][3])
{
    GameState root{currentPlayerIndex, currentAIIndex};

    const int maxDepth = 3; // small but >1 to show growth
    int finalBestMove = 0;

    for (int depth = 1; depth <= maxDepth; depth++)
    {
        long long nodeCount = 0;
        int bestScore = -1000000000;
        int bestMoveVal = 0;

        for (int r = 0; r < 3; r++)
        {
            for (int c = 0; c < 3; c++)
            {
                int moveVal = grid[r][c];
                if (moveVal <= 0)
                    continue;

                GameState child = ApplyMove(root, moveVal, true);
                int v = MinimaxRecursive(child, depth - 1, false, grid, nodeCount);
                if (v > bestScore)
                {
                    bestScore = v;
                    bestMoveVal = moveVal;
                }
            }
        }

        printf("Minimax depth %d: %lld nodes\n", depth, nodeCount);

        if (depth == maxDepth)
            finalBestMove = bestMoveVal;
    }

    if (finalBestMove <= 0)
        finalBestMove = 1; // fallback

    return finalBestMove;
}

// -----------------------------------------------------------------------------
// EXPECTIMINIMAX (one-step: AI move -> chance over player dice + minimizing move)
// -----------------------------------------------------------------------------
double ExpectedValueForPlayerTurn(const GameState &state, long long &nodeCount);

double EvaluateAIMove_Expecti(const GameState &root, int moveVal, long long &nodeCount)
{
    GameState afterAI = ApplyMove(root, moveVal, true);
    nodeCount++;

    if (afterAI.aiIndex >= 40 || afterAI.playerIndex >= 40)
        return (double)EvaluateState(afterAI);

    return ExpectedValueForPlayerTurn(afterAI, nodeCount);
}

// Chance + minimizing player: enumerate all player dice triples, let the player pick the move that minimizes the evaluation, and average.
double ExpectedValueForPlayerTurn(const GameState &state, long long &nodeCount)
{
    if (state.aiIndex >= 40 || state.playerIndex >= 40)
    {
        nodeCount++;
        return (double)EvaluateState(state);
    }

    long long totalOutcomes = 0;
    double sumValues = 0.0;

    for (int pd0 = 1; pd0 <= 8; pd0++)
    {
        for (int pd1 = 1; pd1 <= 8; pd1++)
        {
            for (int pd2 = 1; pd2 <= 8; pd2++)
            {
                int pGrid[3][3];
                BuildGrid(pd0, pd1, pd2, pGrid);

                bool any = false;
                double bestForPlayer = 1e9;

                for (int r = 0; r < 3; r++)
                {
                    for (int c = 0; c < 3; c++)
                    {
                        int moveVal = pGrid[r][c];
                        if (moveVal <= 0)
                            continue;

                        GameState child = ApplyMove(state, moveVal, false);
                        nodeCount++;
                        double v = (double)EvaluateState(child);
                        if (v < bestForPlayer)
                        {
                            bestForPlayer = v;
                            any = true;
                        }
                    }
                }

                if (!any)
                {
                    nodeCount++;
                    bestForPlayer = (double)EvaluateState(state);
                }

                sumValues += bestForPlayer;
                totalOutcomes++;
            }
        }
    }

    if (totalOutcomes == 0)
    {
        nodeCount++;
        return (double)EvaluateState(state);
    }

    return sumValues / (double)totalOutcomes;
}

// Root: choose A.I. move using one-step Expectiminimax.
// Also prints a node count log for the rubric.
int ChooseBestAIMove_Expectiminimax(int currentPlayerIndex, int currentAIIndex,
                                    const int grid[3][3])
{
    GameState root{currentPlayerIndex, currentAIIndex};
    long long nodeCount = 0;

    double bestEV = -1e18;
    int bestMoveVal = 0;

    for (int r = 0; r < 3; r++)
    {
        for (int c = 0; c < 3; c++)
        {
            int moveVal = grid[r][c];
            if (moveVal <= 0)
                continue;

            double ev = EvaluateAIMove_Expecti(root, moveVal, nodeCount);
            if (ev > bestEV)
            {
                bestEV = ev;
                bestMoveVal = moveVal;
            }
        }
    }

    printf("Expectiminimax: %lld nodes evaluated\n", nodeCount);

    if (bestMoveVal <= 0)
        bestMoveVal = 1;

    return bestMoveVal;
}

// -----------------------------------------------------------------------------
// Utility: roll 3 dice (1..8)
// -----------------------------------------------------------------------------
void Roll3(int &d0, int &d1, int &d2)
{
    d0 = 1 + rand() % 8;
    d1 = 1 + rand() % 8;
    d2 = 1 + rand() % 8;
}

// -----------------------------------------------------------------------------
// Text-based board visualization (snaking 1–40 as 2x20, Start = 0 below)
// -----------------------------------------------------------------------------
void InitSnakeBoard(int board[2][20])
{
    int index = 1;
    for (int r = 0; r < 2; r++)
    {
        bool leftToRight = (r % 2 == 0);
        if (leftToRight)
        {
            for (int c = 0; c < 20; c++)
                board[r][c] = index++;
        }
        else
        {
            for (int c = 19; c >= 0; --c)
                board[r][c] = index++;
        }
    }
}

// Format a single cell like [05P ], [23 A], [12PA], or [07  ]
string FormatCell(int index, int playerIndex, int aiIndex)
{
    char buf[6];
    bool hasP = (playerIndex == index);
    bool hasA = (aiIndex == index);

    if (hasP && hasA)
        snprintf(buf, sizeof(buf), "%02dPA", index);
    else if (hasP)
        snprintf(buf, sizeof(buf), "%02d P", index);
    else if (hasA)
        snprintf(buf, sizeof(buf), "%02d A", index);
    else
        snprintf(buf, sizeof(buf), "%02d  ", index);

    string s = "[";
    s += buf;
    s += "]";
    return s;
}

void PrintBoard(int playerIndex, int aiIndex)
{
    int board[2][20];
    InitSnakeBoard(board);

    // print from top row (1) down to row 0
    for (int r = 1; r >= 0; --r)
    {
        for (int c = 0; c < 20; c++)
        {
            int index = board[r][c];
            cout << FormatCell(index, playerIndex, aiIndex) << " ";
        }
        cout << "\n";
    }
    cout << "\nStart tile:\n";
    cout << FormatCell(0, playerIndex, aiIndex) << "\n\n";
}

// UI
void PrintDiceAndGrid(int d0, int d1, int d2, int grid[3][3])
{
    cout << "Dice: " << d0 << ", " << d1 << ", " << d2 << "\n\n";

    cout << "Bitwise Options:\n";
    cout << "∧ (AND)       ∨ (OR)      ⊕ (XOR)\n";

    int choiceID = 1;
    for (int r = 0; r < 3; r++)
    {
        // Row: print all three columns on the same line
        cout << "[" << choiceID << "] ∧: " << grid[r][0]
             << "     [" << choiceID + 1 << "] ∨: " << grid[r][1]
             << "     [" << choiceID + 2 << "] ⊕: " << grid[r][2] << "\n";

        choiceID += 3;
    }

    cout << "\n(Choices 1–3 = row 1, 4–6 = row 2, 7–9 = row 3)\n\n";
}

int main()
{
    srand((unsigned int)time(nullptr));

    int playerIndex = 0, aiIndex = 0;
    bool playerWon = false, aiWon = false;

    bool useMinimax = false; // false = Expectiminimax, true = Minimax

    while (true)
    {
        cout << "Choose AI search method:\n";
        cout << "  1 = Minimax\n";
        cout << "  2 = Expectiminimax\n";
        cout << "Selection: ";
        string line;
        if (!getline(cin, line))
            return 0;
        if (line.empty())
            continue;
        char ch = line[0];
        if (ch == '1')
        {
            useMinimax = true;
            cout << "Minimax selected.\n\n";
            break;
        }
        else if (ch == '2')
        {
            useMinimax = false;
            cout << "Expectiminimax selected.\n\n";
            break;
        }
        cout << "Invalid selection. Please enter 1 or 2.\n";
    }

    int d0 = 0, d1 = 0, d2 = 0;
    Roll3(d0, d1, d2);

    cout << "Bitwise Dice Duel!\n";
    cout << "--------------------------------------\n";
    cout << "Goal: Reach tile 40 first.\n";
    cout << "Landing on your opponent sends them back to 0.\n";
    cout << "Controls:\n";
    cout << "  - Enter 1–9 to choose a grid cell\n";
    cout << "  - 'q' to quit\n\n";

    while (!playerWon && !aiWon)
    {
        PrintBoard(playerIndex, aiIndex);
        cout << "Player (P) at: " << playerIndex << "    "
         << "AI (A) at: " << aiIndex << "\n";

        cout << "AI mode: " << (useMinimax ? "Minimax" : "Expectiminimax") << "\n\n";

        int gridVals[3][3];
        BuildGrid(d0, d1, d2, gridVals);
        PrintDiceAndGrid(d0, d1, d2, gridVals);

        // ---- Player input loop ----
        int choice = 0;
        while (true)
        {
            cout << "Your move (1–9 or q): ";
            string input;
            if (!getline(cin, input))
                return 0; // EOF or error

            if (input.empty())
                continue;

            char ch = input[0];

            if (ch == 'q' || ch == 'Q')
            {
                cout << "Quitting game.\n";
                return 0;
            }

            if (ch >= '1' && ch <= '9')
            {
                choice = ch - '0';
                break;
            }

            cout << "Invalid input. Try again.\n";
        }

        // ---- Apply player move ----
        int row, col;
        if (!ChoiceToRowCol(choice, row, col))
        {
            cout << "Bad mapping (this should not happen).\n";
            continue;
        }

        int move = gridVals[row][col];
        cout << "You chose cell " << choice << " with move value " << move << ".\n";

        GameState state{playerIndex, aiIndex};
        int oldAI = state.aiIndex;

        state = ApplyMove(state, move, false); // player move
        playerIndex = state.playerIndex;
        aiIndex = state.aiIndex;

        if (playerIndex >= 40)
        {
            PrintBoard(playerIndex, aiIndex);
            cout << "\nYou reached 40! You win!\n";
            playerWon = true;
            break;
        }

        bool playerCapturedAI = (oldAI != 0 && aiIndex == 0 && move > 0);
        if (playerCapturedAI)
        {
            cout << "You landed on the AI. AI is sent back to 0.\n";
        }

        // ---- AI turn (always happens unless game is over) ----
        cout << "\n--- AI TURN ---\n";
        Roll3(d0, d1, d2);
        BuildGrid(d0, d1, d2, gridVals);
        cout << "AI dice: " << d0 << ", " << d1 << ", " << d2 << "\n";

        int aiMoveVal = 0;
        if (useMinimax)
        {
            aiMoveVal = ChooseBestAIMove_Minimax(playerIndex, aiIndex, gridVals);
            cout << "AI (Minimax) chooses move value: " << aiMoveVal << "\n";
        }
        else
        {
            aiMoveVal = ChooseBestAIMove_Expectiminimax(playerIndex, aiIndex, gridVals);
            cout << "AI (Expectiminimax) chooses move value: " << aiMoveVal << "\n";
        }

        GameState afterAI{playerIndex, aiIndex};
        int oldPlayer = afterAI.playerIndex;
        afterAI = ApplyMove(afterAI, aiMoveVal, true);
        playerIndex = afterAI.playerIndex;
        aiIndex = afterAI.aiIndex;

        if (aiIndex >= 40)
        {
            PrintBoard(playerIndex, aiIndex);
            cout << "\nAI reached 40! AI wins!\n";
            aiWon = true;
            break;
        }

        bool aiCapturedPlayer = (oldPlayer != 0 && playerIndex == 0 && aiMoveVal > 0);
        if (aiCapturedPlayer)
        {
            cout << "AI landed on you, sending you back to 0.\n";
        }

        // Roll dice for next player turn
        Roll3(d0, d1, d2);
    }

    cout << "\nGame over.\n";
    return 0;
}
