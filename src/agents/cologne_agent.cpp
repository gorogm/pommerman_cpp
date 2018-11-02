#include "bboard.hpp"
#include "agents.hpp"
#include "strategy.hpp"
#include "step_utility.hpp"
#include <list>
#include <cstring>

using namespace bboard;
using namespace bboard::strategy;

namespace agents {
    CologneAgent::CologneAgent() {
        std::random_device rd;  // non explicit seed
        rng = std::mt19937_64(rd());
        intDist = std::uniform_int_distribution<int>(0, 4); // no bombs
    }

    bool CologneAgent::_CheckPos2(const State *state, bboard::Position pos) {
        return !util::IsOutOfBounds(pos) && IS_WALKABLE_OR_AGENT(state->board[pos.y][pos.x]);
    }

    bool CologneAgent::_CheckPos2(const State *state, int x, int y) {
        return !util::IsOutOfBounds(x, y) && IS_WALKABLE_OR_AGENT(state->board[y][x]);
    }

    bool CologneAgent::_CheckPos3(const State *state, int x, int y) {
        return !util::IsOutOfBounds(x, y) && state->board[y][x] != WOOD && state->board[y][x] != RIGID;
    }

    float CologneAgent::laterBetter(float reward, int timestaps) {
        if (reward == 0.0f)
            return reward;

        if (reward > 0)
            return reward * (1.0f / (float) std::pow(reward_sooner_later_ratio, timestaps));
        else
            return reward * (float) std::pow(reward_sooner_later_ratio, timestaps);
    }

    float CologneAgent::soonerBetter(float reward, int timestaps) {
        if (reward == 0.0f)
            return reward;

        if (reward < 0)
            return reward * (1.0f / (float) std::pow(reward_sooner_later_ratio, timestaps));
        else
            return reward * (float) std::pow(reward_sooner_later_ratio, timestaps);
    }

    float CologneAgent::scoreState(State *state) {
        float teamBalance = (ourId < 2 ? 1.01f : 0.99f);
        float point = laterBetter(-10 * state->agents[ourId].dead, state->agents[ourId].diedAt - state->timeStep);
        if (state->agents[teammateId].x >= 0)
            point += laterBetter(-10 * state->agents[teammateId].dead,
                                 state->agents[teammateId].diedAt - state->timeStep);
        point += 3 * soonerBetter(state->agents[enemy1Id].dead, state->agents[enemy1Id].diedAt - state->timeStep);
        point += 3 * soonerBetter(state->agents[enemy2Id].dead, state->agents[enemy2Id].diedAt - state->timeStep);
        point += 0.3f * state->woodDemolished;
        point += reward_collectedPowerup * state->agents[ourId].collectedPowerupPoints * teamBalance;
        point += reward_collectedPowerup * state->agents[teammateId].collectedPowerupPoints / teamBalance;

        if (state->aliveAgents == 0) {
            //point += soonerBetter(??, state->relTimeStep); //we win
        } else if (state->aliveAgents < 3) {
            if (state->agents[ourId].dead && state->agents[teammateId].dead) {
                point += laterBetter(-20.0f, state->relTimeStep); //we lost
            }
            if (state->agents[enemy1Id].dead && state->agents[enemy2Id].dead) {
                point += soonerBetter(+20.0f, state->relTimeStep); //we win
            }
        }

        if (state->agents[enemy1Id].x >= 0)
            point -= (std::abs(state->agents[enemy1Id].x - state->agents[ourId].x) +
                      std::abs(state->agents[enemy1Id].y - state->agents[ourId].y)) / reward_move_to_enemy;
        if (state->agents[enemy2Id].x >= 0)
            point -= (std::abs(state->agents[enemy2Id].x - state->agents[ourId].x) +
                      std::abs(state->agents[enemy2Id].y - state->agents[ourId].y)) / reward_move_to_enemy;

        for (int i = 0; i < state->powerup_kick.count; i++)
            point -= (std::abs(state->powerup_kick[i].x - state->agents[ourId].x) +
                      std::abs(state->powerup_kick[i].y - state->agents[ourId].y)) / reward_move_to_pickup;
        for (int i = 0; i < state->powerup_incr.count; i++)
            point -= (std::abs(state->powerup_incr[i].x - state->agents[ourId].x) +
                      std::abs(state->powerup_incr[i].y - state->agents[ourId].y)) / reward_move_to_pickup;
        for (int i = 0; i < state->powerup_extrabomb.count; i++)
            point -= (std::abs(state->powerup_extrabomb[i].x - state->agents[ourId].x) +
                      std::abs(state->powerup_extrabomb[i].y - state->agents[ourId].y)) / reward_move_to_pickup;
        //Following woods decrease points a little bit, I tried 3 different test setups. It would help, but it doesnt. Turned off.
        if (seenAgents == 0) {
            for (int i = 0; i < state->woods.count; i++)
                point -= (std::abs(state->woods[i].x - state->agents[ourId].x) +
                          std::abs(state->woods[i].y - state->agents[ourId].y)) / 1000.0f;
        }

        if (moves_in_chain[0] == 0) point -= reward_first_step_idle;
        if (lastMoveWasBlocked && ((state->timeStep / 4 + ourId) % 4) == 0 && moves_in_chain[0] == lastBlockedMove)
            point -= 0.1f;
        if (sameAs6_12_turns_ago && ((state->timeStep / 4 + ourId) % 4) == 0 &&
            moves_in_chain[0] == moveHistory[moveHistory.count - 6])
            point -= 0.1f;

        for (int i = 0; i < positions_in_chain.count; i++) {
            if (leadsToDeadEnd[positions_in_chain[i].x + BOARD_SIZE * positions_in_chain[i].y])
                point -= 0.001f;
            break; //only for the first move, as the leadsToDeadEnd can be deprecated if calculated with flames, bombs, woods. Yields better results.
        }

        return point;
    }

