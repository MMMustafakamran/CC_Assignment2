#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <iomanip>

using namespace std;

// -------------------------------------------------------------------
// Type Definitions
// -------------------------------------------------------------------
typedef vector<string> Production;
typedef vector<Production> ProductionList;
typedef map<string, ProductionList> Grammar;
typedef map<string, vector<string>> StringVectorMap;  
typedef map<string, map<string, Production>> ParsingTable;

// We maintain an ordered list of non-terminals as they appear.
vector<string> nonTerminalOrder;

// -------------------------------------------------------------------
// Utility: Checking membership in a vector
// -------------------------------------------------------------------
bool contains(const vector<string>& vec, const string &s) {
    return find(vec.begin(), vec.end(), s) != vec.end();
}

// -------------------------------------------------------------------
// 1) Reading & Storing the Grammar
// -------------------------------------------------------------------
vector<string> split(const string &s) {
    vector<string> tokens;
    istringstream iss(s);
    string token;
    while (iss >> token)
        tokens.push_back(token);
    return tokens;
}

// Insert spaces around certain symbols to help tokenize.
string preprocessLine(const string &line) {
    string result;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '-' && i+1 < line.size() && line[i+1] == '>') {
            result += " -> ";
            i++;
        } else if (c == '|') {
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

void parseProductionLine(const string &line, Grammar &grammar) {
    string processed = preprocessLine(line);
    size_t arrowPos = processed.find("->");
    if (arrowPos == string::npos) return;

    string nonTerminal = processed.substr(0, arrowPos);
    // Trim spaces
    nonTerminal.erase(remove(nonTerminal.begin(), nonTerminal.end(), ' '), nonTerminal.end());
    
    // Record the non-terminal order if not already present
    if(find(nonTerminalOrder.begin(), nonTerminalOrder.end(), nonTerminal) == nonTerminalOrder.end()) {
        nonTerminalOrder.push_back(nonTerminal);
    }

    string rhs = processed.substr(arrowPos + 2);
    while(!rhs.empty() && isspace(rhs[0])) rhs.erase(rhs.begin());

    // Split by '|'
    istringstream iss(rhs);
    string production;
    while(getline(iss, production, '|')) {
        while(!production.empty() && isspace(production.front())) production.erase(production.begin());
        while(!production.empty() && isspace(production.back()))  production.pop_back();
        Production prodTokens = split(production);
        if(!prodTokens.empty()) {
            grammar[nonTerminal].push_back(prodTokens);
        }
    }
}

Grammar readGrammarFromFile(const string &filename) {
    Grammar grammar;
    ifstream infile(filename);
    if (!infile.is_open()) {
        cerr << "Error opening file: " << filename << "\n";
        return grammar;
    }
    string line;
    while(getline(infile, line)) {
        if(line.empty()) continue;
        parseProductionLine(line, grammar);
    }
    infile.close();
    return grammar;
}

// -------------------------------------------------------------------
// 2) Left Factoring
// -------------------------------------------------------------------
Production findLongestCommonPrefix(const vector<Production>& prods) {
    if (prods.empty()) return {};
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
        if(prefix.empty()) break;
    }
    return prefix;
}

void leftFactorNonTerminal(const string &nonTerminal, Grammar &grammar) {
    ProductionList &prods = grammar[nonTerminal];
    bool factoringOccurred = false;

    // Group productions by their first token
    map<string, vector<Production>> groups;
    for(auto &prod : prods) {
        if(!prod.empty()) {
            groups[prod[0]].push_back(prod);
        }
    }

    ProductionList newProds;
    for(auto &groupPair : groups) {
        vector<Production> groupProds = groupPair.second;
        if(groupProds.size() == 1) {
            newProds.push_back(groupProds[0]);
        } else {
            // They share a common prefix
            Production commonPrefix = findLongestCommonPrefix(groupProds);
            if(commonPrefix.empty()) {
                // No common prefix
                for(auto &p : groupProds) newProds.push_back(p);
            } else {
                factoringOccurred = true;
                // Create new non-terminal
                string newNT = nonTerminal + "'";
                if(find(nonTerminalOrder.begin(), nonTerminalOrder.end(), newNT) == nonTerminalOrder.end()) {
                    nonTerminalOrder.push_back(newNT);
                }
                // The factored production: A -> prefix A'
                Production factoredProd = commonPrefix;
                factoredProd.push_back(newNT);
                newProds.push_back(factoredProd);

                // The new productions for A'
                ProductionList newNTProds;
                for(auto &p : groupProds) {
                    Production remainder(p.begin() + commonPrefix.size(), p.end());
                    if(remainder.empty()) remainder.push_back("ε");
                    newNTProds.push_back(remainder);
                }
                grammar[newNT].insert(grammar[newNT].end(), newNTProds.begin(), newNTProds.end());
            }
        }
    }

    if(factoringOccurred) {
        grammar[nonTerminal] = newProds;
    }
}

