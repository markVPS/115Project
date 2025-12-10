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
int EvaluateState(const GameState &state)
{
    // Terminal: AI win / Player win
    if (state.aiIndex >= 40)
        return 10000;
    if (state.playerIndex >= 40)
        return -10000;

    // Positive score if AI is ahead of the player
    int score = state.aiIndex - state.playerIndex;

    // Bonus for having bumped the player back to 0 (and AI not at 0).
    if (state.playerIndex == 0 && state.aiIndex != 0)
        score += 500;

    return score;
}

/*
 ApplyMove
 Applies a move for either AI (aiMoving = true) or player (false).
 Includes collision logic: landing on the opponent sends them to 0.
*/
GameState ApplyMove(const GameState &currentState, int moveValue, bool aiMoving)
{
    GameState nextState = currentState;

    if (aiMoving)
    {
        nextState.aiIndex += moveValue;
        if (nextState.aiIndex >= 40)
            nextState.aiIndex = 40;

        // AI lands on player -> player back to start
        if (nextState.aiIndex == nextState.playerIndex)
            nextState.playerIndex = 0;
    }
    else
    {
        nextState.playerIndex += moveValue;
        if (nextState.playerIndex >= 40)
            nextState.playerIndex = 40;

        // Player lands on AI -> AI back to start
        if (nextState.playerIndex == nextState.aiIndex)
            nextState.aiIndex = 0;
    }

    return nextState;
}

// Bitwise grid helper - Uses ∧ ∨ ⊕ conceptually, implemented with &, |, ^ in C++
void BuildDieGrid(int die1, int die2, int die3, int grid[3][3])
{
    // Row 0: die1, die2
    grid[0][0] = die1 & die2;
    grid[0][1] = die1 | die2;
    grid[0][2] = die1 ^ die2;

    // Row 1: die1, die3
    grid[1][0] = die1 & die3;
    grid[1][1] = die1 | die3;
    grid[1][2] = die1 ^ die3;

    // Row 2: die2, die3
    grid[2][0] = die2 & die3;
    grid[2][1] = die2 | die3;
    grid[2][2] = die2 ^ die3;
}

// Utility: roll three dice (1–8)
void Roll3(int &die1, int &die2, int &die3)
{
    die1 = 1 + rand() % 8;
    die2 = 1 + rand() % 8;
    die3 = 1 + rand() % 8;
}

// PrintBoard - Renders the linear 0–40 board with PL and AI labels underneath positions.
void PrintBoard(int playerIndex, int aiIndex)
{
    for (int position = 0; position <= 40; position++)
        printf("%02d  ", position);
    cout << "\n";

    for (int position = 0; position <= 40; position++)
        (position == playerIndex) ? cout << "PL  " : cout << "    ";
    cout << "\n";

    for (int position = 0; position <= 40; position++)
        (position == aiIndex) ? cout << "AI  " : cout << "    ";
    cout << "\n";
}

