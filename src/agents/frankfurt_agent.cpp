#include "bboard.hpp"
#include "agents.hpp"
#include "strategy.hpp"
#include "step_utility.hpp"
#include <list>
#include <cstring>
#include <omp.h>

using namespace bboard;
using namespace bboard::strategy;

bboard::FixedQueue<int, 40> agents::FrankfurtAgent::moves_in_chain;
bboard::FixedQueue<bboard::Position, 40> agents::FrankfurtAgent::positions_in_chain;

namespace agents {
	FrankfurtAgent::FrankfurtAgent() {
	    for(int timestap=0; timestap<30; timestap++)
        reward_sooner_later_ratio_pow_timestamps[timestap] = (float)std::pow(reward_sooner_later_ratio, timestap);
	}

	bool FrankfurtAgent::_CheckPos2(const State *state, bboard::Position pos, int agentId = -1) {
		return _CheckPos2(state, pos.x, pos.y, agentId);
	}

	bool FrankfurtAgent::_CheckPos2(const State *state, int x, int y, int agentId = -1) {
		return !util::IsOutOfBounds(x, y) && (IS_WALKABLE_OR_AGENT(state->board[y][x]) || (agentId >= 0 && state->agents[agentId].canKick && state->board[y][x] == BOMB));
	}

	float FrankfurtAgent::laterBetter(float reward, int timestaps) {
		if (reward == 0.0f)
			return reward;

		if (reward > 0)
			return reward * (1.0f / reward_sooner_later_ratio_pow_timestamps[timestaps]);
		else
			return reward * reward_sooner_later_ratio_pow_timestamps[timestaps];
	}

	float FrankfurtAgent::soonerBetter(float reward, int timestaps) {
		if (reward == 0.0f)
			return reward;

		if (reward < 0)
			return reward * (1.0f / reward_sooner_later_ratio_pow_timestamps[timestaps]);
		else
			return reward * reward_sooner_later_ratio_pow_timestamps[timestaps];
	}