void leftFactorGrammar(Grammar &grammar) {
    for(size_t i = 0; i < nonTerminalOrder.size(); ++i) {
        string nt = nonTerminalOrder[i];
        if(grammar.find(nt) != grammar.end()) {
            leftFactorNonTerminal(nt, grammar);
        }
    }
}

// -------------------------------------------------------------------
// 3) Left Recursion Removal
// -------------------------------------------------------------------
void removeLeftRecursionForNonTerminal(const string &nonTerminal, Grammar &grammar) {
    ProductionList &prods = grammar[nonTerminal];
    ProductionList alpha, beta;
    for(auto &prod : prods) {
        if(!prod.empty() && prod[0] == nonTerminal) {
            // left recursion
            Production remainder(prod.begin()+1, prod.end());
            alpha.push_back(remainder);
        } else {
            beta.push_back(prod);
        }
    }
    if(alpha.empty()) return;

    string newNT = nonTerminal + "'";
    if(find(nonTerminalOrder.begin(), nonTerminalOrder.end(), newNT) == nonTerminalOrder.end()) {
        nonTerminalOrder.push_back(newNT);
    }

    // A -> β A'
    ProductionList newProds;
    for(auto &b : beta) {
        Production temp = b;
        temp.push_back(newNT);
        newProds.push_back(temp);
    }
    grammar[nonTerminal] = newProds;

    // A' -> α A' | ε
    ProductionList newNTProds;
    for(auto &a : alpha) {
        Production temp = a;
        temp.push_back(newNT);
        newNTProds.push_back(temp);
    }
    Production epsilonProd; 
    epsilonProd.push_back("ε");
    newNTProds.push_back(epsilonProd);

    grammar[newNT].insert(grammar[newNT].end(), newNTProds.begin(), newNTProds.end());
}

void removeLeftRecursion(Grammar &grammar) {
    for(size_t i = 0; i < nonTerminalOrder.size(); ++i) {
        string nt = nonTerminalOrder[i];
        if(grammar.find(nt) != grammar.end()) {
            removeLeftRecursionForNonTerminal(nt, grammar);
        }
    }
}

// -------------------------------------------------------------------
// 4) FIRST Set Computation
// -------------------------------------------------------------------
StringVectorMap computeFirstSets(const Grammar &grammar) {
    StringVectorMap first;
    // Initialize
    for(auto &nt : nonTerminalOrder) {
        if(grammar.find(nt) != grammar.end()) {
            first[nt] = vector<string>();
        }
    }

    bool changed = true;
    while(changed) {
        changed = false;
        for(auto &nt : nonTerminalOrder) {
            if(grammar.find(nt) == grammar.end()) continue;
            ProductionList prods = grammar.at(nt);
            for(auto &prod : prods) {
                // If production is ε
                if(prod.size() == 1 && prod[0] == "ε") {
                    if(!contains(first[nt], "ε")) {
                        first[nt].push_back("ε");
                        changed = true;
                    }
                    continue;
                }
                bool allEpsilon = true;
                for(auto &symbol : prod) {
                    // If terminal
                    if(grammar.find(symbol) == grammar.end()) {
                        if(symbol != "ε" && !contains(first[nt], symbol)) {
                            first[nt].push_back(symbol);
                            changed = true;
                        }
                        allEpsilon = false;
                        break;
                    } else {
                        // symbol is non-terminal
                        for(auto &s : first[symbol]) {
                            if(s != "ε" && !contains(first[nt], s)) {
                                first[nt].push_back(s);
                                changed = true;
                            }
                        }
                        if(!contains(first[symbol], "ε")) {
                            allEpsilon = false;
                            break;
                        }
                    }
                }
                if(allEpsilon && !contains(first[nt], "ε")) {
                    first[nt].push_back("ε");
                    changed = true;
                }
            }
        }
    }
    return first;
}

