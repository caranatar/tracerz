#pragma once

/**
 * @file tracerz.h: a single-header, modern C++ port/extension of @galaxykate's tracery tool
 */

#include <cctype>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <regex>
#include <stack>
#include <string>
#include <vector>

#include "json.hpp"

namespace tracerz {
namespace details {
/**
 * This interface represents a generic modifier function that takes an input string and 0 or more string params, and
 * returns a string.
 */
class IModifierFn {
public:
  /**
   * Calls the modifier function with the given info
   *
   * @param input the input string to modify
   * @param params the addition params to pass into the function
   * @return the modified input string
   */
  virtual std::string callVec(const std::string& input, const std::vector<std::string>& params) = 0;

  /**
   * Default virtual destructor.
   */
  virtual ~IModifierFn() = default;
};

/**
 * Given a function that accepts a string input and at least one additional parameter, the static function of this class
 * recursively unpacks a vector into a parameter pack, and then calls the given function on the input string and
 * the parameter pack.
 *
 * @tparam N the number of items remaining in the vector
 * @tparam F the type of the function to call on the unpacked parameters
 * @tparam PTs a parameter pack of the types of the items already unpacked from the vector
 */
template<int N,
    typename F,
    typename... PTs>
class CallVector_R {
public:
  /**
   * Unpacks the first parameter from the parameter vector, moves it to the parameter pack, and then recurses
   *
   * @param fun the function to ultimately call on the input and list of parameters
   * @param input the input string to pass to `fun`
   * @param paramVec the remaining parameters
   * @param params the parameter pack of parameters that have been unpacked from the vector so far
   * @return the result of calling `fun` on the input string with all parameters
   */
  static decltype(auto) callVec(F fun,
                                const std::string& input,
                                std::vector<std::string> paramVec,
                                PTs... params) {
    // Unpack the first parameter from the vector
    std::string param = paramVec[0];

    // Create a vector containing the rest of the parameters
    std::vector<std::string> restOfVec(++paramVec.begin(), paramVec.end());

    // Recurse, placing `param` at the end of the parameter pack
    return CallVector_R<N - 1, F, const std::string&, PTs...>::callVec(fun,
                                                                       input,
                                                                       restOfVec,
                                                                       params...,
                                                                       param);
  }
};

/**
 * The base case of CallVector_R, in which all parameters have been moved from the parameter vector into the parameter
 * pack, and which calls the supplied function on the input string and parameter pack.
 *
 * @tparam F the type of the function to be called
 * @tparam PTs a parameter pack of the types of the parameters to be passed, with the input string, into the function
 */
template<typename F,
    typename... PTs>
class CallVector_R<0, F, PTs...> {
public:
  /**
   * Calls the given function on the given input string and parameter pack
   *
   * @param fun the function to call
   * @param input the input string
   * @param params the parameter pack of the remaining parameters
   * @return the result of calling `fun` on `params...`
   */
  static decltype(auto) callVec(F fun,
                                const std::string& input,
                                const std::vector<std::string>&/*unused*/,
                                PTs... params) {
    return fun(input, params...);
  }
};

/**
 * This class provides a static function to call a given function with the given input string and the contents of a
 * given parameter vector as parameters to the function
 *
 * @tparam N the number of parameters F takes, excluding the input string
 * @tparam F the type of the function to be called on the supplied parameters
 */
template<int N, typename F>
class CallVector {
public:
  /**
   * Calls function `fun` with `input` and the contents of `params` as parameters
   *
   * @param fun the function to call
   * @param input the input string
   * @param params the remaining parameters
   * @return the result of calling `fun(input, params...)`
   */
  static decltype(auto) callVec(F fun,
                                const std::string& input,
                                const std::vector<std::string>& params) {
    // Peel off the first parameter
    std::string param = params[0];

    // Create a vector of the remaining parameters
    std::vector<std::string> restOfParams(++params.begin(), params.end());

    // Use CallVector_R to recurse over the remaining parameters
    return CallVector_R<N - 1, F, const std::string&>::callVec(fun,
                                                               input,
                                                               restOfParams,
                                                               param);
  }
};

/**
 * Special case of CallVector for functions that only take a single input string
 *
 * @tparam F the type of the supplied function
 */
template<typename F>
class CallVector<0, F> {
public:
  /**
   * Calls `fun` on `input`
   *
   * @param fun the function to call
   * @param input the input string
   * @return the result of `fun(input)`
   */
  static decltype(auto) callVec(F fun,
                                const std::string& input,
                                const std::vector<std::string>& /*unused*/) {
    return fun(input);
  }
};

/**
 * Encapsulates a modifier function taking one or more string inputs and returning a string. Provides two calling
 * methods: a parameter pack of type Ts or a vector of strings with size equal to the number of parameters taken by the
 * function.
 *
 * **Note**: there is no validation on the number of parameters in the vector.
 *
 * @tparam Ts parameter pack of the types of the parameters to the function
 */
template<typename... Ts>
class ModifierFn : public IModifierFn {
public:
  /**
   * Construct a ModifierFn for the given modifier function
   * @param fun the modifier function
   */
  explicit ModifierFn(std::function<std::string(Ts...)> fun)
      : callback(std::move(fun)) {
  }

