#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>

using namespace std;

// Type definitions for clarity.
typedef vector<string> Production;
typedef vector<Production> ProductionList;
typedef map<string, ProductionList> Grammar;
typedef map<string, vector<string>> StringVectorMap;  // For FIRST and FOLLOW sets

// Global vector to preserve non-terminal order.
vector<string> nonTerminalOrder;

// --------------------------
// Utility Functions & Parsing
// --------------------------

// Helper function: split a string by whitespace.
vector<string> split(const string &s) {
    vector<string> tokens;
    istringstream iss(s);
    string token;
    while (iss >> token)
        tokens.push_back(token);
    return tokens;
}

// Preprocess the input line to ensure tokens are separated by spaces.
string preprocessLine(const string &line) {
    string result;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '-' && i+1 < line.size() && line[i+1] == '>') {
            result += " -> ";
            i++; // skip '>'
        } else if (c == '|' ) {
            result += " | ";
        } else if (c == '+' || c == '*' || c == '(' || c == ')') {
            result += " ";
            result.push_back(c);
            result += " ";
        } else {
            result.push_back(c);
        }
    }
    return result;
}

// Parse a production line of the form: NonTerminal -> production1 | production2 | ...
void parseProductionLine(const string &line, Grammar &grammar) {
    string processedLine = preprocessLine(line);
    size_t arrowPos = processedLine.find("->");
    if (arrowPos == string::npos)
        return;
    
    string nonTerminal = processedLine.substr(0, arrowPos);
    nonTerminal.erase(remove(nonTerminal.begin(), nonTerminal.end(), ' '), nonTerminal.end());
    
    // Record nonTerminal order if not already present.
    if(find(nonTerminalOrder.begin(), nonTerminalOrder.end(), nonTerminal) == nonTerminalOrder.end()){
        nonTerminalOrder.push_back(nonTerminal);
    }
    
    string rhs = processedLine.substr(arrowPos + 2);
    while(!rhs.empty() && isspace(rhs[0]))
        rhs.erase(0,1);
    
    istringstream prodStream(rhs);
    string production;
    while(getline(prodStream, production, '|')) {
        while(!production.empty() && isspace(production.front()))
            production.erase(production.begin());
        while(!production.empty() && isspace(production.back()))
            production.pop_back();
        Production prodTokens = split(production);
        if(!prodTokens.empty())
            grammar[nonTerminal].push_back(prodTokens);
    }
}

// Read grammar from file.
Grammar readGrammarFromFile(const string &filename) {
    Grammar grammar;
    ifstream infile(filename);
    if (!infile.is_open()){
        cerr << "Error opening file: " << filename << "\n";
        return grammar;
    }
    string line;
    while(getline(infile, line)) {
        if(line.empty())
            continue;
        parseProductionLine(line, grammar);
    }
    infile.close();
    return grammar;
}

// --------------------------
// Left Factoring and Left Recursion Removal
// --------------------------

Production findLongestCommonPrefix(const vector<Production>& prods) {
    if(prods.empty())
        return {};
    Production prefix = prods[0];
    for(size_t i = 1; i < prods.size(); ++i) {
        Production temp;
        for(size_t j = 0; j < min(prefix.size(), prods[i].size()); ++j) {
            if(prefix[j] == prods[i][j])
                temp.push_back(prefix[j]);
            else
                break;
        }
        prefix = temp;
        if(prefix.empty())
            break;
    }
    return prefix;
}

