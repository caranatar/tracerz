[![Build Status](https://travis-ci.com/caranatar/tracerz.svg?branch=master)](https://travis-ci.com/caranatar/tracerz)
[![Coverage Status](https://coveralls.io/repos/github/caranatar/tracerz/badge.svg)](https://coveralls.io/github/caranatar/tracerz)
# tracerz
A modern C++ (C++17) procedural generation tool based on @galaxykate's tracery language

## Table of contents
* [About](#about)
* [Dependencies](#dependencies)
* [Basic usage](#basic-usage)
    * [Create a grammar](#create-a-grammar)
    * [Expanding rules](#expanding-rules)
* [Advanced usage](#advanced-usage)
    * [Custom RNG](#custom-rng)
        * [Type requirements](#type-requirements)
    * [Adding modifiers](#adding-modifiers)
    * [Step-by-step tree expansion](#step-by-step-tree-expansion)
* [Building API documentation](#building-api-docs)
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

To get a fully expanded tree rooted with the input string, call `getExpandedTree(input)`:
```cpp
std::shared_ptr<tracerz::Tree> tree = grammar.getExpandedTree("output is #rule#");
```

Then, to retrieve the output from the tree, simply call `flatten` on it with the desired modifier function map:

```cpp
std::string output = tree->flatten(grammar.getModifierFunctions()); // returns "output is output"
```

## Advanced usage
### Custom RNG
By default, tracerz uses the C++ standard library's Mersenne Twister algorithm `std::mt19937` for random number
generation, seeded with the current time, and `std::uniform_int_distribution` to provide a uniform distribution across
options when selecting a single expansion from a rule defined as a list. If you want to use a standard library
generator, or any other generator which conforms to the C++ standard's definition of `UniformRandomBitGenerator`, with
a different seed, you can pass the RNG into the constructor, which will automatically deduce the type:

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
tracerz::Grammar<TestRNG, TestDistribution> grammar(inGrammar, TestRNG(seed));
```

#### Type requirements
From the viewpoint of tracerz, there is no requirement on the RNG type. If you are using it with the default
distribution, then you must meet the standard's requirements of `UniformRandomBitGenerator`.

The uniform distribution type has two requirements:
* It must have a constructor taking two parameters *a* & *b*, restricting the output of the distribution to the range
[a, b].
* It must have an `operator()` which can take a parameter of the RNG type and generate a number in the requested range.

### Adding modifiers
To create a new modifier, create a `std::function` that takes at least one `std::string` as input, and returns a
`std::string`. To create a modifier that takes parameters when used in the language, simply add additional `std::string`
parameters to your modifier function.

```cpp
// Modifier with no parameters
std::function<std::string(const std::string&)> meow = [](const std::string& input) {
  return input + " meow!"
};
grammar.addModifier("meow", meow);

// Modifier that takes one parameter
std::function<std::string(const std::string&, const std::string&)> noise = [](const std::string& input,
                                                                              const std::string& param) {
  return input + noise;
};
grammar.addModifier("noise", noise);
```

In the language, these can be used to the same effect as `#rule.meow#` and `#rule.noise( meow!)`. Note the leading space
in the second example. tracerz maintains whitespace in parameters; only commas separating parameters are removed.

### Step-by-step tree expansion
To get an unexpanded tree rooted with the input string instead, call `getTree(input)`:

```cpp
std::shared_ptr<tracerz::Tree> tree = grammar.getTree("output is #rule#");
```

To expand the tree one step, call `expand` on the tree. Note that the `Tree` object does not track modifiers or the RNG
in use, so these have to be retrieved from the `Grammar` object in order to perform expansion as expected on the `Tree`.
In addition, it is necessary to provide template parameters to this method to give the type of the RNG and uniform
distribution:

```cpp
tree->template expand<decltype(grammar)::rng_t, decltype(grammar)::uniform_distribution_t>(grammar.getModifierFunctions(), grammar.getRNG());
```

This method returns true if there are still unexpanded nodes in the tree, so if you wish to expand all nodes, simply
call until it returns false. To get the flattened state of the tree at any step, call `flatten` as above.

## Library concepts
See @galaxykate's [tracery repo](https://github.com/galaxykate/tracery/tree/tracery2#library-concepts) for a description
of language concepts. One important note is that tracerz does not currently support the concept stacks for rulesets.
Only the input grammar's definitions and the latest runtime definition are tracked.

## Building API docs
To build the API docs, from the top level of the repo:

```
mkdir build
cd build
cmake ..
make doxygen
```

The docs will be generated under the directory docs in the build directory.

## Future plans
* Genericize json handling, in the same way the RNG characteristics are, to remove built-in dependency
* Support alternate distributions (weighted, etc...)
* Support modifiers that act on the tree itself
* Support expansion of modifier parameters