  /**
   * Default virtual copy constructor.
   */
  ~ModifierFn() override = default;

  /**
   * Calls the encapsulated function with the given parameter pack
   *
   * @tparam PTs the parameter pack of param types
   * @param params the parameters to pass to the function
   * @return the result of calling the function on the parameters
   */
  template<typename... PTs>
  std::string call(PTs... params) {
    return callf(this->callback, params...);
  }

  /**
   * Calls the encapsulated function with the given intut string and the given vector of params, unpacked to type `PTs...`
   *
   * **NOTE**: there is no checking on the size of vector
   *
   * @param input the input string to the modifier
   * @param params the vector of parameters to pass to the encapsulated function
   * @return the result of calling the function on the parameters
   */
  std::string callVec(const std::string& input, const std::vector<std::string>& params) override {
    return CallVector<sizeof...(Ts) - 1, decltype(this->callback)>::callVec(this->callback, input, params);
  }

private:
  /** The encapsulated function */
  std::function<std::string(Ts...)> callback;
};

/** Represents a mapping of modifier names to modifier functions */
typedef std::map<std::string, std::shared_ptr<IModifierFn>> callback_map_t;

/**
 * Returns the action regular expression
 *
 * An action is defined as follows:
 *   An open bracket: [
 *   Followed by zero or more non-close bracket characters
 *   A close bracket: ]
 *
 * It has one capture group: the entire contents of the bracketed characters
 *
 * @return the regex
 */
const std::regex& getActionRegex() {
  static const std::regex rgx(R"(\[([^\]]*)\])");
  return rgx;
}

/**
 * Gets a comma regex (to make splitting on commas into a container with iterators easy)
 *
 * It's literally just a comma
 *
 * It contains no capture group.
 *
 * @return the regex
 */
const std::regex& getCommaRegex() {
  static const std::regex rgx(",");
  return rgx;
}

/**
 * Gets a basic modifier regex.
 *
 * It consists of the following:
 *   A dot: .
 *   Followed by one or more not dots
 *
 * It has one capture group: the non-dot matching characters
 *
 * @return the regex
 */
const std::regex& getModifierRegex() {
  static const std::regex rgx(R"(\.([^\.]+))");
  return rgx;
}

/**
 * Gets a regex that will match iff the input contains only tracery action groups
 *
 * It consists of the following:
 *   The beginning of a line
 *   One or more of the non-capturing group:
 *     An opening bracket: [
 *     Zero or more non-close brackets
 *     A closing bracket: ]
 *   The end of a line
 *
 * It contains no capture group.
 *
 * @return the regex
 */
const std::regex& getOnlyActionsRegex() {
  // (?: ... ) is a non-capturing group
  static const std::regex rgx(R"(^(?:\[[^\]]*\])+$)");
  return rgx;
}

/**
 * Gets a regex that will match iff the input contains only an action that sets a key to plain text
 *
 * It consists of the following:
 *   The beginning of a line
 *   An opening bracket: [
 *   One or more alpha numeric characters (representing a key name)
 *   A colon: :
 *   One or more non-# and non-] characters
 *   A closing bracket: ]
 *   The end of a line
 *
 * It contains two capture groups:
 *   The key name (the alphanumeric characters before the colon)
 *   The plain text value (the characters between the colon and the closing bracket)
 *
 * @return the regex
 */
const std::regex& getOnlyKeyWithTextActionRegex() {
  static const std::regex rgx(R"(^\[([[:alnum:]]+):([^#\]]+)\]$)");
  return rgx;
}

/**
 * Gets a regex that will match iff the input contains only an action group that sets a key to the result of a rule with
 * zero or more modifiers
 *
 * It consists of the following:
 *   The beginning of a line
 *   An opening bracket: [
 *   One or more alphanumeric characters, representing a key name
 *   A colon: :
 *   An octothorpe: #
 *   One or more alphanumeric characters, representing a rule name
 *   Zero or more of the non-capturing group, each representing a modifier:
 *     A dot: .
 *     One or more non-#, non-. characters
 *   An octothorpe: #
 *   A closing bracket: ]
 *   The end of a line
 *
 * It contains two capture groups:
 *   The key name
 *   The rule and optional modifiers, including surrounding octothorpes
 *
 * @return the regex
 */
const std::regex& getOnlyKeyWithRuleActionRegex() {
  static const std::regex rgx(R"(^\[([[:alnum:]]+):(#[[:alnum:]]+(?:\.[^.#]+)*#)\]$)");
  return rgx;
}

/**
 * Gets a regex that will match iff the input contains only an action group that does not set a key. These action groups
 * contain one rule, possibly with modifiers. Usually these rules create keys themselves, acting as a kind of
 * function within the language.
 *
 * It consists of the following:
 *   The beginning of a line
 *   An opening bracket: [
 *   An octothorpe: #
 *   One or more alphanumeric characters, representing a rule name
 *   Zero or more of the non-capturing group, each representing a modifier:
 *     A dot: .
 *     One or more non-#, non-. characters
 *   An octothorpe: #
 *   A closing bracket: ]
 *   The end of a line
 *
 * It contains one capture groups: the rule and optional modifiers, including surrounding octothorpes
 *
 * @return the regex
 */
const std::regex& getOnlyKeylessRuleActionRegex() {
  static const std::regex rgx(R"(^\[(#[[:alnum:]]+(?:\.[^.#]+)*#)\]$)");
  return rgx;
}

/**
 * Gets a regex that will match iff the input contains only a rule with an optional set of modifiers
 *
 * It consists of the following:
 *   The beginning of a line
 *   An octothorpe: #
 *   One or more alphanumeric characters, representing a rule name
 *   Zero or more of the non-capturing group, each representing a modifier:
 *     A dot: .
 *     One or more non-#, non-. characters
 *   An octothorpe: #
 *   The end of a line
 *
 * It contains two capture groups:
 *   The rule name
 *   The string of all characters representing zero or more modifiers (possibly empty)
 *
 * @return the regex
 */
const std::regex& getOnlyRuleRegex() {
  static const std::regex rgx(R"(^#([[:alnum:]]+)((?:\.[^.#]+)*)#$)");
  return rgx;
}

/**
 * Gets a regex that will match iff the input contains only a rule with at least one action and an optional set of
 * modifiers
 *
 * It consists of the following:
 *   The beginning of a line
 *   An octothorpe: #
 *   One or more of the non-capturing group, each representing an action group:
 *     An opening bracket: [
 *     Zero or more characters
 *     A closing bracket: ]
 *   One or more alphanumeric characters, representing a rule name
 *   Zero or more of the non-capturing group, each representing a modifier:
 *     A dot: .
 *     One or more non-#, non-. characters
 *   An octothorpe: #
 *   The end of a line
 *
 * It contains three capture groups:
 *   The string of all characters representing the one or more action groups
 *   The rule name
 *   The string of all characters representing zero or more modifiers (possibly empty)
 *
 * @return the regex
 */
const std::regex& getOnlyRuleWithActionsRegex() {
  static const std::regex rgx(R"(^#((?:\[.*\])+)([[:alnum:]]+)((?:\.[^.#]+)*)#$)");
  return rgx;
}

/**
 * Gets a regex that will match if the input contains a rule with an optional action and an optional set of modifiers.
 *
 * It consists of the following:
 *   An octothorpe: #
 *   Zero or more of the non-capturing group, each representing an action group:
 *     An opening bracket: [
 *     Zero or more characters
 *     A closing bracket: ]
 *   One or more alphanumeric characters, representing a rule name
 *   Zero or more of the non-capturing group, each representing a modifier:
 *     A dot: .
 *     One or more non-#, non-. characters
 *   An octothorpe: #
 *
 * It contains three capture groups:
 *   The string of all characters representing the one or more action groups
 *   The rule name
 *   The string of all characters representing zero or more modifiers (possibly empty)
 *
 * @return the regex
 */
const std::regex& getRuleRegex() {
  static const std::regex rgx(R"(#(?:\[.*\])*([[:alnum:]]+)((?:\.[^.#]+)*)#)");
  return rgx;
}

/**
 * Gets a regex that will match if the string contains
 *
 * It consists of the following:
 *   One or more non-opening parentheses characters
 *   An open parentheses: (
 *   Zero or more non-closing parentheses
 *   A close parentheses: )
 *
 * It contains two capture groups:
 *   The characters before the open parentheses, representing the modifier name
 *   The characters between the parentheses, representing the parameter list
 *
 * @return the regex
 */
const std::regex& getParametricModifierRegex() {
  static const std::regex rgx(R"(([^\(]+)\(([^\)]*)\))");
  return rgx;
}

/**
 * True if the input contains a match for the regex returned by @see tracerz::details::getRuleRegex()
 *
 * @param input the input string to test
 * @return true if the input contains a match
 */
bool containsRule(const std::string& input) {
  return std::regex_search(input, details::getRuleRegex());
}

/**
 * True if the input contains a match for the regex returned by @see tracerz::details::getOnlyActionsRegex()
 *
 * @param input the input string to test
 * @return true if the input contains a match
 */
bool containsOnlyActions(const std::string& input) {
  return std::regex_match(input, details::getOnlyActionsRegex());
}

/**
 * True if the input contains a match for the regex returned by @see tracerz::details::getOnlyKeylessRuleActionRegex()
 *
 * @param input the input string to test
 * @return true if the input contains a match
 */
bool containsOnlyKeylessRuleAction(const std::string& input) {
  return std::regex_match(input, details::getOnlyKeylessRuleActionRegex());
}

/**
 * True if the input contains a match for the regex returned by @see tracerz::details::getOnlyKeyWithTextActionRegex()
 *
 * @param input the input string to test
 * @return true if the input contains a match
 */
bool containsOnlyKeyWithTextAction(const std::string& input) {
  return std::regex_match(input, details::getOnlyKeyWithTextActionRegex());
}

/**
 * True if the input contains a match for the regex returned by @see tracerz::details::getOnlyKeyWithRuleActionRegex()
 *
 * @param input the input string to test
 * @return true if the input contains a match
 */
bool containsOnlyKeyWithRuleAction(const std::string& input) {
  return std::regex_match(input, details::getOnlyKeyWithRuleActionRegex());
}

/**
 * True if the input contains a match for the regex returned by @see tracerz::details::getOnlyRuleRegex()
 *
 * @param input the input string to test
 * @return true if the input contains a match
 */
bool containsOnlyRule(const std::string& input) {
  return std::regex_match(input, details::getOnlyRuleRegex());
}

/**
 * True if the input contains a match for the regex returned by @see tracerz::details::getOnlyRuleWithActionsRegex()
 *
 * @param input the input string to test
 * @return true if the input contains a match
 */
bool containsOnlyRuleWithActions(const std::string& input) {
  return std::regex_match(input, details::getOnlyRuleWithActionsRegex());
}

/**
 * True if the input contains a match for the regex returned by @see tracerz::details::getOnlyParametricModifierRegex()
 *
 * @param input the input string to test
 * @return true if the input contains a match
 */
bool containsParametricModifier(const std::string& input) {
  return std::regex_match(input, details::getParametricModifierRegex());
}
} // End namespace details

class TreeNode {
public:
  TreeNode()
      : input()
      , isNodeComplete_(false)
      , isNodeExpanded_(false)
      , prevLeaf(nullptr)
      , nextLeaf(nullptr)
      , prevUnexpandedLeaf(nullptr)
      , nextUnexpandedLeaf(nullptr)
      , isNodeHidden_(false) {
  }

