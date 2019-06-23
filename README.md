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
    * [Expanding rules](#expanding-rules)
    * [Custom RNG](#custom-rng)
* [Future plans](#future-plans)

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

### Expanding rules
To create a new tree, expand all the nodes, and retrieve the flattened output string, simply call `flatten(input)` on
the grammar:

```cpp
grammar.flatten("output is #rule#"); // returns "output is output"
```

To get an unexpanded tree rooted with the input string instead, call `getTree(input)`:

```cpp
std::shared_ptr<tracerz::Tree> tree = grammar.getTree("output is #rule#");
```

To expand the tree one step, call `expand` on the tree. Note that the `Tree` object does not track modifiers or the RNG
in use, so these would have to be retrieved from the `Grammar` object in order to perform expansion as expected on the
`Tree`:

```cpp
tree->expand(grammar.getModifierFunctions(), grammar.getRNG());
```

This method returns true if there are still unexpanded nodes in the tree, so if you wish to expand all nodes, simply
call until it returns false:

```cpp
while(tree->expand(grammar.getModifierFunctions(), grammar.getRNG()));
```

Then, to retrieve the output from the tree, simply call `flatten` on it with the desired modifier function map:

```cpp
std::string output = tree->flatten(grammar.getModifierFunctions()); // returns "output is output"
```

### Custom RNG
By default, tracerz uses the C++ standard library's Mersenne Twister algorithm `std::mt19937` for random number
generation, seeded with the current time, and `std::uniform_int_distribution` to provide a uniform distribution across
options when selecting a single expansion from a rule defined as a list. If you want to use a standard library generator
,or any other generator which conforms to the C++ standard's definition of `UniformRandomBitGenerator`, with a different
seed, you can pass the RNG into the constructor, which will automatically deduce the type:

```cpp
// Using a standard random number generator
tracerz::Grammar grammar(inGrammar, std::mt19937(seed));

// TestRNG satifies the requirements of UniformRandomBitGenerator
tracerz::Grammar grammar(inGrammar, TestRNG(seed));
```

To also override the default distribution generator (`std::uniform_int_distribution`), you must specify its type when
constructing a `tracerz::Grammar` object:

```cpp
// Using a custom rng and distribution
tracerz::Grammar<TestRNG, TestDistribution> zgr(grammar, TestRNG(seed));
```

## Future plans
* Genericize json handling, in the same way the RNG characteristics are, to remove built-in dependency
* Support alternate distributions (weighted, etc...)
* Support modifiers that act on the tree itself