// -------------------------------------------------------------------
// 5) FOLLOW Set Computation
// -------------------------------------------------------------------
StringVectorMap computeFollowSets(const Grammar &grammar, const StringVectorMap &first) {
    StringVectorMap follow;
    // Initialize
    for(auto &nt : nonTerminalOrder) {
        if(grammar.find(nt) != grammar.end()) {
            follow[nt] = vector<string>();
        }
    }
    // Add "$" to FOLLOW of start symbol
    if(!nonTerminalOrder.empty()) {
        if(follow.find(nonTerminalOrder[0]) != follow.end()) {
            follow[nonTerminalOrder[0]].push_back("$");
        }
    }

    bool changed = true;
    while(changed) {
        changed = false;
        for(auto &A : nonTerminalOrder) {
            if(grammar.find(A) == grammar.end()) continue;
            ProductionList prods = grammar.at(A);
            for(auto &prod : prods) {
                for(size_t i = 0; i < prod.size(); ++i) {
                    string B = prod[i];
                    // Only for non-terminals
                    if(grammar.find(B) == grammar.end()) continue;

                    // Beta = everything after B
                    vector<string> beta(prod.begin()+i+1, prod.end());
                    bool betaDerivesEpsilon = true;
                    vector<string> firstBeta;
                    
                    if(beta.empty()) {
                        betaDerivesEpsilon = true;
                    } else {
                        for(auto &symbol : beta) {
                            // If terminal
                            if(grammar.find(symbol) == grammar.end()) {
                                if(symbol != "ε" && !contains(firstBeta, symbol)) {
                                    firstBeta.push_back(symbol);
                                }
                                betaDerivesEpsilon = false;
                                break;
                            } else {
                                // Non-terminal
                                for(auto &s : first.at(symbol)) {
                                    if(s != "ε" && !contains(firstBeta, s)) {
                                        firstBeta.push_back(s);
                                    }
                                }
                                if(!contains(first.at(symbol), "ε")) {
                                    betaDerivesEpsilon = false;
                                    break;
                                }
                            }
                        }
                    }
                    // Add FIRST(beta)\{ε} to FOLLOW(B)
                    for(auto &sym : firstBeta) {
                        if(!contains(follow[B], sym)) {
                            follow[B].push_back(sym);
                            changed = true;
                        }
                    }
                    // If beta is empty or derives ε, add FOLLOW(A) to FOLLOW(B)
                    if(betaDerivesEpsilon) {
                        for(auto &sym : follow[A]) {
                            if(!contains(follow[B], sym)) {
                                follow[B].push_back(sym);
                                changed = true;
                            }
                        }
                    }
                }
            }
        }
    }
    return follow;
}

// -------------------------------------------------------------------
// 6) Construct LL(1) Parsing Table
// -------------------------------------------------------------------
vector<string> computeFirstOfProduction(const Production &prod,
                                        const Grammar &grammar,
                                        const StringVectorMap &first) 
{
    vector<string> result;
    bool allEpsilon = true;
    for(auto &symbol : prod) {
        // If terminal
        if(grammar.find(symbol) == grammar.end()) {
            if(symbol != "ε" && !contains(result, symbol)) {
                result.push_back(symbol);
            }
            allEpsilon = false;
            break;
        } else {
            // Non-terminal
            for(auto &s : first.at(symbol)) {
                if(s != "ε" && !contains(result, s)) {
                    result.push_back(s);
                }
            }
            if(!contains(first.at(symbol), "ε")) {
                allEpsilon = false;
                break;
            }
        }
    }
    if(allEpsilon) {
        if(!contains(result, "ε")) {
            result.push_back("ε");
        }
    }
    return result;
}

ParsingTable constructLL1ParsingTable(const Grammar &grammar,
                                      const StringVectorMap &first,
                                      const StringVectorMap &follow)
{
    ParsingTable table;
    for(auto &A : nonTerminalOrder) {
        if(grammar.find(A) == grammar.end()) continue;
        ProductionList prods = grammar.at(A);
        for(auto &prod : prods) {
            vector<string> firstProd = computeFirstOfProduction(prod, grammar, first);
            
            // For each terminal in FIRST(prod) except ε
            for(auto &terminal : firstProd) {
                if(terminal != "ε") {
                    table[A][terminal] = prod;
                }
            }
            // If ε in FIRST(prod), add this production for each symbol in FOLLOW(A)
            if(contains(firstProd, "ε")) {
                for(auto &terminal : follow.at(A)) {
                    table[A][terminal] = prod;
                }
            }
        }
    }
    return table;
}

