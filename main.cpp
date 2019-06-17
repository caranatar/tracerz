#include "tracerz.h"

#define CATCH_CONFIG_MAIN

#include "catch.hpp"

TEST_CASE("Tree", "[tracerz]") {
  nlohmann::json oneSub = {
      {"rule", "output"}
  };
  tracerz::Grammar zgr(oneSub);
  auto tree = zgr.getTree("#rule#");
  REQUIRE(tree->getFirstLeaf() == tree->getRoot());
  REQUIRE(tree->getFirstUnexpandedLeaf() == tree->getRoot());
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
      {"capAllNumStartOrigin", "#numStart.capitalizeAll#"}
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