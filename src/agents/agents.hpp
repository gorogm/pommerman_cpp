#ifndef RANDOM_AGENT_H
#define RANDOM_AGENT_H

#include <random>

#include "bboard.hpp"
#include "strategy.hpp"

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
        bboard::FixedQueue<int, 40> best_moves_in_chain;
        bboard::Position expectedPosInNewTurn;
        bool lastMoveWasBlocked = false;
        int lastBlockedMove = 0;
        unsigned int turns = 0;
        unsigned int totalSimulatedSteps = 0;
        int seenAgents = 0;
        int enemyIteration = 0;
        int myMaxDepth = 0;


        bool _CheckPos2(const bboard::State* state, bboard::Position pos);
        float laterBetter(float reward, int timestaps);
        float soonerBetter(float reward, int timestaps);
    };
// more agents to be included?



}

#endif