    float CologneAgent::runAlreadyPlantedBombs(State *state) {
        for (int i = 0; i < 10; i++) {
            //Exit if match decided, maybe we would die later from an other bomb, so that disturbs pointing and decision making
            if (state->aliveAgents < 2 || (state->aliveAgents == 2 &&
                                           ((state->agents[0].dead && state->agents[2].dead) ||
                                            (state->agents[1].dead && state->agents[3].dead))))
                break;
            //bboard::Step(state, moves);
            util::TickBombs(*state);
            state->relTimeStep++;
            //simulatedSteps++;
        }

        return scoreState(state);
    }

//#define RANDOM_TIEBREAK //With nobomb-random-tiebreak: 10% less simsteps, 3% less wins :( , 5-10% less ties against simple. Turned off by default. See log_test_02_tie.txt
//#define SCENE_HASH_MEMORY //8-10x less simsteps, but 40% less wins :((
    float CologneAgent::runOneStep(const bboard::State *state, int depth) {
        bboard::Move moves_in_one_step[4]; //was: moves
        moves_in_one_step[0] = bboard::Move::IDLE;
        moves_in_one_step[1] = bboard::Move::IDLE;
        moves_in_one_step[2] = bboard::Move::IDLE;
        moves_in_one_step[3] = bboard::Move::IDLE;

        const AgentInfo &a = state->agents[ourId];
        float maxPoint = -100;
#ifdef RANDOM_TIEBREAK
        FixedQueue<int, 6> bestmoves;
#endif
        for (int move = 0; move < 6; move++) {
            Position desiredPos = bboard::util::DesiredPosition(a.x, a.y, (bboard::Move) move);
            // if we don't have bomb
            if (move == (int) bboard::Move::BOMB && a.maxBombCount - a.bombCount <= 0)
                continue;
            // if bomb is already under us
            if (move == (int) bboard::Move::BOMB && state->HasBomb(a.x, a.y))
                continue;
            // if move is impossible
            if (move > 0 && move < 5 && !_CheckPos2(state, desiredPos))
                continue;
            //no two opposite steps please!
            if (depth > 0 && move > 0 && move < 5 && moves_in_chain[depth - 1] > 0 && moves_in_chain[depth - 1] < 5 &&
                std::abs(moves_in_chain[depth - 1] - move) == 2)
                continue;

            moves_in_one_step[ourId] = (bboard::Move) move;
            moves_in_chain.AddElem(move);

            float maxTeammate = -100;
            for (int moveT = 5; moveT >= 0; moveT--) {
                if (moveT > 0) {
                    if (depth >= teammateIteration ||
                        (state->agents[teammateId].dead || state->agents[teammateId].x < 0))
                        continue;
                    // if move is impossible
                    if (moveT > 0 && moveT < 5 && !_CheckPos2(state,
                                                              bboard::util::DesiredPosition(state->agents[teammateId].x,
                                                                                            state->agents[teammateId].y,
                                                                                            (bboard::Move) moveT)))
                        continue;
                    if (moveT == (int) bboard::Move::BOMB &&
                        state->agents[teammateId].maxBombCount - state->agents[teammateId].bombCount <= 0)
                        continue;
                    // if bomb is already under it
                    if (moveT == (int) bboard::Move::BOMB &&
                        state->HasBomb(state->agents[teammateId].x, state->agents[teammateId].y))
                        continue;
                } else {
                    //We'll have same results with IDLE, IDLE
                    if (maxTeammate > -100 && state->agents[teammateId].x == desiredPos.x &&
                        state->agents[teammateId].y == desiredPos.y)
                        continue;
                }

                moves_in_one_step[teammateId] = (bboard::Move) moveT;

                float minPointE1 = 100;
                for (int moveE1 = 5; moveE1 >= 0; moveE1--) {
                    if (moveE1 > 0) {
                        if (depth >= enemyIteration1 ||
                            (state->agents[enemy1Id].dead || state->agents[enemy1Id].x < 0))
                            continue;
                        // if move is impossible
                        if (moveE1 > 0 && moveE1 < 5 && !_CheckPos2(state, bboard::util::DesiredPosition(
                                state->agents[enemy1Id].x, state->agents[enemy1Id].y, (bboard::Move) moveE1)))
                            continue;
                        if (moveE1 == (int) bboard::Move::BOMB &&
                            state->agents[enemy1Id].maxBombCount - state->agents[enemy1Id].bombCount <= 0)
                            continue;
                        // if bomb is already under it
                        if (moveE1 == (int) bboard::Move::BOMB &&
                            state->HasBomb(state->agents[enemy1Id].x, state->agents[enemy1Id].y))
                            continue;
                    } else {
                        //We'll have same results with IDLE, IDLE
                        if (minPointE1 < 100 && state->agents[enemy1Id].x == desiredPos.x &&
                            state->agents[enemy1Id].y == desiredPos.y)
                            continue;
                    }

                    moves_in_one_step[enemy1Id] = (bboard::Move) moveE1;

                    float minPointE2 = 100;
                    for (int moveE2 = 5; moveE2 >= 0; moveE2--) {
                        if (moveE2 > 0) {
                            if (depth >= enemyIteration2 ||
                                (state->agents[enemy2Id].dead || state->agents[enemy2Id].x < 0))
                                continue;
                            // if move is impossible
                            if (moveE2 > 0 && moveE2 < 5 && !_CheckPos2(state, bboard::util::DesiredPosition(
                                    state->agents[enemy2Id].x, state->agents[enemy2Id].y, (bboard::Move) moveE2)))
                                continue;
                            if (moveE2 == (int) bboard::Move::BOMB &&
                                state->agents[enemy2Id].maxBombCount - state->agents[enemy2Id].bombCount <= 0)
                                continue;
                            // if bomb is already under it
                            if (moveE2 == (int) bboard::Move::BOMB &&
                                state->HasBomb(state->agents[enemy2Id].x, state->agents[enemy2Id].y))
                                continue;
                        } else {
                            //We'll have same results with IDLE, IDLE
                            if (minPointE2 < 100 && state->agents[enemy2Id].x == desiredPos.x &&
                                state->agents[enemy2Id].y == desiredPos.y)
                                continue;
                        }

                        moves_in_one_step[enemy2Id] = (bboard::Move) moveE2;

                        bboard::State *newstate = new bboard::State(*state);
                        newstate->relTimeStep++;

                        bboard::Step(newstate, moves_in_one_step);
                        simulatedSteps++;

#ifdef SCENE_HASH_MEMORY
                        uint128_t hash = ((((((((((((uint128_t)(newstate->agents[ourId].x * 11 + newstate->agents[ourId].y) * 121 +
                                             (newstate->agents[enemy1Id].dead || newstate->agents[enemy1Id].x < 0 ? 0 : newstate->agents[enemy1Id].x * 11 + newstate->agents[enemy1Id].y)) * 121 +
                                            (newstate->agents[enemy2Id].dead || newstate->agents[enemy2Id].x < 0 ? 0 : newstate->agents[enemy2Id].x * 11 + newstate->agents[enemy2Id].y)) * 121 +
                                           (newstate->agents[teammateId].dead || newstate->agents[teammateId].x < 0 ? 0 : newstate->agents[teammateId].x * 11 + newstate->agents[teammateId].y)) * 121 +
                                newstate->bombs.count)*6+
                                depth)*6+
                                (newstate->bombs.count > 0 ? newstate->bombs[newstate->bombs.count-1] : 0))*10000 +
                                (newstate->bombs.count > 1 ? newstate->bombs[newstate->bombs.count-2] : 0))*10000 +
                                (newstate->bombs.count > 2 ? newstate->bombs[newstate->bombs.count-3] : 0))*10000 +
                                (newstate->bombs.count > 3 ? newstate->bombs[newstate->bombs.count-4] : 0))*10000 +
                                newstate->agents[ourId].maxBombCount)*10 +
                                newstate->agents[ourId].bombStrength)*10 +
                                newstate->agents[0].dead*8 + newstate->agents[1].dead*4 + newstate->agents[2].dead*2 + newstate->agents[3].dead;

                        if(visitedSteps.count(hash) > 0) {
                            /*if(ourId == 3 && seenAgents == 0 && state->timeStep==23) {
                                for (int i = 0; i < moves_in_chain.count; i++)
                                    std::cout << moves_in_chain[i] << " ";
                                std::cout << "continue " << hash << std::endl;
                            }*/
                            delete newstate;
                            continue;
                        }
                        else {
                            visitedSteps.insert(hash);
                        }
#endif

                        Position myNewPos;
                        myNewPos.x = newstate->agents[newstate->ourId].x;
                        myNewPos.y = newstate->agents[newstate->ourId].y;
                        positions_in_chain[depth] = myNewPos;
                        positions_in_chain.count++;

                        float point;
                        if (depth + 1 < myMaxDepth)
                            point = runOneStep(newstate, depth + 1);
                        else
                            point = runAlreadyPlantedBombs(newstate);

                        if (point > -100 && point < minPointE2) { minPointE2 = point; }

                        positions_in_chain.count--;
                        delete newstate;
                    }
                    if (minPointE2 > -100 && minPointE2 < minPointE1) { minPointE1 = minPointE2; }
                }
                if (minPointE1 < 100 && minPointE1 > maxTeammate) { maxTeammate = minPointE1; }
            }

            moves_in_chain.RemoveAt(moves_in_chain.count - 1);
            best_moves_in_chain.count = std::max(depth + 1, best_moves_in_chain.count);

            if (maxTeammate > -100) {
#ifdef RANDOM_TIEBREAK
                if (maxTeammate == maxPoint && move != 5) { bestmoves[bestmoves.count] = move; bestmoves.count++;}
                if (maxTeammate > maxPoint) { maxPoint = maxTeammate; bestmoves.count = 1; bestmoves[0] = move;}
#else
                if (maxTeammate > maxPoint) {
                    maxPoint = maxTeammate;
                    if (maxPoint > best_points_in_chain[depth]) {
                        best_points_in_chain[depth] = maxPoint;
                        best_moves_in_chain[depth] = move;
                    }
                }
#endif
            }
        }
#ifdef RANDOM_TIEBREAK
        if(bestmoves.count > 0) {
            if(maxPoint > best_points_in_chain[depth])
            {
                best_moves_in_chain[depth] = bestmoves[(state->timeStep / 4) % bestmoves.count];
                best_points_in_chain[depth] = maxPoint;
            }
        }else
            best_moves_in_chain[depth] = 0;
#endif

        return maxPoint;
    }

