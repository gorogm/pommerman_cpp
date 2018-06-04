#include <iostream>
#include <random>

#include "bboard.hpp"

namespace bboard
{


State* InitState(int a0, int a1, int a2, int a3)
{
    State* result = new State();
    int b = Item::AGENT0; // agent no. offset

    // Randomly put obstacles
    std::mt19937_64 rng(0x1337);
    std::uniform_int_distribution<int> intDist(0,2);
    for(int i = 0; i < BOARD_SIZE; i++)
    {
        for(int  j = 0; j < BOARD_SIZE; j++)
        {
            result->board[i][j] = intDist(rng);
        }
    }

    // Put agents
    result->board[0][0] = b + a0;
    result->board[0][BOARD_SIZE - 1] = b + a1;
    result->board[BOARD_SIZE - 1][BOARD_SIZE - 1] = b + a2;
    result->board[BOARD_SIZE - 1][0] = b + a3;

    return result;
}

void Step(State* state, Move* moves)
{
    //TODO: calculate step transition
}

void PrintState(State* state)
{
    std::string result = "";

    for(int i = 0; i < BOARD_SIZE; i++)
    {
        for(int j = 0; j < BOARD_SIZE; j++)
        {
            int item = state->board[i][j];
            result += PrintItem(item);
            if(j == BOARD_SIZE - 1)
            {
                result += "\n";
            }
        }
    }
    std::cout << result;
}

std::string PrintItem(int item)
{
    switch(item)
    {
        case Item::PASSAGE:
            return "   ";
        case Item::RIGID:
            return "[X]";
        case Item::WOOD:
            return "[\u25A0]";
        case Item::BOMB:
            return " \u2B24 ";
    }
    //agent number
    if(item >= Item::AGENT0)
    {
        return " "  +  std::to_string(item - 10) + " ";
    }
    else
    {
        return std::to_string(item);
    }
}

}

