#include <iostream>

#include "bboard.hpp"
#include "step_utility.hpp"

namespace bboard
{


void Step(State* state, Move* moves)
{
    ///////////////////////
    // Flames, Explosion //
    ///////////////////////

    util::TickFlames(*state);

	std::vector<AgentInfo*> aliveAgents;
	aliveAgents.reserve(AGENT_COUNT);
	for (int i = 0; i < AGENT_COUNT; i++)
	{
		// we do not consider out of sight agents either
		if (!state->agents[i].dead && state->agents[i].x >= 0 && state->agents[i].y >= 0)
		{
			aliveAgents.push_back(&state->agents[i]);
		}
	}


    ///////////////////////
    //  Player Movement  //
    ///////////////////////

    Position des_agent_pos[AGENT_COUNT];
    util::FillDestAgentPos(state, aliveAgents, moves, des_agent_pos);

	Position des_bomb_pos[MAX_BOMBS];
	util::FillDestBombPos(state, des_bomb_pos);

    util::FixSwitchMove(state, aliveAgents, des_agent_pos, des_bomb_pos);

	// fill agent, and bomb occupancy
	std::unordered_map<Position, int, PositionHash> agent_occupancy;
	std::unordered_map<Position, int, PositionHash> bomb_occupancy;
	for (int i = 0; i < aliveAgents.size(); i++)
	{
		agent_occupancy[des_agent_pos[i]] += 1;
	}
	for (int i = 0; i < state->bombs.count; i++)
	{
		bomb_occupancy[des_bomb_pos[i]] += 1;
	}

	util::ResolveDependencies(state, aliveAgents, des_agent_pos, des_bomb_pos, agent_occupancy, bomb_occupancy);

	util::HandleKicks(state, aliveAgents, des_agent_pos, des_bomb_pos, agent_occupancy, bomb_occupancy, moves);

	//move agents, collect powerup
	for (int num_agent = 0; num_agent < aliveAgents.size(); num_agent++)
	{
		AgentInfo& agent = *aliveAgents[num_agent];
		Position agent_pos{ agent.x, agent.y };
		if (des_agent_pos[num_agent] != agent_pos)
		{
			Position temp_pos = util::DesiredPosition(agent.x, agent.y, moves[agent.id]);
			agent.x = temp_pos.x;
			agent.y = temp_pos.y;
			int& target_pos_board = state->board[temp_pos.y][temp_pos.x];
			if (IS_POWERUP(target_pos_board))
			{
				util::ConsumePowerup(*state, agent.id, target_pos_board);
				target_pos_board = 0;
			}
		}
	}

	util::TickBombs(*state);

	for (int num_bomb = 0; num_bomb < state->bombs.count; num_bomb++)
	{
		Bomb& bomb = state->bombs[num_bomb];
		int bomb_x = BMB_POS_X(bomb);
		int bomb_y = BMB_POS_Y(bomb);
		state->board[bomb_y][bomb_x] = Item::BOMB;
	};
	
	for (int num_agent = 0; num_agent < aliveAgents.size(); num_agent++)
	{
		AgentInfo& agent = *aliveAgents[num_agent];
		Position agent_pos{ agent.x, agent.y };
		if (IS_FLAME(state->board[agent.y][agent.x]))
		{
			state->Kill(agent.id);
		}
		else
		{
			state->board[agent.y][agent.x] = Item::AGENT0 + agent.id;
		}
	}
	//////////////
}

}
