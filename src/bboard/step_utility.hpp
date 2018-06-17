#ifndef STEP_UTILITY_H
#define STEP_UTILITY_H

#include "bboard.hpp"

namespace bboard
{

/**
 * @brief DesiredPosition returns the x and y values of the agents
 * destination
 * @param x Current agents x position
 * @param y Current agents y position
 * @param m The desired move
 */
Position DesiredPosition(int x, int y, Move m);


/**
 * @brief FillDestPos Fills an array of destination positions.
 * @param s The State
 * @param m An array of all agent moves
 * @param p The array to be filled wih dest positions
 */
void FillDestPos(State* s, Move m[AGENT_COUNT], Position p[AGENT_COUNT]);

/**
 * @brief FixSwitchMove Fixes the desired positions if the agents want
 * switch places in one step.
 * @param s The state
 * @param desiredPositions an array of desired positions
 */
void FixSwitchMove(State* s, Position desiredPositions[AGENT_COUNT]);

/**
 * Given a State s, we want to fill an integer array of size
 * AGENT_COUNT with the following properties:
 *
 */
int ResolveDependencies(State* s, Position des[AGENT_COUNT],
                        int dependency[AGENT_COUNT], int chain[AGENT_COUNT]);


/**
 * @brief PrintDependency Prints a dependency array in a nice
 * format (stdout)
 * @param dependency Integer array that contains the dependencies
 */
void PrintDependency(int dependency[AGENT_COUNT]);

/**
 * @brief PrintDependencyChain Prints a dependency chain in a nice
 * format (stdout)
 * @param dependency Integer array that contains the dependencies
 * @param chain Integer array that contains all chain roots.
 * (-1 is a skip)
 */
void PrintDependencyChain(int dependency[AGENT_COUNT], int chain[AGENT_COUNT]);

}

#endif // STEP_UTILITY_H
