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

Position OriginPosition(int x, int y, Move move)
{
    Position p;
    p.x = x;
    p.y = y;
    if(move == Move::DOWN)
    {
        p.y -= 1;
    }
    else if(move == Move::UP)
    {
        p.y += 1;
    }
    else if(move == Move::RIGHT)
    {
        p.x -= 1;
    }
    else if(move == Move::LEFT)
    {
        p.x += 1;
    }
    return p;
}

Position DesiredPosition(const Bomb b)
{
    return DesiredPosition(BMB_POS_X(b), BMB_POS_Y(b), Move(BMB_DIR(b)));
}

Position AgentBombChainReversion(State& state, Move moves[AGENT_COUNT],
                                 Position destBombs[MAX_BOMBS], int agentID)
{
    AgentInfo& agent = state.agents[agentID];
    Position origin = OriginPosition(agent.x, agent.y, moves[agentID]);

    if(!IsOutOfBounds(origin))
    {
        int indexOriginAgent = state.GetAgent(origin.x, origin.y);

        int bombDestIndex = -1;
        for(int i = 0; i < state.bombs.count; i++)
        {
            if(destBombs[i] == origin)
            {
                bombDestIndex = i;
                break;
            }
        }

        bool hasBomb  = bombDestIndex != -1;

        agent.x = origin.x;
        agent.y = origin.y;

        bool sameAgent = state[origin] == Item::AGENT0 + agentID;
        state[origin] = Item::AGENT0 + agentID;

        if(indexOriginAgent != -1)
        {
            if(sameAgent && agentID == indexOriginAgent)
            {
                std::cout << "AgentBombChainReversion - recursion" << std::endl;
                return origin;
            }
            return AgentBombChainReversion(state, moves, destBombs, indexOriginAgent);
        }
        // move bomb back and check for an agent that needs to be reverted
        else if(hasBomb)
        {
            Bomb& b = state.bombs[bombDestIndex];
            Position bombDest = destBombs[bombDestIndex];

            Position originBomb = OriginPosition(bombDest.x, bombDest.y, Move(BMB_DIR(b)));

            // this is the case when an agent gets bounced back to a bomb he laid
            if(originBomb == bombDest)
            {
                state[originBomb] = Item::AGENT0 + agentID;
                return originBomb;
            }

            int hasAgent = state.GetAgent(originBomb.x, originBomb.y);
            SetBombDirection(b, Direction::IDLE);
            SetBombPosition(b, originBomb.x, originBomb.y);
            state[originBomb] = Item::BOMB;

            if(hasAgent != -1)
            {
                return AgentBombChainReversion(state, moves, destBombs, hasAgent);
            }
            else
            {
                return originBomb;
            }
        }
        return origin;
    }
    else
    {
        return {agent.x, agent.y};
    }
}

void FillPositions(State* s, Position p[AGENT_COUNT])
{
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        p[i] = {s->agents[i].x, s->agents[i].y};
    }
}

void FillDestPos(State* s, Move m[AGENT_COUNT], Position p[AGENT_COUNT])
{
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        p[i] = DesiredPosition(s->agents[i].x, s->agents[i].y, m[i]);
    }
}

void FillBombDestPos(State* s, Position p[MAX_BOMBS])
{
    for(int i = 0; i < s->bombs.count; i++)
    {
        p[i] = DesiredPosition(s->bombs[i]);
    }
}

bool FixSwitchMove(State* s, Position d[AGENT_COUNT])
{
    bool any_switch = false;
    //If they want to step each other's place, nobody goes anywhere
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        if(s->agents[i].dead || s->agents[i].x < 0)
        {
            continue;
        }
        for(int j = i + 1; j < AGENT_COUNT; j++)
        {

            if(s->agents[j].dead || s->agents[j].x < 0)
            {
                continue;
            }
            if(d[i].x == s->agents[j].x && d[i].y == s->agents[j].y &&
                    d[j].x == s->agents[i].x && d[j].y == s->agents[i].y)
            {
                any_switch = true;
                d[i].x = s->agents[i].x;
                d[i].y = s->agents[i].y;
                d[j].x = s->agents[j].x;
                d[j].y = s->agents[j].y;
            }
        }
    }
    return any_switch;
}

