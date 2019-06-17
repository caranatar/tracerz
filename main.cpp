#include "tracerz.h"

#define CATCH_CONFIG_MAIN

#include "catch.hpp"

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
      {"animal", "albatross"},
      {"animalX", "fox"},
      {"animalConsonantY", "guppy"},
      {"animalVowelY", "monkey"},
      {"food", "fish"},
      {"labor", "union"},
      {"vehicle", "car"},
      {"verbS", "pass"},
      {"verbE", "replace"},
      {"verbH", "cash"},
      {"verbX", "box"},
      {"verbConsonantY", "carry"},
      {"verbVowelY", "monkey"},
      {"verb", "hand"},
      {"anOrigin", "#animal.a# ate #food.a#"},
      {"anOrigin2", "the iww is #labor.a#"},
      {"capAllOrigin", "#anOrigin.capitalizeAll#"},
      {"capOrigin", "#anOrigin.capitalize#"},
      {"sOrigin", "#animal.s# eat #food.s#"},
      {"sOrigin2", "#animalX.s# eat #animalConsonantY.s# and #animalVowelY.s#"},
      {"sOrigin3", "people drive #vehicle.s#"},
      {"edOrigin", "#verbS.ed# #verbE.ed# #verbH.ed# #verbX.ed# #verbConsonantY.ed# #verbVowelY.ed# #verb.ed#"}
  };
  tracerz::Grammar zgr(mods);
  zgr.addModifiers(tracerz::Grammar::getBaseEngModifiers());
  REQUIRE(zgr.flatten("#anOrigin#") == "an albatross ate a fish");
  REQUIRE(zgr.flatten("#anOrigin2#") == "the iww is a union");
  REQUIRE(zgr.flatten("#capAllOrigin#") == "An Albatross Ate A Fish");
  REQUIRE(zgr.flatten("#capOrigin#") == "An albatross ate a fish");
  REQUIRE(zgr.flatten("#sOrigin#") == "albatrosses eat fishes");
  REQUIRE(zgr.flatten("#sOrigin2#") == "foxes eat guppies and monkeys");
  REQUIRE(zgr.flatten("#sOrigin3#") == "people drive cars");
  REQUIRE(zgr.flatten("#edOrigin#") == "passed replaced cashed boxed carried monkeyd handed");
}
