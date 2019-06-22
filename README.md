[![Build Status](https://travis-ci.com/caranatar/tracerz.svg?branch=master)](https://travis-ci.com/caranatar/tracerz)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/5d3454d25fcd4b33a173af36b2ee8b6a)](https://app.codacy.com/app/caranatar/tracerz?utm_source=github.com&utm_medium=referral&utm_content=caranatar/tracerz&utm_campaign=Badge_Grade_Dashboard)
[![Coverage Status](https://coveralls.io/repos/github/caranatar/tracerz/badge.svg)](https://coveralls.io/github/caranatar/tracerz)
# tracerz
A modern C++ (C++17) procedural generation tool based on @galaxykate's tracery language

## Table of contents
* [About](#about)
* [Dependencies](#dependencies)
* [Basic usage](#basic-usage)
    * [Create a grammar](#create-a-grammar)

## About
tracerz is a single-header-file procedural generation tool heavily based on @galaxykate's javascript tool tracery. To
avoid confusion with an already existing, but abandoned, C++ implementation of tracery, I incremented the last letter
instead (:

## Dependencies
tracerz uses [JSON for Modern C++](https://github.com/nlohmann/json/) for its json handling. It expects the single
header file version of that library to be in the include path at "json.hpp"

tracerz's test suite uses [Catch2](https://github.com/catchorg/Catch2), but this library is only required for
development on tracerz itself.

## Basic usage
### Create a grammar
Place the tracerz.h header file somewhere accessible in your include path, along with the JSON for Modern C++ header,
then include it:

```cpp
#include "tracerz.h"
```

Create your input grammar ([see JSON for Modern C++ for details](https://github.com/nlohmann/json/))

```cpp
nlohmann::json inGrammar = {
  {"rule", "output"}
};
```

Create the `tracerz::Grammar` object:

```cpp
tracerz::Grammar grammar(inGrammar);
```

To add the base English modifiers supported by tracery, add the following after creating the grammar:

```cpp
grammar.addModifiers(tracerz::getBaseEngModifiers());
```

To create a new tree, expand all the nodes, and retrieve the flattened output string, simply call `flatten(input)` on
the grammar:

```cpp
grammar.flatten("output is #rule#"); // returns "output is output"
```

## Future plans
* Genericize json handling, in the same way the RNG characteristics are, to remove built-in dependency
* Support alternate distributions (weighted, etc...)
* Support modifiers that act on the tree itself