  explicit TreeNode(const std::string& input,
                    std::shared_ptr<TreeNode> prev = nullptr,
                    std::shared_ptr<TreeNode> next = nullptr,
                    std::shared_ptr<TreeNode> prevUnexpanded = nullptr,
                    std::shared_ptr<TreeNode> nextUnexpanded = nullptr)
      : input(input)
      , isNodeComplete_(!details::containsRule(input)
                        && !details::containsOnlyActions(input))
      , isNodeExpanded_(isNodeComplete_)
      , prevLeaf(std::move(prev))
      , nextLeaf(std::move(next))
      , prevUnexpandedLeaf(std::move(prevUnexpanded))
      , nextUnexpandedLeaf(std::move(nextUnexpanded))
      , isNodeHidden_(false) {
  }

  virtual ~TreeNode() = default;

  void addChild(const std::string& inputStr) {
    std::shared_ptr<TreeNode> prev = nullptr;
    std::shared_ptr<TreeNode> next = nullptr;
    if (this->children.empty()) {
      prev = this->prevLeaf;
      next = this->nextLeaf;
    } else {
      prev = this->children.back();
      next = prev->nextLeaf;
    }

    std::shared_ptr<TreeNode> child(new TreeNode(inputStr,
                                                 prev,
                                                 next));
    if (child->prevLeaf)
      child->prevLeaf->nextLeaf = child;

    if (child->nextLeaf)
      child->nextLeaf->prevLeaf = child;

    if (!child->isNodeExpanded()) {
      std::shared_ptr<TreeNode> prevUnexpanded = nullptr;
      std::shared_ptr<TreeNode> nextUnexpanded = nullptr;
      if (this->prevUnexpandedLeaf) {
        prevUnexpanded = this->prevUnexpandedLeaf;
        nextUnexpanded = this->nextUnexpandedLeaf;
      } else if (!this->children.empty()) {
        for (auto i = this->children.rbegin();
             i != this->children.rend();
             i++) {
          if ((*i)->hasPrevUnexpandedLeaf()) {
            prevUnexpanded = *i;
            nextUnexpanded = (*i)->nextUnexpandedLeaf;
            break;
          }
        }
      }

      child->prevUnexpandedLeaf = prevUnexpanded;
      child->nextUnexpandedLeaf = nextUnexpanded;
      if (child->prevUnexpandedLeaf)
        child->prevUnexpandedLeaf->nextUnexpandedLeaf = child;
      if (child->nextUnexpandedLeaf)
        child->nextUnexpandedLeaf->prevUnexpandedLeaf = child;

      // This is no longer part of the chain of unexpanded leaves
      this->prevUnexpandedLeaf = this->nextUnexpandedLeaf = nullptr;
    }

    child->isNodeHidden_ = this->isNodeHidden_;
    this->children.push_back(child);

    // This is no longer a leaf
    this->prevLeaf = this->nextLeaf = nullptr;
  }

