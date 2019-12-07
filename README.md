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
    * [Tree modifiers](#tree-modifiers)
    * [Tree node modifiers](#tree-node-modifiers)
    * [Adding modifiers](#adding-modifiers)
        * [Adding output modifiers](#adding-output-modifiers)
        * [Adding tree modifiers](#adding-tree-modifiers)
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

If you need to pop rules off rulestacks (if you do you'll know), it's sufficient for now to know the following:

* To do so you must import the base extended modifiers:
  ```cpp
  grammar.addModifiers(tracerz::getBaseExtendedModifiers());
  ```
  
* In tracerz, popping is done by applying the Tree modifier `pop!!` to the rule name:
  ```javascript
  // The following in tracerz:
  { "#popKey#", "[#key.pop!!#]" }

  // is equivalent to the following in tracery:
  { "#popKey#", "[key:POP]" }
  ```

To learn more about Tree modifiers like `pop!!`, see [Tree modifiers](#tree-modifiers)

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

### Tree modifiers
Tree modifiers are an extended feature of tracerz not present in the original tracery tool. These modifiers take as
input the tree they are working on as a `std::shared_ptr<Tree>`, as well as the rule name they are being applied to
(i.e., if a tree modifier `foo!!` is called as `#rule.foo!!#`, the underlying function will receive the pointer to the
tree as well as the string "rule"). By convention, tracerz uses two exclamation points at the end of tree modifiers to
visually disambiguate them, but this is not enforced in any way by the library.

### Tree node modifiers
Tree node modifiers are an extended feature of tracerz not present in the original tracery tool. These modifiers take as
input the tree node they are working on as a `std::shared_ptr<TreeNode>`, as well as the rule name they are being
applied to (i.e., if a tree node modifier `foo!` is called as `#rule.foo!#`, the underlying function will receive the
pointer to the tree node as well as the string "rule"). By convention, tracerz uses one exclamation point at the end of
tree node modifiers to visually disambiguate them, but this is not enforced in any way by the library.

### Adding modifiers
#### Adding output modifiers
Output modifiers act on the output of expanding a rule, they are the only type of modifier present in the original
tracery language. To create a new output modifier, create a `std::function` that takes at least one `std::string` as
input, and returns a `std::string`. To create a modifier that takes parameters when used in the language, simply add
additional `std::string` parameters to your modifier function.

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

In the language, these can be used to the same effect as `#rule.meow#` and `#rule.noise( meow!)#`. Note the leading space
in the second example. tracerz maintains whitespace in parameters; only commas separating parameters are removed.

#### Adding tree modifiers
See [Tree modifiers](#tree-modifiers) for details on the definition of tree modifiers. To create a tree modifier, create
a `std::function` that takes a `const std::shared_ptr<Tree>&`, representing the tree being acted on, and at least one
`const std::string&`, representing the rule name the modifier is being called on, and returns an empty `std::string`.
For example:

```cpp
std::shared_ptr<tracerz::Tree> tree = grammar.getExpandedTree("#rule.foo!!#");
std::function<std::string(const std::shared_ptr<tracerz::Tree>&,const std::string&)> foo = [](const std::shared_ptr<tracerz::Tree>& t, const std::string& r) {
  // t == tree
  // r == "rule"
  return "";
};
grammar.addModifier("foo!!", foo);
```

Tree modifiers can make any change to the Tree possible through its public API. This includes modifying the structure of
or runtime state of the tree. In tracerz, popping a ruleset off a rulestack is implemented as a Tree modifier (`pop!!`)
for this reason.

Tree modifiers can also be parameterized in the same way as output modifiers, by adding addition string parameters to
the function.

#### Adding tree node modifiers
See [Tree node modifiers](#tree-node-modifiers) for details on the definition of tree node modifiers. To create a tree
node modifier, create a `std::function` that takes a `const std::shared_ptr<TreeNode>&`, representing the tree node
being acted on, and at least one `const std::string&`, representing the rule name the modifier is being called on, and
returns an empty `std::string`.

Tree node modifiers can make any change to the TreeNode possible through its public API. tracerz does not make use of
this functionality at this time, and it is only being provided for completeness.

Tree node modifiers can also be parameterized in the same way as output modifiers, by adding addition string parameters
to the function.

### Step-by-step tree expansion
To get an unexpanded tree rooted with the input string, call `getTree(input)`:

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
of language concepts. Note that one breaking difference between the languages is that tracerz uses the `pop!!` modifier
to pop rulesets off of rulestacks. Where you would use `[key:POP]` in tracery, use `[#key.pop!!#]` in tracerz.

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
* Support modifiers that act on tree nodes
* Support expansion of modifier parameters
