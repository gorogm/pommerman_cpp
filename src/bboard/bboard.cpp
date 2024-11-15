#include <random>
#include <chrono>
#include <thread>
#include <iostream>
#include <array>

#include "bboard.hpp"
#include "colors.hpp"
#include "agents.hpp"
#include <map>
#include <fstream>
#include <sstream>
#include <cstring>

#ifdef _WIN32
#  define EXPORTIT __declspec( dllexport )
#else
#  define EXPORTIT
#endif

namespace bboard
{
/////////////////////////
// Auxiliary Functions //
/////////////////////////

/**
 * @brief SpawnFlameItem Spawns a single flame item on the board
 * @param s The state on which the flames should be spawned
 * @param x The x position of the fire
 * @param y The y position of the fire
 * @param signature An auxiliary integer less than 255
 * @param agentID ID of agent who planted the bomb
 * @param bombDistance cell distance from exploded bomb
 * @return Could the flame be spawned?
 */
inline bool SpawnFlameItem(State& s, int x, int y, uint16_t signature, int agentID, uint8_t bombDistance)
{
    if(s.board[y][x] >= Item::AGENT0)
    {
        s.Kill(s.board[y][x] - Item::AGENT0);
    }
    if(s.board[y][x] == Item::BOMB || s.board[y][x] >= Item::AGENT0)
    {
        for(int i = 0; i < s.bombs.count; i++)
        {
            if(BMB_POS(s.bombs[i]) == (x + (y << 4)))
            {
                s.longestChainedBombDistance = std::max(s.longestChainedBombDistance, bombDistance);
                s.ExplodeBombAt(i); // chained explosion
                break;
            }
        }
    }

    if(s.board[y][x] != Item::RIGID)
    {
        int old = s.board[y][x];
        bool wasWood = IS_WOOD(old);
        s.board[y][x] = Item::FLAMES + signature;
        if(wasWood)
        {
            /*
            //Give reward to closest agent
            int closestAgent = -1;
            int closestDistance = INT32_MAX;
            for(int agentId=0; agentId<4; agentId++) {
                if (!s.agents[agentId].dead && s.agents[agentId].x >= 0 && (std::abs(s.agents[agentId].x - x) + std::abs(s.agents[agentId].y - y)) < closestDistance) {
                    closestAgent = agentId;
                    closestDistance = (std::abs(s.agents[agentId].x - x) + std::abs(s.agents[agentId].y - y));
                }
            }
            if(closestAgent >= 0) {
                s.agents[closestAgent].woodDemolished += 1.0f - s.relTimeStep / 50.0f;
            }*/
            if(agentID < BMB_ID_UNKNOWN)
            {
                s.agents[agentID].woodDemolished += 1.0f - s.relTimeStep / 50.0f;
            }

            s.board[y][x] += WOOD_POWFLAG(old); // set the powerup flag
        }
        return !wasWood; // if wood, then only destroy 1
    }
    else
    {
        return false;
    }
}

int ChooseItemOuter(int tmp)
{
    if(tmp > 2 || tmp == 0)
    {
        return Item::PASSAGE;
    }
    else if(tmp == 2)
    {
        return Item::WOOD;
    }
    else if(tmp == 1)
    {
        return Item::RIGID;
    }
    return Item::PASSAGE;
}

int ChooseItemInner(int tmp)
{
    if(tmp >= 2)
    {
        return Item::WOOD;
    }
    else if(tmp == 1)
    {
        return Item::RIGID;
    }
    return Item::PASSAGE;
}

/**
 * @brief PopBomb A proxy for FixedQueue::PopElem, but also
 * takes care of agent count
 */
inline void PopBomb(State& state)
{
    if(BMB_ID_KNOWN(state.bombs[0]))
        state.agents[BMB_ID(state.bombs[0])].bombCount--;
    state.bombs.PopElem();
}

/**
 * @brief IsOutOfBounds Checks wether a given position is out of bounds
 */
inline bool IsOutOfBounds(const int& x, const int& y)
{
    return x < 0 || y < 0 || x >= BOARD_SIZE || y >= BOARD_SIZE;
}

bool _CheckPos_basic(State * state, int x, int y)
{
    return !IsOutOfBounds(x, y) && state->board[y][x] != RIGID && !IS_WOOD(state->board[y][x]);
}
bool _CheckPos_any(State * state, int x, int y)
{
    return !IsOutOfBounds(x, y) && (state->board[y][x] == PASSAGE || IS_FLAME(state->board[y][x]));
}

///////////////////
// State Methods //
///////////////////



void State::ExplodeBombAt(int i)
{
    if(BMB_ID_KNOWN(bombs[i]))
        agents[BMB_ID(bombs[i])].bombCount--;
    SpawnFlame(BMB_POS_X(bombs[i]), BMB_POS_Y(bombs[i]), BMB_STRENGTH(bombs[i]), BMB_ID(bombs[i]));
    bombs.RemoveAt(i);
}
void State::PlantBomb(int x, int y, int id, bool setItem)
{
    PlantBombModifiedLife(x, y,  id, BOMB_LIFETIME, setItem);
}

void State::PlantBombModifiedLife(int x, int y, int id, int lifeTime, bool setItem)
{
    if(agents[id].bombCount >= agents[id].maxBombCount)
    {
        std::cout << "Can't place bomb agent " << id << ", already used all" << std::endl;
        return;
    }

    Bomb* b = &bombs.NextPos();
    SetBombID(*b, id);
    SetBombPosition(*b, x, y);
    SetBombStrength(*b, agents[id].bombStrength);
    SetBombDirection(*b, Direction::IDLE);
    SetBombTime(*b, lifeTime);

    if(setItem)
    {
        board[y][x] = Item::BOMB;
    }

    agents[id].bombCount++;
    bombs.count++;
}

void State::PopFlame()
{
    Flame& f = flames[0];
    const int s = f.strength;
    int x = f.position.x;
    int y = f.position.y;

    uint16_t signature = uint16_t(x + BOARD_SIZE * y);

    // iterate over both axis (from x-s to x+s // y-s to y+s)
    for(int i = -s; i <= s; i++)
    {
        if(!IsOutOfBounds(x + i, y) && IS_FLAME(board[y][x + i]))
        {
            // only remove if this is my own flame
            int b = board[y][x + i];
            if(FLAME_ID(b) == signature)
            {
                board[y][x + i] = FlagItem(FLAME_POWFLAG(b));
            }
        }
        if(!IsOutOfBounds(x, y + i) && IS_FLAME(board[y + i][x]))
        {
            int b = board[y + i][x];
            if(FLAME_ID(b) == signature)
            {
                board[y + i][x] = FlagItem(FLAME_POWFLAG(b));
            }
        }
    }

    flames.PopElem();
}

Item State::FlagItem(int pwp)
{
    if     (pwp == 0) return Item::PASSAGE;
    else if(pwp == 1) return Item::EXTRABOMB;
    else if(pwp == 2) return Item::INCRRANGE;
    else if(pwp == 3) return Item::KICK;
    else              return Item::PASSAGE;
}

void State::ExplodeTopBomb()
{
    Bomb& c = bombs[0];
    SpawnFlame(BMB_POS_X(c), BMB_POS_Y(c), BMB_STRENGTH(c), BMB_ID(c));
    PopBomb(*this);
}
void State::SpawnFlame(int x, int y, int strength, int agentID)
{
    __glibcxx_assert(strength > 0);
    Flame& f = flames.NextPos();
    f.position.x = x;
    f.position.y = y;
    f.strength = strength;
    f.timeLeft = FLAME_LIFETIME;

    // unique flame id
    uint16_t signature = uint16_t((x + BOARD_SIZE * y) << 3);
    flames.count++;

    // kill agent possibly in origin
    if(board[y][x] >= Item::AGENT0)
    {
        Kill(board[y][x] - Item::AGENT0);
    }

    // override origin
    board[y][x] = Item::FLAMES + signature;

    // right
    for(int i = 1; i <= strength; i++)
    {
        if(x + i >= BOARD_SIZE) break; // bounds

        if(!SpawnFlameItem(*this, x + i, y, signature, agentID, i))
        {
            break;
        }
    }

    // left
    for(int i = 1; i <= strength; i++)
    {
        if(x - i < 0) break; // bounds

        if(!SpawnFlameItem(*this, x - i, y, signature, agentID, i))
        {
            break;
        }
    }

    // top
    for(int i = 1; i <= strength; i++)
    {
        if(y + i >= BOARD_SIZE) break; // bounds

        if(!SpawnFlameItem(*this, x, y + i, signature, agentID, i))
        {
            break;
        }
    }

    // bottom
    for(int i = 1; i <= strength; i++)
    {
        if(y - i < 0) break; // bounds

        if(!SpawnFlameItem(*this, x, y - i, signature, agentID, i))
        {
            break;
        }
    }
}

void State::SpawnFlame_passive(int x, int y, int strength)
{
    Flame& f = flames.NextPos();
    f.position.x = x;
    f.position.y = y;
    f.strength = strength;
    f.timeLeft = FLAME_LIFETIME-1;

    // unique flame id
    uint16_t signature = uint16_t((x + BOARD_SIZE * y) << 3);

    flames.count++;

    // override origin
    __glibcxx_assert(board[y][x] = Item::FLAMES);
    board[y][x] = Item::FLAMES + signature;

    // right
    for(int i = 1; i <= strength; i++)
    {
        if(x + i >= BOARD_SIZE) break; // bounds

        if(IS_FLAME(board[y][x+i]))
        {
            board[y][x+i] = Item::FLAMES + signature;
        }else{
            break;
        }
    }

    // left
    for(int i = 1; i <= strength; i++)
    {
        if(x - i < 0) break; // bounds

        if(IS_FLAME(board[y][x-i]))
        {
            board[y][x-i] = Item::FLAMES + signature;
        }else{
            break;
        }
    }

    // top
    for(int i = 1; i <= strength; i++)
    {
        if(y + i >= BOARD_SIZE) break; // bounds

        if(IS_FLAME(board[y+i][x]))
        {
            board[y+i][x] = Item::FLAMES + signature;
        }else{
            break;
        }
    }

    // bottom
    for(int i = 1; i <= strength; i++)
    {
        if(y - i < 0) break; // bounds

        if(IS_FLAME(board[y-i][x]))
        {
            board[y-i][x] = Item::FLAMES + signature;
        }else{
            break;
        }
    }
}

bool State::HasBomb(int x, int y) const
{
    for(int i = 0; i < bombs.count; i++)
    {
        if(BMB_POS_X(bombs[i]) == x && BMB_POS_Y(bombs[i]) == y)
        {
            return true;
        }
    }
    return false;
}

Bomb* State::GetBomb(int x, int y)
{
    for(int i = 0; i < bombs.count; i++)
    {
        if(BMB_POS_X(bombs[i]) == x && BMB_POS_Y(bombs[i]) == y)
        {
            return &bombs[i];
        }
    }
    std::cout << "GetBomb: bomb was not found at " << x << ":" << y << std::endl;
    return nullptr;
}

int State::GetAgent(int x, int y)
{
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        if(!agents[i].dead && agents[i].x == x && agents[i].y == y)
        {
            return i;
        }
    }
    return -1;
}