	StepResult FrankfurtAgent::scoreState(State *state) {
		StepResult stepRes;
		float teamBalance = (ourId < 2 ? 1.01f : 0.99f);
		float point = 0.0f;
		if (state->agents[ourId].dead) {
			point += laterBetter(-10 * state->agents[ourId].dead, state->agents[ourId].diedAt - state->timeStep);
		}
		if (state->agents[teammateId].x >= 0 && state->agents[teammateId].dead) {
			point += laterBetter(-10 * state->agents[teammateId].dead, state->agents[teammateId].diedAt - state->timeStep);
		}
		if (state->agents[enemy1Id].x >= 0 && state->agents[enemy1Id].dead) {
			point += 3 * soonerBetter(state->agents[enemy1Id].dead, state->agents[enemy1Id].diedAt - state->timeStep);
		}
		if (state->agents[enemy2Id].x >= 0 && state->agents[enemy2Id].dead) {
			point += 3 * soonerBetter(state->agents[enemy2Id].dead, state->agents[enemy2Id].diedAt - state->timeStep);
		}

		point += reward_woodDemolished * state->agents[ourId].woodDemolished;
		point += reward_woodDemolished * state->agents[teammateId].woodDemolished;
		point -= reward_woodDemolished * state->agents[enemy1Id].woodDemolished;
		point -= reward_woodDemolished * state->agents[enemy2Id].woodDemolished;

		// Reward long chained explosions, they may be difficult to see for enemies. But if only our teammate is around, dont make it more difficult for him :)
		// Only if teammate is not seen, or he is seen but two enemies also
		if(state->longestChainedBombDistance > 1 && (state->agents[teammateId].x < 0 || (state->agents[enemy1Id].x >= 0 && state->agents[enemy2Id].x >= 0)))
		    point += ((float)state->longestChainedBombDistance) / 200.0f;

        point += (reward_extraBombPowerupPoints * state->agents[state->ourId].extraBombPowerupPoints +    reward_firstKickPowerupPoints * state->agents[state->ourId].firstKickPowerupPoints + reward_otherKickPowerupPoints * state->agents[state->ourId].otherKickPowerupPoints + reward_extraRangePowerupPoints * state->agents[state->ourId].extraRangePowerupPoints) * teamBalance;
        point += (reward_extraBombPowerupPoints * state->agents[state->teammateId].extraBombPowerupPoints + reward_firstKickPowerupPoints * state->agents[state->teammateId].firstKickPowerupPoints + reward_otherKickPowerupPoints * state->agents[state->teammateId].otherKickPowerupPoints + reward_extraRangePowerupPoints * state->agents[state->teammateId].extraRangePowerupPoints) / teamBalance;
        point -= reward_extraBombPowerupPoints * state->agents[state->enemy1Id].extraBombPowerupPoints + reward_firstKickPowerupPoints * state->agents[state->enemy1Id].firstKickPowerupPoints + reward_otherKickPowerupPoints * state->agents[state->enemy1Id].otherKickPowerupPoints + reward_extraRangePowerupPoints * state->agents[state->enemy1Id].extraRangePowerupPoints;
        point -= reward_extraBombPowerupPoints * state->agents[state->enemy2Id].extraBombPowerupPoints + reward_firstKickPowerupPoints * state->agents[state->enemy2Id].firstKickPowerupPoints + reward_otherKickPowerupPoints * state->agents[state->enemy2Id].otherKickPowerupPoints + reward_extraRangePowerupPoints * state->agents[state->enemy2Id].extraRangePowerupPoints;

		//Attack same enemy
		if(rushing)
		{
			point -= std::abs(state->agents[ourId].x - 1) / 300.0f;
			point -= std::abs(positions_in_chain[0].x - 1) / 300.0f;

			if(state->ourId % 2) //Agent 1,3: meeting left-top
			{
				point -= std::abs(positions_in_chain[0].y - 1) / 300.0f;
				point -= std::abs(state->agents[ourId].y - 1) / 300.0f;
			}else //Agent 0,2: meeting: left-bottom
			{
				point -= std::abs((BOARD_SIZE - 1 - positions_in_chain[0].y) - 1) / 300.0f;
				point -= std::abs((BOARD_SIZE - 1 - state->agents[ourId].y) - 1) / 300.0f;
			}
		}

		//This assumes that there is a highway channel around
		if (goingAround)
		{
		    bool direction = (state->ourId == 1 || state->ourId == 2); //first: team members should go opposite direction to caress enemies
            direction = ((turns + (state->ourId % 2) * 30) / 100 + (direction ? 1 : 0)) % 2 == 0; //change direction sometimes to not go around and around if 1-1 lives from the two teams

            bool weAreDown = previousPositions[ourId][previousPositions[ourId].count - 1].y >= BOARD_SIZE - 3;
			bool weAreUp = previousPositions[ourId][previousPositions[ourId].count - 1].y < 3;
			bool weAreRight = previousPositions[ourId][previousPositions[ourId].count - 1].x >= BOARD_SIZE - 3;
			bool weAreLeft = previousPositions[ourId][previousPositions[ourId].count - 1].x < 3;
			if (weAreDown || weAreUp)
			{
				//Moving horizontally

				if ((weAreDown && (state->comeAround==1 || (state->comeAround == 0 && direction))) ||
				    (weAreUp &&   (state->comeAround==2 || (state->comeAround == 0 && !direction)))) {
					//Moving right
					point -= std::abs((BOARD_SIZE - 1 - state->agents[ourId].x) - 1) / 1000.0f;
					point -= std::abs((BOARD_SIZE - 1 - positions_in_chain[0].x) - 1) / 1000.0f;
				}
				else {
					//Moving left
					point -= std::abs(state->agents[ourId].x - 1) / 1000.0f;
					point -= std::abs(positions_in_chain[0].x - 1) / 1000.0f;
				}
			}
			if (weAreLeft || weAreRight)
			{
				//Moving vertically

				if ((weAreLeft && (state->comeAround==1 || (state->comeAround == 0 && direction))) ||
				   (weAreRight && (state->comeAround==2 || (state->comeAround == 0 && !direction))))
				{
					//Moving down
					point -= std::abs((BOARD_SIZE - 1 - state->agents[ourId].y) - 1) / 1000.0f;
					point -= std::abs((BOARD_SIZE - 1 - positions_in_chain[0].y) - 1) / 1000.0f;
				}
				else
				{
					//Moving up
					point -= std::abs(state->agents[ourId].y - 1) / 1000.0f;
					point -= std::abs(positions_in_chain[0].y - 1) / 1000.0f;
				}
			}
			if (!weAreDown && !weAreUp && !weAreLeft && !weAreRight)
			{
				//we are somewhere middle
				if (state->agents[ourId].y < BOARD_SIZE / 2) {
					point -= std::abs(state->agents[ourId].y - 1) / 1000.0f;
					point -= std::abs(positions_in_chain[0].y - 1) / 1000.0f;
				}
				else {
					point -= std::abs((BOARD_SIZE - 1 - state->agents[ourId].y) - 1) / 1000.0f;
					point -= std::abs((BOARD_SIZE - 1 - positions_in_chain[0].y) - 1) / 1000.0f;
				}

				if (state->agents[ourId].x < BOARD_SIZE / 2) {
					point -= std::abs(state->agents[ourId].x - 1) / 1000.0f;
					point -= std::abs(positions_in_chain[0].x - 1) / 1000.0f;
				}
				else {
					point -= std::abs((BOARD_SIZE - 1 - state->agents[ourId].x) - 1) / 1000.0f;
					point -= std::abs((BOARD_SIZE - 1 - positions_in_chain[0].x) - 1) / 1000.0f;
				}
			}
		}

		if (state->aliveAgents == 0) {
			//point += soonerBetter(??, state->relTimeStep); //we win
		}
		else if (state->aliveAgents < 3) {
			if (state->agents[ourId].dead && state->agents[teammateId].dead) {
				point += laterBetter(-20.0f, state->relTimeStep); //we lost
			}
			if (state->agents[enemy1Id].dead && state->agents[enemy2Id].dead) {
				point += soonerBetter(+20.0f, state->relTimeStep); //we win
			}
		}

        // Run away -> maximalize distance from enemies, try to have a tie
        // if 3 agents alive, only teammate is dead, but both enemies are within range
		bool move_away_from_enemy =  state->aliveAgents == 3 && state->agents[teammateId].dead && state->agents[enemy1Id].x >= 0 && state->agents[enemy2Id].x >= 0;
		float current_reward_move_to_enemy = move_away_from_enemy ? -reward_move_to_enemy : reward_move_to_enemy;


		if (state->agents[enemy1Id].x >= 0)
			point -= (std::abs(state->agents[enemy1Id].x - state->agents[ourId].x) + std::abs(state->agents[enemy1Id].y - state->agents[ourId].y)) / current_reward_move_to_enemy;
		if (state->agents[enemy2Id].x >= 0)
			point -= (std::abs(state->agents[enemy2Id].x - state->agents[ourId].x) + std::abs(state->agents[enemy2Id].y - state->agents[ourId].y)) / current_reward_move_to_enemy;

		for (int i = 0; i < state->powerup_kick.count; i++)
			point -= (std::abs(state->powerup_kick[i].x - state->agents[ourId].x) + std::abs(state->powerup_kick[i].y - state->agents[ourId].y)) / reward_move_to_pickup;
		for (int i = 0; i < state->powerup_incr.count; i++)
			point -= (std::abs(state->powerup_incr[i].x - state->agents[ourId].x) + std::abs(state->powerup_incr[i].y - state->agents[ourId].y)) / reward_move_to_pickup;
		for (int i = 0; i < state->powerup_extrabomb.count; i++)
			point -= (std::abs(state->powerup_extrabomb[i].x - state->agents[ourId].x) + std::abs(state->powerup_extrabomb[i].y - state->agents[ourId].y)) / reward_move_to_pickup;
		//Following woods decrease points a little bit, I tried 3 different test setups. It would help, but it doesnt. Turned off.
		if (seenAgents == 0) {
			for (int i = 0; i < state->woods.count; i++)
				point -= (std::abs(state->woods[i].x - state->agents[ourId].x) + std::abs(state->woods[i].y - state->agents[ourId].y)) / 1000.0f;
		}

		if (moves_in_chain[0] == 0) point -= reward_first_step_idle; //dont be IDLE if we can do something
		if (lastMoveWasBlocked && ((state->timeStep / 4 + ourId) % 4) == 0 && moves_in_chain[0] == lastBlockedMove)
			point -= 0.1f;
		// Trying to get out of a deadlock situation by discouraging choosing the same action as 6 and 12 turns before
		if (sameAs6_12_turns_ago && ((state->timeStep / 4 + ourId) % 4) == 0 && moves_in_chain[0] == moveHistory[moveHistory.count - 6])
			point -= 0.1f;

		if (!state->agents[teammateId].dead && state->agents[teammateId].x >= 0 && leadsToDeadEnd[state->agents[teammateId].x + BOARD_SIZE * state->agents[teammateId].y])
			point -= 0.0004f;
		if (!state->agents[enemy1Id].dead && state->agents[enemy1Id].x >= 0 && leadsToDeadEnd[state->agents[enemy1Id].x + BOARD_SIZE * state->agents[enemy1Id].y])
			point += 0.0004f;
		if (!state->agents[enemy2Id].dead && state->agents[enemy2Id].x >= 0 && leadsToDeadEnd[state->agents[enemy2Id].x + BOARD_SIZE * state->agents[enemy2Id].y])
			point += 0.0004f;
		for (int i = 0; i < positions_in_chain.count; i++) {
			if (leadsToDeadEnd[positions_in_chain[i].x + BOARD_SIZE * positions_in_chain[i].y] && iteratedAgents > 0)
				point -= 0.001f;
			break; //only for the first move, as the leadsToDeadEnd can be deprecated if calculated with flames, bombs, woods. Yields better results.
		}

		stepRes = point;
		return stepRes;
	}

