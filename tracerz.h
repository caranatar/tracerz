#pragma once

/**
 * @file tracerz.h tracerz.h: a single-header, modern C++ port/extension of @galaxykate's tracery tool
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
 * Given a function that accepts an input and at least one additional parameter, the static function of this class
 * recursively unpacks a vector into a parameter pack, and then calls the given function on the input and the parameter
 * pack.
 *
 * @tparam N the number of items remaining in the vector
 * @tparam F the type of the function to call on the unpacked parameters
 * @tparam I the type of the input
 * @tparam PTs a parameter pack of the types of the items already unpacked from the vector
 */
template<int N,
    typename F,
    typename I,
    typename... PTs>
class CallVector_R {
public:
  /**
   * Unpacks the first parameter from the parameter vector, moves it to the parameter pack, and then recurses
   *
   * @param fun the function to ultimately call on the input and list of parameters
   * @param input the input to pass to `fun`
   * @param paramVec the remaining parameters
   * @param params the parameter pack of parameters that have been unpacked from the vector so far
   * @return the result of calling `fun` on the input string with all parameters
   */
  static decltype(auto) callVec(F fun,
                                I input,
                                std::vector<std::string> paramVec,
                                PTs... params) {
    // Unpack the first parameter from the vector
    std::string param = paramVec[0];

    // Create a vector containing the rest of the parameters
    std::vector<std::string> restOfVec(++paramVec.begin(), paramVec.end());

    // Recurse, placing `param` at the end of the parameter pack
    return CallVector_R<N - 1, F, I, const std::string&, PTs...>::callVec(fun,
                                                                          input,
                                                                          restOfVec,
                                                                          params...,
                                                                          param);
  }
};

/**
 * The base case of CallVector_R, in which all parameters have been moved from the parameter vector into the parameter
 * pack, and which calls the supplied function on the input and parameter pack.
 *
 * @tparam F the type of the function to be called
 * @tparam I the type of the input
 * @tparam PTs a parameter pack of the types of the parameters to be passed, with the input, into the function
 */
template<typename F,
    typename I,
    typename... PTs>
class CallVector_R<0, F, I, PTs...> {
public:
  /**
   * Calls the given function on the given input and parameter pack
   *
   * @param fun the function to call
   * @param input the input
   * @param params the parameter pack of the remaining parameters
   * @return the result of calling `fun` on `params...`
   */
  static decltype(auto) callVec(F fun,
                                I input,
                                const std::vector<std::string>&/*unused*/,
                                PTs... params) {
    return fun(input, params...);
  }
};

/**
 * This class provides a static function to call a given function with the given input and the contents of a given
 * parameter vector as parameters to the function
 *
 * @tparam N the number of parameters F takes, excluding the input string
 * @tparam F the type of the function to be called on the supplied parameters
 * @tparam I the type of the input
 */
template<int N, typename F, typename I>
class CallVector {
public:
  /**
   * Calls function `fun` with `input` and the contents of `params` as parameters
   *
   * @param fun the function to call
   * @param input the input
   * @param params the remaining parameters
   * @return the result of calling `fun(input, params...)`
   */
  static decltype(auto) callVec(F fun,
                                I input,
                                const std::vector<std::string>& params) {
    // Peel off the first parameter
    std::string param = params[0];

    // Create a vector of the remaining parameters
    std::vector<std::string> restOfParams(++params.begin(), params.end());

    // Use CallVector_R to recurse over the remaining parameters
    return CallVector_R<N - 1, F, I, const std::string&>::callVec(fun,
                                                                  input,
                                                                  restOfParams,
                                                                  param);
  }
};

/**
 * Special case of CallVector for functions that only take a single input
 *
 * @tparam F the type of the supplied function
 * @tparam I the type of the input
 */