int State::GetBombIndex(int x, int y)
{
    for(int i = 0; i < bombs.count; i++)
    {
        if(BMB_POS_X(bombs[i]) == x && BMB_POS_Y(bombs[i]) == y)
        {
            return i;
        }
    }
    return -1;
}

void State::PutAgent(int x, int y, int agentID)
{
    int b = Item::AGENT0 + agentID;
    board[y][x] = b;

    agents[agentID].x = x;
    agents[agentID].y = y;
}

void State::PutAgentsInCorners(int a0, int a1, int a2, int a3)
{
    int b = Item::AGENT0;

    board[0][0] = b + a0;
    board[0][BOARD_SIZE - 1] = b + a1;
    board[BOARD_SIZE - 1][BOARD_SIZE - 1] = b + a2;
    board[BOARD_SIZE - 1][0] = b + a3;

    agents[a1].x = agents[a2].x = BOARD_SIZE - 1;
    agents[a2].y = agents[a3].y = BOARD_SIZE - 1;
}

//////////////////////
// bboard namespace //
//////////////////////

void InitState(State* result, int a0, int a1, int a2, int a3)
{
    // Randomly put obstacles
    InitBoardItems(*result);
    result->PutAgentsInCorners(a0, a1, a2, a3);
}

void InitBoardItems(State& result, int seed)
{
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> intDist(0,6);

    FixedQueue<int, BOARD_SIZE * BOARD_SIZE> q;

    for(int i = 0; i < BOARD_SIZE; i++)
    {
        for(int  j = 0; j < BOARD_SIZE; j++)
        {
            int tmp = intDist(rng);
            result.board[i][j] = ChooseItemOuter(tmp);

            if(IS_WOOD(result.board[i][j]))
            {
                q.AddElem(j + BOARD_SIZE * i);
            }
        }
    }

    std::uniform_int_distribution<int> idxSample(0, q.count - 1);
    std::uniform_int_distribution<int> choosePwp(1, 4);
    int total = 0;
    while(true)
    {
        int idx = q[idxSample(rng)];
        if((result.board[0][idx] & 0xFF) == 0)
        {
            result.board[0][idx] += choosePwp(rng);
            total++;
        }

        if(total >= float(q.count)/2)
            break;
    }
}

