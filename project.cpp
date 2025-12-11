#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <chrono>
#include <thread>

using namespace std;

// -----------------------------------------------------------------------------
// GameState: minimal representation of the board
// playerIndex: current tile for the human player
// aiIndex:     current tile for the AI
// -----------------------------------------------------------------------------
struct GameState
{
    int playerIndex, aiIndex;
};

/* EvaluateState
 -------------
 Scoring function from the AI's point of view.
 - Large positive value: good for AI
 - Large negative value: good for player

 Logic:
   1. Check terminal states (win/loss).
   2. Otherwise, compare how far along the track each player is.
   3. Add a bonus if the player has been knocked back to 0 and the AI hasn't.
*/
int EvaluateState(const GameState &state)
{
    // srand((unsigned int)time(nullptr));
    //  Terminal: AI win / Player win dominate everything else
    if (state.aiIndex >= 40)
        return 10000; // AI has reached or passed goal
    if (state.playerIndex >= 40)
        return -10000; // Player has reached or passed goal

    // Base score: who is ahead on the board
    int score = state.aiIndex - state.playerIndex;

    // Bonus for knocking the player back to 0 while AI is still off the start
    if (state.playerIndex == 0 && state.aiIndex != 0)
        score += 500;

    return score;
}

/* TraverseOptions
 ---------
 Simulates applying a move to either the AI or the player.

 Parameters:
   - currentState : original game state (not modified)
   - moveValue    : how many tiles to move
   - aiMoving     : true if AI is moving, false if player is moving

 Behavior:
   - Update the correct index (playerIndex or aiIndex).
   - Clamp position at 40 (the goal).
   - Handle collisions:
       * If AI moves onto the player, player goes back to 0.
       * If player moves onto the AI, AI goes back to 0.
   - Return the new GameState.
*/
GameState TraverseOptions(const GameState &currentState, int moveValue, bool aiMoving)
{
    // Copy state so we do not modify the original directly
    GameState nextState = currentState;

    if (aiMoving)
    {
        // Move the AI forward
        nextState.aiIndex += moveValue;
        if (nextState.aiIndex >= 40)
            nextState.aiIndex = 40;

        // Collision: AI lands on player
        if (nextState.aiIndex == nextState.playerIndex)
            nextState.playerIndex = 0;
    }
    else
    {
        // Move the player forward
        nextState.playerIndex += moveValue;
        if (nextState.playerIndex >= 40)
            nextState.playerIndex = 40;

        // Collision: player lands on AI
        if (nextState.playerIndex == nextState.aiIndex)
            nextState.aiIndex = 0;
    }

    return nextState;
}

// -----------------------------------------------------------------------------
// BuildDieGrid
// ------------
// Given three dice values, build a 3×3 grid of move values using bitwise ops.
//
// Layout:
//   Rows:
//     0 → (die1, die2)
//     1 → (die1, die3)
//     2 → (die2, die3)
//   Columns:
//     0 → AND (&)
//     1 → OR  (|)
//     2 → XOR (^)
//
// This grid is used for both the player's and AI's choices.
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// Roll3
// -----
// Rolls three 8-sided dice (values 1–8) and assigns them to die1, die2, die3.
// Uses references so the caller sees the results directly.
// -----------------------------------------------------------------------------
void Roll3(int &die1, int &die2, int &die3)
{
    die1 = 1 + rand() % 8;
    die2 = 1 + rand() % 8;
    die3 = 1 + rand() % 8;
}

// -----------------------------------------------------------------------------
// PrintBoard
// ----------
// Renders the board as:
//   00  01  02  ...  40
//   PL at playerIndex
//   AI at aiIndex
//
// Only one position is marked for PL and AI respectively.
// -----------------------------------------------------------------------------
void PrintBoard(int playerIndex, int aiIndex)
{
    // Top row: tile numbers
    for (int position = 0; position <= 40; position++)
        printf("%02d  ", position);
    cout << "\n";

    // Middle row: player marker
    for (int position = 0; position <= 40; position++)
        (position == playerIndex) ? cout << "PL  " : cout << "    ";
    cout << "\n";

    // Bottom row: AI marker
    for (int position = 0; position <= 40; position++)
        (position == aiIndex) ? cout << "AI  " : cout << "    ";
    cout << "\n";
}