  bool hasChildren() const {
    return !this->children.empty();
  }

  bool areChildrenComplete() const {
    for (auto& child : this->children) {
      if (!child->isNodeComplete()) return false;
    }
    return true;
  }

  std::shared_ptr<TreeNode> getLastExpandableChild() const {
    for (auto riter = this->children.rbegin();
         riter != this->children.rend();
         riter++) {
      if (!(*riter)->isNodeComplete()) return *riter;
    }

    return nullptr;
  }

  std::string flatten(details::callback_map_t modFuns,
                      bool ignoreHidden = true,
                      bool ignoreModifiers = false) {
    if (!(this->modifiers.empty() || ignoreModifiers)) {
      std::string output = this->flatten(modFuns, ignoreHidden, true);
      if (output.empty()) return "";
      for (auto& mod : this->modifiers) {
        std::vector<std::string> params;
        std::string modName = mod;

        if (details::containsParametricModifier(mod)) {
          // $1 name
          modName = std::regex_replace(mod,
                                       details::getParametricModifierRegex(),
                                       "$1");
          // $2 params
          std::string paramsStr = std::regex_replace(mod,
                                                     details::getParametricModifierRegex(),
                                                     "$2");

          // use comma regex to separate params and fill params vector
          std::copy(std::sregex_token_iterator(paramsStr.begin(),
                                               paramsStr.end(),
                                               details::getCommaRegex(),
                                               -1),
                    std::sregex_token_iterator(),
                    std::back_inserter(params));
        }

        if (modFuns.find(modName) != modFuns.end()) {
          output = modFuns[modName]->callVec(output, params);
        }
      }
      return output;
    }

    if (!this->hasChildren()) {
      if (ignoreHidden && this->isNodeHidden()) {
        return "";
      }
      return this->input;
    }

    std::string ret;
    for (auto& child : this->children) {
      ret += child->flatten(modFuns, ignoreHidden, false);
    }
    return ret;
  }