void MoveBombs(State* state, Position d[AGENT_COUNT])
{
    for(int bombIndex=0; bombIndex <state->bombs.count; bombIndex++) {
        if (BMB_DIR(state->bombs[bombIndex]) > 0) {
            Position desiredPos = bboard::util::DesiredPosition(BMB_POS_X(state->bombs[bombIndex]), BMB_POS_Y(state->bombs[bombIndex]),
                                                                (bboard::Move) BMB_DIR(state->bombs[bombIndex]));
            if (bboard::_CheckPos_any(state, desiredPos.x, desiredPos.y)) {
                bool agentWantsToMoveThere = false;
                for (int i = 0; i < AGENT_COUNT; i++) {
                    if(!state->agents[i].dead && state->agents[i].x >=0 && d[i].x == desiredPos.x && d[i].y == desiredPos.y) {
                        //Agent stays
                        d[i].x = state->agents[i].x;
                        d[i].y = state->agents[i].y;
                        SetBombDirection(state->bombs[bombIndex], Direction::IDLE);
                        break;
                    }
                }
                if (!agentWantsToMoveThere) {
                    bool explodes = IS_FLAME(state->board[desiredPos.y][desiredPos.x]);
                    state->board[BMB_POS_Y(state->bombs[bombIndex])][BMB_POS_X(state->bombs[bombIndex])] = PASSAGE;
                    SetBombPosition(state->bombs[bombIndex], desiredPos.x, desiredPos.y);
                    state->board[desiredPos.y][desiredPos.x] = BOMB;
                    if (explodes) {
                        state->ExplodeBombAt(bombIndex);
                        bombIndex--;
                    }
                }
            } else {
                SetBombDirection(state->bombs[bombIndex], Direction::IDLE);
            }
        }
    }
}

int ResolveDependencies(State* s, Position des[AGENT_COUNT],
                        int dependency[AGENT_COUNT], int chain[AGENT_COUNT])
{
    int rootCount = 0;
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        // dead agents are handled as roots
        // also invisible agents
        if(s->agents[i].dead || s->agents[i].x < 0)
        {
            chain[rootCount] = i;
            rootCount++;
            continue;
        }

        bool isChainRoot = true;
        for(int j = 0; j < AGENT_COUNT; j++)
        {
            if(i == j || s->agents[j].dead || s->agents[j].x < 0) continue;

            if(des[i].x == s->agents[j].x && des[i].y == s->agents[j].y)
            {
                if(dependency[j] == -1) {
                    dependency[j] = i;
                }else{
                    dependency[dependency[j]] = i;
                }
                isChainRoot = false;
                break;
            }
        }
        if(isChainRoot)
        {
            chain[rootCount] = i;
            rootCount++;
        }
    }
    return rootCount;
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
    for(int i = 0; i < state.bombs.count; i++)
    {
        if(BMB_TIME(state.bombs[0]) == 0)
        {
            __glibcxx_assert(i == 0);
            state.ExplodeTopBomb();
            i--;
        }else{
            break;
        }
    }
}


    void TickAndMoveBombs10(State& state){
        //explode timed-out bombs
        int stepSize = 1;
        for(int remainingTime = BOMB_LIFETIME; remainingTime>0; remainingTime-=stepSize) {
            __glibcxx_assert(stepSize > 0);
            bool anyBodyMoves = false;
            int minRemaining = 100;
            state.relTimeStep += stepSize;
            for (int i = 0; i < state.bombs.count; i++) {
                for(int t = 0; t<stepSize; t++) //TODO: shift in one turn?
                    ReduceBombTimer(state.bombs[i]);

                if (BMB_TIME(state.bombs[i]) == 0) {
                    __glibcxx_assert(i == 0);
                    state.ExplodeTopBomb();
                    i--;
                } else {
                    minRemaining = std::min(minRemaining, BMB_TIME(state.bombs[i]));
                    if (BMB_DIR(state.bombs[i]) > 0) {
                        Position desiredPos = bboard::util::DesiredPosition(BMB_POS_X(state.bombs[i]), BMB_POS_Y(state.bombs[i]),
                                                                            (bboard::Move) BMB_DIR(state.bombs[i]));
                        if (_CheckPos_any(&state, desiredPos.x, desiredPos.y)) {
                            //TODO: Add if(explodes)... from other locations function is used
                            state.board[BMB_POS_Y(state.bombs[i])][BMB_POS_X(state.bombs[i])] = PASSAGE;
                            SetBombPosition(state.bombs[i], desiredPos.x, desiredPos.y);
                            state.board[desiredPos.y][desiredPos.x] = BOMB;
                            anyBodyMoves = true;
                        } else {
                            SetBombDirection(state.bombs[i], Direction::IDLE);
                        }
                    }
                }
            }

            stepSize = (anyBodyMoves ? 1 : minRemaining);
            //Exit if match decided, maybe we would die later from an other bomb, so that disturbs pointing and decision making
            if (state.aliveAgents < 2 || (state.aliveAgents == 2 && (state.agents[0].dead == state.agents[2].dead)))
                break;
        }
    }

