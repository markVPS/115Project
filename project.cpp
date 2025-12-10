#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <chrono>
#include <thread>

using namespace std;

// Game state & evaluation (used by Expectiminimax)
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

    // Positive score if AI is ahead of the player
    int score = s.aiIndex - s.playerIndex;

    // Bonus for having bumped the player back to 0 (and AI not at 0).
    if (s.playerIndex == 0 && s.aiIndex != 0)
        score += 500;

    return score;
}

/*
 ApplyMove
 Applies a move for either AI (aiMoving = true) or player (false).
 Includes collision logic: landing on the opponent sends them to 0.
*/
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

/*
 Bitwise grid helper
 Uses ∧ ∨ ⊕ conceptually, implemented with &, |, ^ in C++
*/
void BuildDieGrid(int d0, int d1, int d2, int grid[3][3])
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

/*
 EXPECTIMINIMAX
 One-step search: AI move → chance over player dice → minimizing player move
*/

/*
 Player turn evaluation under randomness:
 Enumerates all dice outcomes, lets player choose the MIN move,
 then averages outcomes (chance node).
*/
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
                BuildDieGrid(pd0, pd1, pd2, pGrid);

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

// Utility: roll three dice (1–8)
void Roll3(int &d0, int &d1, int &d2)
{
    d0 = 1 + rand() % 8;
    d1 = 1 + rand() % 8;
    d2 = 1 + rand() % 8;
}

/*
 PrintBoard
 Renders the linear 0–40 board with PL and AI labels underneath positions.
*/
void PrintBoard(int playerIndex, int aiIndex)
{
    // First row: 00 01 02 ... 40
    for (int i = 0; i <= 40; i++)
        printf("%02d  ", i);
    cout << "\n";

    // Second row: Player marker
    for (int i = 0; i <= 40; i++)
        (i == playerIndex) ? cout << "PL  " : cout << "    ";
    cout << "\n";

    // Third row: AI marker
    for (int i = 0; i <= 40; i++)
        (i == aiIndex) ? cout << "AI  " : cout << "    ";
    cout << "\n";
}

/*
 main()
 Game loop: player turn → AI turn → repeat until winner.
*/
int main()
{
    srand((unsigned int)time(nullptr));

    int playerIndex = 0, aiIndex = 0;
    bool playerWon = false, aiWon = false;

    cout << "Bitwise Dice Duel!\n";
    cout << "--------------------------------------\n";
    cout << "Goal: Reach tile 40 first.\n";
    cout << "Landing on your opponent sends them to 0.\n";
    cout << "Enter 1–9 to choose a grid cell.\n\n";

    int d0 = 0, d1 = 0, d2 = 0;
    Roll3(d0, d1, d2);

    while (!playerWon && !aiWon)
    {
        PrintBoard(playerIndex, aiIndex);

        int bitwiseGrid[3][3];
        BuildDieGrid(d0, d1, d2, bitwiseGrid);

        /*
            Show dice results and the 3×3 bitwise operation grid.
            Each row corresponds to pairs of dice:
              Row 1: d0 and d1
              Row 2: d0 and d2
              Row 3: d1 and d2
            Columns correspond to:
              [1] AND   [2] OR   [3] XOR
        */
        cout << "Dice: " << d0 << ", " << d1 << ", " << d2 << "\n\n";
        cout << "∧ (AND)       ∨ (OR)      ⊕ (XOR)\n";

        int optionNumber = 1;
        for (int row = 0; row < 3; ++row)
        {
            cout << "[" << optionNumber << "] ∧: " << bitwiseGrid[row][0]
                 << "     [" << optionNumber + 1 << "] ∨: " << bitwiseGrid[row][1]
                 << "     [" << optionNumber + 2 << "] ⊕: " << bitwiseGrid[row][2] << "\n";

            optionNumber += 3;
        }

        cout << "\n";

        // Player input: validated 1–9
        int choice = 0;
        while (true)
        {
            cout << "Your move (1–9): ";
            string input;
            if (!getline(cin, input))
                return 0;

            if (!input.empty() && input[0] >= '1' && input[0] <= '9')
            {
                choice = input[0] - '0';
                break;
            }
            cout << "Invalid input. Try again.\n";
        }

        /*
         Convert 1–9 into (row, col) in the 3×3 grid
         Layout:
             1 2 3
             4 5 6
             7 8 9
        */
        int index = choice - 1;
        int row = index / 3; // 0,1,2
        int col = index % 3; // 0,1,2

        int move = bitwiseGrid[row][col];

        GameState state{playerIndex, aiIndex};

        state = ApplyMove(state, move, false);
        playerIndex = state.playerIndex;
        aiIndex = state.aiIndex;

        if (playerIndex >= 40)
        {
            PrintBoard(playerIndex, aiIndex);
            cout << "You reached 40! You win!\n";
            break;
        }

        if (state.aiIndex != 0 && aiIndex == 0 && move > 0)
            cout << "You landed on the AI, sending it back to 0.\n";

        // AI TURN
        cout << "\n--- AI TURN ---\n";
        Roll3(d0, d1, d2);
        BuildDieGrid(d0, d1, d2, bitwiseGrid);

        cout << "AI dice: " << d0 << ", " << d1 << ", " << d2 << "\n";
        PrintBoard(playerIndex, aiIndex);
        this_thread::sleep_for(chrono::seconds(5));

        /*
            Inline Expectiminimax:
            Determine AI’s best move by evaluating all 9 grid options
            and selecting the one with the highest expected value.
        */
        long long nodeCount = 0;
        double bestExpectedValue = -1e18;
        int aiMoveValue = 0;

        // Root state for evaluating AI moves
        GameState rootState{playerIndex, aiIndex};

        // Examine all possible moves in the 3×3 grid
        for (int aiRow = 0; aiRow < 3; ++aiRow)
        {
            for (int aiCol = 0; aiCol < 3; ++aiCol)
            {
                int moveValue = bitwiseGrid[aiRow][aiCol];
                if (moveValue <= 0)
                    continue;

                // ---- Inlined EvaluateAIMove_Expecti logic ----
                GameState afterAI = ApplyMove(rootState, moveValue, true);
                nodeCount++;

                double expectedValue;
                if (afterAI.aiIndex >= 40 || afterAI.playerIndex >= 40)
                {
                    expectedValue = (double)EvaluateState(afterAI);
                }
                else
                {
                    expectedValue = ExpectedValueForPlayerTurn(afterAI, nodeCount);
                }
                // ----------------------------------------------

                // Keep the move with the highest expected value
                if (expectedValue > bestExpectedValue)
                {
                    bestExpectedValue = expectedValue;
                    aiMoveValue = moveValue;
                }
            }
        }

        // Print how many nodes were evaluated for grading/rubric
        printf("Expectiminimax: %lld nodes evaluated\n", nodeCount);

        // AI must move at least 1
        if (aiMoveValue <= 0)
            aiMoveValue = 1;

        cout << "AI chooses move value: " << aiMoveValue << "\n";

        GameState afterAI{playerIndex, aiIndex};

        afterAI = ApplyMove(afterAI, aiMoveValue, true);
        playerIndex = afterAI.playerIndex;
        aiIndex = afterAI.aiIndex;

        if (aiIndex >= 40)
        {
            PrintBoard(playerIndex, aiIndex);
            cout << "AI reached 40! AI wins!\n";
            break;
        }

        if (afterAI.playerIndex != 0 && playerIndex == 0 && aiMoveValue > 0)
            cout << "AI landed on you, sending you back to 0.\n";

        Roll3(d0, d1, d2);
    }

    return 0;
}
