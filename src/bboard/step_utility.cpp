#include <iostream>

#include "bboard.hpp"
#include "step_utility.hpp"

namespace bboard::util
{

Position DesiredPosition(int x, int y, Move move)
{
    Position p;
    p.x = x;
    p.y = y;
    if(move == Move::UP)
    {
        p.y -= 1;
    }
    else if(move == Move::DOWN)
    {
        p.y += 1;
    }
    else if(move == Move::LEFT)
    {
        p.x -= 1;
    }
    else if(move == Move::RIGHT)
    {
        p.x += 1;
    }
    return p;
}

bool IsValidDirection(State* s, int x, int y, Move m)
{
	Position des_pos = DesiredPosition(x, y, m);
	if (!IsPositionOnBoard(des_pos))
	{
		return false;
	}
	int board_val = s->board[des_pos.y][des_pos.x];
	return board_val != Item::RIGID && !IS_WOOD(board_val);
}

// fun fact: also lays bomb, and takes agent out of the board
void FillDestAgentPos(State* s, std::vector<AgentInfo*> aliveAgents, Move m[AGENT_COUNT], Position dest_agent_bos[AGENT_COUNT])
{
	for (int i = 0; i < aliveAgents.size(); i++)
	{
		AgentInfo& agent = *aliveAgents[i];
		dest_agent_bos[i] = { agent.x, agent.y };
		s->board[agent.y][agent.x] = Item::PASSAGE;
		Move move = m[agent.id];
		
		if (move == Move::IDLE || agent.x < 0)
		{
			continue;
		}
		if (move == Move::BOMB)
		{
			s->PlantBomb(agent.x, agent.y, agent.id);
		}
		else if (util::IsValidDirection(s, agent.x, agent.y, move))
		{
			dest_agent_bos[i] = DesiredPosition(agent.x, agent.y, move);
		}
	}
}

// takes bombs out of board also
void FillDestBombPos(State* s, Position dest_bomb_pos[MAX_BOMBS])
{
	FixedQueue<Bomb, MAX_BOMBS>& bombs = s->bombs;
	for (int i = 0; i < bombs.count; i++)
	{
		Bomb bomb = bombs[i];
		Position bomb_pos{ BMB_POS_X(bomb), BMB_POS_Y(bomb) };
		s->board[bomb_pos.y][bomb_pos.x] = Item::PASSAGE;
		dest_bomb_pos[i] = bomb_pos;
		if (BMB_VEL(bomb) > 0)
		{
			Position des_pos = DesiredPosition(BMB_POS_X(bomb), BMB_POS_Y(bomb), Move(BMB_VEL(bomb)));
			if (IsPositionOnBoard(des_pos))
			{
				int board_val = s->board[des_pos.y][des_pos.x];
				if (board_val != Item::RIGID && !IS_WOOD(board_val) && !IS_POWERUP(board_val))
				{
					dest_bomb_pos[i] = des_pos;
				}
			}
		}
	}
}
struct Crossing
{
	bool horizontal;
	int x, y;