    void CologneAgent::createDeadEndMap(const State *state) {
        short walkable_neighbours[BOARD_SIZE * BOARD_SIZE];
        memset(walkable_neighbours, 0, BOARD_SIZE * BOARD_SIZE * sizeof(short));
        std::list<Position> deadEnds;
        for (int x = 0; x < BOARD_SIZE; x++) {
            for (int y = 0; y < BOARD_SIZE; y++) {
                if (_CheckPos2(state, x, y)) {
                    walkable_neighbours[x + BOARD_SIZE * y] =
                            (int) _CheckPos2(state, x - 1, y) + (int) _CheckPos2(state, x, y - 1) +
                            (int) _CheckPos2(state, x + 1, y) + (int) _CheckPos2(state, x, y + 1);
                    if (walkable_neighbours[x + BOARD_SIZE * y] < 2) {
                        Position p;
                        p.x = x;
                        p.y = y;
                        deadEnds.push_back(p);
                    }
                }
            }
        }


        memset(leadsToDeadEnd, 0, BOARD_SIZE * BOARD_SIZE * sizeof(bool));

        std::function<void(int, int)> recurseFillWaysToDeadEnds = [&](int x, int y) {
            leadsToDeadEnd[x + BOARD_SIZE * y] = true;

            if (x > 0 && walkable_neighbours[x - 1 + BOARD_SIZE * y] < 2 &&
                leadsToDeadEnd[x - 1 + BOARD_SIZE * y] == false)
                recurseFillWaysToDeadEnds(x - 1, y);
            if (x < BOARD_SIZE - 1 && walkable_neighbours[x + 1 + BOARD_SIZE * y] < 2 &&
                leadsToDeadEnd[x + 1 + BOARD_SIZE * y] == false)
                recurseFillWaysToDeadEnds(x + 1, y);
            if (y > 0 && walkable_neighbours[x + BOARD_SIZE * (y - 1)] < 2 &&
                leadsToDeadEnd[x + BOARD_SIZE * (y - 1)] == false)
                recurseFillWaysToDeadEnds(x, y - 1);
            if (y < BOARD_SIZE - 1 && walkable_neighbours[x + BOARD_SIZE * (y + 1)] < 2 &&
                leadsToDeadEnd[x + BOARD_SIZE * (y + 1)] == false)
                recurseFillWaysToDeadEnds(x, y + 1);
        };

        for (Position p : deadEnds) {
            recurseFillWaysToDeadEnds(p.x, p.y);
        }
    }

