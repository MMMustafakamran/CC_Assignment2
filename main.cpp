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
// We insert spaces around operators and parentheses: +, *, (, and )
string preprocessLine(const string &line) {
    string result;
    // We'll leave the arrow "->" unchanged.
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '-' && i + 1 < line.size() && line[i+1] == '>') {
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

// Parse a line of the form: NonTerminal -> production1 | production2 | ...
// Each production is split into tokens.
void parseProductionLine(const string &line, Grammar &grammar) {
    // Preprocess the line to add spaces around operators.
    string processedLine = preprocessLine(line);
    
    size_t arrowPos = processedLine.find("->");
    if (arrowPos == string::npos)
        return; // Skip malformed lines.

    // Get the non-terminal (trim spaces).
    string nonTerminal = processedLine.substr(0, arrowPos);
    // Remove any extra spaces.
    nonTerminal.erase(remove(nonTerminal.begin(), nonTerminal.end(), ' '), nonTerminal.end());
    
    string rhs = processedLine.substr(arrowPos + 2); // production part after "->"
    // Remove leading spaces.
    while (!rhs.empty() && isspace(rhs[0]))
        rhs.erase(0, 1);

    // Split productions separated by '|'
    istringstream prodStream(rhs);
    string production;
    while (getline(prodStream, production, '|')) {
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
    while (getline(infile, line)) {
        if (line.empty())
            continue;
        parseProductionLine(line, grammar);
    }
    infile.close();
    return grammar;
}

// Find the longest common prefix among a list of productions.
Production findLongestCommonPrefix(const vector<Production>& prods) {
    if (prods.empty())
        return {};
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
        if (prefix.empty())
            break;
    }
    return prefix;
}

// Left factor productions for a given non-terminal.
void leftFactorNonTerminal(const string &nonTerminal, Grammar &grammar) {
    ProductionList &prods = grammar[nonTerminal];
    bool factoringOccurred = false;

    // Group productions by their first token.
    map<string, vector<Production>> groups;
    for (auto &prod : prods) {
        if (!prod.empty()) {
            groups[prod[0]].push_back(prod);
        }
    }
    
    // New productions for the non-terminal after factoring.
    ProductionList newProds;
    
    // Process each group.
    for (auto &groupPair : groups) {
        vector<Production> groupProds = groupPair.second;
        if (groupProds.size() == 1) {
            newProds.push_back(groupProds[0]);
        } else {
            Production commonPrefix = findLongestCommonPrefix(groupProds);
            if (commonPrefix.empty()) {
                for (auto &p : groupProds)
                    newProds.push_back(p);
            } else {
                factoringOccurred = true;
                // Create a new non-terminal name, e.g., A' (if nonTerminal is A).
                string newNonTerminal = nonTerminal + "'";
                
                // Production for original non-terminal: commonPrefix newNonTerminal.
                Production newProd = commonPrefix;
                newProd.push_back(newNonTerminal);
                newProds.push_back(newProd);
                
                // For the new non-terminal, add productions that are the remainder.
                ProductionList newNTProds;
                for (auto &p : groupProds) {
                    Production remainder(p.begin() + commonPrefix.size(), p.end());
                    if (remainder.empty()) {
                        remainder.push_back("ε");
                    }
                    newNTProds.push_back(remainder);
                }
                
                if (grammar.find(newNonTerminal) != grammar.end()) {
                    grammar[newNonTerminal].insert(grammar[newNonTerminal].end(), newNTProds.begin(), newNTProds.end());
                } else {
                    grammar[newNonTerminal] = newNTProds;
                }
            }
        }
    }
    
    if (factoringOccurred) {
        grammar[nonTerminal] = newProds;
    }
}

// Perform left factoring on the entire grammar.
void leftFactorGrammar(Grammar &grammar) {
    vector<string> nonTerminals;
    for (auto &pair : grammar)
        nonTerminals.push_back(pair.first);
    
    for (size_t i = 0; i < nonTerminals.size(); ++i) {
        string nt = nonTerminals[i];
        leftFactorNonTerminal(nt, grammar);
        for (auto &pair : grammar) {
            if (find(nonTerminals.begin(), nonTerminals.end(), pair.first) == nonTerminals.end())
                nonTerminals.push_back(pair.first);
        }
    }
}

// ------------------------
// Left Recursion Removal
// ------------------------

// Remove immediate left recursion for a given non-terminal.
void removeLeftRecursionForNonTerminal(const string &nonTerminal, Grammar &grammar) {
    ProductionList prods = grammar[nonTerminal];
    ProductionList newProds;
    ProductionList alpha; // Productions where left recursion occurs (A -> Aα).
    ProductionList beta;  // Productions without left recursion (A -> β).

    // Partition productions into alpha and beta.
    for (auto &prod : prods) {
        if (!prod.empty() && prod[0] == nonTerminal) {
            // Production is left-recursive.
            Production remainder(prod.begin() + 1, prod.end());
            alpha.push_back(remainder);
        } else {
            beta.push_back(prod);
        }
    }
    
    // If no left recursion exists, nothing to do.
    if (alpha.empty())
        return;
    
    // Create a new non-terminal, e.g., A'.
    string newNonTerminal = nonTerminal + "'";
    
    // For every production A -> β, replace it with A -> β newNonTerminal.
    for (auto &prod : beta) {
        Production newProd = prod;
        newProd.push_back(newNonTerminal);
        newProds.push_back(newProd);
    }
    grammar[nonTerminal] = newProds;
    
    // For the new non-terminal, for every production A -> Aα, add A' -> α A'
    ProductionList newNTProds;
    for (auto &prod : alpha) {
        Production newProd = prod;
        newProd.push_back(newNonTerminal);
        newNTProds.push_back(newProd);
    }
    // Also add A' -> ε.
    Production epsilonProd;
    epsilonProd.push_back("ε");
    newNTProds.push_back(epsilonProd);
    
    // Update the grammar with the new non-terminal.
    if (grammar.find(newNonTerminal) != grammar.end()) {
        grammar[newNonTerminal].insert(grammar[newNonTerminal].end(), newNTProds.begin(), newNTProds.end());
    } else {
        grammar[newNonTerminal] = newNTProds;
    }
}

// Remove immediate left recursion from the entire grammar.
void removeLeftRecursion(Grammar &grammar) {
    vector<string> nonTerminals;
    for (auto &pair : grammar)
        nonTerminals.push_back(pair.first);
    
    for (auto &nt : nonTerminals) {
        removeLeftRecursionForNonTerminal(nt, grammar);
    }
}

// Utility function to print the grammar.
void printGrammar(const Grammar &grammar) {
    for (auto &rule : grammar) {
        cout << rule.first << " -> ";
        bool firstProd = true;
        for (auto &prod : rule.second) {
            if (!firstProd)
                cout << " | ";
            for (auto &token : prod) {
                cout << token << " ";
            }
            firstProd = false;
        }
        cout << "\n";
    }
}

int main() {
    // Change the filename if necessary.
    string filename = "grammar.txt";
    
    Grammar grammar = readGrammarFromFile(filename);
    
    cout << "Original Grammar:\n";
    printGrammar(grammar);
    cout << "\n----------------------\n";
    
    // Step 1: Perform left factoring.
    leftFactorGrammar(grammar);
    cout << "Grammar after Left Factoring:\n";
    printGrammar(grammar);
    cout << "\n----------------------\n";
    
    // Step 2: Remove left recursion.
    removeLeftRecursion(grammar);
    cout << "Grammar after Left Recursion Removal:\n";
    printGrammar(grammar);
    
    return 0;
}