  template<typename RNG, typename UniformIntDistributionT>
  void expandNode(const nlohmann::json&, RNG&, nlohmann::json&);

  const std::string& getInput() const { return this->input; }

  std::shared_ptr<TreeNode> getNextLeaf() const {
    return this->nextLeaf;
  }

  std::shared_ptr<TreeNode> getNextUnexpandedLeaf() const {
    return this->nextUnexpandedLeaf;
  }

  std::shared_ptr<TreeNode> getPrevLeaf() const {
    return this->prevLeaf;
  }

  std::shared_ptr<TreeNode> getPrevUnexpandedLeaf() const {
    return this->prevUnexpandedLeaf;
  }

  bool hasNextUnexpandedLeaf() const {
    return this->nextUnexpandedLeaf ? true : false;
  }

  bool hasPrevUnexpandedLeaf() const {
    return this->prevUnexpandedLeaf ? true : false;
  }

  bool isNodeComplete() const { return this->isNodeComplete_; }

  bool isNodeExpanded() const { return this->isNodeExpanded_; }

  void setNextLeaf(std::shared_ptr<TreeNode> next) {
    this->nextLeaf = std::move(next);
  }

  void setNextUnexpandedLeaf(std::shared_ptr<TreeNode> next) {
    this->nextUnexpandedLeaf = std::move(next);
  }

  void setPrevLeaf(std::shared_ptr<TreeNode> prev) {
    this->prevLeaf = std::move(prev);
  }

  void setPrevUnexpandedLeaf(std::shared_ptr<TreeNode> prev) {
    this->prevUnexpandedLeaf = std::move(prev);
  }

  void setKeyName(std::optional<std::string> key) {
    this->keyName = std::move(key);
  }