void StartGame(State* state, Agent* agents[AGENT_COUNT], int timeSteps)
{
    Move moves[4];

    for(int i = 0; i < timeSteps; i++)
    {
        std::cout << "\033c"; // clear console on linux
        for(int j = 0; j < AGENT_COUNT; j++)
        {
            moves[j] = agents[j]->act(state);
        }

        Step(state, moves);
        PrintState(state);

        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }
}

void PrintState(const State* state, bool clearConsole)
{
    std::string result = "";

    // clears console on linux
    if(clearConsole)
        std::cout << "\033c";

    for(int y = 0; y < BOARD_SIZE; y++)
    {
        for(int x = 0; x < BOARD_SIZE; x++)
        {
            int item = state->board[y][x];
            result += PrintItem(item);
        }
        std::cout << (result) << "          ";
        result = "";
        // Print AgentInfo
        if(y < AGENT_COUNT)
        {
            int i = y;
            if(state->agents[i].dead)
            {
                std::printf("Agent %d:  X-(", i);
            }else {
                std::printf("Agent %d: %s %d/%d  %s %d  %s %d",
                            i,
                            PrintItem(Item::EXTRABOMB).c_str(), state->agents[i].maxBombCount, state->agents[i].bombCount,
                            PrintItem(Item::INCRRANGE).c_str(), state->agents[i].bombStrength,
                            PrintItem(Item::KICK).c_str(), state->agents[i].canKick);
            }
        }
        else if(y == AGENT_COUNT + 1)
        {
            std::cout << "Bombs:  [  ";
            for(int i = 0; i < state->bombs.count; i++)
            {
                std::cout << BMB_ID(state->bombs[i]) << "(" << BMB_POS_Y(state->bombs[i]) << ":" << BMB_POS_X(state->bombs[i]) << " d:" << BMB_STRENGTH(state->bombs[i]) << " -" << BMB_TIME(state->bombs[i]);
                if(BMB_DIR(state->bombs[i]) == 0)
                    std::cout << " S";
                else if(BMB_DIR(state->bombs[i]) == 1)
                    std::cout << " U";
                else if(BMB_DIR(state->bombs[i]) == 2)
                    std::cout << " D";
                else if(BMB_DIR(state->bombs[i]) == 3)
                    std::cout << " L";
                else if(BMB_DIR(state->bombs[i]) == 4)
                    std::cout << " R";
                else
                    std::cout << " ? ";
                std::cout << ") ";
            }
            std::cout << "]";
        }
        else if(y == AGENT_COUNT + 2)
        {
            std::cout << "Flames: [  ";
            for(int i = 0; i < state->flames.count; i++)
            {
                std::cout << state->flames[i].timeLeft << "  ";
            }
            std::cout << "]";
        }
        std::cout << std::endl;
    }
}

