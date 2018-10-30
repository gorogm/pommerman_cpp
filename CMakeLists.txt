cmake_minimum_required(VERSION 2.8)

project(munchen)

add_library(munchen SHARED src/agents/basic_agents.cpp src/agents/berlin_agent.cpp src/agents/cologne_agent.cpp src/agents/simple_agent.cpp src/agents/uint128_t.cpp src/bboard/bboard.cpp src/bboard/environment.cpp src/bboard/step.cpp src/bboard/step_utility.cpp src/bboard/strategy.cpp)

message( STATUS ${CMAKE_SOURCE_DIR} )
include_directories(${CMAKE_SOURCE_DIR}/src/agents ${CMAKE_SOURCE_DIR}/src/bboard)


#install(TARGETS test DESTINATION lib)
#install(FILES test.h DESTINATION include)