  std::optional<std::string> getKeyName() const {
    return this->keyName;
  }

  bool isNodeHidden() const {
    return this->isNodeHidden_;
  }

  void addModifier(const std::string& mod) {
    this->modifiers.push_back(mod);
  }

  std::vector<std::string> getModifiers() const {
    return this->modifiers;
  }

private:
  std::string input;
  bool isNodeComplete_;
  bool isNodeExpanded_;
  std::vector<std::shared_ptr<TreeNode>> children;
  std::shared_ptr<TreeNode> prevLeaf;
  std::shared_ptr<TreeNode> nextLeaf;
  std::shared_ptr<TreeNode> prevUnexpandedLeaf;
  std::shared_ptr<TreeNode> nextUnexpandedLeaf;
  std::optional<std::string> keyName;
  bool isNodeHidden_;
  std::vector<std::string> modifiers;
};

template<typename RNG, typename UniformIntDistributionT>
void TreeNode::expandNode(const nlohmann::json& jsonGrammar,
                          RNG& rng,
                          nlohmann::json& runtimeDictionary) {
  if (this->isNodeExpanded()) return;

  if (details::containsOnlyRule(this->input)) {
    // expand rule
    std::string ruleName = std::regex_replace(this->input,
                                              details::getOnlyRuleRegex(),
                                              "$1");
    std::string modsStr = std::regex_replace(this->input,
                                             details::getOnlyRuleRegex(),
                                             "$2");

    std::vector<std::string> mods;
    std::for_each(std::sregex_token_iterator(modsStr.begin(),
                                             modsStr.end(),
                                             details::getModifierRegex(),
                                             0),
                  std::sregex_token_iterator(),
                  [&mods](auto& mtch) {
                    std::string modifier = mtch.str().substr(1, mtch.str().size());
                    mods.push_back(modifier);
                  });

    auto ruleContents = runtimeDictionary[ruleName];
    std::string output;
    if (ruleContents.is_null())
      ruleContents = jsonGrammar[ruleName];
    if (ruleContents.is_string()) {
      output = ruleContents;
    }

    if (ruleContents.is_array()) {
      UniformIntDistributionT dist(0, ruleContents.size() - 1);
      output = ruleContents[dist(rng)];
    }

    for (auto& modifier : mods) {
      this->addModifier(modifier);
    }
    this->addChild(output);
  } else if (details::containsOnlyRuleWithActions(this->input)) {
    std::string actions = std::regex_replace(this->input,
                                             details::getOnlyRuleWithActionsRegex(),
                                             "$1");
    std::string ruleName = std::regex_replace(this->input,
                                              details::getOnlyRuleWithActionsRegex(),
                                              "$2$3");
    this->addChild(actions);
    this->addChild("#" + ruleName + "#");
  } else if (details::containsOnlyKeylessRuleAction(this->input)) {
    std::string rule = std::regex_replace(this->input,
                                          details::getOnlyKeylessRuleActionRegex(),
                                          "$1");
    this->addChild(rule);
  } else if (details::containsOnlyKeyWithRuleAction(this->input)) {
    std::string key = std::regex_replace(this->input,
                                         details::getOnlyKeyWithRuleActionRegex(),
                                         "$1");
    std::string rule = std::regex_replace(this->input,
                                          details::getOnlyKeyWithRuleActionRegex(),
                                          "$2");
    this->isNodeHidden_ = true;
    this->addChild(rule);
    this->children.back()->setKeyName(key);
  } else if (details::containsOnlyKeyWithTextAction(this->input)) {
    //   [key:text] - sets key to "text"
    //   [key:a,b,c,...] - sets key to a list
    std::string key = std::regex_replace(this->input,
                                         details::getOnlyKeyWithTextActionRegex(),
                                         "$1");
    std::string txt = std::regex_replace(this->input,
                                         details::getOnlyKeyWithTextActionRegex(),
                                         "$2");

    this->isNodeHidden_ = true;
    nlohmann::json arr = nlohmann::json::array();
    std::for_each(std::sregex_token_iterator(txt.begin(),
                                             txt.end(),
                                             details::getCommaRegex(),
                                             -1),
                  std::sregex_token_iterator(),
                  [&arr, this, &key](auto& mtch) {
                    arr.push_back(mtch.str());
                    this->addChild(mtch.str());
                  });
    runtimeDictionary[key] = arr;
  } else if (details::containsOnlyActions(this->input)) {
    // Actions:
    //   [#text#] - calls a function, essentially
    //   [key:#text#] - sets key to expansion of #text#
    //   [key:text] - sets key to "text"
    //   [key:a,b,c,...] - sets key to a list
    std::for_each(std::sregex_token_iterator(this->input.begin(),
                                             this->input.end(),
                                             details::getActionRegex(),
                                             {-1, 0}),
                  std::sregex_token_iterator(),
                  [this](auto& mtch) {
                    auto action = mtch.str();
                    // Ignore empty tokens, add action with []
                    if (!action.empty()) {
                      this->addChild(action);
                    }
                  });
  } else {
    std::for_each(std::sregex_token_iterator(this->input.begin(),
                                             this->input.end(),
                                             details::getRuleRegex(),
                                             {-1, 0}),
                  std::sregex_token_iterator(),
                  [this](auto& mtch) {
                    if (!mtch.str().empty())
                      this->addChild(mtch.str());
                  });
  }

  if (this->prevUnexpandedLeaf)
    this->prevUnexpandedLeaf->nextUnexpandedLeaf = this->nextUnexpandedLeaf;

  if (this->nextUnexpandedLeaf)
    this->nextUnexpandedLeaf->prevUnexpandedLeaf = this->prevUnexpandedLeaf;
}

class Tree {
public:
  Tree(const std::string& input,
       const nlohmann::json& grammar)
      : leafIndex(new TreeNode)
      , unexpandedLeafIndex(new TreeNode)
      , root(new TreeNode(input))
      , nextUnexpandedLeaf(nullptr)
      , jsonGrammar(grammar) {
    this->root->setPrevLeaf(this->leafIndex);
    this->leafIndex->setNextLeaf(this->root);

    if (!this->root->isNodeExpanded()) {
      this->unexpandedLeafIndex->setNextUnexpandedLeaf(this->root);
      this->root->setPrevUnexpandedLeaf(this->unexpandedLeafIndex);
    }
  }

