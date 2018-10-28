#include "bboard.hpp"
#include "agents.hpp"
#include "strategy.hpp"
#include "step_utility.hpp"

using namespace bboard;
using namespace bboard::strategy;

namespace agents
{
CologneAgent::CologneAgent()
{
    std::random_device rd;  // non explicit seed
    rng = std::mt19937_64(rd());
    intDist = std::uniform_int_distribution<int>(0, 4); // no bombs
}

bool CologneAgent::_CheckPos2(const State* state, bboard::Position pos)
{
    return !util::IsOutOfBounds(pos) && IS_WALKABLE_OR_AGENT(state->board[pos.y][pos.x]);
}

float CologneAgent::laterBetter(float reward, int timestaps)
{
    if(reward == 0.0f)
        return reward;

    if(reward > 0)
        return reward * (1.0f / (float)std::pow(0.98f, timestaps));
    else
        return reward * (float)std::pow(0.98f, timestaps);
}

float CologneAgent::soonerBetter(float reward, int timestaps)
{
    if(reward == 0.0f)
        return reward;

    if(reward < 0)
        return reward * (1.0f / (float)std::pow(0.98f, timestaps));
    else
        return reward * (float)std::pow(0.98f, timestaps);
}

float CologneAgent::scoreState(State * state) {
    float teamBalance = (state->ourId < 2 ? 1.01f : 0.99f);
    float point = laterBetter(-10 * state->agents[state->ourId].dead, state->agents[state->ourId].diedAt - state->timeStep);
    point += laterBetter(-10 * state->agents[state->teammateId].dead, state->agents[state->teammateId].diedAt - state->timeStep);
    point += 3 * soonerBetter(state->agents[state->enemy1Id].dead, state->agents[state->enemy1Id].diedAt - state->timeStep);
    point += 3 * soonerBetter(state->agents[state->enemy2Id].dead, state->agents[state->enemy2Id].diedAt - state->timeStep);
    point += 0.3f * state->woodDemolished;
    point += 0.5f * state->agents[state->ourId].collectedPowerupPoints * teamBalance;
    point += 0.5f * state->agents[state->teammateId].collectedPowerupPoints / teamBalance;

    if(state->agents[state->enemy1Id].x >= 0)
        point -= (std::abs(state->agents[state->enemy1Id].x - state->agents[state->ourId].x) + std::abs(state->agents[state->enemy1Id].y - state->agents[state->ourId].y)) / 100.0f;
    if(state->agents[state->enemy2Id].x >= 0)
        point -= (std::abs(state->agents[state->enemy2Id].x - state->agents[state->ourId].x) + std::abs(state->agents[state->enemy2Id].y - state->agents[state->ourId].y)) / 100.0f;

    for(int i=0; i < state->powerup_kick.count; i++)
        point -= (std::abs(state->powerup_kick[i].x - state->agents[state->ourId].x) + std::abs(state->powerup_kick[i].y - state->agents[state->ourId].y)) / 1000.0f;
    for(int i=0; i < state->powerup_incr.count; i++)
        point -= (std::abs(state->powerup_incr[i].x - state->agents[state->ourId].x) + std::abs(state->powerup_incr[i].y - state->agents[state->ourId].y)) / 1000.0f;
    for(int i=0; i < state->powerup_extrabomb.count; i++)
        point -= (std::abs(state->powerup_extrabomb[i].x - state->agents[state->ourId].x) + std::abs(state->powerup_extrabomb[i].y - state->agents[state->ourId].y)) / 1000.0f;
    for(int i=0; i < state->woods.count; i++)
        point -= (std::abs(state->woods[i].x - state->agents[state->ourId].x) + std::abs(state->woods[i].y - state->agents[state->ourId].y)) / 1000.0f;

    if(moves_in_chain[0] != 0) point += 0.001f; //not IDLE
    if(lastMoveWasBlocked && ((state->timeStep + state->ourId) % 4) == 0 && moves_in_chain[0] == lastBlockedMove)
        point -= 0.1f;

    return point;
}

float CologneAgent::runAlreadyPlantedBombs(State * state)
{
    for(int i=0; i<10; i++)
    {
        //Exit if match decided, maybe we would die later from an other bomb, so that disturbs pointing and decision making
        if(state->aliveAgents < 2 || (state->aliveAgents == 2 && ((state->agents[0].dead && state->agents[2].dead) || (state->agents[1].dead && state->agents[3].dead))))
            break;
        //bboard::Step(newstate4, moves);
        util::TickBombs(*state);
        state->relTimeStep++;
        //simulatedSteps++;
    }

    float point = scoreState(state);
    /*if(point > bestPoint)
    {
        bestPoint = point;
        best_moves_in_chain = moves_in_chain;
    }*/

    return point;
}

float CologneAgent::runOneStep(const bboard::State * state, int depth)
{
    bboard::Move moves_in_one_step[4]; //was: moves
    moves_in_one_step[0] = bboard::Move::IDLE;
    moves_in_one_step[1] = bboard::Move::IDLE;
    moves_in_one_step[2] = bboard::Move::IDLE;
    moves_in_one_step[3] = bboard::Move::IDLE;

    const AgentInfo& a = state->agents[state->ourId];
    float maxPoint = -100;
    FixedQueue<int, 6> bestmoves;
    for(int move=0; move<6; move++)
    {
        Position desiredPos = bboard::util::DesiredPosition(a.x, a.y, (bboard::Move)move);
        // if we don't have bomb
        if(move == (int)bboard::Move::BOMB && a.maxBombCount - a.bombCount <= 0)
            continue;
        // if bomb is already under us
        if(move == (int)bboard::Move::BOMB && state->HasBomb(a.x, a.y))
            continue;
        // if move is impossible
        if(move>0 && move<5 && !_CheckPos2(state, desiredPos))
            continue;
        //no two opposite steps please!
        if(depth > 0 && move>0 && move<5 && moves_in_chain[depth-1] > 0 && moves_in_chain[depth-1] < 5 && std::abs(moves_in_chain[depth-1]-move) == 2)
            continue;

        moves_in_one_step[state->ourId] = (bboard::Move) move;
        moves_in_chain.AddElem(move);

        float maxTeammate = -100;
        for(int moveT=5; moveT >= 0; moveT--) {
            if (moveT > 0) {
                if (depth >= teammateIteration || (state->agents[state->teammateId].dead || state->agents[state->teammateId].x < 0)) continue;
                // if move is impossible
                if (moveT > 0 && moveT < 5 && !_CheckPos2(state, bboard::util::DesiredPosition(state->agents[state->teammateId].x, state->agents[state->teammateId].y, (bboard::Move) moveT)))
                    continue;
                if (moveT == (int) bboard::Move::BOMB && state->agents[state->teammateId].maxBombCount - state->agents[state->teammateId].bombCount <= 0)
                    continue;
                // if bomb is already under it
                if(moveT == (int)bboard::Move::BOMB && state->HasBomb(state->agents[state->teammateId].x, state->agents[state->teammateId].y))
                    continue;
            }else{
                //We'll have same results with IDLE, IDLE
                if(maxTeammate > -100 && state->agents[state->teammateId].x == desiredPos.x && state->agents[state->teammateId].y == desiredPos.y)
                    continue;
            }

            moves_in_one_step[state->teammateId] = (bboard::Move) moveT;

            float minPointE1 = 100;
            for (int moveE1 = 5; moveE1 >= 0; moveE1--) {
                if (moveE1 > 0) {
                    if (depth >= enemyIteration1 || (state->agents[state->enemy1Id].dead || state->agents[state->enemy1Id].x < 0)) continue;
                    // if move is impossible
                    if (moveE1 > 0 && moveE1 < 5 && !_CheckPos2(state, bboard::util::DesiredPosition(state->agents[state->enemy1Id].x, state->agents[state->enemy1Id].y, (bboard::Move) moveE1)))
                        continue;
                    if (moveE1 == (int) bboard::Move::BOMB &&
                        state->agents[state->enemy1Id].maxBombCount - state->agents[state->enemy1Id].bombCount <= 0)
                        continue;
                    // if bomb is already under it
                    if(moveE1 == (int)bboard::Move::BOMB && state->HasBomb(state->agents[state->enemy1Id].x, state->agents[state->enemy1Id].y))
                        continue;
                }else{
                    //We'll have same results with IDLE, IDLE
                    if(minPointE1 < 100 && state->agents[state->enemy1Id].x == desiredPos.x && state->agents[state->enemy1Id].y == desiredPos.y)
                        continue;
                }

                moves_in_one_step[state->enemy1Id] = (bboard::Move) moveE1;

                float minPointE2 = 100;
                for (int moveE2 = 5; moveE2 >= 0; moveE2--) {
                    if (moveE2 > 0) {
                        if (depth >= enemyIteration2 || (state->agents[state->enemy2Id].dead || state->agents[state->enemy2Id].x < 0)) continue;
                        // if move is impossible
                        if (moveE2 > 0 && moveE2 < 5 && !_CheckPos2(state, bboard::util::DesiredPosition(state->agents[state->enemy2Id].x, state->agents[state->enemy2Id].y, (bboard::Move) moveE2)))
                            continue;
                        if (moveE2 == (int) bboard::Move::BOMB &&
                            state->agents[state->enemy2Id].maxBombCount - state->agents[state->enemy2Id].bombCount <= 0)
                            continue;
                        // if bomb is already under it
                        if(moveE2 == (int)bboard::Move::BOMB && state->HasBomb(state->agents[state->enemy2Id].x, state->agents[state->enemy2Id].y))
                            continue;
                    }else{
                        //We'll have same results with IDLE, IDLE
                        if(minPointE2 < 100 && state->agents[state->enemy2Id].x == desiredPos.x && state->agents[state->enemy2Id].y == desiredPos.y)
                          continue;
                    }

                    moves_in_one_step[state->enemy2Id] = (bboard::Move) moveE2;

                    bboard::State *newstate = new bboard::State(*state);
                    newstate->relTimeStep++;

                    bboard::Step(newstate, moves_in_one_step);
                    simulatedSteps++;

                    float point;
                    if (depth + 1 < myMaxDepth)
                        point = runOneStep(newstate, depth + 1);
                    else
                        point = runAlreadyPlantedBombs(newstate);

                    if (point < minPointE2) { minPointE2 = point; }

                    delete newstate;
                }
                if (minPointE2 < minPointE1) { minPointE1 = minPointE2; }
            }
            if (minPointE1 > maxTeammate) { maxTeammate = minPointE1;}
        }

        moves_in_chain.RemoveAt(moves_in_chain.count - 1);
        best_moves_in_chain.count = std::max(depth+1, best_moves_in_chain.count);
        if (maxTeammate == maxPoint) { bestmoves[bestmoves.count] = move; bestmoves.count++;}
        if (maxTeammate > maxPoint) { maxPoint = maxTeammate; bestmoves.count = 1; bestmoves[0] = move;}
    }

    if(bestmoves.count > 0) {
        best_moves_in_chain[depth] = bestmoves[(state->timeStep / 4) % bestmoves.count];
    }else
        best_moves_in_chain[depth] = 0;

    return maxPoint;
}

Move CologneAgent::act(const State* state)
{
    simulatedSteps = 0;
    bestPoint = -100.0f;
    enemyIteration1 = 0; enemyIteration2 = 0; teammateIteration = 0; seenAgents = 0;
    best_moves_in_chain.count = 0;
    if(!state->agents[state->teammateId].dead && state->agents[state->teammateId].x >= 0)
    {
        seenAgents++;
        int dist = std::abs(state->agents[state->ourId].x - state->agents[state->teammateId].x) + std::abs(state->agents[state->ourId].y - state->agents[state->teammateId].y);
        if(dist < 3) teammateIteration++;
        if(dist < 5) teammateIteration++;
    }
    if(!state->agents[state->enemy1Id].dead && state->agents[state->enemy1Id].x >= 0)
    {
        seenAgents++;
        int dist = std::abs(state->agents[state->ourId].x - state->agents[state->enemy1Id].x) + std::abs(state->agents[state->ourId].y - state->agents[state->enemy1Id].y);
        if(dist < 2) enemyIteration1++;
        if(dist < 5) enemyIteration1++;
    }
    if(!state->agents[state->enemy2Id].dead && state->agents[state->enemy2Id].x >= 0)
    {
        seenAgents++;
        int dist = std::abs(state->agents[state->ourId].x - state->agents[state->enemy2Id].x) + std::abs(state->agents[state->ourId].y - state->agents[state->enemy2Id].y);
        if(dist < 2) enemyIteration2++;
        if(dist < 5) enemyIteration2++;
    }
    myMaxDepth = 6 - seenAgents;

    for(int i=0; i<4; i++)
    {
        if(previousPositions[i].count == 12)
            previousPositions[i].RemoveAt(0);
        Position p;
        p.x = state->agents[i].x;
        p.y = state->agents[i].y;
        previousPositions[i][previousPositions[i].count] = p;
    }

    const AgentInfo& a = state->agents[state->ourId];
    if(state->timeStep > 1 && (expectedPosInNewTurn.x != a.x || expectedPosInNewTurn.y != a.y))
    {
        std::cout << "Couldn't move to " << expectedPosInNewTurn.y << ":" << expectedPosInNewTurn.x;
        if(std::abs(state->agents[state->teammateId].x - expectedPosInNewTurn.x) + std::abs(state->agents[state->teammateId].y - expectedPosInNewTurn.y) == 1)
            std::cout << " - Racing with teammate, probably";
        if(std::abs(state->agents[state->enemy1Id].x - expectedPosInNewTurn.x) + std::abs(state->agents[state->enemy1Id].y - expectedPosInNewTurn.y) == 1)
            std::cout << " - Racing with enemy1, probably";
        if(std::abs(state->agents[state->enemy2Id].x - expectedPosInNewTurn.x) + std::abs(state->agents[state->enemy2Id].y - expectedPosInNewTurn.y) == 1)
            std::cout << " - Racing with enemy2, probably";
        std::cout << std::endl;
        lastMoveWasBlocked = true;
        lastBlockedMove = best_moves_in_chain[0];
    }else{
        lastMoveWasBlocked = false;
    }

    float point = runOneStep(state, 0);

    std::cout << "turn#" << state->timeStep << " ourId:" << state->ourId << " point: " << point << " selected: ";
    for(int i=0; i<best_moves_in_chain.count; i++)
        std::cout << (int)best_moves_in_chain[i] << " > ";
    std::cout << " simulated steps: " << simulatedSteps;
    std::cout << ", depth " << myMaxDepth << " " << teammateIteration << " " << enemyIteration1 << " " << enemyIteration2 << std::endl;

    totalSimulatedSteps += simulatedSteps;
    turns++;
    expectedPosInNewTurn = bboard::util::DesiredPosition(a.x, a.y, (bboard::Move)best_moves_in_chain[0]);
    return (bboard::Move)best_moves_in_chain[0];
}

void CologneAgent::PrintDetailedInfo()
{
}

}