std::string PrintItem(int item)
{
    std::string wood = "[\u25A0]";
    std::string fire = " \U0000263C ";

    switch(item)
    {
        case Item::PASSAGE:
            return "   ";
        case Item::RIGID:
            return "[X]";
        case Item::BOMB:
            return " \u25CF ";
        case Item::EXTRABOMB:
            return " \u24B7 ";
        case Item::INCRRANGE:
            return " \u24C7 ";
        case Item::KICK:
            return " \u24C0 ";
    }
    if(IS_WOOD(item))
    {
        return FBLU(wood);
    }
    if(IS_FLAME(item))
    {
        return FRED(fire);
    }
    //agent number
    if(item >= Item::AGENT0)
    {
        return " "  +  std::to_string(item - Item::AGENT0) + " ";
    }
    else
    {
        return "[?]";
    }
}

}

typedef std::map<std::string, float> ConfigInfo;

ConfigInfo readHyperparams(std::string configFile) {
    ConfigInfo configValues;

    std::ifstream fileStream(configFile);
    if(!fileStream.is_open())
        return configValues;

    std::string line;
    while (std::getline(fileStream, line)) {
        std::istringstream is_line(line);
        std::string key;
        if (std::getline(is_line, key, '=')) {
            std::string value;
            if (key[0] == '#')
                continue;

            if (std::getline(is_line, value)) {
                configValues[key] = std::stof(value);
            }
        }
    }
    fileStream.close();
    return configValues;
}

std::array<std::shared_ptr<bboard::Environment>, 4> envs;
std::array<std::shared_ptr<agents::FrankfurtAgent>, 4> frankfurtAgents;
std::array<std::shared_ptr<agents::GottingenAgent>, 4> gottingenAgents;

void init_agent_frankfurt(int id)
{
    envs[id] = std::make_shared<bboard::Environment>();
    frankfurtAgents[id] = std::make_shared<agents::FrankfurtAgent>();
    envs[id]->MakeGameFromPython(id);
    return;
/*
	ConfigInfo hyperparams = readHyperparams("/tmp/hyperparams.txt");

	if (hyperparams.count("reward_first_step_idle") > 0) {
		std::cout << "Settings reward_first_step_idle to " << hyperparams["reward_first_step_idle"] << std::endl;
		frankfurtAgents[id]->reward_first_step_idle = hyperparams["reward_first_step_idle"];
	}
	if (hyperparams.count("reward_sooner_later_ratio") > 0) {
		std::cout << "Settings reward_sooner_later_ratio to " << hyperparams["reward_sooner_later_ratio"] << std::endl;
		frankfurtAgents[id]->reward_sooner_later_ratio = hyperparams["reward_sooner_later_ratio"];
	}

    if (hyperparams.count("reward_extraBombPowerupPoints") > 0) {
        std::cout << "Settings reward_extraBombPowerupPoints to " << hyperparams["reward_extraBombPowerupPoints"] << std::endl;
        frankfurtAgents[id]->reward_extraBombPowerupPoints = hyperparams["reward_extraBombPowerupPoints"];
    }
    if (hyperparams.count("reward_extraRangePowerupPoints") > 0) {
        std::cout << "Settings reward_extraRangePowerupPoints to " << hyperparams["reward_extraRangePowerupPoints"] << std::endl;
        frankfurtAgents[id]->reward_extraRangePowerupPoints = hyperparams["reward_extraRangePowerupPoints"];
    }
    if (hyperparams.count("reward_otherKickPowerupPoints") > 0) {
        std::cout << "Settings reward_otherKickPowerupPoints to " << hyperparams["reward_otherKickPowerupPoints"] << std::endl;
        frankfurtAgents[id]->reward_otherKickPowerupPoints = hyperparams["reward_otherKickPowerupPoints"];
    }
    if (hyperparams.count("reward_firstKickPowerupPoints") > 0) {
        std::cout << "Settings reward_firstKickPowerupPoints to " << hyperparams["reward_firstKickPowerupPoints"] << std::endl;
        frankfurtAgents[id]->reward_firstKickPowerupPoints = hyperparams["reward_firstKickPowerupPoints"];
    }

	if (hyperparams.count("reward_move_to_enemy") > 0) {
		std::cout << "Settings reward_move_to_enemy to " << hyperparams["reward_move_to_enemy"] << std::endl;
		frankfurtAgents[id]->reward_move_to_enemy = hyperparams["reward_move_to_enemy"];
	}
	if (hyperparams.count("reward_move_to_pickup") > 0) {
		std::cout << "Settings reward_move_to_pickup to " << hyperparams["reward_move_to_pickup"] << std::endl;
		frankfurtAgents[id]->reward_move_to_pickup = hyperparams["reward_move_to_pickup"];
	}
    if (hyperparams.count("reward_woodDemolished") > 0) {
        std::cout << "Settings reward_woodDemolished to " << hyperparams["reward_woodDemolished"] << std::endl;
        frankfurtAgents[id]->reward_woodDemolished = hyperparams["reward_woodDemolished"];
    }
    if (hyperparams.count("weight_of_average_Epoint") > 0) {
        std::cout << "Settings weight_of_average_Epoint to " << hyperparams["weight_of_average_Epoint"] << std::endl;
        frankfurtAgents[id]->weight_of_average_Epoint = hyperparams["weight_of_average_Epoint"];
    }*/
}
void init_agent_gottingen(int id)
{
    envs[id] = std::make_shared<bboard::Environment>();
    gottingenAgents[id] = std::make_shared<agents::GottingenAgent>();
    envs[id]->MakeGameFromPython(id);
}


