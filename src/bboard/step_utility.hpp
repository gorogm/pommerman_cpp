#ifndef STEP_UTILITY_H
#define STEP_UTILITY_H

#include "bboard.hpp"

namespace bboard::util
{

/**
 * @brief DesiredPosition returns the x and y values of the agents
 * destination
 * @param x Current agents x position
 * @param y Current agents y position
 * @param m The desired move
 */
Position DesiredPosition(int x, int y, Move m);



// check if move is valid for an agent
bool IsValidDirection(State* s, int x, int y, Move m);

/**
 * @brief FillDestPos Fills an array of destination positions.
 * @param s The State
 * @param m An array of all agent moves
 * @param p The array to be filled wih dest positions
 */
void FillDestAgentPos(State* s, std::vector<AgentInfo*> aliveAgents, Move m[AGENT_COUNT], Position dest_agent_bos[AGENT_COUNT]);

/**
 * @brief FillDestPos Fills an array of destination positions for bombs.
 * @param s The State
 * @param p The array to be filled wih dest positions
 */
void FillDestBombPos(State* s, Position dest_bomb_pos[MAX_BOMBS]);

/**
 * @brief FixSwitchMove Fixes the desired positions if the agents want
 * switch places in one step.
 * @param s The state
 * @param desiredPositions an array of desired positions
 */
void FixSwitchMove(State* s, std::vector<AgentInfo*> aliveAgents, Position d[AGENT_COUNT], Position dest_bomb_pos[MAX_BOMBS]);

/**
 * TODO: Fill doc for dependency resolving
 *
 */
void ResolveDependencies(State* s, std::vector<AgentInfo*> aliveAgents, Position des_agent_pos[AGENT_COUNT], Position des_bomb_pos[MAX_BOMBS],
	std::unordered_map<Position, int, PositionHash>& agent_occupancy,
	std::unordered_map<Position, int, PositionHash>& bomb_occupancy);

void HandleKicks(State* s, std::vector<AgentInfo*> aliveAgents, Position des_agent_pos[AGENT_COUNT], Position des_bomb_pos[MAX_BOMBS],
	std::unordered_map<Position, int, PositionHash>& agent_occupancy,
	std::unordered_map<Position, int, PositionHash>& bomb_occupancy,
	Move* moves);


/**
 * @brief TickFlames Counts down all flames in the flame queue
 * (and possible extinguishes the flame)
 */
void TickFlames(State& state);

/**
 * @brief TickBombs Counts down all bomb timers and explodes them
 * if they arrive at 10
 */
void TickBombs(State& state);

/**
 * @brief ConsumePowerup Lets an agent consume a powerup
 * @param agentID The agent's ID that consumes the item
 * @param powerUp A powerup item. If it's something else,
 * this function will do nothing.
 */
void ConsumePowerup(State& state, int agentID, int powerUp);

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

/**
 * @brief HasDPCollision Checks if the given agent has a destination
 * position collision with another agent
 * @param The agent that's checked for collisions
 * @return True if there is at least one collision
 */
bool HasDPCollision(const State& state, Position dp[AGENT_COUNT], int agentID);

/**
 * @brief IsOutOfBounds Checks wether a given position is out of bounds
 */
inline bool IsOutOfBounds(const Position& pos)
{
    return pos.x < 0 || pos.y < 0 || pos.x >= BOARD_SIZE || pos.y >= BOARD_SIZE;
}

/**
 * @brief IsOutOfBounds Checks wether a given position is out of bounds
 */
inline bool IsOutOfBounds(const int& x, const int& y)
{
    return x < 0 || y < 0 || x >= BOARD_SIZE || y >= BOARD_SIZE;
}

inline bool IsPositionOnBoard(Position pos)
{
	return !IsOutOfBounds(pos);
}

}

#endif // STEP_UTILITY_H