// -----------------------------------------------------------------------------
// main
// ----
// Overall game loop:
//   1. Initialize state and print instructions.
//   2. Player turn: roll dice, build grid, choose move, apply move.
//   3. AI turn: roll dice, run Expectiminimax search, apply best move.
//   4. Repeat until someone reaches tile 40.
// -----------------------------------------------------------------------------
int main()
{
    // Seed RNG with current time for different runs
    srand((unsigned int)time(nullptr));

    // Start positions for both players
    int playerIndex = 0, aiIndex = 0;

    // Intro / instructions
    cout << "Bitwise Dice Duel!\n";
    cout << "--------------------------------------\n";
    cout << "Goal: Reach tile 40 first.\n";
    cout << "Landing on your opponent sends them back to 0.\n";
    cout << "Enter 1–9 to choose a grid cell.\n\n";

    // Initial dice roll for the first player turn
    int die1 = 0, die2 = 0, die3 = 0;
    Roll3(die1, die2, die3);

    // Main game loop: continues until we break on a win condition
    while (true)
    {
        // -------------------------
        // PLAYER TURN
        // -------------------------
        PrintBoard(playerIndex, aiIndex);

        // Build bitwise grid from the current dice
        int bitwiseGrid[3][3];
        BuildDieGrid(die1, die2, die3, bitwiseGrid);

        // Show dice and corresponding 3×3 grid of move values
        cout << "Dice: " << die1 << ", " << die2 << ", " << die3 << "\n\n";
        cout << "∧ (AND)       ∨ (OR)      ⊕ (XOR)\n";

        // Print all 9 options with labels [1]..[9]
        int optionNumber = 1;
        for (int row = 0; row < 3; ++row)
        {
            cout << "[" << optionNumber << "] ∧: " << bitwiseGrid[row][0]
                 << "     [" << optionNumber + 1 << "] ∨: " << bitwiseGrid[row][1]
                 << "     [" << optionNumber + 2 << "] ⊕: " << bitwiseGrid[row][2] << "\n";

            optionNumber += 3;
        }

        cout << "\n";

        // -------------------------
        // Player input: must be 1–9
        // -------------------------
        int choice = 0;
        while (true)
        {
            cout << "Your move (1–9): ";
            string input;
            if (!getline(cin, input))
                return 0; // handle EOF or input failure gracefully

            if (!input.empty() && input[0] >= '1' && input[0] <= '9')
            {
                choice = input[0] - '0'; // convert char digit to int
                break;
            }
            cout << "Invalid input. Try again.\n";
        }

        // Convert choice 1–9 into 0-based (row, col) in the 3×3 grid
        // Layout:
        //   1 2 3
        //   4 5 6
        //   7 8 9
        int index = choice - 1;
        int row = index / 3; // 0,1,2
        int col = index % 3; // 0,1,2

        // Player's chosen move distance
        int moveValue = bitwiseGrid[row][col];

        // Apply the player's move using our state transition function
        GameState currentState{playerIndex, aiIndex};

        currentState = TraverseOptions(currentState, moveValue, false);
        playerIndex = currentState.playerIndex;
        aiIndex = currentState.aiIndex;

        // Check if the player has won
        if (playerIndex >= 40)
        {
            PrintBoard(playerIndex, aiIndex);
            cout << "You reached 40! You win!\n";
            break;
        }

        // Inform the player if they knocked the AI back to start
        if (currentState.aiIndex != 0 && aiIndex == 0 && moveValue > 0)
            cout << "You landed on the AI, sending it back to 0.\n";

        // -------------------------
        // AI TURN
        // -------------------------
        cout << "\n--- AI TURN ---\n";

        // Roll new dice for the AI and rebuild its grid
        Roll3(die1, die2, die3);
        BuildDieGrid(die1, die2, die3, bitwiseGrid);

        cout << "AI dice: " << die1 << ", " << die2 << ", " << die3 << "\n";
        // Print all 9 options with labels [1]..[9]
        optionNumber = 1;
        for (int row = 0; row < 3; ++row)
        {
            cout << "[" << optionNumber << "] ∧: " << bitwiseGrid[row][0]
                 << "     [" << optionNumber + 1 << "] ∨: " << bitwiseGrid[row][1]
                 << "     [" << optionNumber + 2 << "] ⊕: " << bitwiseGrid[row][2] << "\n";

            optionNumber += 3;
        }

        cout << "\n";
        PrintBoard(playerIndex, aiIndex);

        // Pause briefly so the player can see the AI's dice and board state
        this_thread::sleep_for(chrono::seconds(5));

        // Expectiminimax search variables
        long long nodeCount = 0;         // number of simulated states visited
        double bestExpectedValue = -1e8; // best expected utility found so far
        int aiMoveValue = 0;             // move value associated with that utility

        // Root state for AI's decision: capture current positions
        GameState rootState{playerIndex, aiIndex};

        int chance = (rand() % 2);

        // Loop over all 9 possible AI moves in the 3×3 bitwise grid
        for (int aiRow = 0; aiRow < 3; ++aiRow)
        {
            for (int aiCol = 0; aiCol < 3; ++aiCol)
            {
                // Candidate move value from the bitwise grid
                int candidateMove = bitwiseGrid[aiRow][aiCol];

                // Apply the AI move to produce the next game state
                GameState afterAI = TraverseOptions(rootState, candidateMove, true);
                nodeCount++; // Count this node in the search

                double expectedValue; // Expected value of choosing this move

                // If AI wins or the game is terminal, no need to simulate further
                if (afterAI.aiIndex >= 40 || afterAI.playerIndex >= 40)
                {
                    expectedValue = (double)EvaluateState(afterAI);
                }
                else
                {
                    // ---------------------------------------------------------
                    // EXPECTIMINIMAX: Player's turn as a chance + min node
                    // ---------------------------------------------------------
                    //
                    // We treat the player's next turn as:
                    //   - a chance node over all dice rolls (8×8×8 outcomes)
                    //   - followed by a minimizing choice (player picks best move for them,
                    //     which is worst for AI according to our evaluation).
                    //
                    long long totalOutcomes = 0; // how many dice outcomes we considered
                    double sumValues = 0.0;      // sum of best-for-player evaluations

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

                                        // Player applies this move
                                        GameState childState = TraverseOptions(afterAI, playerMoveValue, false);
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

                // Update best move if this expected value is higher and random number is greater than 0
                if (expectedValue > bestExpectedValue && chance > 0)
                {
                    bestExpectedValue = expectedValue;
                    aiMoveValue = candidateMove;
                }
            }
        }

        /// -----------------------------------------------
        /// After Expectiminimax: choose AI move
        /// 50% chance = optimal move
        /// 50% chance = random move
        /// -----------------------------------------------

        cout << "Expectiminimax: " << nodeCount << "nodes evaluated\n";

        /// Store the optimal move chosen by Expectiminimax.
        /// This is ALWAYS valid at this point because we no longer filter 0-values.
        int optimalMove = aiMoveValue;

        /// Collect ALL moves from the 3×3 grid, including 0-values.
        /// We now ALWAYS populate 9 entries, so moveCount can never be 0.
        int legalMoves[9];
        int moveCount = 0;

        for (int r = 0; r < 3; r++)
        {
            for (int c = 0; c < 3; c++)
            {
                /// Every bitwise result (even 0) is a valid move.
                legalMoves[moveCount++] = bitwiseGrid[r][c];
            }
        }

        /// -----------------------------------------------
        /// 50% chance to choose optimal move
        /// 50% chance to choose a random move from the grid
        /// -----------------------------------------------
        int coin = rand() % 2; // 0 or 1

        if (coin == 0)
        {
            /// Random move from the full list (may be 0)
            aiMoveValue = legalMoves[rand() % moveCount];
            cout << "(Random choice) ";
        }
        else
        {
            /// Use the best move found by Expectiminimax
            aiMoveValue = optimalMove;
            cout << "(Optimal choice) ";
        }

        cout << "AI chooses move value: " << aiMoveValue << "\n";

        // Apply the chosen AI move to the real game state
        GameState afterAI{playerIndex, aiIndex};

        afterAI = TraverseOptions(afterAI, aiMoveValue, true);
        playerIndex = afterAI.playerIndex;
        aiIndex = afterAI.aiIndex;

        // Check if AI has won
        if (aiIndex >= 40)
        {
            PrintBoard(playerIndex, aiIndex);
            cout << "AI reached 40! AI wins!\n";
            break;
        }

        // Inform player if AI knocked them back to start
        if (afterAI.playerIndex != 0 && playerIndex == 0 && aiMoveValue > 0)
            cout << "AI landed on you, sending you back to 0.\n";

        // Prepare new dice for the next player turn
        Roll3(die1, die2, die3);
    }

    return 0;
}