float episode_end_frankfurt(int id)
{
    if (frankfurtAgents[id]->turns == 0)
        frankfurtAgents[id]->turns++;
    float avg_simsteps_per_turn = frankfurtAgents[id]->totalSimulatedSteps / (float)frankfurtAgents[id]->turns;
    std::cout << "Episode end for agent " << id << ". Turns: " << frankfurtAgents[id]->turns << " avg.sim.steps: " << avg_simsteps_per_turn << std::endl;
    frankfurtAgents[id] = std::make_shared<agents::FrankfurtAgent>();
    envs[id] = std::make_shared<bboard::Environment>();
    envs[id]->MakeGameFromPython(id);
    return avg_simsteps_per_turn;
}
float episode_end_gottingen(int id)
{
    if (gottingenAgents[id]->turns == 0)
        gottingenAgents[id]->turns++;
    float avg_simsteps_per_turn = gottingenAgents[id]->totalSimulatedSteps / (float)gottingenAgents[id]->turns;
    std::cout << "Episode end for agent " << id << ". Turns: " << gottingenAgents[id]->turns << " avg.sim.steps: " << avg_simsteps_per_turn << std::endl;
    gottingenAgents[id] = std::make_shared<agents::GottingenAgent>();
    envs[id] = std::make_shared<bboard::Environment>();
    envs[id]->MakeGameFromPython(id);
    return avg_simsteps_per_turn;
}

int getStep_frankfurt(int id, bool agent0Alive, bool agent1Alive, bool agent2Alive, bool agent3Alive, uint8_t * board, double * bomb_life, double * bomb_blast_strength, double * bomb_moving_direction, double * flame_life, int posx, int posy, int blast_strength, bool can_kick, int ammo, int game_type, int teammate_id, int message1, int message2)
{
    frankfurtAgents[id]->start_time = std::chrono::high_resolution_clock::now();
#ifdef VERBOSE_STATE
    std::cout << std::endl;
#endif

    envs[id]->MakeGameFromPython_frankfurt(agent0Alive, agent1Alive, agent2Alive, agent3Alive, board, bomb_life, bomb_blast_strength, bomb_moving_direction, flame_life, posx, posy, blast_strength, can_kick, ammo, game_type, teammate_id, message1, message2);

    frankfurtAgents[id]->id = envs[id]->GetState().ourId;
#ifdef VERBOSE_STATE
    PrintState(&envs[id]->GetState());
#endif

    // Ask the agent where to go
    return (int)frankfurtAgents[id]->act(&envs[id]->GetState());
}
int getStep_gottingen(int id, bool agent0Alive, bool agent1Alive, bool agent2Alive, bool agent3Alive, uint8_t * board, double * bomb_life, double * bomb_blast_strength, double * bomb_moving_direction, double * flame_life, int posx, int posy, int blast_strength, bool can_kick, int ammo, int game_type, int teammate_id, int message1, int message2)
{
    gottingenAgents[id]->start_time = std::chrono::high_resolution_clock::now();
#ifdef VERBOSE_STATE
    std::cout << std::endl;
#endif

    envs[id]->MakeGameFromPython_gottingen(agent0Alive, agent1Alive, agent2Alive, agent3Alive, board, bomb_life, bomb_blast_strength, bomb_moving_direction, flame_life, posx, posy, blast_strength, can_kick, ammo, game_type, teammate_id, message1, message2);

    gottingenAgents[id]->id = envs[id]->GetState().ourId;
#ifdef VERBOSE_STATE
    PrintState(&envs[id]->GetState());
#endif

    // Ask the agent where to go
    return (int)gottingenAgents[id]->act(&envs[id]->GetState());
}

int getMessage_frankfurt(int id, int messagePart)
{
    return frankfurtAgents[id]->message[messagePart];
}

int getMessage_gottingen(int id, int messagePart)
{
    return gottingenAgents[id]->message[messagePart];
}