	StepResult FrankfurtAgent::runAlreadyPlantedBombs(State *state) {
		//#define USE_NEW_ONECALL_EXPLOSION //Bit slower! 'TickAndMoveBombs10' can be deleted!!
		for (int i = 0; i < BOMB_LIFETIME; i++) {
			//Exit if match decided, maybe we would die later from an other bomb, so that disturbs pointing and decision making
			if (state->aliveAgents < 2 || (state->aliveAgents == 2 && (state->agents[0].dead == state->agents[2].dead)))
				break;

			util::TickAndMoveBombs(*state);
			state->relTimeStep++;
		}
		return scoreState(state);
	}

	//#define RANDOM_TIEBREAK //With nobomb-random-tiebreak: 10% less simsteps, 3% less wins :( , 5-10% less ties against simple. Turned off by default. See log_test_02_tie.txt
	//#define SCENE_HASH_MEMORY //8-10x less simsteps, but 40% less wins :((
	StepResult FrankfurtAgent::runOneStep(const bboard::State *state, const int depth) {
        StepResult stepRes;
        bboard::Move moves_in_one_step[4];
        const AgentInfo &a = state->agents[ourId];
        int choosenMove = 100;
		stepRes = -100;

        StepResult stepRess[6];
#pragma omp set_dynamic(0)
#pragma omp parallel for private(moves_in_one_step) shared(stepRes,stepRess) num_threads(depth < 1? 6 : 1)
		//int moves[]{1,2,3,4,0,5};
		//for(int move : moves)
		for (int move = 0; move < 6; move++)
		{
            stepRess[move] = -10000.0f;

			Position myDesiredPos = bboard::util::DesiredPosition(a.x, a.y, (bboard::Move) move);
			// if we don't have bomb
			if (move == (int)bboard::Move::BOMB && a.maxBombCount - a.bombCount <= 0)
				continue;
			// if bomb is already under us
			if (move == (int)bboard::Move::BOMB && state->HasBomb(a.x, a.y))
				continue;
			// if move is impossible
			if (move > 0 && move < 5 && !_CheckPos2(state, myDesiredPos, ourId))
				continue;
			//no two opposite steps please - only after bomb if we can kick or powerup. Slower and worse.
			//if ((state->agents[ourId].collectedPowerupPoints == 0 || depth < 2 || !state->agents[ourId].canKick || moves_in_chain[4*(depth - 2)+0] < 5) && depth > 0 &&  util::AreOppositeMoves(moves_in_chain[4*(depth - 1)], move))
			//    continue;

			moves_in_one_step[ourId] = (bboard::Move) move;
			moves_in_chain.AddElem(move);

			float maxTeammate = -100;
			StepResult futureStepsT;
			for (int moveT = 5; moveT >= 0; moveT--) {
				if (moveT > 0) {
					if (depth >= teammateIteration || (state->agents[teammateId].dead || state->agents[teammateId].x < 0))
						continue;
					// if move is impossible
					if (moveT > 0 && moveT < 5 && !_CheckPos2(state, bboard::util::DesiredPosition(state->agents[teammateId].x,
						state->agents[teammateId].y, (bboard::Move) moveT), teammateId))
						continue;
					if (moveT == (int)bboard::Move::BOMB && state->agents[teammateId].maxBombCount - state->agents[teammateId].bombCount <= 0)
						continue;
					// if bomb is already under it
					if (moveT == (int)bboard::Move::BOMB && state->HasBomb(state->agents[teammateId].x, state->agents[teammateId].y))
						continue;
					//no two opposite steps please - only after bomb if we can kick or powerup. Slower and worse.
					//if ((state->agents[teammateId].collectedPowerupPoints == 0 || depth < 2 || !state->agents[teammateId].canKick || moves_in_chain[4*(depth - 2)+1] < 5) && depth > 0 &&  util::AreOppositeMoves(moves_in_chain[4*(depth - 1)+1], moveT))
					//    continue;
				}
				else {
					//We'll have same results with IDLE, IDLE
					if (maxTeammate > -100 && state->agents[teammateId].x == myDesiredPos.x &&
                        state->agents[teammateId].y == myDesiredPos.y)
						continue;
				}

				moves_in_one_step[teammateId] = (bboard::Move) moveT;
				moves_in_chain.AddElem(moveT);

				float Eavg = 0.f;
				int Eavg_count = 0;
				float minPointE1 = 100;
				StepResult futureStepsE1;
				for (int moveE1 = 5; moveE1 >= 0; moveE1--) {
					if (moveE1 > 0) {
						if (depth >= enemyIteration1 || (state->agents[enemy1Id].dead || state->agents[enemy1Id].x < 0))
							continue;
						// if move is impossible
						if (moveE1 > 0 && moveE1 < 5 && !_CheckPos2(state, bboard::util::DesiredPosition(
							state->agents[enemy1Id].x, state->agents[enemy1Id].y, (bboard::Move) moveE1), enemy1Id))
							continue;
						if (moveE1 == (int)bboard::Move::BOMB && state->agents[enemy1Id].maxBombCount - state->agents[enemy1Id].bombCount <= 0)
							continue;
						// if bomb is already under it
						if (moveE1 == (int)bboard::Move::BOMB && state->HasBomb(state->agents[enemy1Id].x, state->agents[enemy1Id].y))
							continue;
						//no two opposite steps please - only after bomb if we can kick or powerup. Slower and worse.
						//if ((state->agents[enemy1Id].collectedPowerupPoints == 0 || depth < 2 || !state->agents[enemy1Id].canKick || moves_in_chain[4*(depth - 2)+2] < 5) && depth > 0 &&  util::AreOppositeMoves(moves_in_chain[4*(depth - 1)+2], moveE1))
						//    continue;
						//No long simulations if no step-bomb-step cycle
						if (depth > 1 && moves_in_chain[4 * (depth - 2) + 2] != 5 && moves_in_chain[4 * (depth - 1) + 2] != 5 && moveE1 != 5)
							continue;
					}
					else {
						//We'll have same results with IDLE, IDLE
						if (minPointE1 < 100 && state->agents[enemy1Id].x == myDesiredPos.x &&
                            state->agents[enemy1Id].y == myDesiredPos.y)
							continue;
					}

					moves_in_one_step[enemy1Id] = (bboard::Move) moveE1;
					moves_in_chain.AddElem(moveE1);

					float minPointE2 = 100;
					StepResult futureStepsE2;
					for (int moveE2 = 5; moveE2 >= 0; moveE2--) {
						if (moveE2 > 0) {
							if (depth >= enemyIteration2 || (state->agents[enemy2Id].dead || state->agents[enemy2Id].x < 0))
								continue;
							// if move is impossible
							if (moveE2 > 0 && moveE2 < 5 && !_CheckPos2(state, bboard::util::DesiredPosition(
								state->agents[enemy2Id].x, state->agents[enemy2Id].y, (bboard::Move) moveE2), enemy2Id))
								continue;
							if (moveE2 == (int)bboard::Move::BOMB && state->agents[enemy2Id].maxBombCount - state->agents[enemy2Id].bombCount <= 0)
								continue;
							// if bomb is already under it
							if (moveE2 == (int)bboard::Move::BOMB && state->HasBomb(state->agents[enemy2Id].x, state->agents[enemy2Id].y))
								continue;
							//no two opposite steps please - only after bomb if we can kick or powerup. Slower and worse.
							//if ((state->agents[enemy2Id].collectedPowerupPoints == 0 || depth < 2 || !state->agents[enemy2Id].canKick || moves_in_chain[4*(depth - 2)+3] < 5) && depth > 0 &&  util::AreOppositeMoves(moves_in_chain[4*(depth - 1)+3], moveE2))
							//    continue;
							//No long simulations if no step-bomb-step cycle
							if (depth > 1 && moves_in_chain[4 * (depth - 2) + 3] != 5 && moves_in_chain[4 * (depth - 1) + 3] != 5 && moveE2 != 5)
								continue;
						}
						else {
							//We'll have same results with IDLE, IDLE
							if (minPointE2 < 100 && state->agents[enemy2Id].x == myDesiredPos.x &&
                                state->agents[enemy2Id].y == myDesiredPos.y)
								continue;
						}

						moves_in_one_step[enemy2Id] = (bboard::Move) moveE2;
						moves_in_chain.AddElem(moveE2);

						bboard::State newstate(*state);
						newstate.relTimeStep++;

						if (!bboard::Step(&newstate, moves_in_one_step))
						{
							moves_in_chain.count--;
							continue;
						}
#pragma omp atomic
						simulatedSteps++;

						Position myNewPos;
						myNewPos.x = newstate.agents[newstate.ourId].x;
						myNewPos.y = newstate.agents[newstate.ourId].y;
						positions_in_chain[depth] = myNewPos;
						positions_in_chain.count++;

						StepResult futureSteps;
						if (depth + 1 < myMaxDepth)
						{
#ifdef TIME_LIMIT_ON
							size_t millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time).count();
							if (millis > 147)
								std::cout << "OVERTIME " << millis << std::endl;
							if ((millis < 130) || (depth < 3 && millis < 140) || (depth < 2 && millis < 145))
								futureSteps = runOneStep(&newstate, depth + 1);
							else
								futureSteps = runAlreadyPlantedBombs(&newstate);
#else
							futureSteps = runOneStep(&newstate, depth + 1);
#endif
						}
						else
							futureSteps = runAlreadyPlantedBombs(&newstate);

						Eavg += (float)futureSteps;
						Eavg_count++;

						if ((float)futureSteps > -100 && (float)futureSteps < minPointE2) {
							minPointE2 = (float)futureSteps;
							futureStepsE2 = futureSteps;
						}

						positions_in_chain.count--;
						moves_in_chain.count--;
					}
					if (minPointE2 > -100 && minPointE2 < minPointE1) {
						minPointE1 = minPointE2;
						futureStepsE1 = futureStepsE2;
					}
					moves_in_chain.count--;
				}
				if (minPointE1 < 100 && minPointE1 > maxTeammate) {
					maxTeammate = minPointE1;
					futureStepsT = futureStepsE1;
				}

				Eavg = Eavg_count ? Eavg / (float)Eavg_count : Eavg;
				futureStepsT += Eavg * weight_of_average_Epoint;
				maxTeammate += Eavg * weight_of_average_Epoint;

				moves_in_chain.count--;
			}

			if (maxTeammate > -100) {

//Save results
				{
						stepRess[move] = futureStepsT;
				}
			}
			moves_in_chain.count--;
		}