void leftFactorNonTerminal(const string &nonTerminal, Grammar &grammar) {
    ProductionList &prods = grammar[nonTerminal];
    bool factoringOccurred = false;
    
    // Group productions by their first token.
    map<string, vector<Production>> groups;
    for(auto &prod : prods) {
        if(!prod.empty())
            groups[prod[0]].push_back(prod);
    }
    
    ProductionList newProds;
    for(auto &groupPair : groups) {
        vector<Production> groupProds = groupPair.second;
        if(groupProds.size() == 1) {
            newProds.push_back(groupProds[0]);
        } else {
            Production commonPrefix = findLongestCommonPrefix(groupProds);
            if(commonPrefix.empty()){
                for(auto &p : groupProds)
                    newProds.push_back(p);
            } else {
                factoringOccurred = true;
                string newNonTerminal = nonTerminal + "'";
                if(find(nonTerminalOrder.begin(), nonTerminalOrder.end(), newNonTerminal) == nonTerminalOrder.end())
                    nonTerminalOrder.push_back(newNonTerminal);
                
                Production newProd = commonPrefix;
                newProd.push_back(newNonTerminal);
                newProds.push_back(newProd);
                
                ProductionList newNTProds;
                for(auto &p : groupProds) {
                    Production remainder(p.begin() + commonPrefix.size(), p.end());
                    if(remainder.empty())
                        remainder.push_back("ε");
                    newNTProds.push_back(remainder);
                }
                if(grammar.find(newNonTerminal) != grammar.end())
                    grammar[newNonTerminal].insert(grammar[newNonTerminal].end(), newNTProds.begin(), newNTProds.end());
                else
                    grammar[newNonTerminal] = newNTProds;
            }
        }
    }
    if(factoringOccurred)
        grammar[nonTerminal] = newProds;
}

void leftFactorGrammar(Grammar &grammar) {
    for(size_t i = 0; i < nonTerminalOrder.size(); ++i) {
        string nt = nonTerminalOrder[i];
        if(grammar.find(nt) != grammar.end())
            leftFactorNonTerminal(nt, grammar);
    }
}

void removeLeftRecursionForNonTerminal(const string &nonTerminal, Grammar &grammar) {
    ProductionList prods = grammar[nonTerminal];
    ProductionList newProds, alpha, beta;
    
    for(auto &prod : prods) {
        if(!prod.empty() && prod[0] == nonTerminal) {
            Production remainder(prod.begin() + 1, prod.end());
            alpha.push_back(remainder);
        } else {
            beta.push_back(prod);
        }
    }
    if(alpha.empty())
        return;
    
    string newNonTerminal = nonTerminal + "'";
    if(find(nonTerminalOrder.begin(), nonTerminalOrder.end(), newNonTerminal) == nonTerminalOrder.end())
        nonTerminalOrder.push_back(newNonTerminal);
    
    for(auto &prod : beta) {
        Production newProd = prod;
        newProd.push_back(newNonTerminal);
        newProds.push_back(newProd);
    }
    grammar[nonTerminal] = newProds;
    
    ProductionList newNTProds;
    for(auto &prod : alpha) {
        Production newProd = prod;
        newProd.push_back(newNonTerminal);
        newNTProds.push_back(newProd);
    }
    Production epsilonProd; epsilonProd.push_back("ε");
    newNTProds.push_back(epsilonProd);
    
    if(grammar.find(newNonTerminal) != grammar.end())
        grammar[newNonTerminal].insert(grammar[newNonTerminal].end(), newNTProds.begin(), newNTProds.end());
    else
        grammar[newNonTerminal] = newNTProds;
}

void removeLeftRecursion(Grammar &grammar) {
    for(size_t i = 0; i < nonTerminalOrder.size(); ++i) {
        string nt = nonTerminalOrder[i];
        if(grammar.find(nt) != grammar.end())
            removeLeftRecursionForNonTerminal(nt, grammar);
    }
}

// --------------------------
// FIRST Set Computation (already defined)
// --------------------------
bool contains(const vector<string>& vec, const string &s) {
    return find(vec.begin(), vec.end(), s) != vec.end();
}

