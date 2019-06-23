#include "tracerz.h"

#define CATCH_CONFIG_MAIN

#include "catch.hpp"

/**
 * Test class satisfying UniformRandomBitGenerator. Returns the value
 * it was constructed with
 */
 template <int MIN, int MAX>
class TestRNG {
public:
  typedef int result_type;

  TestRNG(const result_type& val, bool floppy = false)
      : value(val)
      , flippy(floppy) {
  }

  TestRNG(const TestRNG& t)
      : value(t.value)
      , flippy(t.flippy) {
  }

  static result_type min() {
    return MIN;
  }

  static result_type max() {
    return MAX;
  }

  result_type operator()() {
    if (this->flippy) {
      result_type ret = this->value;
      this->value = (this->value + 1) % 2;
      return ret;
    }
    return this->value;
  }

private:
  result_type value;
  bool flippy;
};

class TestDistribution {
public:
  typedef int result_type;

  TestDistribution(result_type min, result_type max) {
  }

  template<typename RNG>
  result_type operator()(RNG& rng) {
    return rng();
  }
};

TEST_CASE("Tree", "[tracerz]") {
  nlohmann::json oneSub = {
      {"rule", "output"}
  };
  tracerz::Grammar zgr(oneSub);
  auto tree = zgr.getTree("#rule#");
  REQUIRE(tree->getFirstLeaf() == tree->getRoot());
  REQUIRE(tree->getFirstUnexpandedLeaf() == tree->getRoot());
  while (tree->template expand<decltype(zgr)::rng_t, decltype(zgr)::uniform_distribution_t>(zgr.getModifierFunctions(),
      zgr.getRNG()));
  REQUIRE(tree->flatten(zgr.getModifierFunctions()) == "output");
  // Test flattening already expanded tree
  while (tree->template expand<decltype(zgr)::rng_t, decltype(zgr)::uniform_distribution_t>(zgr.getModifierFunctions(),
                                                                                            zgr.getRNG()));
  REQUIRE("abc" + tree->flatten(zgr.getModifierFunctions()) == "abcoutput");
}

TEST_CASE("TreeNode", "[tracerz]") {
  tracerz::Grammar zgr("{}"_json);
  REQUIRE(zgr.getTree("blah")->getRoot()->getLastExpandableChild() == nullptr);
}

TEST_CASE("Basic substitution", "[tracerz]") {
  nlohmann::json oneSub = {
      {"rule",   "output"},
      {"origin", "#rule#"}
  };
  tracerz::Grammar zgr(oneSub);
  REQUIRE(zgr.flatten("#origin#") == "output");
}

TEST_CASE("Nested substitution", "[tracerz]") {
  nlohmann::json nestedSub = {
      {"rule5",  "output"},
      {"rule4",  "#rule5#"},
      {"rule3",  "#rule4#"},
      {"rule2",  "#rule3#"},
      {"rule1",  "#rule2#"},
      {"origin", "#rule1#"}
  };
  tracerz::Grammar zgr(nestedSub);
  REQUIRE(zgr.flatten("#origin#") == "output");
}

TEST_CASE("Basic modifiers", "[tracerz]") {
  nlohmann::json mods = {
      {"animal",               "albatross"},
      {"animalX",              "fox"},
      {"animalConsonantY",     "guppy"},
      {"animalVowelY",         "monkey"},
      {"food",                 "fish"},
      {"labor",                "union"},
      {"vehicle",              "car"},
      {"verbS",                "pass"},
      {"verbE",                "replace"},
      {"verbH",                "cash"},
      {"verbX",                "box"},
      {"verbConsonantY",       "carry"},
      {"verbVowelY",           "monkey"},
      {"verb",                 "hand"},
      {"numStart",             "00flour from italy"},
      {"anOrigin",             "#animal.a# ate #food.a#"},
      {"anOrigin2",            "the iww is #labor.a#"},
      {"capAllOrigin",         "#anOrigin.capitalizeAll#"},
      {"capOrigin",            "#anOrigin.capitalize#"},
      {"sOrigin",              "#animal.s# eat #food.s#"},
      {"sOrigin2",             "#animalX.s# eat #animalConsonantY.s# and #animalVowelY.s#"},
      {"sOrigin3",             "people drive #vehicle.s#"},
      {"edOrigin",             "#verbS.ed# #verbE.ed# #verbH.ed# #verbX.ed# #verbConsonantY.ed# #verbVowelY.ed# #verb.ed#"},
      {"replaceOrigin",        "#anOrigin.replace(a,b)#"},
      {"capAllNumStartOrigin", "#numStart.capitalizeAll#"},
      {"chainedOrigin",        "#verbH.a.ed.capitalize# out"}
  };
  tracerz::Grammar zgr(mods);
  zgr.addModifiers(tracerz::getBaseEngModifiers());
  REQUIRE(zgr.flatten("#anOrigin#") == "an albatross ate a fish");
  REQUIRE(zgr.flatten("#anOrigin2#") == "the iww is a union");
  REQUIRE(zgr.flatten("#capAllOrigin#") == "An Albatross Ate A Fish");
  REQUIRE(zgr.flatten("#capOrigin#") == "An albatross ate a fish");
  REQUIRE(zgr.flatten("#sOrigin#") == "albatrosses eat fishes");
  REQUIRE(zgr.flatten("#sOrigin2#") == "foxes eat guppies and monkeys");
  REQUIRE(zgr.flatten("#sOrigin3#") == "people drive cars");
  REQUIRE(zgr.flatten("#edOrigin#") == "passed replaced cashed boxed carried monkeyd handed");
  REQUIRE(zgr.flatten("#replaceOrigin#") == "bn blbbtross bte b fish");
  REQUIRE(zgr.flatten("#capAllNumStartOrigin#") == "00flour From Italy");
  REQUIRE(zgr.flatten("#chainedOrigin#") == "A cashed out");

  // Test calling a string modifier with Tree input
  REQUIRE(zgr.getModifierFunctions()["a"]->callVec(nullptr, "rule", std::vector<std::string>()).empty());
}

