# The Pommerman 2019 Winner Agent

This repository contans the winner entry for the Pommerman competition of 2019.

## Contributors

I've built my agent using _ython's C++ implementation: https://github.com/m2q/pomcpp
The repository was forked from his, see README_ython.md as well!
The agent's actual decision logic and search method was developed by Márton Görög.

## Prerequisites

The code compiles under Windows, but it was mostly developed under Linux using:
- Ubuntu 18.04
- gcc 8 with openmp
- make, cmake

## Setup

#### Download

To fully clone the repository use
```
$ git clone --recurse-submodules https://github.com/gorogm/pommerman_cpp
```

#### Compilation
Under Linux CLion is a recommended environment, which will be able to use the CMakeLists.txt included.
Alternatively, you can build from command line:
```
$ cmake .
$ make
```

Under Windows Visual Studio is recommended.

For unit tests see _ython's README_ython.md

#### Running
```
$ python3 playground/frankfurt_vs_gottingen.py
```

Uncomment "env.render()" to see the UI.

## Project Structure

All of the main source code is in `src/*` and all testing code is in `unit_test/*`. The source is divided into modules

```
-BayesianOptimization (out of use)
-docker (helpers to create docker image)
-playground (this is the python section)
-src
 |-agents (the agent logic, mostly by Márton Görög)
 |-bboard (the C++ engine, mostly by _ython)
-unit_test (game engine testing code)
```

All environment specific functions (forward, board init, board masking etc) reside in `bboard`. Agents can be declared
in the `agents` header and implemented in the same module.

All test cases will be in the module `unit_test`. The bboard should be tested thoroughly so it exactly matches the specified behaviour of Pommerman. The compiled `test` binary can be found in `/bin`

## Existing Search-based Agents

* frankfurt_agent: this was the winner of Pommerman 2019. This file will be preserved as it is.
* gottingen_agent: forked from frankfurt - currently they are same, but gottingen includes debug features. Try to develop this and compete agains frankfurt.

Most of your enchancements could probably be in gottingen_agent.cpp. 

## Defining New Agents

Adding a new agent requires modification in a lot of files, mostly due to python-c++ interaction, so for a first step I suggest using the ready-made gottingen_agent. To add a new agent:
* In src/agents copy gottingen or frankfurt to a new agent. I used German city names in alphabetic order (there was once Berlin, Cologne, Dortmund, Eisenach 2018) as a tribute to _ython's work, who lives in Germany.
* Copy class in agents.hpp
* Copy python-C-C++ bridge functions in bboard.cpp
* Copy python agent in playground/pommerman/agents
* Probably useful to copy starter scripts in playground

## Testing

See README_ython.md

## Citing This Work

Please cite the corresponding report from arxiv.