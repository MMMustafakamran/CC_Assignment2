#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>

using namespace std;

// Type definitions for clarity.
typedef vector<string> Production;
typedef vector<Production> ProductionList;
typedef map<string, ProductionList> Grammar;

// Helper function: split a string by whitespace.
vector<string> split(const string &s) {
    vector<string> tokens;
    istringstream iss(s);
    string token;
    while(iss >> token)
        tokens.push_back(token);
    return tokens;
}

// Parse a line of the form: NonTerminal -> production1 | production2 | ...
// Each production is split into tokens.
void parseProductionLine(const string &line, Grammar &grammar) {
    size_t arrowPos = line.find("->");
    if (arrowPos == string::npos) return; // skip malformed lines

    // Get the non-terminal (trim spaces)
    string nonTerminal = line.substr(0, arrowPos);
    // Remove trailing spaces from nonTerminal.
    nonTerminal.erase(remove(nonTerminal.begin(), nonTerminal.end(), ' '), nonTerminal.end());
    
    string rhs = line.substr(arrowPos + 2); // production part after "->"
    // Remove leading spaces.
    while (!rhs.empty() && isspace(rhs[0]))
        rhs.erase(0, 1);

    // Split productions separated by '|'
    istringstream prodStream(rhs);
    string production;
    while(getline(prodStream, production, '|')) {
        // Trim leading/trailing spaces.
        while (!production.empty() && isspace(production.front()))
            production.erase(production.begin());
        while (!production.empty() && isspace(production.back()))
            production.pop_back();

        Production prodTokens = split(production);
        if (!prodTokens.empty())
            grammar[nonTerminal].push_back(prodTokens);
    }
}

// Function to read a grammar from a file.
Grammar readGrammarFromFile(const string &filename) {
    Grammar grammar;
    ifstream infile(filename);
    if (!infile.is_open()) {
        cerr << "Error opening file: " << filename << "\n";
        return grammar;
    }
    string line;
    while(getline(infile, line)) {
        if (line.empty()) continue;
        parseProductionLine(line, grammar);
    }
    infile.close();
    return grammar;
}

// Find the longest common prefix among a list of productions.
Production findLongestCommonPrefix(const vector<Production>& prods) {
    if(prods.empty()) return {};
    Production prefix = prods[0];
    for (size_t i = 1; i < prods.size(); ++i) {
        Production temp;
        for (size_t j = 0; j < min(prefix.size(), prods[i].size()); ++j) {
            if (prefix[j] == prods[i][j])
                temp.push_back(prefix[j]);
            else
                break;
        }
        prefix = temp;
        if(prefix.empty()) break;
    }
    return prefix;
}

// Left factor productions for a given non-terminal.
void leftFactorNonTerminal(const string &nonTerminal, Grammar &grammar) {
    ProductionList &prods = grammar[nonTerminal];
    bool factoringOccurred = false;

    // For each production, try to group those with a common prefix.
    // We'll use a simple approach: check all pairs and group them.
    map<string, vector<Production>> groups;
    // Group productions by their first token.
    for (auto &prod : prods) {
        if (!prod.empty()) {
            groups[prod[0]].push_back(prod);
        }
    }
    
    // New productions for nonTerminal after factoring.
    ProductionList newProds;
    
    // Process each group.
    for (auto &groupPair : groups) {
        vector<Production> groupProds = groupPair.second;
        if (groupProds.size() == 1) {
            // Only one production in this group, keep as is.
            newProds.push_back(groupProds[0]);
        } else {
            // More than one production share the same first token.
            Production commonPrefix = findLongestCommonPrefix(groupProds);
            if (commonPrefix.empty()) {
                // Should not happen, but just in case.
                for (auto &p : groupProds)
                    newProds.push_back(p);
            } else {
                factoringOccurred = true;
                // Create a new non-terminal name, e.g., A' (if nonTerminal is A).
                string newNonTerminal = nonTerminal + "'";
                
                // Production for original non-terminal: commonPrefix newNonTerminal
                Production newProd = commonPrefix;
                newProd.push_back(newNonTerminal);
                newProds.push_back(newProd);
                
                // For the new non-terminal, add productions that are the remainder.
                ProductionList newNTProds;
                for (auto &p : groupProds) {
                    // Remove the common prefix tokens.
                    Production remainder(p.begin() + commonPrefix.size(), p.end());
                    if (remainder.empty()) {
                        // If nothing remains, use epsilon (represented here by "ε").
                        remainder.push_back("ε");
                    }
                    newNTProds.push_back(remainder);
                }
                
                // If newNonTerminal already exists, append productions; otherwise add new rule.
                if (grammar.find(newNonTerminal) != grammar.end()) {
                    // Append productions.
                    grammar[newNonTerminal].insert(grammar[newNonTerminal].end(),
                                                   newNTProds.begin(), newNTProds.end());
                } else {
                    grammar[newNonTerminal] = newNTProds;
                }
            }
        }
    }
    
    // Replace the productions of the current non-terminal with the new ones.
    if (factoringOccurred) {
        grammar[nonTerminal] = newProds;
    }
}

// Perform left factoring on the entire grammar.
void leftFactorGrammar(Grammar &grammar) {
    // We'll iterate over the grammar keys. Note that new non-terminals
    // might be added during factoring, so iterate until no changes occur.
    vector<string> nonTerminals;
    for (auto &pair : grammar)
        nonTerminals.push_back(pair.first);
    
    for (size_t i = 0; i < nonTerminals.size(); ++i) {
        string nt = nonTerminals[i];
        leftFactorNonTerminal(nt, grammar);
        // If new non-terminals were added, add them to our list.
        for (auto &pair : grammar) {
            if (find(nonTerminals.begin(), nonTerminals.end(), pair.first) == nonTerminals.end())
                nonTerminals.push_back(pair.first);
        }
    }
}

// Utility function to print the grammar.
void printGrammar(const Grammar &grammar) {
    for (auto &rule : grammar) {
        cout << rule.first << " -> ";
        bool firstProd = true;
        for (auto &prod : rule.second) {
            if (!firstProd) cout << " | ";
            for (auto &token : prod) {
                cout << token << " ";
            }
            firstProd = false;
        }
        cout << "\n";
    }
}

int main() {
    // Change the filename as needed.
    string filename = "grammar.txt";
    
    Grammar grammar = readGrammarFromFile(filename);
    
    cout << "Original Grammar:\n";
    printGrammar(grammar);
    cout << "\n----------------------\n";
    
    // Perform left factoring.
    leftFactorGrammar(grammar);
    
    cout << "Grammar after Left Factoring:\n";
    printGrammar(grammar);
    
    return 0;
}