void tests()
{
    int tests_run = 0;
    int successful_tests = 0;
    int id = 1;
    bboard::Move moves_in_one_step[4];

    uint8_t * board = new uint8_t[11*11];
    double * bomb_life = new double[11*11];
    double * bomb_blast_strength = new double[11*11];
    double * bomb_movement = new double[11*11];
    double * flame_life = new double[11*11];
    std::chrono::high_resolution_clock::time_point start_time;
    int expected_result;
    size_t millis;
    bool success;

std::cout << "TEST: DEFENSE KICK" << std::endl;
    memset(board, 0, 11*11*sizeof(uint8_t));
    memset(bomb_life, 0, 11*11*sizeof(double));
    memset(bomb_blast_strength, 0, 11*11*sizeof(double));
    memset(bomb_movement, 0, 11*11*sizeof(double));
    memset(flame_life, 0, 11*11*sizeof(double));
    board[4 * 11 + 4] = bboard::PyAGENT1;
    board[4 * 11 + 3] = bboard::PyBOMB;
    board[4 * 11 + 5] = bboard::PyBOMB;
    board[3 * 11 + 4] = bboard::PyBOMB;
    board[5 * 11 + 4] = bboard::PyBOMB;
    //board[4 * 11 + 2] = bboard::PyRIGID;
    board[4 * 11 + 6] = bboard::PyRIGID;
    board[2 * 11 + 4] = bboard::PyRIGID;
    board[6 * 11 + 4] = bboard::PyRIGID;
    bomb_life[4 * 11 + 3] = 2;
    bomb_life[4 * 11 + 5] = 2;
    bomb_life[3 * 11 + 4] = 2;
    bomb_life[5 * 11 + 4] = 2;
    bomb_blast_strength[4 * 11 + 3] = 2;
    bomb_blast_strength[4 * 11 + 5] = 2;
    bomb_blast_strength[3 * 11 + 4] = 2;
    bomb_blast_strength[5 * 11 + 4] = 2;

    init_agent_frankfurt(1);
    start_time = std::chrono::high_resolution_clock::now();
    int step = getStep_frankfurt(1, true, true, false, false, board, bomb_life, bomb_blast_strength, bomb_movement, flame_life, 4, 4, 1, true, 0, bboard::GameType::Team, 13, -1, -1);
    PrintState(&envs[id]->GetState());
    expected_result = 3;
    success = expected_result == step;
    tests_run++;
    if(success) successful_tests++;
    millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time).count();
    std::cout << "TEST RESULT: " << expected_result << " =?=  " << step << " success: " << success << " time: " << millis << std::endl;
    episode_end_frankfurt(1);

std::cout << "TEST: DON'T STEP ON FLAME" << std::endl;
    memset(board, 0, 11*11*sizeof(uint8_t));
    memset(bomb_life, 0, 11*11*sizeof(double));
    memset(bomb_blast_strength, 0, 11*11*sizeof(double));
    memset(bomb_movement, 0, 11*11*sizeof(double));
    memset(flame_life, 0, 11*11*sizeof(double));
    board[4 * 11 + 4] = bboard::PyAGENT0 + id;
    board[4 * 11 + 4] = bboard::PyAGENT1;
    board[4 * 11 + 3] = bboard::PyFLAMES;
    board[4 * 11 + 5] = bboard::PyFLAMES;
    board[3 * 11 + 4] = bboard::PyFLAMES;
    board[5 * 11 + 4] = bboard::PyFLAMES;
    board[4 * 11 + 2] = bboard::PyFLAMES;
    board[4 * 11 + 6] = bboard::PyFLAMES;
    board[2 * 11 + 4] = bboard::PyFLAMES;
    board[6 * 11 + 4] = bboard::PyFLAMES;

    init_agent_frankfurt(id);
    getStep_frankfurt(id, true, true, true, true, board, bomb_life, bomb_blast_strength, bomb_movement, flame_life, 4, 4, 1, true, 0, bboard::GameType::Team, 13, -1, -1);
    moves_in_one_step[0] = bboard::Move::IDLE;
    moves_in_one_step[1] = bboard::Move::IDLE;
    moves_in_one_step[2] = bboard::Move::IDLE;
    moves_in_one_step[3] = bboard::Move::IDLE;
    envs[id]->GetState().timeStep = 100;
    PrintState(&envs[id]->GetState());
    moves_in_one_step[id] = frankfurtAgents[id]->act(&envs[id]->GetState());
    bboard::Step(&envs[id]->GetState(), moves_in_one_step);
    //std::this_thread::sleep_for(std::chrono::milliseconds(50)); // ez mi???
    expected_result = 0;
    success = expected_result == (int)moves_in_one_step[id];
    tests_run++;
    if(success) successful_tests++;
    millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time).count();
    std::cout << "TEST RESULT: " << expected_result << " =?=  " << (int)moves_in_one_step[id] << " success: " << success << " time: " << millis << std::endl;
    episode_end_frankfurt(id);