void ConsumePowerup(State& state, int agentID, int powerUp)
{
    if(powerUp == Item::EXTRABOMB)
    {
        state.agents[agentID].maxBombCount++;
        state.agents[agentID].extraBombPowerupPoints += 1.0 - state.relTimeStep/100.0f;
    }
    else if(powerUp == Item::INCRRANGE)
    {
        state.agents[agentID].bombStrength++;
        state.agents[agentID].extraRangePowerupPoints += 1.0 - state.relTimeStep/100.0f;
    }
    else if(powerUp == Item::KICK)
    {
        if(state.agents[agentID].canKick)
        {
            state.agents[agentID].otherKickPowerupPoints += 1.0 - state.relTimeStep/100.0f;
        }else{
            state.agents[agentID].firstKickPowerupPoints += 1.0 - state.relTimeStep/100.0f;
        }
        state.agents[agentID].canKick = true;
    }

}

bool HasDPCollision(const State& state, Position dp[AGENT_COUNT], int agentID)
{
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        if(agentID == i || state.agents[i].dead || state.agents[i].x < 0) continue;
        if(dp[agentID] == dp[i])
        {
            // a destination position conflict will never
            // result in a valid move
            return true;
        }
    }
    return false;
}

bool HasBombCollision(const State& state, const Bomb& b, int index)
{
    Position bmbTarget = util::DesiredPosition(b);

    for(int i = index; i < state.bombs.count; i++)
    {
        Position target = util::DesiredPosition(state.bombs[i]);

        if(b != state.bombs[i] && target == bmbTarget)
        {
            return true;
        }
    }
    return false;
}

void ResolveBombCollision(State& state, Move moves[AGENT_COUNT],
                          Position destBombs[MAX_BOMBS], int index)
{
    Bomb& b = state.bombs[index];
    Bomb collidees[4]; //more than 4 bombs cannot collide
    Position bmbTarget = util::DesiredPosition(b);
    bool hasCollided = false;

    for(int i = index; i < state.bombs.count; i++)
    {
        Position target = util::DesiredPosition(state.bombs[i]);

        if(b != state.bombs[i] && target == bmbTarget)
        {
            SetBombDirection(state.bombs[i], Direction::IDLE);
            hasCollided = true;
        }
    }
    if(hasCollided)
    {
        if(Direction(BMB_DIR(b)) != Direction::IDLE)
        {
            SetBombDirection(b, Direction::IDLE);
            int index = state.GetAgent(BMB_POS_X(b), BMB_POS_Y(b));
            // move != idle means the agent moved on it this turn
            if(index > -1 && moves[index] != Move::IDLE && moves[index] != Move::BOMB)
            {
                Position origin = AgentBombChainReversion(state, moves, destBombs, index);
                state.board[BMB_POS_Y(b)][BMB_POS_X(b)] = Item::BOMB;
            }

        }
    }

}

void ResetBombFlags(State& state)
{
    for(int i = 0; i < state.bombs.count; i++)
    {
        SetBombMovedFlag(state.bombs[i], false);
    }
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