TEST_CASE("Custom modifiers", "[tracerz]") {
  nlohmann::json grammar = {
      {"rule",   "output"},
      {"origin", "#rule#"}
  };
  tracerz::Grammar zgr(grammar);
  auto mods = zgr.getModifierFunctions();

  // Ensure there are no modifiers registered with the test name
  REQUIRE(mods.find("eris") == mods.end());

  SECTION("Custom non-parametric modifier") {
    std::function<std::string(const std::string&)> nonParametricMod = [](const std::string& input) -> std::string {
      return "hail eris";
    };
    zgr.addModifier("eris", nonParametricMod);
    REQUIRE(zgr.flatten("#rule.eris#") == "hail eris");

    // Test calling with no parentheses and no params
    REQUIRE(zgr.flatten("#rule.eris()#") == "hail eris");
  }

  SECTION("Custom single parameter modifier") {
    std::function<std::string(const std::string&, const std::string&)>
        oneParamMod = [](const std::string& input, const std::string& param) {
      return input + param;
    };
    zgr.addModifier("eris", oneParamMod);
    REQUIRE(zgr.flatten("#rule.eris(hail eris)#") == "outputhail eris");
  }

  SECTION("Custom multi-parameter modifier") {
    std::function<std::string(const std::string&,
                              const std::string&,
                              const std::string&,
                              const std::string&,
                              const std::string&)>
        multiParamMod = [](const std::string& input,
                           const std::string& pOne,
                           const std::string& pTwo,
                           const std::string& pThree,
                           const std::string& pFour) {
      if (input == pOne) return pFour;
      if (input == pTwo) return pThree;
      if (input == pThree) return pTwo;
      if (input == pFour) return input;
      return pOne;
    };
    zgr.addModifier("eris", multiParamMod);
    REQUIRE(zgr.flatten("#rule.eris(output,no2,no3,yes)#") == "yes");
    REQUIRE(zgr.flatten("#rule.eris(no1,output,yes,no4)#") == "yes");
    REQUIRE(zgr.flatten("#rule.eris(no1,yes,output,no4)#") == "yes");
    REQUIRE(zgr.flatten("#rule.eris(no1,no2,no3,output)#") == "output");
    REQUIRE(zgr.flatten("#rule.eris(yes,no2,no3,no4)#") == "yes");
  }
}

TEST_CASE("Tree modifiers", "[tracerz]") {
  nlohmann::json grammar = {
      {"popSubject", "[#subject.pop!!#]"},
      {"animal",     "dog"},
      {"object",     "door"},
      {"noise",      "#subject# made a noise"},
      {"story2",     "#noise##popSubject#"},
      {"story",      "#[subject:#animal#]subject# opened the #[subject:#object#]subject#. #story2#. #story2#"}
  };
  tracerz::Grammar zgr(grammar);
  zgr.addModifiers(tracerz::getBaseEngModifiers());
  zgr.addModifiers(tracerz::getBaseExtendedModifiers());
  REQUIRE(zgr.flatten("#story#") == "dog opened the door. door made a noise. dog made a noise");

  // Test calling a tree modifier with string input
  REQUIRE(zgr.getModifierFunctions()["pop!!"]->callVec("input", std::vector<std::string>()).empty());
}