std::cout << "TEST: DON'T PUSH BOMB TO FLAME" << std::endl;
    memset(board, 0, 11*11*sizeof(uint8_t));
    memset(bomb_life, 0, 11*11*sizeof(double));
    memset(bomb_blast_strength, 0, 11*11*sizeof(double));
    memset(bomb_movement, 0, 11*11*sizeof(double));
    memset(flame_life, 0, 11*11*sizeof(double));
    board[4 * 11 + 4] = bboard::PyAGENT0 + id;
    board[4 * 11 + 4] = bboard::PyAGENT1;
    board[4 * 11 + 3] = bboard::PyBOMB;
    board[4 * 11 + 5] = bboard::PyBOMB;
    board[3 * 11 + 4] = bboard::PyBOMB;
    board[5 * 11 + 4] = bboard::PyBOMB;
    bomb_blast_strength[4 * 11 + 3] = 4;
    bomb_blast_strength[4 * 11 + 5] = 4;
    bomb_blast_strength[3 * 11 + 4] = 4;
    bomb_blast_strength[5 * 11 + 4] = 4;
    bomb_life[4 * 11 + 3] = 6;
    bomb_life[4 * 11 + 5] = 6;
    bomb_life[3 * 11 + 4] = 6;
    bomb_life[5 * 11 + 4] = 6;
    board[4 * 11 + 2] = bboard::PyFLAMES;
    //board[4 * 11 + 6] = bboard::PyFLAMES;
    board[2 * 11 + 4] = bboard::PyFLAMES;
    board[6 * 11 + 4] = bboard::PyFLAMES;

    init_agent_frankfurt(id);
    getStep_frankfurt(id, true, true, true, true, board, bomb_life, bomb_blast_strength, bomb_movement, flame_life, 4, 4, 1, true, 0, bboard::GameType ::Team, 13, -1, -1);
    PrintState(&envs[id]->GetState());
    moves_in_one_step[0] = bboard::Move::IDLE;
    moves_in_one_step[1] = bboard::Move::IDLE;
    moves_in_one_step[2] = bboard::Move::IDLE;
    moves_in_one_step[3] = bboard::Move::IDLE;
    envs[id]->GetState().timeStep = 100;
    moves_in_one_step[id] = frankfurtAgents[id]->act(&envs[id]->GetState());
    bboard::Step(&envs[id]->GetState(), moves_in_one_step);
    //std::this_thread::sleep_for(std::chrono::milliseconds(50)); // ez mi??
    expected_result = 4;
    success = expected_result == (int)moves_in_one_step[id];
    tests_run++;
    if(success) successful_tests++;
    millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time).count();
    std::cout << "TEST RESULT: " << expected_result << " =?=  " << (int)moves_in_one_step[id] << " success: " << success << " time: " << millis << std::endl;

    episode_end_frankfurt(id);

std::cout << "TEST: GOING AROUND" << std::endl;
    memset(board, 0, 11*11*sizeof(uint8_t));
    memset(bomb_life, 0, 11*11*sizeof(double));
    memset(bomb_blast_strength, 0, 11*11*sizeof(double));
    memset(bomb_movement, 0, 11*11*sizeof(double));
    memset(flame_life, 0, 11*11*sizeof(double));
    board[4 * 11 + 4] = bboard::PyAGENT0 + id;

    init_agent_frankfurt(id);
    getStep_frankfurt(id, true, true, true, true, board, bomb_life, bomb_blast_strength, bomb_movement, flame_life, 4, 4, 1, true, 0, bboard::GameType::Team, 13, -1, -1);
    PrintState(&envs[id]->GetState());
    moves_in_one_step[0] = bboard::Move::IDLE;
    moves_in_one_step[1] = bboard::Move::IDLE;
    moves_in_one_step[2] = bboard::Move::IDLE;
    moves_in_one_step[3] = bboard::Move::IDLE;
    envs[id]->GetState().timeStep = 100;
    for(int i=0; i<30; i++) {
        moves_in_one_step[id] = frankfurtAgents[id]->act(&envs[id]->GetState());
        //PrintState(&envs[id]->GetState());
        if(i == 0) {
            expected_result = 1;
            success = expected_result == (int)moves_in_one_step[id];
            tests_run++;
            if(success) successful_tests++;
            millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time).count();
            std::cout << "TEST RESULT: " << expected_result << " =?=  " << (int)moves_in_one_step[id] << " success: " << success << " time: " << millis << std::endl;
        }
        bboard::Step(&envs[id]->GetState(), moves_in_one_step);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    episode_end_frankfurt(id);

std::cout << "TEST: RUSHING" << std::endl;
    id = 3;
    memset(board, 0, 11*11*sizeof(uint8_t));
    memset(bomb_life, 0, 11*11*sizeof(double));
    memset(bomb_blast_strength, 0, 11*11*sizeof(double));
    memset(bomb_movement, 0, 11*11*sizeof(double));
    memset(flame_life, 0, 11*11*sizeof(double));
    board[4 * 11 + 4] = bboard::PyAGENT0 + id;

    init_agent_frankfurt(id);
    getStep_frankfurt(id, true, true, true, true, board, bomb_life, bomb_blast_strength, bomb_movement, flame_life, 4, 4, 1, true, 0, bboard::GameType::Team, 11, -1, -1);
    PrintState(&envs[id]->GetState());
    moves_in_one_step[4]; //was: moves
    moves_in_one_step[0] = bboard::Move::IDLE;
    moves_in_one_step[1] = bboard::Move::IDLE;
    moves_in_one_step[2] = bboard::Move::IDLE;
    moves_in_one_step[3] = bboard::Move::IDLE;
    envs[id]->GetState().timeStep = 0;
    for(int i=0; i<20; i++) {
        moves_in_one_step[id] = frankfurtAgents[id]->act(&envs[id]->GetState());
        if(i == 0) {
            expected_result = 1;
            success = expected_result == (int)moves_in_one_step[id];
            tests_run++;
            if(success) successful_tests++;
            millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time).count();
            std::cout << "TEST RESULT: " << expected_result << " =?=  " << (int)moves_in_one_step[id] << " success: " << success << " time: " << millis << std::endl;
        }
        bboard::Step(&envs[id]->GetState(), moves_in_one_step);
        //std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    episode_end_frankfurt(id);