    Move CologneAgent::act(const State *state) {
        createDeadEndMap(state);
        visitedSteps.clear();
        simulatedSteps = 0;
        bestPoint = -100.0f;
        enemyIteration1 = 0;
        enemyIteration2 = 0;
        teammateIteration = 0;
        seenAgents = 0;
        best_moves_in_chain.count = 0;
        best_points_in_chain.count = 10;
        ourId = state->ourId;
        enemy1Id = state->enemy1Id;
        enemy2Id = state->enemy2Id;
        teammateId = state->teammateId;
        positions_in_chain.count = 0;
        for (int i = 0; i < 10; i++)
            best_points_in_chain[i] = -1000;
        int seenEnemies = 0;
        if (!state->agents[teammateId].dead && state->agents[teammateId].x >= 0) {
            seenAgents++;
        }
        if (!state->agents[enemy1Id].dead && state->agents[enemy1Id].x >= 0) {
            seenAgents++;
            seenEnemies++;
        }
        if (!state->agents[enemy2Id].dead && state->agents[enemy2Id].x >= 0) {
            seenAgents++;
            seenEnemies++;
        }

        if (!state->agents[teammateId].dead && state->agents[teammateId].x >= 0) {
            int dist = std::abs(state->agents[ourId].x - state->agents[teammateId].x) +
                       std::abs(state->agents[ourId].y - state->agents[teammateId].y);
            if (dist < 3) teammateIteration++;
            if (dist < 5) teammateIteration++;
        }
        if (!state->agents[enemy1Id].dead && state->agents[enemy1Id].x >= 0) {
            int dist = std::abs(state->agents[ourId].x - state->agents[enemy1Id].x) +
                       std::abs(state->agents[ourId].y - state->agents[enemy1Id].y);
            if (dist < 2) enemyIteration1++;
            if (dist < 3 && seenAgents == 1) enemyIteration1++;
            if (dist < 5) enemyIteration1++;
        }
        if (!state->agents[enemy2Id].dead && state->agents[enemy2Id].x >= 0) {
            int dist = std::abs(state->agents[ourId].x - state->agents[enemy2Id].x) +
                       std::abs(state->agents[ourId].y - state->agents[enemy2Id].y);
            if (dist < 2) enemyIteration2++;
            if (dist < 3 && seenAgents == 1) enemyIteration2++;
            if (dist < 5) enemyIteration2++;
        }
        myMaxDepth = 6 - seenAgents;

        sameAs6_12_turns_ago = true;
        for (int agentId = 0; agentId < 4; agentId++) {
            if (previousPositions[agentId].count == 12) {
                if (previousPositions[agentId][0].x != state->agents[agentId].x ||
                    previousPositions[agentId][0].y != state->agents[agentId].y)
                    sameAs6_12_turns_ago = false;
                if (previousPositions[agentId][6].x != state->agents[agentId].x ||
                    previousPositions[agentId][6].y != state->agents[agentId].y)
                    sameAs6_12_turns_ago = false;
                previousPositions[agentId].RemoveAt(0);
            } else {
                sameAs6_12_turns_ago = false;
            }

            Position p;
            p.x = state->agents[agentId].x;
            p.y = state->agents[agentId].y;
            previousPositions[agentId][previousPositions[agentId].count] = p;
            previousPositions[agentId].count++;
        }
        if (sameAs6_12_turns_ago)
            std::cout << "SAME AS BEFORE!!!!" << std::endl;

        const AgentInfo &a = state->agents[ourId];
        if (state->timeStep > 1 && (expectedPosInNewTurn.x != a.x || expectedPosInNewTurn.y != a.y)) {
            std::cout << "Couldn't move to " << expectedPosInNewTurn.y << ":" << expectedPosInNewTurn.x;
            if (std::abs(state->agents[teammateId].x - expectedPosInNewTurn.x) +
                std::abs(state->agents[teammateId].y - expectedPosInNewTurn.y) == 1)
                std::cout << " - Racing with teammate, probably";
            if (std::abs(state->agents[enemy1Id].x - expectedPosInNewTurn.x) +
                std::abs(state->agents[enemy1Id].y - expectedPosInNewTurn.y) == 1)
                std::cout << " - Racing with enemy1, probably";
            if (std::abs(state->agents[enemy2Id].x - expectedPosInNewTurn.x) +
                std::abs(state->agents[enemy2Id].y - expectedPosInNewTurn.y) == 1)
                std::cout << " - Racing with enemy2, probably";
            std::cout << std::endl;
            lastMoveWasBlocked = true;
            lastBlockedMove = best_moves_in_chain[0];
        } else {
            lastMoveWasBlocked = false;
        }

        float point = runOneStep(state, 0);

        if (moveHistory.count == 12) {
            moveHistory.RemoveAt(0);
        }
        moveHistory[moveHistory.count] = best_moves_in_chain[0];
        moveHistory.count++;

        std::cout << "turn#" << state->timeStep << " ourId:" << ourId << " point: " << point << " selected: ";
        for (int i = 0; i < best_moves_in_chain.count; i++)
            std::cout << (int) best_moves_in_chain[i] << " > ";
        std::cout << " simulated steps: " << simulatedSteps;
        std::cout << ", depth " << myMaxDepth << " " << teammateIteration << " " << enemyIteration1 << " "
                  << enemyIteration2 << std::endl;

        totalSimulatedSteps += simulatedSteps;
        turns++;
        expectedPosInNewTurn = bboard::util::DesiredPosition(a.x, a.y, (bboard::Move) best_moves_in_chain[0]);
        return (bboard::Move) best_moves_in_chain[0];
    }

    void CologneAgent::PrintDetailedInfo() {
    }

}
