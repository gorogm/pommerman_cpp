#include <thread>
#include <chrono>
#include <functional>

#include "bboard.hpp"

namespace bboard
{

void Pause(bool timeBased)
{
    if(!timeBased)
    {
        std::cin.get();
    }
    else
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void PrintGameResult(Environment& env)
{
    std::cout << std::endl;

    if(env.IsDone())
    {
        if(env.IsDraw())
        {
            std::cout << "Draw! All agents are dead"
                      << std::endl;
        }
        else
        {
            std::cout << "Finished! The winner is Agent "
                      << env.GetWinner() << std::endl;
        }

    }
    else
    {
        std::cout << "Draw! Max timesteps reached "
                  << std::endl;
    }
}

Environment::Environment()
{
    state = std::make_unique<State>();
}

void Environment::MakeGame(std::array<Agent*, AGENT_COUNT> a)
{
    bboard::InitState(state.get(), 0, 1, 2, 3);

    state->PutAgentsInCorners(0, 1, 2, 3);

    SetAgents(a);
    hasStarted = true;
}

void Environment::MakeGameFromPython(int ourId)
{
    state->ourId = ourId;
    for(int x=0; x<11; x++)
        for(int y=0; y<11; y++)
            state->board[y][x] = FOG;
}

    void Environment::MakeGameFromPython_berlin(bool agent1Alive, bool agent2Alive, bool agent3Alive, uint8_t * board, double * bomb_life,
                                                double * bomb_blast_strength, int posx, int posy, int blast_strength, bool can_kick, int ammo, int teammate_id)
    {
        state->bombs.count = 0;
        //state->flames.count = 0; //keep!
        state->agents[0].x = -1;
        state->agents[0].y = -1;
        state->agents[1].x = -1;
        state->agents[1].y = -1;
        state->agents[2].x = -1;
        state->agents[2].y = -1;
        state->agents[3].x = -1;
        state->agents[3].y = -1;

        state->agents[0].collectedPowerupPoints = 0;
        state->agents[1].collectedPowerupPoints = 0;
        state->agents[2].collectedPowerupPoints = 0;
        state->agents[3].collectedPowerupPoints = 0;
        state->timeStep++;
        state->teammateId = teammate_id - 10;

        if(state->ourId == 1 || state->ourId == 3)
        {
            state->enemy1Id = 0;
            state->enemy2Id = 2;
        }else
        {
            state->enemy1Id = 1;
            state->enemy2Id = 3;
        }

        //std::cout << state->ourId << " " << state->teammateId << " : " << state->enemy1Id << " " << state->enemy2Id << std::endl;

        for(int i=0; i<11*11; i++)
        {
            int y = i/11;
            int x = i%11;
            switch(board[i])
            {
                case PyPASSAGE:
                    state->board[y][x] = PASSAGE;
                    break;
                case PyRIGID:
                    state->board[y][x] = RIGID;
                    break;
                case PyWOOD:
                    state->board[y][x] = WOOD;
                    break;
                case PyBOMB:
                    state->board[y][x] = BOMB;
                    {
                        int bombId = state->ourId;//TODO: should be the agent who placed it
                        Bomb* b = &state->bombs.NextPos();
                        SetBombID(*b, bombId);
                        SetBombPosition(*b, x, y);
                        SetBombStrength(*b, bomb_blast_strength[i] - 1);
                        // TODO: velocity
                        SetBombTime(*b, bomb_life[i]);
                        state->agents[bombId].bombCount++;
                        state->bombs.count++;
                    }

                    break;
                case PyFLAMES:
                    state->board[y][x] = FLAMES;
                    break;
                case PyFOG:
                    state->board[y][x] = FOG;
                    break;
                case PyEXTRABOMB:
                    state->board[y][x] = EXTRABOMB;
                    break;
                case PyINCRRANGE:
                    state->board[y][x] = INCRRANGE;
                    break;
                case PyKICK:
                    state->board[y][x] = KICK;
                    break;
                case PyAGENTDUMMY:
                    state->board[y][x] = AGENTDUMMY;
                    break;
                case PyAGENT0:
                case PyAGENT1:
                case PyAGENT2:
                case PyAGENT3:
                    state->board[y][x] = AGENT0 + (board[i] - PyAGENT0);
                    state->agents[board[i] - PyAGENT0].x = x;
                    state->agents[board[i] - PyAGENT0].y = y;
                    if(bomb_blast_strength[i] > 0) //hidden bomb under us
                    {
                        int bombId = state->ourId;//TODO: should be the agent who placed it
                        Bomb* b = &state->bombs.NextPos();
                        SetBombID(*b, bombId);
                        SetBombPosition(*b, x, y);
                        SetBombStrength(*b, bomb_blast_strength[i] - 1);
                        // TODO: velocity
                        SetBombTime(*b, bomb_life[i]);
                        state->agents[bombId].bombCount++;
                        state->bombs.count++;
                        std::cout << "Bomb under agent " << board[i] - PyAGENT0 << " at " << x << " " << y << " time: " << bomb_life[i] << std::endl;
                    }
                    break;
                default:
                    std::cout << "Unknown item in py board: " << board[i] << std::endl;
            }
        }

        state->aliveAgents = 1 + int(agent1Alive) + int(agent2Alive) + int(agent3Alive);
        state->agents[state->ourId].x = posy;
        state->agents[state->ourId].y = posx;
        state->agents[state->ourId].canKick = can_kick;
        state->agents[state->ourId].bombCount = state->agents[state->ourId].maxBombCount - ammo;
        state->agents[state->ourId].bombStrength = blast_strength - 1;

        int i, j;
        for (i = 0; i < state->bombs.count-1; i++)
            for (j = 0; j < state->bombs.count-1-i; j++)
                if (BMB_TIME(state->bombs[j]) > BMB_TIME(state->bombs[j+1]))
                    std::swap(state->bombs[j], state->bombs[j+1]);
    }

    void Environment::MakeGameFromPython_cologne(bool agent1Alive, bool agent2Alive, bool agent3Alive, uint8_t * board, double * bomb_life,
                                                double * bomb_blast_strength, int posx, int posy, int blast_strength, bool can_kick, int ammo, int teammate_id)
    {
        state->bombs.count = 0;
        state->woods.count = 0;
        state->powerup_incr.count = 0;
        state->powerup_kick.count = 0;
        state->powerup_extrabomb.count = 0;
        //state->flames.count = 0; //keep!
        state->agents[0].x = -1;
        state->agents[0].y = -1;
        state->agents[1].x = -1;
        state->agents[1].y = -1;
        state->agents[2].x = -1;
        state->agents[2].y = -1;
        state->agents[3].x = -1;
        state->agents[3].y = -1;

        state->agents[0].collectedPowerupPoints = 0;
        state->agents[1].collectedPowerupPoints = 0;
        state->agents[2].collectedPowerupPoints = 0;
        state->agents[3].collectedPowerupPoints = 0;
        state->timeStep++;
        state->teammateId = teammate_id - 10;

        if(state->ourId == 1 || state->ourId == 3)
        {
            state->enemy1Id = 0;
            state->enemy2Id = 2;
        }else
        {
            state->enemy1Id = 1;
            state->enemy2Id = 3;
        }

        //std::cout << state->ourId << " " << state->teammateId << " : " << state->enemy1Id << " " << state->enemy2Id << std::endl;

        for(int i=0; i<11*11; i++)
        {
            int y = i/11;
            int x = i%11;
            switch(board[i])
            {
                case PyPASSAGE:
                    state->board[y][x] = PASSAGE;
                    break;
                case PyRIGID:
                    state->board[y][x] = RIGID;
                    break;
                case PyWOOD:
                    state->board[y][x] = WOOD;
                    {Position p; p.x = x; p.y = y; state->woods.NextPos() = p;}
                    break;
                case PyBOMB:
                    state->board[y][x] = BOMB;
                    {
                        int bombId = state->ourId;//TODO: should be the agent who placed it
                        Bomb* b = &state->bombs.NextPos();
                        SetBombID(*b, bombId);
                        SetBombPosition(*b, x, y);
                        SetBombStrength(*b, bomb_blast_strength[i] - 1);
                        // TODO: velocity
                        SetBombTime(*b, bomb_life[i]);
                        state->agents[bombId].bombCount++;
                        state->bombs.count++;
                    }

                    break;
                case PyFLAMES:
                    state->board[y][x] = FLAMES;
                    break;
                case PyFOG:
                    /*if(state->board[y][x] == RIGID)
                        state->board[y][x] = RIGID;
                    else
                        state->board[y][x] = FOG;*/
                    break;
                case PyEXTRABOMB:
                    state->board[y][x] = EXTRABOMB;
                    {Position p; p.x = x; p.y = y; state->powerup_extrabomb.NextPos() = p;}
                    break;
                case PyINCRRANGE:
                    state->board[y][x] = INCRRANGE;
                    {Position p; p.x = x; p.y = y; state->powerup_incr.NextPos() = p;}
                    break;
                case PyKICK:
                    state->board[y][x] = KICK;
                    {Position p; p.x = x; p.y = y; state->powerup_kick.NextPos() = p;}
                    break;
                case PyAGENTDUMMY:
                    state->board[y][x] = AGENTDUMMY;
                    break;
                case PyAGENT0:
                case PyAGENT1:
                case PyAGENT2:
                case PyAGENT3:
                    state->board[y][x] = AGENT0 + (board[i] - PyAGENT0);
                    state->agents[board[i] - PyAGENT0].x = x;
                    state->agents[board[i] - PyAGENT0].y = y;
                    if(bomb_blast_strength[i] > 0) //hidden bomb under us
                    {
                        int bombId = state->ourId;//TODO: should be the agent who placed it
                        Bomb* b = &state->bombs.NextPos();
                        SetBombID(*b, bombId);
                        SetBombPosition(*b, x, y);
                        SetBombStrength(*b, bomb_blast_strength[i] - 1);
                        // TODO: velocity
                        SetBombTime(*b, bomb_life[i]);
                        state->agents[bombId].bombCount++;
                        state->bombs.count++;
                        std::cout << "Bomb under agent " << board[i] - PyAGENT0 << " at " << x << " " << y << " time: " << bomb_life[i] << std::endl;
                    }
                    break;
                default:
                    std::cout << "Unknown item in py board: " << board[i] << std::endl;
            }
        }

        state->aliveAgents = 1 + int(agent1Alive) + int(agent2Alive) + int(agent3Alive);
        state->agents[state->ourId].x = posy;
        state->agents[state->ourId].y = posx;
        state->agents[state->ourId].canKick = can_kick;
        state->agents[state->ourId].bombCount = state->agents[state->ourId].maxBombCount - ammo;
        state->agents[state->ourId].bombStrength = blast_strength - 1;

        int i, j;
        for (i = 0; i < state->bombs.count-1; i++)
            for (j = 0; j < state->bombs.count-1-i; j++)
                if (BMB_TIME(state->bombs[j]) > BMB_TIME(state->bombs[j+1]))
                    std::swap(state->bombs[j], state->bombs[j+1]);
    }

void Environment::StartGame(int timeSteps, bool render, bool stepByStep)
{
    state->timeStep = 0;
    while(!this->IsDone() && state->timeStep < timeSteps)
    {

        if(render)
        {
            Print();

            if(listener)
                listener(*this);

            if(stepByStep)
                Pause(false);
        }
        this->Step(true);
    }
    Print();
    PrintGameResult(*this);
}

void ProxyAct(Move& writeBack, Agent& agent, State& state)
{
    writeBack = agent.act(&state);
}

void CollectMovesAsync(Move m[AGENT_COUNT], Environment& e)
{
    std::thread threads[AGENT_COUNT];
    for(uint i = 0; i < AGENT_COUNT; i++)
    {
        if(!e.GetState().agents[i].dead)
        {
            threads[i] = std::thread(ProxyAct,
                                     std::ref(m[i]),
                                     std::ref(*e.GetAgent(i)),
                                     std::ref(e.GetState()));
        }
    }
    Pause(true); //competitive pause
    for(uint i = 0; i < AGENT_COUNT; i++)
    {
        if(!e.GetState().agents[i].dead)
        {
            threads[i].join();
        }
    }
}

void Environment::Step(bool competitiveTimeLimit)
{
    if(!hasStarted || finished)
    {
        return;
    }

    Move m[AGENT_COUNT];


    if(competitiveTimeLimit)
    {
        CollectMovesAsync(m, *this);
    }
    else
    {
        for(uint i = 0; i < AGENT_COUNT; i++)
        {
            if(!state->agents[i].dead)
            {
                m[i] = agents[i]->act(state.get());
            }
        }
    }


    bboard::Step(state.get(), m);
    state->timeStep++;

    if(state->aliveAgents == 1)
    {
        finished = true;
        for(int i = 0; i < AGENT_COUNT; i++)
        {
            if(!state->agents[i].dead)
            {
                agentWon = i;
                // teamwon = team of agent
            }
        }
    }
    if(state->aliveAgents == 0)
    {
        finished = true;
        isDraw = true;
    }
}

void Environment::Print(bool clear)
{
    if(clear)
        std::cout << "\033c"; // clear console on linux
    PrintState(state.get());
}

State& Environment::GetState() const
{
    return *state.get();
}

Agent* Environment::GetAgent(uint agentID) const
{
    return agents[agentID];
}

void Environment::SetAgents(std::array<Agent*, AGENT_COUNT> agents)
{
    for(uint i = 0; i < AGENT_COUNT; i++)
    {
        agents[i]->id = int(i);
    }
    this->agents = agents;
}

bool Environment::IsDone()
{
    return finished;
}

bool Environment::IsDraw()
{
    return isDraw;
}

int Environment::GetWinner()
{
    return agentWon;
}

void Environment::SetStepListener(const std::function<void(const Environment&)>& f)
{
    listener = f;
}

}