  template<typename RNG, typename UniformIntDistributionT>
  bool expand(const details::callback_map_t& modFuns, RNG& rng) {
    auto next = this->unexpandedLeafIndex->getNextUnexpandedLeaf();

    this->expandingNodes.push(next);

    next->expandNode<RNG, UniformIntDistributionT>(this->jsonGrammar,
                                                   rng,
                                                   this->runtimeDictionary);

    if (next->areChildrenComplete()) {
      auto poppedNode = this->expandingNodes.top();
      this->expandingNodes.pop();
      if (poppedNode->getKeyName()) {
        std::string key = *poppedNode->getKeyName();
        std::string value = poppedNode->flatten(modFuns, false);
        this->runtimeDictionary[key] = value;
      }

      bool shouldContinue = !this->expandingNodes.empty();
      while (shouldContinue) {
        auto newTop = this->expandingNodes.top();
        if (newTop->getLastExpandableChild() == poppedNode) {
          // yes, pop and continue looking
          poppedNode = newTop;
          this->expandingNodes.pop();
          shouldContinue = !this->expandingNodes.empty();
          if (poppedNode->getKeyName()) {
            std::string key = *poppedNode->getKeyName();
            std::string value = poppedNode->flatten(modFuns, false);
            this->runtimeDictionary[key] = value;
          }
        } else {
          shouldContinue = false;
        }
      }
    }

    return this->unexpandedLeafIndex->hasNextUnexpandedLeaf();
  }

  template<typename RNG>
  bool expandBF(RNG& rng) {
    if (!this->nextUnexpandedLeaf) {
      if (this->unexpandedLeafIndex->hasNextUnexpandedLeaf()) {
        this->nextUnexpandedLeaf = this->unexpandedLeafIndex->getNextUnexpandedLeaf();
      } else {
        return false;
      }
    }

    std::shared_ptr<TreeNode> next = this->nextUnexpandedLeaf->getNextUnexpandedLeaf();
    this->nextUnexpandedLeaf->expandNode(this->jsonGrammar,
                                         rng,
                                         this->runtimeDictionary);
    this->nextUnexpandedLeaf = next;

    return true;
  }

  std::shared_ptr<TreeNode> getFirstLeaf() const {
    return this->leafIndex->getNextLeaf();
  }

  std::shared_ptr<TreeNode> getFirstUnexpandedLeaf() const {
    return this->unexpandedLeafIndex->getNextUnexpandedLeaf();
  }

  std::shared_ptr<TreeNode> getRoot() const { return this->root; }