	Crossing(Position pos, Position des)
	{
		horizontal = pos.x != des.x;
		x = std::min(pos.x, des.x);
		y = std::min(pos.y, des.y);
	}
	bool operator==(const Crossing& other) const
	{
		return horizontal == other.horizontal && x == other.x && y == other.y;
	}
};
struct CrossingHash
{
	std::size_t operator()(const Crossing& c) const
	{
		return BOARD_SIZE * c.y + c.x + (int)c.horizontal * (BOARD_SIZE * BOARD_SIZE);
	}
};

void FixSwitchMove(State* s, std::vector<AgentInfo*> aliveAgents, Position d[AGENT_COUNT], Position d_bomb[MAX_BOMBS])
{
	std::unordered_map<Crossing, std::pair<int, bool>, CrossingHash> crossings;

	for (int i = 0; i < aliveAgents.size(); i++)
	{
		AgentInfo& agent = *aliveAgents[i];
		if (agent.dead || d[i] == Position{agent.x, agent.y})
		{
			continue;
		}
		Crossing border{ {agent.x, agent.y}, d[i] };
		if (crossings.count(border) > 0)
		{
			d[i] = { agent.x, agent.y };
			int num_agent2 = crossings[border].first;
			d[num_agent2] = { s->agents[aliveAgents[num_agent2]->id].x, s->agents[aliveAgents[num_agent2]->id].y };
		}
		else {
			crossings[border] = { i, true };
		}
    }

	FixedQueue<Bomb, MAX_BOMBS>& bombs = s->bombs;
	for (int i = 0; i < bombs.count; i++)
	{
		Bomb bomb = bombs[i];
		int bx = BMB_POS_X(bomb);
		int by = BMB_POS_Y(bomb);

		if (d_bomb[i] == Position{bx, by})
		{
			continue;
		}
		Crossing border{ {bx, by}, d_bomb[i] };
		if (crossings.count(border) > 0)
		{
			d_bomb[i] = { bx, by };
			const auto& num_and_isagent = crossings.at(border);
			if (!num_and_isagent.second)
			{
				Bomb bomb2 = bombs[num_and_isagent.first];
				d_bomb[num_and_isagent.first] = { BMB_POS_X(bomb2), BMB_POS_Y(bomb2) };
			}
		}
		else {
			crossings[border] = { i, false };
		}
	}
}

void ResolveDependencies(State* s, std::vector<AgentInfo*> aliveAgents, Position des_agent_pos[AGENT_COUNT], Position des_bomb_pos[MAX_BOMBS],
	std::unordered_map<Position, int, PositionHash>& agent_occupancy,
	std::unordered_map<Position, int, PositionHash>& bomb_occupancy)
{
	bool change = true;
	while(change)
	{
		change = false;
		for (int num_agent = 0; num_agent < aliveAgents.size(); num_agent++)
		{
			AgentInfo& agent = *aliveAgents[num_agent];
			Position des_pos = des_agent_pos[num_agent];
			Position curr_pos{ agent.x, agent.y };
			if (!(des_pos == curr_pos) && 
				(agent_occupancy[des_pos] > 1 || bomb_occupancy[des_pos] > 1))
			{
				des_agent_pos[num_agent] = curr_pos;
				agent_occupancy[curr_pos] += 1;
				change = true;
			}
		}
		FixedQueue<Bomb, MAX_BOMBS>& bombs = s->bombs;
		for (int num_bomb = 0; num_bomb < bombs.count; num_bomb++)
		{
			Bomb bomb = bombs[num_bomb];
			Position des_pos = des_bomb_pos[num_bomb];
			Position curr_pos{ BMB_POS_X(bomb), BMB_POS_Y(bomb) };
			if (!(des_pos == curr_pos) &&
				(bomb_occupancy[des_pos] > 1 || agent_occupancy[des_pos] > 1))
			{
				des_bomb_pos[num_bomb] = curr_pos;
				bomb_occupancy[curr_pos] += 1;
				change = true;
			}
		}
	};
}

void HandleKicks(State* s, std::vector<AgentInfo*> aliveAgents, Position des_agent_pos[AGENT_COUNT], Position des_bomb_pos[MAX_BOMBS],
	std::unordered_map<Position, int, PositionHash>& agent_occupancy,
	std::unordered_map<Position, int, PositionHash>& bomb_occupancy,
	Move* moves)
{
	std::vector<std::pair<int, Position>> delayed_bomb_updates;
	std::vector<std::pair<int, Position>> delayed_agent_updates;
	std::unordered_map<int, int> agent_indexed_by_kicked_bomb;
	std::unordered_map<int, int> kicked_bomb_indexed_by_agent;

	FixedQueue<Bomb, MAX_BOMBS>& bombs = s->bombs;
	for (int i = 0; i < bombs.count; i++)
	{
		Bomb& bomb = bombs[i];
		Position bomb_pos{ BMB_POS_X(bomb), BMB_POS_Y(bomb) };
		Position des_pos = des_bomb_pos[i];
		if (agent_occupancy.count(des_pos) == 0)
		{
			continue;
		}
		std::vector<std::pair<int, AgentInfo*>> agent_list;
		for (int j = 0; j < aliveAgents.size(); j++)
		{
			if (des_pos == des_agent_pos[j])
			{
				agent_list.push_back({ j, aliveAgents[j] });
			}
		}
		if (agent_list.size() == 0)
		{
			continue;
		}
		if (agent_list.size() != 1)
		{
			throw std::runtime_error("agent_list should contain a single element at this point");
		}
		int num_agent = agent_list[0].first;
		AgentInfo& agent = *agent_list[0].second;
		Position agent_pos{ agent.x, agent.y };
		if (des_pos == agent_pos)
		{
			if (des_pos != bomb_pos)
			{
				delayed_bomb_updates.push_back({ i, bomb_pos });
			}
			continue;
		}

		if (!agent.canKick)
		{
			delayed_bomb_updates.push_back({ i, bomb_pos });
			delayed_agent_updates.push_back({ i, agent_pos });
			continue;
		}

		bboard::Move direction = moves[agent.id];
		Position target_pos = DesiredPosition(des_pos.x, des_pos.y, direction);

		int target_pos_board = s->board[target_pos.y][target_pos.x];
		if (IsPositionOnBoard(target_pos) &&
			agent_occupancy.count(target_pos) == 0 &&
			bomb_occupancy.count(target_pos) == 0 &&
			target_pos_board != Item::RIGID &&
			!IS_WOOD(target_pos_board)
			&& !IS_POWERUP(target_pos_board))
		{
			bomb_occupancy[des_pos] = 0;
			delayed_bomb_updates.push_back({ i, target_pos });
			agent_indexed_by_kicked_bomb[i] = num_agent;
			kicked_bomb_indexed_by_agent[num_agent] = i;
			SetBombVelocity(bomb, (int)direction);
		}
		else
		{
			delayed_bomb_updates.push_back({ i, bomb_pos });
			delayed_agent_updates.push_back({ num_agent, agent_pos });
		}
	}

	bool change = false;
	for (auto& num_bomb_and_position : delayed_bomb_updates)
	{
		des_bomb_pos[num_bomb_and_position.first] = num_bomb_and_position.second;
		bomb_occupancy[num_bomb_and_position.second] += 1;
		change = true;
	}
	for (auto& num_agent_and_position : delayed_agent_updates)
	{
		des_agent_pos[num_agent_and_position.first] = num_agent_and_position.second;
		agent_occupancy[num_agent_and_position.second] += 1;
		change = true;
	}

	while (change)
	{
		change = false;

		for (int i = 0; i < aliveAgents.size(); i++)
		{
			AgentInfo& agent = *aliveAgents[i];
			Position des_pos = des_agent_pos[i];
			Position curr_pos{ agent.x, agent.y };

			if (des_pos != curr_pos && (agent_occupancy[des_pos] > 1 || bomb_occupancy.count(des_pos) > 0))
			{
				if (kicked_bomb_indexed_by_agent.count(i) > 0)
				{
					int num_bomb = kicked_bomb_indexed_by_agent[i];
					Bomb& bomb = bombs[num_bomb];
					Position bomb_pos{ BMB_POS_X(bomb), BMB_POS_Y(bomb) };
					des_bomb_pos[num_bomb] = bomb_pos;
					bomb_occupancy[bomb_pos] += 1;
					agent_indexed_by_kicked_bomb.erase(num_bomb);
					kicked_bomb_indexed_by_agent.erase(i);
				}
				des_agent_pos[i] = curr_pos;
				agent_occupancy[curr_pos] += 1;
				change = true;
			}
		}

		for (int num_bomb = 0; num_bomb < bombs.count; num_bomb++)
		{
			Bomb& bomb = bombs[num_bomb];
			Position des_pos = des_bomb_pos[num_bomb];
			Position curr_pos{ BMB_POS_X(bomb), BMB_POS_Y(bomb) };

			if (des_pos == curr_pos && agent_indexed_by_kicked_bomb.count(num_bomb) == 0)
			{
				continue;
			}

			int bomb_occ = bomb_occupancy[des_pos];
			int agent_occ = agent_occupancy[des_pos];
			if (bomb_occ > 1 || agent_occ != 0)
			{
				des_bomb_pos[num_bomb] = curr_pos;
				bomb_occupancy[curr_pos] += 1;
				if (agent_indexed_by_kicked_bomb.count(num_bomb) > 0)
				{
					int num_agent = agent_indexed_by_kicked_bomb[num_bomb];
					AgentInfo& agent = *aliveAgents[num_agent];
					Position agent_pos{ agent.x, agent.y };
					des_agent_pos[num_agent] = agent_pos;
					agent_occupancy[agent_pos] += 1;
					kicked_bomb_indexed_by_agent.erase(num_agent);
					agent_indexed_by_kicked_bomb.erase(num_bomb);
				}
				change = true;
			}
		}

	}

	for (int num_bomb = 0; num_bomb < bombs.count; num_bomb++)
	{
		Bomb& bomb = bombs[num_bomb];
		if (des_bomb_pos[num_bomb] == Position{ BMB_POS_X(bomb), BMB_POS_Y(bomb) } &&
			agent_indexed_by_kicked_bomb.count(num_bomb) == 0)
		{
			SetBombVelocity(bomb, 0);
		}
		else
		{
			SetBombPosition(bomb, des_bomb_pos[num_bomb].x, des_bomb_pos[num_bomb].y);
		}
	}
}


void TickFlames(State& state)
{
    for(int i = 0; i < state.flames.count; i++)
    {
        state.flames[i].timeLeft--;
    }
    int flameCount = state.flames.count;
    for(int i = 0; i < flameCount; i++)
    {
        if(state.flames[0].timeLeft == 0)
        {
            state.PopFlame();
        }
    }
}

void TickBombs(State& state)
{
    for(int i = 0; i < state.bombs.count; i++)
    {
        ReduceBombTimer(state.bombs[i]);
    }

    //explode timed-out bombs
    int bombCount = state.bombs.count;
    for(int i = 0; i < bombCount && state.bombs.count > 0; i++)
    {
        if(BMB_TIME(state.bombs[0]) == 0)
        {
            state.ExplodeTopBomb();
            i--;
        }
        else
        {
            break;
        }

    }
}

void ConsumePowerup(State& state, int agentID, int powerUp)
{
    if(powerUp == Item::EXTRABOMB)
    {
        state.agents[agentID].maxBombCount++;
    }
    else if(powerUp == Item::INCRRANGE)
    {
        state.agents[agentID].bombStrength++;
    }
    else if(powerUp == Item::KICK)
    {
        state.agents[agentID].canKick = true;
    }

    state.agents[agentID].collectedPowerupPoints += 1.0 - state.relTimeStep/100.0;
}

bool HasDPCollision(const State& state, Position dp[AGENT_COUNT], int agentID)
{
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        if(agentID == i || state.agents[i].dead) continue;
        if(dp[agentID] == dp[i])
        {
            // a destination position conflict will never
            // result in a valid move
            return true;
        }
    }
    return false;
}

void PrintDependency(int dependency[AGENT_COUNT])
{
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        if(dependency[i] == -1)
        {
            std::cout << "[" << i << " <- ]";
        }
        else
        {
            std::cout << "[" << i << " <- " << dependency[i] << "]";
        }
        std::cout << std::endl;
    }
}

void PrintDependencyChain(int dependency[AGENT_COUNT], int chain[AGENT_COUNT])
{
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        if(chain[i] == -1) continue;

        std::cout << chain[i];
        int k = dependency[chain[i]];

        while(k != -1)
        {
            std::cout << " <- " << k;
            k = dependency[k];
        }
        std::cout << std::endl;
    }
}


}