        int bestIndex = 0;
        for(int i=1; i<6; i++)
        {
            if((float)stepRess[i] > stepRess[bestIndex])
                bestIndex = i;
        }

        if(depth == 0)
            depth_0_Move = bestIndex;

		return stepRess[bestIndex];
	}

	void FrankfurtAgent::createDeadEndMap(const State *state) {
		short walkable_neighbours[BOARD_SIZE * BOARD_SIZE];
		memset(walkable_neighbours, 0, BOARD_SIZE * BOARD_SIZE * sizeof(short));
		std::list<Position> deadEnds;
		for (int x = 0; x < BOARD_SIZE; x++) {
			for (int y = 0; y < BOARD_SIZE; y++) {
				if (_CheckPos2(state, x, y)) {
					walkable_neighbours[x + BOARD_SIZE * y] =
						(int)_CheckPos2(state, x - 1, y) + (int)_CheckPos2(state, x, y - 1) +
						(int)_CheckPos2(state, x + 1, y) + (int)_CheckPos2(state, x, y + 1);
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

			if (x > 0 && walkable_neighbours[x - 1 + BOARD_SIZE * y] < 3 && !leadsToDeadEnd[x - 1 + BOARD_SIZE * y] && walkable_neighbours[x - 1 + BOARD_SIZE * y] > 0)
				recurseFillWaysToDeadEnds(x - 1, y);
			if (x < BOARD_SIZE - 1 && walkable_neighbours[x + 1 + BOARD_SIZE * y] < 3 && !leadsToDeadEnd[x + 1 + BOARD_SIZE * y] && walkable_neighbours[x + 1 + BOARD_SIZE * y] > 0)
				recurseFillWaysToDeadEnds(x + 1, y);
			if (y > 0 && walkable_neighbours[x + BOARD_SIZE * (y - 1)] < 3 && !leadsToDeadEnd[x + BOARD_SIZE * (y - 1)] && walkable_neighbours[x + BOARD_SIZE * (y - 1)] > 0)
				recurseFillWaysToDeadEnds(x, y - 1);
			if (y < BOARD_SIZE - 1 && walkable_neighbours[x + BOARD_SIZE * (y + 1)] < 3 && !leadsToDeadEnd[x + BOARD_SIZE * (y + 1)] && walkable_neighbours[x + BOARD_SIZE * (y + 1)] > 0)
				recurseFillWaysToDeadEnds(x, y + 1);
		};

		//#define DEBUG_LEADS_TO_DEAD_END
		for (Position p : deadEnds) {
			recurseFillWaysToDeadEnds(p.x, p.y);
		}

	}

	Move FrankfurtAgent::act(const State *state) {
		createDeadEndMap(state);
		visitedSteps.clear();
		simulatedSteps = 0;
		enemyIteration1 = 0;
		enemyIteration2 = 0;
		teammateIteration = 0;
		seenAgents = 0;
		ourId = state->ourId;
		enemy1Id = state->enemy1Id;
		enemy2Id = state->enemy2Id;
		teammateId = state->teammateId;
		positions_in_chain.count = 0;
		int seenEnemies = 0;
		if (!state->agents[teammateId].dead && state->agents[teammateId].x >= 0) {
			seenAgents++;
		}
		if (!state->agents[enemy1Id].dead && state->agents[enemy1Id].x >= 0) {
			seenAgents++;
			seenEnemies++;
			lastSeenEnemy = state->timeStep;
		}
		if (!state->agents[enemy2Id].dead && state->agents[enemy2Id].x >= 0) {
			seenAgents++;
			seenEnemies++;
			lastSeenEnemy = state->timeStep;
		}

		iteratedAgents = 0;
		if (!state->agents[teammateId].dead && state->agents[teammateId].x >= 0) {
			int dist = std::abs(state->agents[ourId].x - state->agents[teammateId].x) +
				std::abs(state->agents[ourId].y - state->agents[teammateId].y);
			if (dist < 3) teammateIteration++;
			if (dist < 10) { teammateIteration++; }
			if(state->agents[teammateId].starts_on_bomb)
                teammateIteration = std::max(2, teammateIteration);
		}
		if (!state->agents[enemy1Id].dead && state->agents[enemy1Id].x >= 0) {
			int dist = std::abs(state->agents[ourId].x - state->agents[enemy1Id].x) +
				std::abs(state->agents[ourId].y - state->agents[enemy1Id].y);
			if (dist < 3) enemyIteration1++;
			if (dist < 3 && seenAgents == 1) enemyIteration1++;
			if (dist < 10) { enemyIteration1++; }
            if(state->agents[enemy1Id].starts_on_bomb)
                enemyIteration1 = std::max(2, enemyIteration1);
		}
		if (!state->agents[enemy2Id].dead && state->agents[enemy2Id].x >= 0) {
			int dist = std::abs(state->agents[ourId].x - state->agents[enemy2Id].x) +
				std::abs(state->agents[ourId].y - state->agents[enemy2Id].y);
			if (dist < 3) enemyIteration2++;
			if (dist < 3 && seenAgents == 1) enemyIteration2++;
			if (dist < 10) { enemyIteration2++; }
            if(state->agents[enemy2Id].starts_on_bomb)
                enemyIteration2 = std::max(2, enemyIteration2);
		}
		iteratedAgents = (teammateIteration > 0 ? 1 : 0) + (enemyIteration1 > 0 ? 1 : 0) + (enemyIteration2 > 0 ? 1 : 0);
		myMaxDepth = 6 - iteratedAgents;

		rushing = state->timeStep < 75 && !state->agents[enemy1Id].dead && !state->agents[enemy2Id].dead && seenAgents < 2;

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
			}
			else {
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
			lastBlockedMove = moveHistory[moveHistory.count - 1];
		}
		else {
			lastMoveWasBlocked = false;
		}

		goingAround = state->timeStep > 75 && (state->timeStep - lastSeenEnemy) > 2;

		StepResult stepRes = runOneStep(state, 0);

		int myMove = depth_0_Move;

		if (moveHistory.count == 12) {
			moveHistory.RemoveAt(0);
		}
		moveHistory[moveHistory.count] = myMove;
		moveHistory.count++;

		std::cout << "turn#" << state->timeStep << " ourId:" << ourId << " point: " << (float)stepRes << " selected: ";
		std::cout << myMove << " simulated steps: " << simulatedSteps;
		std::cout << ", depth " << myMaxDepth << " " << teammateIteration << " " << enemyIteration1 << " "
			<< enemyIteration2 << (rushing ? " rushing" : "") << (goingAround ? " goingAround" : "") << std::endl;
		std::cout << std::endl;

		if(sameAs6_12_turns_ago && turns > 80 && seenEnemies > 0 && seenAgents == seenEnemies) {
            message[0] = FrankfurtMessageTypes::ComeAround7;
            //1: come CCW    2: come CW
            if(state->agents[id].y == BOARD_SIZE-2) //we are at botttom
            {
                int enemyX = enemyIteration1 > enemyIteration2 ? state->agents[enemy1Id].x : state->agents[enemy2Id].x;
                message[1] = enemyX < state->agents[id].x ? 1 : 2;
            }else if(state->agents[id].y == 1) // we are at the top
            {
                int enemyX = enemyIteration1 > enemyIteration2 ? state->agents[enemy1Id].x : state->agents[enemy2Id].x;
                message[1] = enemyX < state->agents[id].x ? 2 : 1;
            }else if(state->agents[id].x == BOARD_SIZE-2) //we are at right
            {
                int enemyY = enemyIteration1 > enemyIteration2 ? state->agents[enemy1Id].y : state->agents[enemy2Id].y;
                message[1] = enemyY < state->agents[id].x ? 2 : 1;
            }else if(state->agents[id].x == 1) // we are left
            {
                int enemyY = enemyIteration1 > enemyIteration2 ? state->agents[enemy1Id].y : state->agents[enemy2Id].y;
                message[1] = enemyY < state->agents[id].x ? 1 : 2;
            }
        }else{
            switch (1 + state->timeStep % 7) {
                case 1:
                    message[0] = FrankfurtMessageTypes::MaxBombCount1;
                    message[1] = state->agents[id].maxBombCount;
                    break;
                case 2:
                    message[0] = FrankfurtMessageTypes::CanKick2;
                    message[1] = (state->agents[id].canKick ? 1 : 0) + (state->agents[enemy1Id].canKick ? 2 : 0) + (state->agents[enemy2Id].canKick ? 4 : 0); //TODO: would be nice to use newState
                    break;
                case 3:
                    if (state->agents[teammateId].x)
                        message[0] = FrankfurtMessageTypes::PositionX3 * 0;
                    message[1] = state->agents[id].x;
                    break;
                case 4:
                    message[0] = FrankfurtMessageTypes::PositionY4 * 0;
                    message[1] = state->agents[id].y;
                    break;
                case 5:
                    message[0] = FrankfurtMessageTypes::AttackNorth5;
                    message[1] = 0;
                    break;
                case 6:
                    message[0] = FrankfurtMessageTypes::BombStrength6;
                    message[1] = state->agents[id].bombStrength;
            }
        }
        message[1] = std::min(7, message[1]);

		totalSimulatedSteps += simulatedSteps;
		turns++;
		expectedPosInNewTurn = bboard::util::DesiredPosition(a.x, a.y, (bboard::Move) myMove);
		return (bboard::Move) myMove;
	}

	void FrankfurtAgent::PrintDetailedInfo() {
	}

}