StringVectorMap computeFirstSets(const Grammar &grammar) {
    StringVectorMap first;
    // Initialize FIRST for each non-terminal.
    for(auto &nt : nonTerminalOrder) {
        if(grammar.find(nt) != grammar.end())
            first[nt] = vector<string>();
    }
    
    bool changed = true;
    while(changed) {
        changed = false;
        for(auto &nt : nonTerminalOrder) {
            if(grammar.find(nt) == grammar.end()) continue;
            ProductionList prods = grammar.at(nt);
            for(auto &prod : prods) {
                if(prod.size() == 1 && prod[0] == "ε") {
                    if(!contains(first[nt], "ε")) {
                        first[nt].push_back("ε");
                        changed = true;
                    }
                    continue;
                }
                bool allHaveEpsilon = true;
                for(auto &symbol : prod) {
                    // Terminal: not a key in grammar.
                    if(grammar.find(symbol) == grammar.end()) {
                        if(symbol != "ε" && !contains(first[nt], symbol)) {
                            first[nt].push_back(symbol);
                            changed = true;
                        }
                        allHaveEpsilon = false;
                        break;
                    } else {
                        for(auto &s : first[symbol]) {
                            if(s != "ε" && !contains(first[nt], s)) {
                                first[nt].push_back(s);
                                changed = true;
                            }
                        }
                        if(!contains(first[symbol], "ε")) {
                            allHaveEpsilon = false;
                            break;
                        }
                    }
                }
                if(allHaveEpsilon && !contains(first[nt], "ε")) {
                    first[nt].push_back("ε");
                    changed = true;
                }
            }
        }
    }
    return first;
}