template<typename F, typename I>
class CallVector<0, F, I> {
public:
  /**
   * Calls `fun` on `input`
   *
   * @param fun the function to call
   * @param input the input
   * @return the result of `fun(input)`
   */
  static decltype(auto) callVec(F fun,
                                I input,
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
    return CallVector<sizeof...(Ts) - 1, decltype(this->callback), const std::string&>::callVec(this->callback, input,
                                                                                                params);
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

/**
 * Represents a single node in the parse tree
 *
 * TODO: enumerate node types
 */
class TreeNode {
public:
  /**
   * Default constructor. Contains no input, used by the tree to point to certain nodes within the tree, from outside it
   */
  TreeNode()
      : input()
      , isNodeComplete_(false)
      , prevLeaf(nullptr)
      , nextLeaf(nullptr)
      , prevUnexpandedLeaf(nullptr)
      , nextUnexpandedLeaf(nullptr)
      , isNodeHidden_(false) {
  }

  /**
   * Constructs a new tree node from the given parameters. The only required parameter is the input string, all others
   * default to nullptr
   *
   * @param input the input string
   * @param prev the leaf prior to this leaf
   * @param next the next leaf from this leaf
   * @param prevUnexpanded the unexpanded leaf prior to this unexpanded leaf
   * @param nextUnexpanded the next unexpanded leaf from this unexpanded leaf
   */
  explicit TreeNode(const std::string& input,
                    std::shared_ptr<TreeNode> prev = nullptr,
                    std::shared_ptr<TreeNode> next = nullptr,
                    std::shared_ptr<TreeNode> prevUnexpanded = nullptr,
                    std::shared_ptr<TreeNode> nextUnexpanded = nullptr)
      : input(input)
      , isNodeComplete_(!details::containsRule(input)
                        && !details::containsOnlyActions(input))
      , prevLeaf(std::move(prev))
      , nextLeaf(std::move(next))
      , prevUnexpandedLeaf(std::move(prevUnexpanded))
      , nextUnexpandedLeaf(std::move(nextUnexpanded))
      , isNodeHidden_(false) {
  }

  /**
   * Default virtual destructor
   */
  virtual ~TreeNode() = default;

  /**
   * Creates a new TreeNode from the given input string and adds it as a child to this node.
   *
   * @param inputStr the input string
   */
  void addChild(const std::string& inputStr) {
    std::shared_ptr<TreeNode> prev = nullptr;
    std::shared_ptr<TreeNode> next = nullptr;
    if (this->children.empty()) {
      // If there are no children, then the previous and next leaves are the ones of this node
      prev = this->prevLeaf;
      next = this->nextLeaf;
    } else {
      // If there are children, the new node's previous leaf is the last child and its next leaf is the one of this node
      prev = this->children.back();
      next = prev->nextLeaf;
    }

    // Create the child with the input string, previous leaf, and next leaf
    std::shared_ptr<TreeNode> child(new TreeNode(inputStr,
                                                 prev,
                                                 next));

    // If the child's previous leaf is not null, set its next leaf to the new node
    if (child->prevLeaf)
      child->prevLeaf->nextLeaf = child;

    // If the child's next leaf is not null, set its previous leaf to the new node
    if (child->nextLeaf)
      child->nextLeaf->prevLeaf = child;

    // If this new child is not complete, set its previous and next unexpanded leaves
    if (!child->isNodeComplete()) {
      std::shared_ptr<TreeNode> prevUnexpanded = nullptr;
      std::shared_ptr<TreeNode> nextUnexpanded = nullptr;

      if (this->prevUnexpandedLeaf) {
        // If this leaf has a previous unexpanded leaf, then this is its first expansion. Set the child's previous and
        // next unexpanded leaves to this one's.
        prevUnexpanded = this->prevUnexpandedLeaf;
        nextUnexpanded = this->nextUnexpandedLeaf;
      } else if (!this->children.empty()) {
        // If this node has children, get the last child *C* with a previous unexpanded leaf, set the new child's
        // previous unexpanded leaf to *C*, and set the new child's next unexpanded leaf to *C*'s next unexpanded node.
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

      // Set the child object's previous and next unexpanded leaves
      child->prevUnexpandedLeaf = prevUnexpanded;
      child->nextUnexpandedLeaf = nextUnexpanded;

      // If the child's previous unexpanded leaf is not null, set its next unexpanded leaf to the new child
      if (child->prevUnexpandedLeaf)
        child->prevUnexpandedLeaf->nextUnexpandedLeaf = child;

      // If the child's next unexpanded leaf is not null, set its previous unexpanded leaf to the new child
      if (child->nextUnexpandedLeaf)
        child->nextUnexpandedLeaf->prevUnexpandedLeaf = child;

      // This node has been at least partially expanded and is no longer part of the chain of unexpanded leaves
      this->prevUnexpandedLeaf = this->nextUnexpandedLeaf = nullptr;
    }

    // If this is a hidden node, hide the child
    child->isNodeHidden_ = this->isNodeHidden_;

    // Add the child to the list of children
    this->children.push_back(child);

    // This is no longer a leaf, unset its previous and next leaves
    this->prevLeaf = this->nextLeaf = nullptr;
  }

  /**
   * Returns true if this node has children nodes
   *
   * @return true if this node has children nodes
   */
  bool hasChildren() const {
    return !this->children.empty();
  }

  /**
   * Returns true if every child of this node is complete, or there are no children
   *
   * @return true if every child of this node is complete, or there are no children
   */
  bool areChildrenComplete() const {
    for (auto& child : this->children) {
      if (!child->isNodeComplete()) return false;
    }
    return true;
  }

  /**
   * Gets the last child of this node that is unexpanded
   *
   * @return the last child of this node that is unexpanded
   */
  std::shared_ptr<TreeNode> getLastExpandableChild() const {
    for (auto riter = this->children.rbegin();
         riter != this->children.rend();
         riter++) {
      // If this child is not complete, return it
      if (!(*riter)->isNodeComplete()) return *riter;
    }

    // No expandable child found, return nullptr
    return nullptr;
  }

  /**
   * Flattens the sub-tree represented by this node and its descendents into a single string representation, using the
   * given modifier map to handle any modifiers (if applicable).
   *
   * @param modFuns the modifier function map
   * @param ignoreHidden exclude hidden subtrees from the flattened string
   * @param ignoreModifiers if true, modifier functions will not be called
   * @return the flattened string representation
   */
  std::string flatten(details::callback_map_t modFuns,
                      bool ignoreHidden = true,
                      bool ignoreModifiers = false) {
    if (!(this->modifiers.empty() || ignoreModifiers)) {
      // If this node has modifiers and ignoreModifiers is not true, then we need to apply modifiers. First we get the
      // flattened string representation without calling modifiers
      std::string output = this->flatten(modFuns, ignoreHidden, true);

      // If the output is empty at this point, then return the empty string
      if (output.empty()) return output;

      // Loop over each modifier being applied to this node
      for (auto& mod : this->modifiers) {
        std::vector<std::string> params; // Starts empty, may be filled if this is a parametric modifier
        std::string modName = mod;

        if (details::containsParametricModifier(mod)) {
          // If it contains a modifier that takes parameters, we need to handle those. First extract the modifier name
          // from the first capture group of this regex
          modName = std::regex_replace(mod,
                                       details::getParametricModifierRegex(),
                                       "$1");
          // Get the string representing the list of params from the second capture group
          std::string paramsStr = std::regex_replace(mod,
                                                     details::getParametricModifierRegex(),
                                                     "$2");

          // Use comma regex to separate params and fill params vector
          std::copy(std::sregex_token_iterator(paramsStr.begin(),
                                               paramsStr.end(),
                                               details::getCommaRegex(),
                                               -1),
                    std::sregex_token_iterator(),
                    std::back_inserter(params));
        }

        if (modFuns.find(modName) != modFuns.end()) {
          // If the modifier name names a real modifier, call it with the input string and parameters (if any), and
          // update the output string.
          output = modFuns[modName]->callVec(output, params);
        }
      }

      // Return the modified string
      return output;
    }

    // If this doesn't have children...
    if (!this->hasChildren()) {
      // Then if the node is hidden and we are ignoring hidden nodes...
      if (ignoreHidden && this->isNodeHidden()) {
        // Return an empty string, since it's ignored
        return "";
      }

      // Node isn't hidden, return its input string
      return this->input;
    }

    // This node has children and isn't hidden, flatten and assemble the output of all its children
    std::string ret;
    for (auto& child : this->children) {
      // We can't ignore modifiers, because we will get the wrong output from flattening our children if we do.
      ret += child->flatten(modFuns, ignoreHidden, false);
    }

    // Return the flattened children's string
    return ret;
  }

  template<typename RNG, typename UniformIntDistributionT>
  void expandNode(const nlohmann::json&, RNG&, std::map<std::string, std::stack<nlohmann::json>>&);

  /**
   * Gets the input string for this node
   *
   * @return the input string for this node
   */
  const std::string& getInput() const { return this->input; }

  /**
   * Gets the next leaf (if applicable). If this is the last leaf or if this node is not a leaf, will return nullptr.
   *
   * @return the next leaf (if applicable)
   */
  std::shared_ptr<TreeNode> getNextLeaf() const {
    return this->nextLeaf;
  }

  /**
   * Gets the next unexpanded leaf (if applicable). If this is the last unexpanded leaf, is partiall or fully expanded,
   * or is not a leaf, will return nullptr.
   *
   * @return the next unexpanded leaf (if applicable)
   */
  std::shared_ptr<TreeNode> getNextUnexpandedLeaf() const {
    return this->nextUnexpandedLeaf;
  }

  /**
   * Gets the previous leaf (if applicable). If this node is not a leaf, will return nullptr.
   *
   * @return the previous leaf (if applicable)
   */
  std::shared_ptr<TreeNode> getPrevLeaf() const {
    return this->prevLeaf;
  }

  /**
   * Gets the previous unexpanded leaf (if applicable). If this is partially or fully expanded, or is not a leaf, this
   * will return nullptr.
   *
   * @return the previous unexpanded leaf (if applicable)
   */
  std::shared_ptr<TreeNode> getPrevUnexpandedLeaf() const {
    return this->prevUnexpandedLeaf;
  }

  /**
   * Returns true if this node has a next unexpanded leaf
   *
   * @return true if this node has a next unexpanded leaf
   */
  bool hasNextUnexpandedLeaf() const {
    return this->nextUnexpandedLeaf ? true : false;
  }

  /**
   * Returns true if this node has a previous unexpanded leaf
   *
   * @return true if this node has a previous unexpanded leaf
   */
  bool hasPrevUnexpandedLeaf() const {
    return this->prevUnexpandedLeaf ? true : false;
  }

  /**
   * Returns true if this node is complete (cannot be expanded)
   *
   * @return true if this node is complete (cannot be expanded)
   */
  bool isNodeComplete() const { return this->isNodeComplete_; }

  /**
   * Sets the next leaf
   *
   * @param next the next leaf
   */
  void setNextLeaf(std::shared_ptr<TreeNode> next) {
    this->nextLeaf = std::move(next);
  }

  /**
   * Sets the next unexpanded leaf
   *
   * @param next the next unexpanded leaf
   */
  void setNextUnexpandedLeaf(std::shared_ptr<TreeNode> next) {
    this->nextUnexpandedLeaf = std::move(next);
  }

  /**
   * Sets the previous leaf
   *
   * @param next the previous leaf
   */
  void setPrevLeaf(std::shared_ptr<TreeNode> prev) {
    this->prevLeaf = std::move(prev);
  }

  /**
   * Sets the previous unexpanded leaf
   *
   * @param next the previous unexpanded leaf
   */
  void setPrevUnexpandedLeaf(std::shared_ptr<TreeNode> prev) {
    this->prevUnexpandedLeaf = std::move(prev);
  }

  /**
   * Marks this node has having a key with the given name, which will be set to the result of flattening this node and
   * stored in the runtime grammar.
   *
   * @param next the key name
   */
  void setKeyName(std::optional<std::string> key) {
    this->keyName = std::move(key);
  }

  /**
   * Gets the key name or nothing, if it doesn't have one
   *
   * @return the key name or std::nullopt
   */
  std::optional<std::string> getKeyName() const {
    return this->keyName;
  }

  /**
   * Returns true if this node is hidden
   *
   * @return true if this node is hidden
   */
  bool isNodeHidden() const {
    return this->isNodeHidden_;
  }

  /**
   * Adds a modifier with the given name to this node's list of modifiers
   *
   * @param mod the name of the modifier to add
   */
  void addModifier(const std::string& mod) {
    this->modifiers.push_back(mod);
  }

  /**
   * Gets the list of modifiers to apply to this node
   *
   * @return the list of modifiers
   */
  std::vector<std::string> getModifiers() const {
    return this->modifiers;
  }

private:
  /** The input string for this node. This can be any combination or none of: rules, actions, and modifiers. */
  std::string input;

  /** True if this node is complete - if it contains no rules, actions, or modifiers. */
  bool isNodeComplete_;

  /** The list of children this node has. */
  std::vector<std::shared_ptr<TreeNode>> children;

  /** A pointer to the previous leaf if this is a leaf and one exists. */
  std::shared_ptr<TreeNode> prevLeaf;

  /** A pointer to the next leaf if this is a leaf and one exists. */
  std::shared_ptr<TreeNode> nextLeaf;

  /**  A pointer to the previous unexpanded leaf if this is an unexpanded leaf and one exists. */
  std::shared_ptr<TreeNode> prevUnexpandedLeaf;

  /** A pointer to the next unexpanded leaf if this is an unexpanded leaf and one exists. */
  std::shared_ptr<TreeNode> nextUnexpandedLeaf;

  /** A key name if one has been set on this node. */
  std::optional<std::string> keyName;

  /** True if this node is hidden. */
  bool isNodeHidden_;

  /** The list of modifiers that have been added to this node. */
  std::vector<std::string> modifiers;
};

/**
 * Expands this node, processing any rules, actions, and modifiers in it, and creating child nodes for each. If the node
 * contains no such expandable elements, does nothing.
 *
 * @tparam RNG the type of the random number generator in use
 * @tparam UniformIntDistributionT the type to use to perform equal probability expansion of rules
 * @param jsonGrammar the input grammar for the tree containing this node
 * @param rng the random number generator to use
 * @param runtimeDictionary the runtime dictionary in use by the tree containing this node
 */
template<typename RNG, typename UniformIntDistributionT>
void TreeNode::expandNode(const nlohmann::json& jsonGrammar,
                          RNG& rng,
                          std::map<std::string, std::stack<nlohmann::json>>& runtimeDictionary) {
  // If the node is complete, nothing to do
  if (this->isNodeComplete()) return;

  if (details::containsOnlyRule(this->input)) {
    // If this node contains only a rule, get the rule name from the first capture group and the list of modifiers from
    // the second capture group.
    std::string ruleName = std::regex_replace(this->input,
                                              details::getOnlyRuleRegex(),
                                              "$1");
    std::string modsStr = std::regex_replace(this->input,
                                             details::getOnlyRuleRegex(),
                                             "$2");

    std::vector<std::string> mods;
    // Split the single string representing the list of modifiers into individual strings representing a single modifier
    // each.
    std::for_each(std::sregex_token_iterator(modsStr.begin(),
                                             modsStr.end(),
                                             details::getModifierRegex(),
                                             0),
                  std::sregex_token_iterator(),
        // This lambda takes in a match object, gets the string representation of the match, removes the
        // leading dot (.) from the modifier string, and adds it to the list of modifiers
                  [&mods](auto& mtch) {
                    std::string modifier = mtch.str().substr(1, mtch.str().size());
                    mods.push_back(modifier);
                  });

    // Attempt to get an expansion of the rule from the runtime grammar
    nlohmann::json ruleContents;
    if (runtimeDictionary.find(ruleName) != runtimeDictionary.end())
      ruleContents = runtimeDictionary[ruleName].top();

    // This will be the output of expanding the rule
    std::string output;

    // If ruleContents doesn't contain anything, there is no runtime definition for this rule name, attempt to get it
    // from the input grammar
    if (ruleContents.is_null())
      ruleContents = jsonGrammar[ruleName];

    // If the rule contents are a string, assign the output
    if (ruleContents.is_string()) {
      output = ruleContents;
    }

    // If the rule contents are a list, create an instance of the uniform distribution type, and use it to select
    // a single item from the array
    if (ruleContents.is_array()) {
      UniformIntDistributionT dist(0, ruleContents.size() - 1);
      output = ruleContents[dist(rng)];
    }

    // For each modifier in the list of modifiers, add it to this node's list of modifiers
    for (auto& modifier : mods) {
      this->addModifier(modifier);
    }

    // Create the new child node from the output
    this->addChild(output);
  } else if (details::containsOnlyRuleWithActions(this->input)) {
    // If this node contains only a rule with actions, split into actions and rule, emitting a child node for each.
    // First, get the string representing the list of actions from the first capture group
    std::string actions = std::regex_replace(this->input,
                                             details::getOnlyRuleWithActionsRegex(),
                                             "$1");

    // Then, get the rule name and any modifiers from the second and third capture groups.
    std::string ruleName = std::regex_replace(this->input,
                                              details::getOnlyRuleWithActionsRegex(),
                                              "$2$3");

    // Add a child using the extracted actions
    this->addChild(actions);

    // Add a child using the extracted rule name. Since it's been stripped of surrounding octothorpes, add then back in.
    this->addChild("#" + ruleName + "#");
  } else if (details::containsOnlyKeylessRuleAction(this->input)) {
    // This node contains only a rule action, with no key being assigned. Get the rule name from the first capture group
    std::string rule = std::regex_replace(this->input,
                                          details::getOnlyKeylessRuleActionRegex(),
                                          "$1");

    // Add a child using the rule.
    this->addChild(rule);
  } else if (details::containsOnlyKeyWithRuleAction(this->input)) {
    // This node contains an action which expands a rule and assigns the output to a key. First, get the key name from
    // the first capture group.
    std::string key = std::regex_replace(this->input,
                                         details::getOnlyKeyWithRuleActionRegex(),
                                         "$1");

    // Then get the rule name from the second capture group.
    std::string rule = std::regex_replace(this->input,
                                          details::getOnlyKeyWithRuleActionRegex(),
                                          "$2");

    // Since this node is assigning to a key, its output must be suppressed. Set to hidden.
    this->isNodeHidden_ = true;

    // Create a child from the extracted rule
    this->addChild(rule);

    // Get the newly added child, and pass down the key name
    this->children.back()->setKeyName(key);
  } else if (details::containsOnlyKeyWithTextAction(this->input)) {
    //   [key:text] - sets key to "text"
    //   [key:a,b,c,...] - sets key to a list
    // Extract the key name using the first capture group.
    std::string key = std::regex_replace(this->input,
                                         details::getOnlyKeyWithTextActionRegex(),
                                         "$1");

    // Extract the text or key from the second capture group
    std::string txt = std::regex_replace(this->input,
                                         details::getOnlyKeyWithTextActionRegex(),
                                         "$2");

    // Since this is setting a key, this node is hidden
    this->isNodeHidden_ = true;

    // Create a json array
    nlohmann::json arr = nlohmann::json::array();

    // Split the txt using the comma regex, and call the given function on each token. Note that because a single item
    // will contain no commas, this will be called once even if no list is present.
    std::for_each(std::sregex_token_iterator(txt.begin(),
                                             txt.end(),
                                             details::getCommaRegex(),
                                             -1),
                  std::sregex_token_iterator(),
        // This lambda extracts the string from each match object and adds it to the json list created above.
                  [&arr](auto& mtch) {
                    arr.push_back(mtch.str());
                  });

    // Set the key in the runtime dictionary to the json array
    runtimeDictionary[key].push(arr);
  } else if (details::containsOnlyActions(this->input)) {
    // This node contains only one or more actions.
    // Action types:
    //   [#text#] - calls a function, essentially, by expanding a rule
    //   [key:#text#] - sets key to expansion of #text#
    //   [key:text] - sets key to "text"
    //   [key:a,b,c,...] - sets key to a list

    // For each action string, add a child to this node representing a single action
    std::for_each(std::sregex_token_iterator(this->input.begin(),
                                             this->input.end(),
                                             details::getActionRegex(),
                                             0),
                  std::sregex_token_iterator(),
                  [this](auto& mtch) {
                    auto action = mtch.str();
                    // Ignore empty tokens, add action with []
                    if (!action.empty()) {
                      this->addChild(action);
                    }
                  });
  } else {
    // This node represents some mix of strings, use the rule regex to separate it into strings representing rules and
    // strings representing non-rules. For each of those, add a child to this node with it as an input string.
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

  // Remove this node from the linked list of unexpanded leaves, if applicable
  if (this->prevUnexpandedLeaf)
    this->prevUnexpandedLeaf->nextUnexpandedLeaf = this->nextUnexpandedLeaf;
  if (this->nextUnexpandedLeaf)
    this->nextUnexpandedLeaf->prevUnexpandedLeaf = this->prevUnexpandedLeaf;
}

/**
 * Represents a partially or fully constructed parse tree. Maintains two linked list heads for indexing into the tree:
 * one for the first leaf and the other for the first unexpanded leaf. Also provides methods to expand nodes and flatten
 * the tree into a single output string.
 */
class Tree {
public:
  /**
   * Creates a new input tree rooted with the given input string, using the given grammar.
   *
   * @param input the input string to construct the tree from
   * @param grammar the grammar to use to construct the tree
   */
  Tree(const std::string& input,
       const nlohmann::json& grammar)
      : leafIndex(new TreeNode)
      , unexpandedLeafIndex(new TreeNode)
      , root(new TreeNode(input))
      , nextUnexpandedLeaf(nullptr)
      , jsonGrammar(grammar) {
    // Insert the root at the beginning of the leaf linked list, after the head
    this->root->setPrevLeaf(this->leafIndex);
    this->leafIndex->setNextLeaf(this->root);

    // If the node is not complete, add it after the head of the unexpanded leaf linked list
    if (!this->root->isNodeComplete()) {
      this->unexpandedLeafIndex->setNextUnexpandedLeaf(this->root);
      this->root->setPrevUnexpandedLeaf(this->unexpandedLeafIndex);
    }
  }

  /**
   * Expands the next unexpanded node, depth-first.
   *
   * @tparam RNG the type of the random number generator
   * @tparam UniformIntDistributionT the type of the equal probability distribution
   * @param modFuns the map of modifier names to functions
   * @param rng the random number generator
   * @return true if there is still at least one unexpanded node
   */
  template<typename RNG, typename UniformIntDistributionT>
  bool expand(const details::callback_map_t& modFuns, RNG& rng) {
    // Get the leftmost unexpanded leaf
    auto next = this->unexpandedLeafIndex->getNextUnexpandedLeaf();

    // Return false if the tree is already fully expanded
    if (!next) return false;

    // Push it onto the stack of nodes currently being expanded
    this->expandingNodes.push(next);

    // Expand the node
    next->expandNode<RNG, UniformIntDistributionT>(this->jsonGrammar,
                                                   rng,
                                                   this->runtimeDictionary);

    // If the node has no children awaiting expansion
    if (next->areChildrenComplete()) {
      // Save and pop the top node off the stack
      auto poppedNode = this->expandingNodes.top();
      this->expandingNodes.pop();

      // Check if it has a key name
      if (poppedNode->getKeyName()) {
        // If so, get it
        std::string key = *poppedNode->getKeyName();

        // Flatten the subtree to get the value of the key
        std::string value = poppedNode->flatten(modFuns, false);

        // Set the key in the runtime grammar
        this->runtimeDictionary[key].push(value);
      }

      // Keep going if there are other nodes we were expanding
      bool shouldContinue = !this->expandingNodes.empty();
      while (shouldContinue) {
        // Peek at the top of the stack
        auto newTop = this->expandingNodes.top();

        // If newTop's last expandable child is equal to the previously popped node, then newTop is also finished
        // expanding.
        if (newTop->getLastExpandableChild() == poppedNode) {
          // Rotate newTop to poppedNode
          poppedNode = newTop;

          // Remove the top of the stack
          this->expandingNodes.pop();

          // If there are still nodes in the stack, keep looking
          shouldContinue = !this->expandingNodes.empty();

          // Check if the poppedNode has a key name
          if (poppedNode->getKeyName()) {
            // If so, get it.
            std::string key = *poppedNode->getKeyName();

            // Flatten the subtree to get the value of the key
            std::string value = poppedNode->flatten(modFuns, false);

            // Set the key in the runtime grammar
            this->runtimeDictionary[key].push(value);
          }
        } else {
          // poppedNode is not the last expandable child of newTop. There are still more children to expand, so we can
          // stop examining the stack.
          shouldContinue = false;
        }
      }
    }

    // Return true if there are still nodes to expand
    return this->unexpandedLeafIndex->hasNextUnexpandedLeaf();
  }

  /**
   * Expand the tree in a breadth-first manner.
   *
   * @tparam RNG the type of the random number generator
   * @param rng the random number generator
   * @return true if there are still unexpanded nodes
   */
  template<typename RNG>
  bool expandBF(RNG& rng) {
    // If the pointer to the next unexpanded leaf is null
    if (!this->nextUnexpandedLeaf) {
      // But the unexpanded leaf linked list is not empty
      if (this->unexpandedLeafIndex->hasNextUnexpandedLeaf()) {
        // Then start at the beginning of the linked list
        this->nextUnexpandedLeaf = this->unexpandedLeafIndex->getNextUnexpandedLeaf();
      } else {
        // If the linked list is also empty, the tree is fully expanded
        return false;
      }
    }

    // Get the next unexpanded leaf
    std::shared_ptr<TreeNode> next = this->nextUnexpandedLeaf->getNextUnexpandedLeaf();

    // Expand the current unexpanded leaf
    this->nextUnexpandedLeaf->expandNode(this->jsonGrammar,
                                         rng,
                                         this->runtimeDictionary);

    // Update the cursor
    this->nextUnexpandedLeaf = next;

    // Return true since there are unexpanded leaves
    return true;
  }

  /**
   * Gets the current leftmost leaf of the tree.
   *
   * @return the current leftmost leaf of the tree
   */
  std::shared_ptr<TreeNode> getFirstLeaf() const {
    return this->leafIndex->getNextLeaf();
  }

  /**
   * Gets the leftmost unexpanded leaf of the tree.
   *
   * @return the leftmost unexpanded leaf of the tree
   */
  std::shared_ptr<TreeNode> getFirstUnexpandedLeaf() const {
    return this->unexpandedLeafIndex->getNextUnexpandedLeaf();
  }

  /**
   * Gets the root of the tree.
   *
   * @return the root of the tree
   */
  std::shared_ptr<TreeNode> getRoot() const { return this->root; }

  /**
   * Flatten the tree into a single output string, based on the given input.
   *
   * @param modFuns the map of modifier names to functions to use
   * @param ignoreHidden if true, hidden nodes will not be included in the output
   * @param ignoreMods if true, no modifiers will be applied
   * @return the flattened output string
   */
  std::string flatten(details::callback_map_t modFuns,
                      bool ignoreHidden = true,
                      bool ignoreMods = false) {
    // Forward the call to the root of the tree
    return this->root->flatten(std::move(modFuns), ignoreHidden, ignoreMods);
  }

private:
  /** Points to the leftmost leaf of the tree */
  std::shared_ptr<TreeNode> leafIndex;

  /** Points to the leftmost unexpanded leaf */
  std::shared_ptr<TreeNode> unexpandedLeafIndex;

  /** The root of the tree */
  std::shared_ptr<TreeNode> root;

  /** Points to the current node to expand for breadth-first expansion */
  std::shared_ptr<TreeNode> nextUnexpandedLeaf;

  /** The input grammar */
  const nlohmann::json& jsonGrammar;

  /** The runtime grammar, consisting of keys created while expanding the tree with the input grammar. */
  std::map<std::string, std::stack<nlohmann::json>> runtimeDictionary;

  /** A stack of nodes currently being expanded by the depth-first expansion */
  std::stack<std::shared_ptr<TreeNode>> expandingNodes;
};

/**
 * Gets the set of base english modifiers, as defined by galaxykate's tracery
 *
 * @return a map of the names of the modifiers to functions
 */
const details::callback_map_t& getBaseEngModifiers() {
  // Wraps a function in a shared_ptr to a ModifierFn object and returns it
  static auto wrap = [](const std::function<std::string(const std::string&)>& fun) {
    std::shared_ptr<details::IModifierFn> ptr(new details::ModifierFn<const std::string&>(fun));
    return ptr;
  };

  // Defines the map
  static details::callback_map_t baseMods;

  // Helper function returning true if the character is a vowel
  static auto isVowel = [](char letter) {
    char lower = tolower(letter);
    return (lower == 'a') || (lower == 'e') ||
           (lower == 'i') || (lower == 'o') ||
           (lower == 'u');
  };

  // Helper function returning true if the character is alphanumeric
  static auto isAlphaNum = [](char chr) {
    return ((chr >= 'a' && chr <= 'z') ||
            (chr >= 'A' && chr <= 'Z') ||
            (chr >= '0' && chr <= '9'));
  };

  // If baseMods isn't empty, then it's already been created. Return it.
  if (!baseMods.empty()) return baseMods;

  // "a" adds an "a" or "an" to the beginning of a string, as appropriate
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

  // "capitalizeAll" capitalizes the first character of every word of a string
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

  // "capitalize" capitalizes the first character of the input string
  baseMods["capitalize"] = wrap([](const std::string& input) {
    std::string ret = input;
    ret[0] = toupper(ret[0]);
    return ret;
  });

  // "s" pluralizes the input string based on the end of the string
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

  // "ed" makes a verb past tense based on the end of the input string
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

  // "replace" is a parametric modifier that takes two parameters when used: a & b. It replaces all ocurrences of a in
  // the input string with b
  baseMods["replace"] =
      std::shared_ptr<details::IModifierFn>(new details::ModifierFn<const std::string&,
          const std::string&, const std::string&>([](const std::string& input,
                                                     const std::string& target,
                                                     const std::string& replacement) {
        return std::regex_replace(input,
                                  std::regex(target),
                                  replacement);
      }));

  // Return the map
  return baseMods;
}

/**
 * Represents a grammar, based on a given input grammar, using a given random number generator and uniform distribution
 * type.
 *
 * @tparam RNG the type of the random number generator to use
 * @tparam UniformIntDistributionT the type of the uniform distribution to use
 */
template<typename RNG = std::mt19937,
    typename UniformIntDistributionT = std::uniform_int_distribution<>>
class Grammar {
public:
  /** Make the underlying RNG type accessible */
  typedef RNG rng_t;

  /** Make the underlying uniform distribution type accessible */
  typedef UniformIntDistributionT uniform_distribution_t;

  /**
   * Creates a new grammar from the given parameters.
   *
   * @param grammar the input grammar
   * @param _rng the random number generator to use
   */
  explicit Grammar(nlohmann::json grammar = "{}"_json,
                   RNG _rng = RNG(time(nullptr)))
      : jsonGrammar(std::move(grammar))
      , rng(_rng) {
  }

  /**
   * Creates and returns a tree with the given input string as the input to its root node.
   *
   * @param input the input string for the root of the tree
   * @return the tree with that root
   */
  std::shared_ptr<Tree> getTree(const std::string& input) const {
    std::shared_ptr<Tree> tree(new Tree(input, this->jsonGrammar));
    return tree;
  }

  /**
   * Creates a tree with the given input string as the input to its root node, fully expands the tree, then returns it.
   */
  std::shared_ptr<Tree> getExpandedTree(const std::string& input) {
    auto tree = this->getTree(input);
    // While there are unexpanded nodes, expand the next one
    while (tree->template expand<RNG, UniformIntDistributionT>(this->getModifierFunctions(), this->rng));
    // Return the tree
    return tree;
  }

  /**
   * Adds the given modifiers to this grammar
   *
   * @param mfs a map of modifier names to functions
   */
  void addModifiers(const details::callback_map_t& mfs) {
    for (auto& mf : mfs) {
      this->addModifier(mf.first, mf.second);
    }
  }

  /**
   * Adds a pointer to a modifier to this grammar
   *
   * @param name the name of the modifier
   * @param mod the pointer to the modifier function
   */
  void addModifier(const std::string& name,
                   std::shared_ptr<details::IModifierFn> mod) {
    this->modifierFunctions[name] = std::move(mod);
  }

  /**
   * Adds a parametric modifier to this grammar
   *
   * @tparam PTs the parameter types of the modifier
   * @param name the name of the modifier
   * @param fun the modifier function
   */
  template<typename... PTs>
  void addModifier(const std::string& name,
                   std::function<std::string(PTs...)> fun) {
    std::shared_ptr<details::IModifierFn> fptr(new details::ModifierFn<PTs...>(fun));
    this->addModifier(name, fptr);
  }

  /**
   * Returns the map of modifier names to functions
   *
   * @return the map of modifier names to functions
   */
  const details::callback_map_t& getModifierFunctions() const {
    return this->modifierFunctions;
  }

  /**
   * Gets this grammar's random number generator
   *
   * @return this grammar's random number generator
   */
  RNG& getRNG() {
    return this->rng;
  }

  /**
   * Flattens the given input string into a single output string using this grammar.
   *
   * @param input the input string to flatten
   * @return the flattened output string
   */
  std::string flatten(const std::string& input) {
    // Get a tree rooted with the given input string
    auto tree = this->getExpandedTree(input);

    // Flatten the tree using this grammar's modifier functions
    return tree->flatten(this->getModifierFunctions());
  }

private:
  /** The input json grammar */
  nlohmann::json jsonGrammar;

  /** The random number generator */
  RNG rng;

  /** The map from modifier names to modifier functions */
  details::callback_map_t modifierFunctions;
};

} // End namespace tracerz
