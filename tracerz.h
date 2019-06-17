#pragma once

#include <cctype>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <regex>
#include <stack>
#include <string>
#include <vector>

#include "json.hpp"

namespace tracerz {
using json = nlohmann::json;

namespace details {
class ICallback {
public:
  virtual std::string callVec(const std::string &input, const std::vector<std::string> &params) = 0;

  virtual ~ICallback() = default;
};

template<typename G, typename PT>
std::string callg(G gFn, PT param) {
  return gFn(param);
}

template<typename G, typename PT, typename... PTs>
std::string callg(G gFn, PT param, PTs... params) {
  return callg([&gFn, &param](PTs... params) {
    return gFn(param, params...);
  }, params...);
}

template<typename F, typename... PTs>
std::string callf(F fun, const std::string &input, PTs... params) {
  return callg([&input, &fun](PTs... params) {
    return fun(input, params...);
  }, params...);
}

template<int N,
    typename F,
    typename... PTs>
class CallVec {
public:
  static std::string callVec(F fun,
                             const std::string &input,
                             std::vector<std::string> paramVec,
                             PTs... params) {
    std::string param = paramVec[0];
    std::vector<std::string> restOfVec(++paramVec.begin(), paramVec.end());
    return CallVec<N - 1, F, const std::string &, PTs...>::callVec(fun,
                                                                   input,
                                                                   restOfVec,
                                                                   params...,
                                                                   param);
  }
};

template<typename F,
    typename... PTs>
class CallVec<0, F, PTs...> {
public:
  static std::string callVec(F fun,
                             const std::string &input,
                             const std::vector<std::string> &/*unused*/,
                             PTs... params) {
    return callf(fun, input, params...);
  }
};

template<int N, typename F>
class CallVecF {
public:
  static std::string callVecF(F fun,
                              const std::string &input,
                              const std::vector<std::string> &params) {
    std::string param = params[0];
    std::vector<std::string> restOfParams(++params.begin(), params.end());
    return CallVec<N - 1, F, const std::string &>::callVec(fun,
                                                           input,
                                                           restOfParams,
                                                           param);
  }
};

template<typename F>
class CallVecF<0, F> {
public:
  static std::string callVecF(F fun,
                              const std::string &input,
                              const std::vector<std::string> & /*unused*/) {
    return fun(input);
  }
};

template<typename... Ts>
class Callback : public ICallback {
public:
  explicit Callback(std::function<std::string(Ts...)> fun)
      : callback(std::move(fun)) {
  }

  ~Callback() override = default;

  template<typename... PTs>
  std::string call(PTs... params) {
    return callf(this->callback, params...);
  }

