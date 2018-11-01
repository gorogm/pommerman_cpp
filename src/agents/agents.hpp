#ifndef RANDOM_AGENT_H
#define RANDOM_AGENT_H

#include <random>

#include "bboard.hpp"
#include "strategy.hpp"
#include <set>
#include "uint128_t.h"

namespace agents
{

/**
 * Use this as an example to implement more sophisticated
 * agents.
 *
 * @brief Randomly selects actions
 */
struct RandomAgent : bboard::Agent
{
    std::mt19937_64 rng;
    std::uniform_int_distribution<int> intDist;

    RandomAgent();

    bboard::Move act(const bboard::State* state) override;
};


/**
 * @brief Randomly selects actions that are not laying bombs
 */
struct HarmlessAgent : bboard::Agent
{
    std::mt19937_64 rng;
    std::uniform_int_distribution<int> intDist;

    HarmlessAgent();

    bboard::Move act(const bboard::State* state) override;
};

/**
 * @brief Selects Idle for every action
 */
struct LazyAgent : bboard::Agent
{
    bboard::Move act(const bboard::State* state) override;
};


/**
 * @brief Selects Idle for every action
 */
struct SimpleAgent : bboard::Agent
{
    std::mt19937_64 rng;
    std::uniform_int_distribution<int> intDist;

    SimpleAgent();

    //////////////
    // Specific //
    //////////////
    int danger = 0;
    bboard::strategy::RMap r;
    bboard::FixedQueue<bboard::Move, bboard::MOVE_COUNT> moveQueue;
    bboard::FixedQueue<bboard::Position, 4> recentPositions;

    bboard::Move act(const bboard::State* state) override;

    void PrintDetailedInfo();
};
/**
 * @brief Selects Idle for every action
 */
    struct BerlinAgent : bboard::Agent
    {
        std::mt19937_64 rng;
        std::uniform_int_distribution<int> intDist;

        BerlinAgent();

        bboard::Move act(const bboard::State* state) override;

        float runAlreadyPlantedBombs(bboard::State * state);
        float runOneStep(const bboard::State * state, int depth);
        float scoreState(bboard::State * state);

        void PrintDetailedInfo();

        float bestPoint;
        int simulatedSteps = 0;
        bboard::FixedQueue<int, 5> myMoves;
        bboard::FixedQueue<int, 40> moves_in_chain;
        bboard::FixedQueue<int, 40> best_moves_in_chain;
        bboard::Position expectedPosInNewTurn;
        bool lastMoveWasBlocked = false;
        int lastBlockedMove = 0;
        unsigned int turns = 0;
        unsigned int totalSimulatedSteps = 0;

        bool _CheckPos2(const bboard::State* state, bboard::Position pos);
        float laterBetter(float reward, int timestaps);
        float soonerBetter(float reward, int timestaps);
    };
    struct CologneAgent : bboard::Agent
    {
        std::mt19937_64 rng;
        std::uniform_int_distribution<int> intDist;

        CologneAgent();

        bboard::Move act(const bboard::State* state) override;

        float runAlreadyPlantedBombs(bboard::State * state);
        float runOneStep(const bboard::State * state, int depth);
        float scoreState(bboard::State * state);

        void PrintDetailedInfo();

        float bestPoint;
        int simulatedSteps = 0;
        bboard::FixedQueue<int, 5> myMoves;
        bboard::FixedQueue<int, 40> moves_in_chain;
        bboard::FixedQueue<bboard::Position, 40> positions_in_chain;
        bboard::FixedQueue<int, 40> best_moves_in_chain;
        bboard::FixedQueue<float, 40> best_points_in_chain;
        bboard::Position expectedPosInNewTurn;
        bool lastMoveWasBlocked = false;
        int lastBlockedMove = 0;
        unsigned int turns = 0;
        unsigned int totalSimulatedSteps = 0;
        int seenAgents = 0;
        int enemyIteration1 = 0, enemyIteration2 = 0, teammateIteration = 0;
        int myMaxDepth = 0;
        std::array<bboard::FixedQueue<bboard::Position, 13>, 4 > previousPositions;


        bool _CheckPos2(const bboard::State* state, bboard::Position pos);
        bool _CheckPos2(const bboard::State* state, int x, int y);
        bool _CheckPos3(const bboard::State* state, int x, int y);
        void createDeadEndMap(const bboard::State* state);
        float laterBetter(float reward, int timestaps);
        float soonerBetter(float reward, int timestaps);

        float reward_first_step_idle = 0.001f;
        float reward_sooner_later_ratio = 0.98f;
        float reward_collectedPowerup = 0.5f;
        float reward_move_to_enemy = 100.0f;
        float reward_move_to_pickup = 1000.0f;

        std::set<uint128_t> visitedSteps;
        int ourId, teammateId, enemy1Id, enemy2Id;
        bool leadsToDeadEnd[bboard::BOARD_SIZE*bboard::BOARD_SIZE];
    };
// more agents to be included?



}

#endif
