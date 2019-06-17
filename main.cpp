#include "tracerz.h"

#define CATCH_CONFIG_MAIN

#include "catch.hpp"

TEST_CASE("TreeNode", "[tracerz]") {
  tracerz::Grammar zgr("{}"_json);
  REQUIRE(zgr.getTree("blah")->getRoot()->getLastExpandableChild() == nullptr);
}

TEST_CASE("Basic substitution", "[tracerz]") {
  nlohmann::json oneSub = {
      {"rule", "output"},
      {"origin", "#rule#"}
  };
  tracerz::Grammar zgr(oneSub);
  REQUIRE(zgr.flatten("#origin#") == "output");
}

TEST_CASE("Nested substitution", "[tracerz]") {
  nlohmann::json nestedSub = {
      {"rule5", "output"},
      {"rule4", "#rule5#"},
      {"rule3", "#rule4#"},
      {"rule2", "#rule3#"},
      {"rule1", "#rule2#"},
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