// -------------------------------------------------------------------
// 7) Print Functions
// -------------------------------------------------------------------
void printGrammar(const Grammar &grammar) {
    for(auto &nt : nonTerminalOrder) {
        if(grammar.find(nt) == grammar.end()) continue;
        cout << nt << " -> ";
        auto &prods = grammar.at(nt);
        for(size_t i = 0; i < prods.size(); ++i) {
            for(auto &token : prods[i]) {
                cout << token << " ";
            }
            if(i != prods.size() - 1) {
                cout << "| ";
            }
        }
        cout << "\n";
    }
}

void printFirstSets(const StringVectorMap &first) {
    cout << "\nFIRST Sets:\n";
    for(auto &nt : nonTerminalOrder) {
        if(first.find(nt) == first.end()) continue;
        cout << "FIRST(" << nt << ") = { ";
        auto &vec = first.at(nt);
        for(size_t i = 0; i < vec.size(); ++i) {
            cout << vec[i];
            if(i != vec.size()-1) cout << ", ";
        }
        cout << " }\n";
    }
}

void printFollowSets(const StringVectorMap &follow) {
    cout << "\nFOLLOW Sets:\n";
    for(auto &nt : nonTerminalOrder) {
        if(follow.find(nt) == follow.end()) continue;
        cout << "FOLLOW(" << nt << ") = { ";
        auto &vec = follow.at(nt);
        for(size_t i = 0; i < vec.size(); ++i) {
            cout << vec[i];
            if(i != vec.size()-1) cout << ", ";
        }
        cout << " }\n";
    }
}

// Print an ASCII table with columns for every terminal in the table.
void printParsingTable(const ParsingTable &table) {
    // 1. Collect all terminals used as keys in the table
    set<string> terminals;
    for(auto &row : table) {
        for(auto &col : row.second) {
            terminals.insert(col.first);
        }
    }
    // Turn into a vector for stable iteration
    vector<string> terminalVec(terminals.begin(), terminals.end());

    // 2. Print header row
    const int COL_WIDTH = 12;
    cout << "\nLL(1) Parsing Table:\n\n";
    cout << setw(COL_WIDTH) << left << " " << "|";
    for(auto &t : terminalVec) {
        cout << setw(COL_WIDTH) << left << t << "|";
    }
    cout << "\n";
    // Separator line
    cout << string((terminalVec.size()+1)*(COL_WIDTH+1), '-') << "\n";

    // 3. For each non-terminal
    for(auto &nt : nonTerminalOrder) {
        if(table.find(nt) == table.end()) continue;
        // Print the row label
        cout << setw(COL_WIDTH) << left << nt << "|";
        
        // For each terminal
        for(auto &t : terminalVec) {
            if(table.at(nt).find(t) != table.at(nt).end()) {
                auto &prod = table.at(nt).at(t);
                // Build "A-> X Y Z"
                string cell = nt + "->";
                for(auto &sym : prod) {
                    cell += sym + " ";
                }
                if(!cell.empty() && cell.back() == ' ')
                    cell.pop_back();
                
                cout << setw(COL_WIDTH) << left << cell << "|";
            } else {
                cout << setw(COL_WIDTH) << left << " " << "|";
            }
        }
        cout << "\n";
    }
    cout << "\n";
}

// -------------------------------------------------------------------
// Main
// -------------------------------------------------------------------
int main() {
    // Adjust filename if needed
    string filename = "grammar.txt";
    Grammar grammar = readGrammarFromFile(filename);

    cout << "Original Grammar:\n";
    printGrammar(grammar);
    cout << "\n----------------------\n";

    // 1) Left Factoring
    leftFactorGrammar(grammar);
    cout << "Grammar after Left Factoring:\n";
    printGrammar(grammar);
    cout << "\n----------------------\n";

    // 2) Left Recursion Removal
    removeLeftRecursion(grammar);
    cout << "Grammar after Left Recursion Removal:\n";
    printGrammar(grammar);
    cout << "\n----------------------\n";

    // 3) FIRST sets
    StringVectorMap first = computeFirstSets(grammar);
    printFirstSets(first);
    cout << "\n----------------------\n";

    // 4) FOLLOW sets
    StringVectorMap follow = computeFollowSets(grammar, first);
    printFollowSets(follow);
    cout << "\n----------------------\n";

    // 5) Construct LL(1) Table
    ParsingTable table = constructLL1ParsingTable(grammar, first, follow);

    // 6) Print the table in a tabular format (including ) and $ if they exist)
    printParsingTable(table);

    return 0;
}
