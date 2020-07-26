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

    // capacity of recent positions
    static const int rpCapacity = 4;
    bboard::FixedQueue<bboard::Position, rpCapacity> recentPositions;

    bboard::Move act(const bboard::State* state) override;

    void PrintDetailedInfo();
};

//#define DISPLAY_EXPECTATION
//#define DISPLAY_DEPTH0_POINTS
//#define DEBUGMODE_ON
#ifdef DEBUGMODE_ON
#define DEBUGMODE_STEPS //5% slower
//#define DEBUGMODE_COMMENTS //30% slower
#endif

#ifdef DEBUGMODE_ON
    struct StepResult
    {
        float point = -100.0f;
#ifdef DEBUGMODE_STEPS
        bboard::FixedQueue<int, 40> steps;
#endif
#ifdef DEBUGMODE_COMMENTS
        std::string comment;
#endif
        explicit operator float() { return point; }
    };
#else
    typedef float StepResult;
#endif

    struct FrankfurtAgent : bboard::Agent
    {
        FrankfurtAgent();

        bboard::Move act(const bboard::State* state) override;

        StepResult runAlreadyPlantedBombs(bboard::State * state);
        StepResult runOneStep(const bboard::State * state, int depth);
        StepResult scoreState(bboard::State * state);
        void PrintDetailedInfo();
        int simulatedSteps = 0;
        int message[2];

#ifndef DEBUGMODE_STEPS
        int depth_0_Move = 0;
#endif
        static bboard::FixedQueue<int, 40> moves_in_chain;
#pragma omp threadprivate(moves_in_chain)

        static bboard::FixedQueue<bboard::Position, 40> positions_in_chain;
#pragma omp threadprivate(positions_in_chain)

        bboard::Position expectedPosInNewTurn;
        bool lastMoveWasBlocked = false;
        int lastBlockedMove = 0;
        unsigned int turns = 0;
        unsigned int totalSimulatedSteps = 0;
        int seenAgents = 0, iteratedAgents = 0;
        int enemyIteration1 = 0, enemyIteration2 = 0, teammateIteration = 0, myMaxDepth = 0;
        std::array<bboard::FixedQueue<bboard::Position, 15>, 4 > previousPositions;
        bboard::FixedQueue<int, 15> moveHistory;
        bool rushing = false, goingAround = false;


        bool _CheckPos2(const bboard::State* state, bboard::Position pos, int agentId);
        bool _CheckPos2(const bboard::State* state, int x, int y, int agentId);
        void createDeadEndMap(const bboard::State* state);
        float laterBetter(float reward, int timestamps);
        float soonerBetter(float reward, int timestmaps);
        float reward_sooner_later_ratio_pow_timestamps[30];

        const float reward_first_step_idle = 0.001f;
        const float reward_sooner_later_ratio = 0.98f;
        const float reward_extraBombPowerupPoints = 0.6f;
        const float reward_extraRangePowerupPoints = 0.4f;
        const float reward_otherKickPowerupPoints = 0.2f;
        const float reward_firstKickPowerupPoints = 0.7f;
        const float reward_move_to_enemy = 100.0f;
        const float reward_move_to_pickup = 1000.0f;
        const float reward_woodDemolished = 0.40f;
        const float weight_of_average_Epoint = 0.1f;

        std::set<uint128_t> visitedSteps;
        int ourId, teammateId, enemy1Id, enemy2Id, lastSeenEnemy = 0;
        bool leadsToDeadEnd[bboard::BOARD_SIZE*bboard::BOARD_SIZE];
        bool sameAs6_12_turns_ago = true; // Indicates if the agent is stuck in a repeated situation
        std::chrono::high_resolution_clock::time_point start_time;
    };


    struct GottingenAgent : bboard::Agent
    {
        GottingenAgent();

        bboard::Move act(const bboard::State* state) override;

        StepResult runAlreadyPlantedBombs(bboard::State * state);
        StepResult runOneStep(const bboard::State * state, int depth);
        StepResult scoreState(bboard::State * state);
        void PrintDetailedInfo();
        int simulatedSteps = 0;
        int message[2];

#ifndef DEBUGMODE_STEPS
        int depth_0_Move = 0;
#endif
        static bboard::FixedQueue<int, 40> moves_in_chain;
#pragma omp threadprivate(moves_in_chain)

        static bboard::FixedQueue<bboard::Position, 40> positions_in_chain;
#pragma omp threadprivate(positions_in_chain)

        bboard::Position expectedPosInNewTurn;
        bool lastMoveWasBlocked = false;
        int lastBlockedMove = 0;
        unsigned int turns = 0;
        unsigned int totalSimulatedSteps = 0;
        int seenAgents = 0, iteratedAgents = 0;
        int enemyIteration1 = 0, enemyIteration2 = 0, teammateIteration = 0, myMaxDepth = 0;
        std::array<bboard::FixedQueue<bboard::Position, 15>, 4 > previousPositions;
        bboard::FixedQueue<int, 15> moveHistory;
        bool rushing = false, goingAround = false;


        bool _CheckPos2(const bboard::State* state, bboard::Position pos, int agentId);
        bool _CheckPos2(const bboard::State* state, int x, int y, int agentId);
        void createDeadEndMap(const bboard::State* state);
        float laterBetter(float reward, int timestamps);
        float soonerBetter(float reward, int timestmaps);
        float reward_sooner_later_ratio_pow_timestamps[30];

        const float reward_first_step_idle = 0.001f;
        const float reward_sooner_later_ratio = 0.98f;
        const float reward_extraBombPowerupPoints = 0.6f;
        const float reward_extraRangePowerupPoints = 0.4f;
        const float reward_otherKickPowerupPoints = 0.2f;
        const float reward_firstKickPowerupPoints = 0.7f;
        const float reward_move_to_enemy = 100.0f;
        const float reward_move_to_pickup = 1000.0f;
        const float reward_woodDemolished = 0.40f;
        const float weight_of_average_Epoint = 0.1f;

        std::set<uint128_t> visitedSteps;
        int ourId, teammateId, enemy1Id, enemy2Id, lastSeenEnemy = 0;
        bool leadsToDeadEnd[bboard::BOARD_SIZE*bboard::BOARD_SIZE];
        bool sameAs6_12_turns_ago = true; // Indicates if the agent is stuck in a repeated situation
        std::chrono::high_resolution_clock::time_point start_time;
    };
}

#endif