  std::string callVec(const std::string &input, const std::vector<std::string> &params) override {
    return CallVecF<sizeof...(Ts) - 1,
        decltype(this->callback)>::callVecF(this->callback, input, params);
  }

private:
  std::function<std::string(Ts...)> callback;
};

typedef std::map<std::string, std::shared_ptr<ICallback>> callback_map_t;

const std::regex &getActionRegex() {
  static const std::regex rgx(R"(\[([^\]]*)\])");
  return rgx;
}

const std::regex &getCommaRegex() {
  static const std::regex rgx(",");
  return rgx;
}

const std::regex &getModifierRegex() {
  static const std::regex rgx(".([^.]+)");
  return rgx;
}

const std::regex &getOnlyActionsRegex() {
  static const std::regex rgx(R"(^(?:\[[^\]]*\])+$)");
  return rgx;
}

const std::regex &getOnlyKeyWithTextActionRegex() {
  static const std::regex rgx(R"(^\[([[:alnum:]]+):([^#\]]+)\]$)");
  return rgx;
}

const std::regex &getOnlyKeyWithRuleActionRegex() {
  static const std::regex rgx(R"(^\[([[:alnum:]]+):(#[[:alnum:]]+(?:\.[^.#]+)*#)\]$)");
  return rgx;
}

const std::regex &getOnlyKeylessRuleActionRegex() {
  static const std::regex rgx(R"(^\[(#[[:alnum:]]+(?:\.[^.#]+)*#)\]$)");
  return rgx;
}

const std::regex &getOnlyRuleRegex() {
  static const std::regex rgx(R"(^#([[:alnum:]]+)((?:\.[^.#]+)*)#$)");
  return rgx;
}

const std::regex &getOnlyRuleWithActionsRegex() {
  static const std::regex rgx(R"(^#((?:\[.*\])+)([[:alnum:]]+)((?:\.[^.#]+)*)#$)");
  return rgx;
}

const std::regex &getRuleRegex() {
  static const std::regex rgx(R"(#(?:\[.*\])*([[:alnum:]]+)((?:\.[^.#]+)*)#)");
  return rgx;
}

const std::regex &getParametricModifierRegex() {
  static const std::regex rgx(R"(([^\(]+)\(([^\)]+)\))");
  return rgx;
}

bool containsRule(const std::string &input) {
  return std::regex_search(input, details::getRuleRegex());
}

bool containsOnlyActions(const std::string &input) {
  return std::regex_match(input, details::getOnlyActionsRegex());
}

bool containsOnlyKeylessRuleAction(const std::string &input) {
  return std::regex_match(input, details::getOnlyKeylessRuleActionRegex());
}

bool containsOnlyKeyWithTextAction(const std::string &input) {
  return std::regex_match(input, details::getOnlyKeyWithTextActionRegex());
}

bool containsOnlyKeyWithRuleAction(const std::string &input) {
  return std::regex_match(input, details::getOnlyKeyWithRuleActionRegex());
}

bool containsOnlyRule(const std::string &input) {
  return std::regex_match(input, details::getOnlyRuleRegex());
}

bool containsOnlyRuleWithActions(const std::string &input) {
  return std::regex_match(input, details::getOnlyRuleWithActionsRegex());
}

bool containsParametricModifier(const std::string &input) {
  return std::regex_match(input, details::getParametricModifierRegex());
}
} // End namespace details

class TreeNode {
public:
  TreeNode()
      : input(), isNodeComplete_(false), isNodeExpanded_(false), prevLeaf(nullptr), nextLeaf(nullptr),
        prevUnexpandedLeaf(nullptr), nextUnexpandedLeaf(nullptr), isNodeHidden_(false) {
  }

  explicit TreeNode(const std::string &input,
                    std::shared_ptr<TreeNode> prev = nullptr,
                    std::shared_ptr<TreeNode> next = nullptr,
                    std::shared_ptr<TreeNode> prevUnexpanded = nullptr,
                    std::shared_ptr<TreeNode> nextUnexpanded = nullptr)
      : input(input), isNodeComplete_(!details::containsRule(input)
                                      && !details::containsOnlyActions(input)), isNodeExpanded_(isNodeComplete_),
        prevLeaf(std::move(prev)), nextLeaf(std::move(next)), prevUnexpandedLeaf(std::move(prevUnexpanded)),
        nextUnexpandedLeaf(std::move(nextUnexpanded)), isNodeHidden_(false) {
  }

  virtual ~TreeNode() = default;

  void addChild(const std::string &inputStr) {
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
    for (auto &child : this->children) {
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
      for (auto &mod : this->modifiers) {
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
        } else {
          std::cerr << "no mod " << modName << std::endl;
        }
      }
      return output;
    }

    if (!this->hasChildren()) {
      if (ignoreHidden && this->isNodeHidden())
        return "";
      return this->input;
    }

    std::string ret;
    for (auto &child : this->children) {
      ret += child->flatten(modFuns, ignoreHidden, false);
    }
    return ret;
  }

  void expandNode(const json &, std::mt19937 &, json &);

  const std::string &getInput() const { return this->input; }

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