std::cout<< "TEST: Attack with kick-bomb" << std::endl;
    id = 1;
    memset(board, 0, 11*11*sizeof(uint8_t));
    memset(bomb_life, 0, 11*11*sizeof(double));
    memset(bomb_blast_strength, 0, 11*11*sizeof(double));
    memset(bomb_movement, 0, 11*11*sizeof(double));
    memset(flame_life, 0, 11*11*sizeof(double));
    board[4 * 11 + 4] = bboard::PyAGENT0 + id;
    board[4 * 11 + 8] = bboard::PyAGENT0 + id + 1;
    board[4 * 11 + 2] = bboard::PyRIGID;
    board[3 * 11 + 3] = bboard::PyRIGID;
    board[3 * 11 + 5] = bboard::PyRIGID;
    board[3 * 11 + 6] = bboard::PyRIGID;
    board[3 * 11 + 7] = bboard::PyRIGID;
    board[3 * 11 + 4] = bboard::PyRIGID;
    board[3 * 11 + 8] = bboard::PyRIGID;
    board[3 * 11 + 9] = bboard::PyRIGID;
    board[5 * 11 + 3] = bboard::PyRIGID;
    board[5 * 11 + 5] = bboard::PyRIGID;
    board[5 * 11 + 6] = bboard::PyRIGID;
    board[5 * 11 + 7] = bboard::PyRIGID;
    board[5 * 11 + 4] = bboard::PyRIGID;
    board[5 * 11 + 8] = bboard::PyRIGID;
    board[5 * 11 + 9] = bboard::PyRIGID;
    board[4 * 11 + 10] = bboard::PyRIGID;

    init_agent_frankfurt(id);
    getStep_frankfurt(id, true, true, true, true, board, bomb_life, bomb_blast_strength, bomb_movement, flame_life, 4, 4, 3, true, 1, bboard::GameType::Team, 13, -1, -1);
    PrintState(&envs[id]->GetState());
    moves_in_one_step[4]; //was: moves
    moves_in_one_step[0] = bboard::Move::IDLE;
    moves_in_one_step[1] = bboard::Move::IDLE;
    moves_in_one_step[2] = bboard::Move::IDLE;
    moves_in_one_step[3] = bboard::Move::IDLE;
    envs[id]->GetState().timeStep = 100;
    for(int i=0; i<13; i++) {
        envs[id]->GetState().timeStep++;
        PrintState(&envs[id]->GetState());
        moves_in_one_step[id] = frankfurtAgents[id]->act(&envs[id]->GetState());
        if(i == 0) {
            expected_result = 5;
            success = expected_result == (int)moves_in_one_step[id];
            tests_run++;
            if(success) successful_tests++;
            millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time).count();
            std::cout << "TEST RESULT: " << expected_result << " =?=  " << (int)moves_in_one_step[id] << " success: " << success << " time: " << millis << std::endl;
        }
        bboard::Step(&envs[id]->GetState(), moves_in_one_step);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    episode_end_frankfurt(id);


    std::cout << "\n\nFrom " << std::to_string(tests_run) << " tests " << std::to_string(successful_tests) << " were successful, that is " << std::to_string(100.0f * successful_tests / tests_run) << "%" << std::endl;
}


extern "C"
{
EXPORTIT void c_tests()
{
    tests();
}


EXPORTIT void c_init_agent_frankfurt(int id)
{
    init_agent_frankfurt(id);
}

EXPORTIT float c_episode_end_frankfurt(int id)
{
    return episode_end_frankfurt(id);
}

EXPORTIT int c_getStep_frankfurt(int id, bool agent0Alive, bool agent1Alive, bool agent2Alive, bool agent3Alive, uint8_t * board, double * bomb_life, double * bomb_blast_strength, double * bomb_moving_direction, double * flame_life, int posx, int posy, int blast_strength, bool can_kick, int ammo, int game_type, int teammate_id, int message1, int message2)
{
    return getStep_frankfurt(id, agent0Alive, agent1Alive, agent2Alive, agent3Alive, board, bomb_life, bomb_blast_strength, bomb_moving_direction, flame_life, posx, posy, blast_strength, can_kick, ammo, game_type, teammate_id, message1, message2);
}
EXPORTIT int c_getMessage_frankfurt(int id, int messagePart)
{
    return getMessage_frankfurt(id, messagePart);
}


EXPORTIT void c_init_agent_gottingen(int id)
{
    init_agent_gottingen(id);
}

EXPORTIT float c_episode_end_gottingen(int id)
{
    return episode_end_gottingen(id);
}

EXPORTIT int c_getStep_gottingen(int id, bool agent0Alive, bool agent1Alive, bool agent2Alive, bool agent3Alive, uint8_t * board, double * bomb_life, double * bomb_blast_strength, double * bomb_moving_direction, double * flame_life, int posx, int posy, int blast_strength, bool can_kick, int ammo, int game_type, int teammate_id, int message1, int message2)
{
    return getStep_gottingen(id, agent0Alive, agent1Alive, agent2Alive, agent3Alive, board, bomb_life, bomb_blast_strength, bomb_moving_direction, flame_life, posx, posy, blast_strength, can_kick, ammo, game_type, teammate_id, message1, message2);
}
EXPORTIT int c_getMessage_gottingen(int id, int messagePart)
{
    return getMessage_gottingen(id, messagePart);
}

}