// main() - Game loop: player turn → AI turn → repeat until winner.
int main()
{
    srand((unsigned int)time(nullptr));

    int playerIndex = 0, aiIndex = 0;

    cout << "Bitwise Dice Duel!\n";
    cout << "--------------------------------------\n";
    cout << "Goal: Reach tile 40 first.\n";
    cout << "Landing on your opponent sends them back to 0.\n";
    cout << "Enter 1–9 to choose a grid cell.\n\n";

    int die1 = 0, die2 = 0, die3 = 0;
    Roll3(die1, die2, die3);

    while (true)
    {
        PrintBoard(playerIndex, aiIndex);

        int bitwiseGrid[3][3];
        BuildDieGrid(die1, die2, die3, bitwiseGrid);

        cout << "Dice: " << die1 << ", " << die2 << ", " << die3 << "\n\n";
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

        int index = choice - 1;
        int row = index / 3;
        int col = index % 3;

        int moveValue = bitwiseGrid[row][col];

        GameState currentState{playerIndex, aiIndex};

        currentState = ApplyMove(currentState, moveValue, false);
        playerIndex = currentState.playerIndex;
        aiIndex = currentState.aiIndex;

        if (playerIndex >= 40)
        {
            PrintBoard(playerIndex, aiIndex);
            cout << "You reached 40! You win!\n";
            break;
        }

        if (currentState.aiIndex != 0 && aiIndex == 0 && moveValue > 0)
            cout << "You landed on the AI, sending it back to 0.\n";

        cout << "\n--- AI TURN ---\n";
        Roll3(die1, die2, die3);
        BuildDieGrid(die1, die2, die3, bitwiseGrid);

        cout << "AI dice: " << die1 << ", " << die2 << ", " << die3 << "\n";
        PrintBoard(playerIndex, aiIndex);
        this_thread::sleep_for(chrono::seconds(5));

        long long nodeCount = 0;
        double bestExpectedValue = -1e18;
        int aiMoveValue = 0;

        GameState rootState{playerIndex, aiIndex};

        // Loop over all 9 possible AI moves in the 3×3 bitwise grid
        for (int aiRow = 0; aiRow < 3; ++aiRow)
        {
            for (int aiCol = 0; aiCol < 3; ++aiCol)
            {
                // Candidate move value from the bitwise grid
                int candidateMove = bitwiseGrid[aiRow][aiCol];
                if (candidateMove <= 0)
                    continue; // Skip unusable moves (AND/XOR may produce 0)

                // Apply the AI move to produce the next game state
                GameState afterAI = ApplyMove(rootState, candidateMove, true);
                nodeCount++; // Count this node in the search

                double expectedValue; // Expected value of choosing this move

                // If AI wins immediately, no need to compute further chance nodes
                if (afterAI.aiIndex >= 40 || afterAI.playerIndex >= 40)
                {
                    expectedValue = (double)EvaluateState(afterAI);
                }
                else
                {
                    // ----- PLAYER CHANCE NODE (Expectiminimax) -----
                    // Evaluate the player's turn by averaging over all dice outcomes

                    long long totalOutcomes = 0; // 8×8×8 = 512 total possibilities
                    double sumValues = 0.0;      // Sum of best player responses

                    // Enumerate all possible triples of dice the player might roll
                    for (int playerDie1 = 1; playerDie1 <= 8; playerDie1++)
                    {
                        for (int playerDie2 = 1; playerDie2 <= 8; playerDie2++)
                        {
                            for (int playerDie3 = 1; playerDie3 <= 8; playerDie3++)
                            {
                                // Build the player's bitwise move grid for this dice roll
                                int playerGrid[3][3];
                                BuildDieGrid(playerDie1, playerDie2, playerDie3, playerGrid);

                                bool foundMove = false;
                                double bestForPlayer = 1e9; // Player MINIMIZES evaluation

                                // Player may choose any of the 9 bitwise outcomes
                                for (int playerRow = 0; playerRow < 3; playerRow++)
                                {
                                    for (int playerCol = 0; playerCol < 3; playerCol++)
                                    {
                                        int playerMoveValue = playerGrid[playerRow][playerCol];
                                        if (playerMoveValue <= 0)
                                            continue; // Can't move zero distance

                                        // Player applies this move
                                        GameState childState = ApplyMove(afterAI, playerMoveValue, false);
                                        nodeCount++; // Count search node

                                        double evaluationValue = (double)EvaluateState(childState);

                                        // MIN player chooses the move worst for the AI
                                        if (evaluationValue < bestForPlayer)
                                        {
                                            bestForPlayer = evaluationValue;
                                            foundMove = true;
                                        }
                                    }
                                }

                                // If player had no valid moves, evaluate staying in same state
                                if (!foundMove)
                                {
                                    nodeCount++;
                                    bestForPlayer = (double)EvaluateState(afterAI);
                                }

                                // Add to the expectation sum
                                sumValues += bestForPlayer;
                                totalOutcomes++;
                            }
                        }
                    }

                    // Compute expected value: average over all player dice rolls
                    if (totalOutcomes == 0)
                    {
                        nodeCount++;
                        expectedValue = (double)EvaluateState(afterAI);
                    }
                    else
                    {
                        expectedValue = sumValues / (double)totalOutcomes;
                    }
                }

                // Update best move if this expected value is higher
                if (expectedValue > bestExpectedValue)
                {
                    bestExpectedValue = expectedValue;
                    aiMoveValue = candidateMove;
                }
            }
        }

        cout << "Expectiminimax: " << nodeCount << " nodes evaluated\n";

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

        Roll3(die1, die2, die3);
    }

    return 0;
}