  void addModifier(const std::string &mod) {
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

void TreeNode::expandNode(const json &jsonGrammar,
                          std::mt19937 &rng,
                          json &runtimeDictionary) {
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
                  [&mods](auto &mtch) {
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
      std::uniform_int_distribution<> dist(0, ruleContents.size() - 1);
      output = ruleContents[dist(rng)];
    }

    for (auto &modifier : mods) {
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
    json arr = json::array();
    std::for_each(std::sregex_token_iterator(txt.begin(),
                                             txt.end(),
                                             details::getCommaRegex(),
                                             -1),
                  std::sregex_token_iterator(),
                  [&arr, this, &key](auto &mtch) {
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
                  [this](auto &mtch) {
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
                  [this](auto &mtch) {
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
  Tree(const std::string &input,
       const json &grammar,
       std::mt19937 rng = std::mt19937(time(0)))
      : leafIndex(new TreeNode), unexpandedLeafIndex(new TreeNode), root(new TreeNode(input)),
        nextUnexpandedLeaf(nullptr), jsonGrammar(grammar), rng(rng) {
    this->root->setPrevLeaf(this->leafIndex);
    this->leafIndex->setNextLeaf(this->root);

    if (!this->root->isNodeExpanded()) {
      this->unexpandedLeafIndex->setNextUnexpandedLeaf(this->root);
      this->root->setPrevUnexpandedLeaf(this->unexpandedLeafIndex);
    }
  }

  bool expand(const details::callback_map_t &modFuns) {
    auto next = this->unexpandedLeafIndex->getNextUnexpandedLeaf();

    this->expandingNodes.push(next);

    next->expandNode(this->jsonGrammar,
                     this->rng,
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

  bool expandBF() {
    if (!this->nextUnexpandedLeaf) {
      if (this->unexpandedLeafIndex->hasNextUnexpandedLeaf()) {
        this->nextUnexpandedLeaf = this->unexpandedLeafIndex->getNextUnexpandedLeaf();
      } else {
        return false;
      }
    }

    std::shared_ptr<TreeNode> next = this->nextUnexpandedLeaf->getNextUnexpandedLeaf();
    this->nextUnexpandedLeaf->expandNode(this->jsonGrammar,
                                         this->rng,
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
  const json &jsonGrammar;
  std::mt19937 rng;
  json runtimeDictionary;
  std::stack<std::shared_ptr<TreeNode>> expandingNodes;
};

class Grammar {
public:
  explicit Grammar(json grammar = "{}"_json,
                   std::mt19937 _rng = std::mt19937(time(0)))
      : jsonGrammar(std::move(grammar)),
        rng(_rng) {
  }

  std::shared_ptr<Tree> getTree(const std::string &input) const {
    std::shared_ptr<Tree> tree(new Tree(input, this->jsonGrammar));
    return tree;
  }

  static const details::callback_map_t &getBaseEngModifiers() {
    static auto wrap = [](const std::function<std::string(const std::string &)> &fun) {
      std::shared_ptr<details::ICallback> ptr(new details::Callback<const std::string &>(fun));
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

    baseMods["a"] = wrap([](const std::string &input) {
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
    baseMods["capitalizeAll"] = wrap([](const std::string &input) {
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
    baseMods["capitalize"] = wrap([](const std::string &input) {
      std::string ret = input;
      ret[0] = toupper(ret[0]);
      return ret;
    });
    baseMods["s"] = wrap([](const std::string &input) {
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
    baseMods["ed"] = wrap([](const std::string &input) {
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
        std::shared_ptr<details::ICallback>(new details::Callback<const std::string &,
            const std::string &, const std::string &>([](const std::string &input,
                                                         const std::string &target,
                                                         const std::string &replacement) {
          return std::regex_replace(input,
                                    std::regex(target),
                                    replacement);
        }));
    return baseMods;
  }

  void addModifiers(const details::callback_map_t &mfs) {
    for (auto &mf : mfs) {
      this->addModifier(mf.first, mf.second);
    }
  }

  void addModifier(const std::string &name,
                   std::shared_ptr<details::ICallback> mod) {
    this->modifierFunctions[name] = std::move(mod);
  }

  details::callback_map_t getModifierFunctions() {
    return this->modifierFunctions;
  }

  std::string flatten(const std::string& input)
  {
    auto tree = this->getTree(input);
    while(tree->expand(this->getModifierFunctions()));
    return tree->flatten(this->getModifierFunctions());
  }

private:
  json jsonGrammar;
  std::mt19937 rng;
  details::callback_map_t modifierFunctions;
};

} // End namespace tracerz