// --------------------------
// FOLLOW Set Computation
// --------------------------
//
// The FOLLOW set of a non-terminal A is the set of terminals that can appear immediately to the right of A.
// Algorithm:
// 1. Place "$" (end-of-input marker) in FOLLOW(S) where S is the start symbol.
// 2. For each production A -> αBβ, add FIRST(β) (except for epsilon) to FOLLOW(B).
// 3. If FIRST(β) contains epsilon (or β is empty), add FOLLOW(A) to FOLLOW(B).
// 4. Iterate until no changes occur.
//
StringVectorMap computeFollowSets(const Grammar &grammar, const StringVectorMap &first) {
    StringVectorMap follow;
    // Initialize FOLLOW sets for each non-terminal.
    for(auto &nt : nonTerminalOrder) {
        if(grammar.find(nt) != grammar.end())
            follow[nt] = vector<string>();
    }
    // Assume the first non-terminal is the start symbol; add "$" to its FOLLOW set.
    if(!nonTerminalOrder.empty() && follow.find(nonTerminalOrder[0]) != follow.end()){
        follow[nonTerminalOrder[0]].push_back("$");
    }
    
    bool changed = true;
    while(changed) {
        changed = false;
        // For each production A -> α.
        for(auto &A : nonTerminalOrder) {
            if(grammar.find(A) == grammar.end()) continue;
            ProductionList prods = grammar.at(A);
            for(auto &prod : prods) {
                // For each symbol in the production.
                for(size_t i = 0; i < prod.size(); ++i) {
                    string B = prod[i];
                    // Only process non-terminals.
                    if(grammar.find(B) == grammar.end())
                        continue;
                    // Let beta be the rest of the production after B.
                    vector<string> beta(prod.begin() + i + 1, prod.end());
                    vector<string> firstBeta;
                    // Compute FIRST(beta):
                    if(beta.empty()){
                        // If beta is empty, then add FOLLOW(A) to FOLLOW(B).
                        for(auto &sym : follow[A]) {
                            if(!contains(follow[B], sym)){
                                follow[B].push_back(sym);
                                changed = true;
                            }
                        }
                    } else {
                        bool betaDerivesEpsilon = true;
                        for(auto &symbol : beta) {
                            // If symbol is terminal:
                            if(grammar.find(symbol) == grammar.end()){
                                if(symbol != "ε" && !contains(firstBeta, symbol))
                                    firstBeta.push_back(symbol);
                                betaDerivesEpsilon = false;
                                break;
                            } else {
                                // symbol is non-terminal; add its FIRST (except epsilon)
                                for(auto &s : first.at(symbol)) {
                                    if(s != "ε" && !contains(firstBeta, s))
                                        firstBeta.push_back(s);
                                }
                                if(!contains(first.at(symbol), "ε")){
                                    betaDerivesEpsilon = false;
                                    break;
                                }
                            }
                        }
                        // Add FIRST(beta) (without epsilon) to FOLLOW(B).
                        for(auto &sym : firstBeta) {
                            if(!contains(follow[B], sym)){
                                follow[B].push_back(sym);
                                changed = true;
                            }
                        }
                        // If beta derives epsilon, add FOLLOW(A) to FOLLOW(B).
                        if(betaDerivesEpsilon){
                            for(auto &sym : follow[A]) {
                                if(!contains(follow[B], sym)){
                                    follow[B].push_back(sym);
                                    changed = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return follow;
}

// --------------------------
// Printing Functions
// --------------------------

void printGrammar(const Grammar &grammar) {
    for(auto &nt : nonTerminalOrder) {
        if(grammar.find(nt) == grammar.end()) continue;
        cout << nt << " -> ";
        ProductionList prods = grammar.at(nt);
        for(size_t i = 0; i < prods.size(); ++i) {
            for(auto &token : prods[i])
                cout << token << " ";
            if(i != prods.size()-1)
                cout << "| ";
        }
        cout << "\n";
    }
}

void printFirstSets(const StringVectorMap &first) {
    cout << "\nFIRST Sets:\n";
    for(auto &nt : nonTerminalOrder) {
        if(first.find(nt) == first.end()) continue;
        cout << "FIRST(" << nt << ") = { ";
        for(size_t i = 0; i < first.at(nt).size(); ++i) {
            cout << first.at(nt)[i];
            if(i != first.at(nt).size()-1)
                cout << ", ";
        }
        cout << " }\n";
    }
}

void printFollowSets(const StringVectorMap &follow) {
    cout << "\nFOLLOW Sets:\n";
    for(auto &nt : nonTerminalOrder) {
        if(follow.find(nt) == follow.end()) continue;
        cout << "FOLLOW(" << nt << ") = { ";
        for(size_t i = 0; i < follow.at(nt).size(); ++i) {
            cout << follow.at(nt)[i];
            if(i != follow.at(nt).size()-1)
                cout << ", ";
        }
        cout << " }\n";
    }
}

// Detailed printing for FIRST sets as per previous format.
string detailedAltStr(const Production &alt, const Grammar &grammar) {
    if(alt.empty())
        return "";
    if(grammar.find(alt[0]) == grammar.end()) {
        return "{ " + alt[0] + " }";
    } else {
        string s;
        for(auto token : alt)
            s += token;
        return "FIRST(" + s + ")";
    }
}

void printDetailedFirstSets(const Grammar &grammar, const StringVectorMap &first) {
    cout << "\nFIRST setsg\n";
    for(auto &nt : nonTerminalOrder) {
        if(grammar.find(nt) == grammar.end()) continue;
        cout << "FIRST(" << nt << ") = ";
        ProductionList prods = grammar.at(nt);
        if(prods.size() == 1) {
            cout << detailedAltStr(prods[0], grammar);
        } else {
            for(size_t i = 0; i < prods.size(); ++i) {
                cout << detailedAltStr(prods[i], grammar);
                if(i != prods.size()-1)
                    cout << " U ";
            }
        }
        cout << "\n         = { ";
        const vector<string> &vec = first.at(nt);
        for(size_t i = 0; i < vec.size(); ++i) {
            cout << vec[i];
            if(i != vec.size()-1)
                cout << ", ";
        }
        cout << " }\n";
    }
}

// --------------------------
// Main Function
// --------------------------

int main() {
    string filename = "grammar.txt";
    Grammar grammar = readGrammarFromFile(filename);
    
    cout << "Original Grammar:\n";
    printGrammar(grammar);
    cout << "\n----------------------\n";
    
    leftFactorGrammar(grammar);
    cout << "Grammar after Left Factoring:\n";
    printGrammar(grammar);
    cout << "\n----------------------\n";
    
    removeLeftRecursion(grammar);
    cout << "Grammar after Left Recursion Removal:\n";
    printGrammar(grammar);
    cout << "\n----------------------\n";
    
    StringVectorMap first = computeFirstSets(grammar);
    printDetailedFirstSets(grammar, first);
    
    // Compute and print FOLLOW sets.
    StringVectorMap follow = computeFollowSets(grammar, first);
    printFollowSets(follow);
    
    return 0;
}