  std::string flatten(details::callback_map_t modFuns,
                      bool ignoreHidden = true,
                      bool ignoreMods = false) {
    return this->root->flatten(std::move(modFuns), ignoreHidden, ignoreMods);
  }

private:
  std::shared_ptr<TreeNode> leafIndex;
  std::shared_ptr<TreeNode> unexpandedLeafIndex;
  std::shared_ptr<TreeNode> root;
  std::shared_ptr<TreeNode> nextUnexpandedLeaf;
  const nlohmann::json& jsonGrammar;
  nlohmann::json runtimeDictionary;
  std::stack<std::shared_ptr<TreeNode>> expandingNodes;
};

const details::callback_map_t& getBaseEngModifiers() {
  static auto wrap = [](const std::function<std::string(const std::string&)>& fun) {
    std::shared_ptr<details::IModifierFn> ptr(new details::ModifierFn<const std::string&>(fun));
    return ptr;
  };
  static details::callback_map_t baseMods;

  static auto isVowel = [](char letter) {
    char lower = tolower(letter);
    return (lower == 'a') || (lower == 'e') ||
           (lower == 'i') || (lower == 'o') ||
           (lower == 'u');
  };
  static auto isAlphaNum = [](char chr) {
    return ((chr >= 'a' && chr <= 'z') ||
            (chr >= 'A' && chr <= 'Z') ||
            (chr >= '0' && chr <= '9'));
  };

  if (!baseMods.empty()) return baseMods;

  baseMods["a"] = wrap([](const std::string& input) {
    if (input.empty()) return input;

    if (input.size() > 2) {
      if (tolower(input[0]) == 'u' &&
          tolower(input[2]) == 'i') {
        return "a " + input;
      }
    }

    if (isVowel(input[0])) {
      return "an " + input;
    }

    return "a " + input;
  });
  baseMods["capitalizeAll"] = wrap([](const std::string& input) {
    std::string ret;
    bool capNext = true;

    for (char chr : input) {
      if (isAlphaNum(chr)) {
        if (capNext) {
          ret += (char) toupper(chr);
          capNext = false;
        } else {
          ret += chr;
        }
      } else {
        capNext = true;
        ret += chr;
      }
    }

    return ret;
  });
  baseMods["capitalize"] = wrap([](const std::string& input) {
    std::string ret = input;
    ret[0] = toupper(ret[0]);
    return ret;
  });
  baseMods["s"] = wrap([](const std::string& input) {
    switch (input.back()) {
      case 's':
        return input + "es";
      case 'h':
        return input + "es";
      case 'x':
        return input + "es";
      case 'y':
        if (isVowel(input[input.size() - 2]))
          return input + "s";
        else
          return input.substr(0, input.size() - 1) + "ies";
      default:
        return input + "s";
    }
  });
  baseMods["ed"] = wrap([](const std::string& input) {
    switch (input.back()) {
      case 's':
        return input + "ed";
      case 'e':
        return input + "d";
      case 'h':
        return input + "ed";
      case 'x':
        return input + "ed";
      case 'y':
        if (isVowel(input[input.size() - 2]))
          return input + "d"; // TODO: this seems incorrect
        else
          return input.substr(0, input.size() - 1) + "ied";
      default:
        return input + "ed";
    }
  });

  baseMods["replace"] =
      std::shared_ptr<details::IModifierFn>(new details::ModifierFn<const std::string&,
          const std::string&, const std::string&>([](const std::string& input,
                                                     const std::string& target,
                                                     const std::string& replacement) {
        return std::regex_replace(input,
                                  std::regex(target),
                                  replacement);
      }));
  return baseMods;
}

template<typename RNG = std::mt19937,
    typename UniformIntDistributionT = std::uniform_int_distribution<>>
class Grammar {
public:
  explicit Grammar(nlohmann::json grammar = "{}"_json,
                   RNG _rng = RNG(time(nullptr)))
      : jsonGrammar(std::move(grammar))
      , rng(_rng) {
  }

  std::shared_ptr<Tree> getTree(const std::string& input) const {
    std::shared_ptr<Tree> tree(new Tree(input, this->jsonGrammar));
    return tree;
  }

  void addModifiers(const details::callback_map_t& mfs) {
    for (auto& mf : mfs) {
      this->addModifier(mf.first, mf.second);
    }
  }

  void addModifier(const std::string& name,
                   std::shared_ptr<details::IModifierFn> mod) {
    this->modifierFunctions[name] = std::move(mod);
  }

  template<typename... PTs>
  void addModifier(const std::string& name,
                   std::function<std::string(PTs...)> fun) {
    std::shared_ptr<details::IModifierFn> fptr(new details::ModifierFn<PTs...>(fun));
    this->addModifier(name, fptr);
  }

  const details::callback_map_t& getModifierFunctions() const {
    return this->modifierFunctions;
  }

  std::string flatten(const std::string& input) {
    auto tree = this->getTree(input);
    while (tree->template expand<RNG, UniformIntDistributionT>(this->getModifierFunctions(), this->rng));
    return tree->flatten(this->getModifierFunctions());
  }

private:
  nlohmann::json jsonGrammar;
  RNG rng;
  details::callback_map_t modifierFunctions;
};

} // End namespace tracerz