TEST_CASE("Basic actions", "[tracerz]") {
  nlohmann::json actions = {
      {"getKey",           "key is #key#"},
      {"getKey2",          "#key2# is key2"},
      {"animal",           "seagull"},
      {"fun",              "[key:whale][key2:dolphin]"},
      {"dll",              "#animal.s# "},
      {"dlr",              "are neat"},
      {"drl",              ". just kidding. "},
      {"drr",              "#animal.s# are annoying"},
      {"dl",               "#dll##dlr#"},
      {"dr",               "#drl##drr#"},
      {"deep",             "#dl##dr#"},
      {"textGetKeyOrigin", "#[key:blurf]getKey#"},
      {"ruleGetKeyOrigin", "#[key:#animal#]getKey#"},
      {"funOrigin",        "#[#fun#]getKey# #getKey2#"},
      {"deepOrigin",       "#[key:#deep#]getKey#"}
  };
  tracerz::Grammar zgr(actions);
  REQUIRE(zgr.flatten("#[key:testkey]getKey#") == "key is testkey");
  REQUIRE(zgr.flatten("#textGetKeyOrigin#") == "key is blurf");
  REQUIRE(zgr.flatten("#ruleGetKeyOrigin#") == "key is seagull");
  REQUIRE(zgr.flatten("#funOrigin#") == "key is whale dolphin is key2");
  zgr.addModifiers(tracerz::getBaseEngModifiers());
  REQUIRE(zgr.flatten("#deepOrigin#") == "key is seagulls are neat. just kidding. seagulls are annoying");
}

TEST_CASE("Custom rng", "[tracerz]") {
  nlohmann::json grammar = {
      {"rule", {"one",   "two"}},
      {"dll",  "one"},
      {"dlr",  "two"},
      {"drl",  "three"},
      {"drr",  "four"},
      {"dl",   {"#dll#", "#dlr#"}},
      {"dr",   {"#drl#", "#drr#"}},
      {"deep", {"#dl#",  "#dr#"}}
  };

  SECTION("RNG specialization only") {
    tracerz::Grammar zgr(grammar, TestRNG<0,1>(0));
    REQUIRE(zgr.flatten("#rule#") == "one");
  }

  SECTION("Left selection") {
    tracerz::Grammar<TestRNG<0,1>, TestDistribution> zgr(grammar, TestRNG<0,1>(0));
    REQUIRE(zgr.flatten("#rule#") == "one");
    REQUIRE(zgr.flatten("#deep#") == "one");
  }

  SECTION("Right selection") {
    tracerz::Grammar<TestRNG<0,1>, TestDistribution> zgr(grammar, TestRNG<0,1>(1));
    REQUIRE(zgr.flatten("#rule#") == "two");
    REQUIRE(zgr.flatten("#deep#") == "four");
  }

  SECTION("Left alternating selection") {
    tracerz::Grammar<TestRNG<0,1>, TestDistribution> zgr(grammar, TestRNG<0,1>(0, true));
    REQUIRE(zgr.flatten("#deep#") == "two");
  }

  SECTION("Right alternating selection") {
    tracerz::Grammar<TestRNG<0,1>, TestDistribution> zgr(grammar, TestRNG<0,1>(1, true));
    REQUIRE(zgr.flatten("#deep#") == "three");
  }
}

TEST_CASE("Complex grammar", "[tracerz]") {
  nlohmann::json grammar = {
    {"name", {"Arjun", "Yuuma", "Darcy", "Mia", "Chiaki", "Izzi", "Azra", "Lina"}},
    {"animal", {"unicorn", "raven", "sparrow", "scorpion", "coyote", "eagle", "owl", "lizard", "zebra", "duck",
                "kitten"}},
    {"occupationBase", {"wizard", "witch", "detective", "ballerina", "criminal", "pirate", "lumberjack", "spy",
                        "doctor", "scientist", "captain", "priest"}},
    {"occupationMod", {"occult ", "space ", "professional ", "gentleman ", "erotic ", "time ", "cyber", "paleo",
                       "techno", "super"}},
    {"strange", {"mysterious", "portentous", "enchanting", "strange", "eerie"}},
    {"tale", {"story", "saga", "tale", "legend"}},
    {"occupation", {"#occupationMod##occupationBase#"}},
    {"mood", {"vexed", "indignant", "impassioned", "wistful", "astute", "courteous"}},
    {"setPronouns", {"[heroThey:they][heroThem:them][heroTheir:their][heroTheirs:theirs]",
                     "[heroThey:she][heroThem:her][heroTheir:her][heroTheirs:hers]",
                     "[heroThey:he][heroThem:him][heroTheir:his][heroTheirs:his]"}},
    {"setSailForAdventure", {"set sail for adventure", "left #heroTheir# home", "set out for adventure",
                             "went to seek #heroTheir# forture"}},
    {"setCharacter", {"[#setPronouns#][hero:#name#][heroJob:#occupation#]"}},
    {"openBook", {"An old #occupation# told #hero# a story. 'Listen well' she said to #hero#, 'to this #strange# #tale#. ' #origin#'",
                  "#hero# went home.",
                  "#hero# found an ancient book and opened it.  As #hero# read, the book told #strange.a# #tale#: #origin#"}},
    {"story", {"#hero# the #heroJob# #setSailForAdventure#. #openBook#"}},
    {"origin", {"Once upon a time, #[#setCharacter#]story#"}}
    };
    tracerz::Grammar zgr(grammar);
    zgr.addModifiers(tracerz::getBaseEngModifiers());
    REQUIRE_NOTHROW(zgr.flatten("#origin#"));
  }