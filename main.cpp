#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <sstream>
#include <iomanip>

using namespace std;

// Structure to represent a production rule
struct Production {
    string lhs;               // Left-hand side (non-terminal)
    vector<string> rhs;       // Right-hand side (sequence of terminals and non-terminals)
    
    // Equality operator for Production
    bool operator==(const Production& other) const {
        return lhs == other.lhs && rhs == other.rhs;
    }
};

// Structure to represent a context-free grammar
class Grammar {
private:
    vector<string> nonTerminals;           // List of non-terminals
    vector<string> terminals;              // List of terminals
    vector<Production> productions;        // List of production rules
    string startSymbol;                    // Start symbol

public:
    // Constructor
    Grammar() {}
    
    // Getters and setters
    vector<string> getNonTerminals() const { return nonTerminals; }
    vector<string> getTerminals() const { return terminals; }
    vector<Production> getProductions() const { return productions; }
    string getStartSymbol() const { return startSymbol; }
    
    void setNonTerminals(const vector<string>& nt) { nonTerminals = nt; }
    void setTerminals(const vector<string>& t) { terminals = t; }
    void setProductions(const vector<Production>& p) { productions = p; }
    void setStartSymbol(const string& s) { startSymbol = s; }
    
    // Add a production rule
    void addProduction(const Production& p) {
        productions.push_back(p);
    }
    
    // Remove a production rule
    void removeProduction(const Production& p) {
        auto it = find(productions.begin(), productions.end(), p);
        if (it != productions.end()) {
            productions.erase(it);
        }
    }
    
    // Add a non-terminal
    void addNonTerminal(const string& nt) {
        if (find(nonTerminals.begin(), nonTerminals.end(), nt) == nonTerminals.end()) {
            nonTerminals.push_back(nt);
        }
    }
    
    // Add a terminal
    void addTerminal(const string& t) {
        if (find(terminals.begin(), terminals.end(), t) == terminals.end()) {
            terminals.push_back(t);
        }
    }
    
    // Check if a symbol is a non-terminal
    bool isNonTerminal(const string& symbol) const {
        return find(nonTerminals.begin(), nonTerminals.end(), symbol) != nonTerminals.end();
    }
    
    // Check if a symbol is a terminal
    bool isTerminal(const string& symbol) const {
        return find(terminals.begin(), terminals.end(), symbol) != terminals.end();
    }
    
    // Get all productions for a given non-terminal
    vector<Production> getProductionsFor(const string& nt) const {
        vector<Production> result;
        for (const auto& p : productions) {
            if (p.lhs == nt) {
                result.push_back(p);
            }
        }
        return result;
    }
    
    // Print the grammar
    void print() const {
        cout << "Non-terminals: ";
        for (const auto& nt : nonTerminals) {
            cout << nt << " ";
        }
        cout << endl;
        
        cout << "Terminals: ";
        for (const auto& t : terminals) {
            cout << t << " ";
        }
        cout << endl;
        
        cout << "Productions:" << endl;
        for (const auto& p : productions) {
            cout << p.lhs << " -> ";
            for (const auto& symbol : p.rhs) {
                cout << symbol << " ";
            }
            cout << endl;
        }
        
        cout << "Start Symbol: " << startSymbol << endl;
    }
};

// Parse a line into Productions
vector<Production> parseProduction(const string& line) {
    vector<Production> productions;
    
    // Find the arrow separator
    size_t arrowPos = line.find("->");
    if (arrowPos == string::npos) {
        cerr << "Invalid production format: " << line << endl;
        return productions;
    }
    
    // Extract LHS (left-hand side)
    string lhs = line.substr(0, arrowPos);
    lhs.erase(0, lhs.find_first_not_of(" \t"));
    lhs.erase(lhs.find_last_not_of(" \t") + 1);
    
    // Extract RHS (right-hand side)
    string rhs = line.substr(arrowPos + 2);
    
    // Split by vertical bar (|)
    size_t pos = 0;
    string alternative;
    
    while ((pos = rhs.find("|")) != string::npos) {
        alternative = rhs.substr(0, pos);
        // Trim whitespace
        alternative.erase(0, alternative.find_first_not_of(" \t"));
        alternative.erase(alternative.find_last_not_of(" \t") + 1);
        
        Production p;
        p.lhs = lhs;
        
        // Split into symbols
        istringstream iss(alternative);
        string symbol;
        while (iss >> symbol) {
            p.rhs.push_back(symbol);
        }
        
        productions.push_back(p);
        rhs = rhs.substr(pos + 1);
    }
    
    // Process the last alternative
    rhs.erase(0, rhs.find_first_not_of(" \t"));
    rhs.erase(rhs.find_last_not_of(" \t") + 1);
    
    Production p;
    p.lhs = lhs;
    
    istringstream iss(rhs);
    string symbol;
    while (iss >> symbol) {
        p.rhs.push_back(symbol);
    }
    
    productions.push_back(p);
    
    return productions;
}

// Read the grammar from a file
Grammar readGrammarFromFile(const string& filename) {
    Grammar grammar;
    ifstream file(filename);
    
    if (!file.is_open()) {
        cerr << "Failed to open file: " << filename << endl;
        return grammar;
    }
    
    string line;
    while (getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        vector<Production> productions = parseProduction(line);
        for (const auto& p : productions) {
            grammar.addProduction(p);
            
            // Add the LHS to non-terminals
            grammar.addNonTerminal(p.lhs);
            
            // Check each symbol in RHS
            for (const auto& symbol : p.rhs) {
                if (isupper(symbol[0])) {
                    grammar.addNonTerminal(symbol);
                } else if (symbol != "ε") {  // Epsilon (empty string)
                    grammar.addTerminal(symbol);
                }
            }
        }
    }
    
    // Set the start symbol to be the first production's LHS
    if (!grammar.getProductions().empty()) {
        grammar.setStartSymbol(grammar.getProductions()[0].lhs);
    }
    
    file.close();
    return grammar;
}

// Apply left factoring to a grammar
Grammar applyLeftFactoring(const Grammar& grammar) {
    Grammar result = grammar;  // Start with a copy of the original grammar
    
    // Process each non-terminal
    for (const auto& nt : grammar.getNonTerminals()) {
        // Get all productions for this non-terminal
        vector<Production> prods = result.getProductionsFor(nt);
        
        if (prods.size() <= 1) {
            // No factoring needed for a single production
            continue;
        }
        
        // Keep applying left factoring until no more factoring is possible
        bool factored = true;
        while (factored) {
            factored = false;
            
            // Group productions by their first symbol
            map<string, vector<Production>> prefixGroups;
            
            for (const auto& p : prods) {
                if (p.rhs.empty()) {
                    prefixGroups["ε"].push_back(p);
                } else {
                    prefixGroups[p.rhs[0]].push_back(p);
                }
            }
            
            // Check each prefix group for factoring
            for (auto& pair : prefixGroups) {
                if (pair.second.size() <= 1) {
                    continue;  // No factoring needed for a single production
                }
                
                // Find the longest common prefix
                vector<string> commonPrefix;
                bool prefixEnded = false;
                size_t minLength = SIZE_MAX;
                
                // Find the shortest RHS length
                for (const auto& p : pair.second) {
                    minLength = min(minLength, p.rhs.size());
                }
                
                // Find the common prefix
                for (size_t i = 0; i < minLength && !prefixEnded; i++) {
                    string currentSymbol = pair.second[0].rhs[i];
                    for (size_t j = 1; j < pair.second.size(); j++) {
                        if (pair.second[j].rhs[i] != currentSymbol) {
                            prefixEnded = true;
                            break;
                        }
                    }
                    
                    if (!prefixEnded) {
                        commonPrefix.push_back(currentSymbol);
                    }
                }
                
                if (commonPrefix.size() > 0) {
                    factored = true;
                    
                    // Create a new non-terminal
                    string newNT = nt + "_prime";
                    while (find(result.getNonTerminals().begin(), result.getNonTerminals().end(), newNT) != result.getNonTerminals().end()) {
                        newNT += "_prime";
                    }
                    
                    result.addNonTerminal(newNT);
                    
                    // Remove the factored productions
                    for (const auto& p : pair.second) {
                        result.removeProduction(p);
                    }
                    
                    // Create a new production for the original non-terminal
                    Production newP;
                    newP.lhs = nt;
                    newP.rhs = commonPrefix;
                    newP.rhs.push_back(newNT);
                    result.addProduction(newP);
                    
                    // Create productions for the new non-terminal
                    for (const auto& p : pair.second) {
                        Production newP;
                        newP.lhs = newNT;
                        
                        // Skip the common prefix
                        for (size_t i = commonPrefix.size(); i < p.rhs.size(); i++) {
                            newP.rhs.push_back(p.rhs[i]);
                        }
                        
                        // If the RHS is empty, use epsilon
                        if (newP.rhs.empty()) {
                            newP.rhs.push_back("ε");
                        }
                        
                        result.addProduction(newP);
                    }
                    
                    // Update prods for the next iteration
                    prods = result.getProductionsFor(nt);
                    break;  // Start over with the new productions
                }
            }
        }
    }
    
    return result;
}

// Remove direct left recursion for a non-terminal
void removeDirectLeftRecursion(Grammar& grammar, const string& nt) {
    vector<Production> prods = grammar.getProductionsFor(nt);
    
    vector<Production> alphaProds;  // Productions of the form A → Aα
    vector<Production> betaProds;   // Productions of the form A → β (where β doesn't start with A)
    
    // Separate alpha and beta productions
    for (const auto& p : prods) {
        if (!p.rhs.empty() && p.rhs[0] == nt) {
            // This is an alpha production: A → Aα
            alphaProds.push_back(p);
        } else {
            // This is a beta production: A → β
            betaProds.push_back(p);
        }
    }
    
    if (alphaProds.empty()) {
        // No left recursion to remove
        return;
    }
    
    // Remove all productions for this non-terminal
    for (const auto& p : prods) {
        grammar.removeProduction(p);
    }
    
    // Create a new non-terminal
    string newNT = nt + "_prime";
    while (find(grammar.getNonTerminals().begin(), grammar.getNonTerminals().end(), newNT) != grammar.getNonTerminals().end()) {
        newNT += "_prime";
    }
    
    grammar.addNonTerminal(newNT);
    
    // Add new productions for the original non-terminal: A → βA'
    for (const auto& p : betaProds) {
        Production newP;
        newP.lhs = nt;
        newP.rhs = p.rhs;  // β
        newP.rhs.push_back(newNT);  // A'
        grammar.addProduction(newP);
    }
    
    // Add new productions for the new non-terminal: A' → αA'
    for (const auto& p : alphaProds) {
        Production newP;
        newP.lhs = newNT;
        
        // Skip the left recursive symbol to get α
        for (size_t i = 1; i < p.rhs.size(); i++) {
            newP.rhs.push_back(p.rhs[i]);
        }
        
        newP.rhs.push_back(newNT);  // A'
        grammar.addProduction(newP);
    }
    
    // Add epsilon production for the new non-terminal: A' → ε
    Production epsilonProd;
    epsilonProd.lhs = newNT;
    epsilonProd.rhs.push_back("ε");
    grammar.addProduction(epsilonProd);
}

// Remove all left recursion from a grammar
Grammar removeLeftRecursion(const Grammar& grammar) {
    Grammar result = grammar;
    
    // Get a topological ordering of non-terminals
    vector<string> nonTerminals = result.getNonTerminals();
    
    // For each non-terminal Ai
    for (size_t i = 0; i < nonTerminals.size(); i++) {
        string ai = nonTerminals[i];
        
        // For each previous non-terminal Aj (j < i)
        for (size_t j = 0; j < i; j++) {
            string aj = nonTerminals[j];
            
            // Replace Ai → Ajγ with Ai → δ1γ | δ2γ | ... | δnγ
            // where Aj → δ1 | δ2 | ... | δn
            vector<Production> aiProds = result.getProductionsFor(ai);
            vector<Production> ajProds = result.getProductionsFor(aj);
            
            for (const auto& aiProd : aiProds) {
                if (!aiProd.rhs.empty() && aiProd.rhs[0] == aj) {
                    // This production has Aj as its first symbol
                    
                    // Remove this production
                    result.removeProduction(aiProd);
                    
                    // Add new productions for each Aj production
                    for (const auto& ajProd : ajProds) {
                        Production newP;
                        newP.lhs = ai;
                        
                        // Add the RHS of the Aj production
                        newP.rhs = ajProd.rhs;
                        
                        // Add the rest of the original production
                        for (size_t k = 1; k < aiProd.rhs.size(); k++) {
                            newP.rhs.push_back(aiProd.rhs[k]);
                        }
                        
                        result.addProduction(newP);
                    }
                }
            }
        }
        
        // Remove direct left recursion for Ai
        removeDirectLeftRecursion(result, ai);
    }
    
    return result;
}

// Compute FIRST set for a single symbol
set<string> computeFirstSetForSymbol(const Grammar& grammar, const string& symbol, map<string, set<string>>& firstSets, set<string>& visited) {
    // If already computed, return
    if (firstSets.find(symbol) != firstSets.end() && !firstSets[symbol].empty()) {
        return firstSets[symbol];
    }
    
    // If terminal, FIRST(a) = {a}
    if (grammar.isTerminal(symbol)) {
        firstSets[symbol].insert(symbol);
        return firstSets[symbol];
    }
    
    // If epsilon, FIRST(ε) = {ε}
    if (symbol == "ε") {
        firstSets[symbol].insert("ε");
        return firstSets[symbol];
    }
    
    // If non-terminal, compute FIRST set
    if (visited.find(symbol) != visited.end()) {
        // Already visiting this non-terminal, skip to avoid infinite recursion
        return firstSets[symbol];
    }
    
    visited.insert(symbol);
    
    // For each production A → α
    for (const auto& p : grammar.getProductionsFor(symbol)) {
        if (p.rhs.empty() || p.rhs[0] == "ε") {
            // A → ε
            firstSets[symbol].insert("ε");
        } else {
            // A → X1X2...Xn
            bool allDeriveEpsilon = true;
            for (size_t i = 0; i < p.rhs.size(); i++) {
                string X = p.rhs[i];
                
                // Compute FIRST(X)
                set<string> firstX = computeFirstSetForSymbol(grammar, X, firstSets, visited);
                
                // Add FIRST(X) - {ε} to FIRST(A)
                for (const auto& terminal : firstX) {
                    if (terminal != "ε") {
                        firstSets[symbol].insert(terminal);
                    }
                }
                
                // If X doesn't derive epsilon, stop processing this production
                if (firstX.find("ε") == firstX.end()) {
                    allDeriveEpsilon = false;
                    break;
                }
            }
            
            // If all symbols in the production can derive epsilon, add epsilon to FIRST(A)
            if (allDeriveEpsilon) {
                firstSets[symbol].insert("ε");
            }
        }
    }
    
    visited.erase(symbol);
    return firstSets[symbol];
}

// Compute FIRST sets for all symbols in a grammar
map<string, set<string>> computeFirstSets(const Grammar& grammar) {
    map<string, set<string>> firstSets;
    
    // Initialize FIRST sets as empty
    for (const auto& t : grammar.getTerminals()) {
        firstSets[t] = {};
    }
    
    for (const auto& nt : grammar.getNonTerminals()) {
        firstSets[nt] = {};
    }
    
    // Compute FIRST sets for all symbols
    set<string> visited;
    for (const auto& nt : grammar.getNonTerminals()) {
        computeFirstSetForSymbol(grammar, nt, firstSets, visited);
    }
    
    return firstSets;
}

// Compute FOLLOW sets for all non-terminals in a grammar
map<string, set<string>> computeFollowSets(const Grammar& grammar, const map<string, set<string>>& firstSets) {
    map<string, set<string>> followSets;
    
    // Initialize FOLLOW sets for all non-terminals as empty
    for (const auto& nt : grammar.getNonTerminals()) {
        followSets[nt] = {};
    }
    
    // Add $ to FOLLOW(S), where S is the start symbol
    followSets[grammar.getStartSymbol()].insert("$");
    
    // Repeat until no more changes
    bool changed = true;
    while (changed) {
        changed = false;
        
        // For each production A → α
        for (const auto& p : grammar.getProductions()) {
            string A = p.lhs;
            
            // For each position in the RHS
            for (size_t i = 0; i < p.rhs.size(); i++) {
                string B = p.rhs[i];
                
                if (!grammar.isNonTerminal(B)) {
                    continue;  // Skip terminals
                }
                
                // If B is the last symbol, add FOLLOW(A) to FOLLOW(B)
                if (i == p.rhs.size() - 1) {
                    size_t oldSize = followSets[B].size();
                    for (const auto& terminal : followSets[A]) {
                        followSets[B].insert(terminal);
                    }
                    
                    if (oldSize != followSets[B].size()) {
                        changed = true;
                    }
                } else {
                    // If B is not the last symbol, compute FIRST of the rest of the string
                    set<string> firstBeta;
                    bool betaDerivesEpsilon = true;
                    
                    for (size_t j = i + 1; j < p.rhs.size() && betaDerivesEpsilon; j++) {
                        string X = p.rhs[j];
                        const auto& firstX = firstSets.at(X);
                        
                        // Add FIRST(X) - {ε} to first(β)
                        for (const auto& terminal : firstX) {
                            if (terminal != "ε") {
                                firstBeta.insert(terminal);
                            }
                        }
                        
                        // If X doesn't derive epsilon, β doesn't derive epsilon
                        if (firstX.find("ε") == firstX.end()) {
                            betaDerivesEpsilon = false;
                        }
                    }
                    
                    // Add FIRST(β) - {ε} to FOLLOW(B)
                    size_t oldSize = followSets[B].size();
                    for (const auto& terminal : firstBeta) {
                        if (terminal != "ε") {
                            followSets[B].insert(terminal);
                        }
                    }
                    
                    if (oldSize != followSets[B].size()) {
                        changed = true;
                    }
                    
                    // If β derives ε, add FOLLOW(A) to FOLLOW(B)
                    if (betaDerivesEpsilon) {
                        oldSize = followSets[B].size();
                        for (const auto& terminal : followSets[A]) {
                            followSets[B].insert(terminal);
                        }
                        
                        if (oldSize != followSets[B].size()) {
                            changed = true;
                        }
                    }
                }
            }
        }
    }
    
    return followSets;
}

// Construct an LL(1) parsing table
map<pair<string, string>, Production> constructLL1Table(const Grammar& grammar, const map<string, set<string>>& firstSets, const map<string, set<string>>& followSets) {
    map<pair<string, string>, Production> table;
    
    // For each production A → α
    for (const auto& p : grammar.getProductions()) {
        string A = p.lhs;
        
        // Check if this is an epsilon production
        if (p.rhs.size() == 1 && p.rhs[0] == "ε") {
            // For each terminal b in FOLLOW(A), add A → ε to table[A, b]
            for (const auto& terminal : followSets.at(A)) {
                // Check for conflicts
                if (table.find({A, terminal}) != table.end()) {
                    cerr << "Conflict in table[" << A << ", " << terminal << "]: grammar is not LL(1)" << endl;
                }
                
                table[{A, terminal}] = p;
            }
        } else {
            // Compute FIRST(α)
            set<string> firstAlpha;
            bool alphaDerivesEpsilon = true;
            
            for (size_t i = 0; i < p.rhs.size() && alphaDerivesEpsilon; i++) {
                string X = p.rhs[i];
                const auto& firstX = firstSets.at(X);
                
                // Add FIRST(X) - {ε} to FIRST(α)
                for (const auto& terminal : firstX) {
                    if (terminal != "ε") {
                        firstAlpha.insert(terminal);
                    }
                }
                
                // If X doesn't derive epsilon, α doesn't derive epsilon
                if (firstX.find("ε") == firstX.end()) {
                    alphaDerivesEpsilon = false;
                }
            }
            
            // For each terminal a in FIRST(α), add A → α to table[A, a]
            for (const auto& terminal : firstAlpha) {
                // Check for conflicts
                if (table.find({A, terminal}) != table.end()) {
                    cerr << "Conflict in table[" << A << ", " << terminal << "]: grammar is not LL(1)" << endl;
                }
                
                table[{A, terminal}] = p;
            }
            
            // If α derives ε, for each terminal b in FOLLOW(A), add A → α to table[A, b]
            if (alphaDerivesEpsilon) {
                for (const auto& terminal : followSets.at(A)) {
                    // Check for conflicts
                    if (table.find({A, terminal}) != table.end()) {
                        cerr << "Conflict in table[" << A << ", " << terminal << "]: grammar is not LL(1)" << endl;
                    }
                    
                    table[{A, terminal}] = p;
                }
            }
        }
    }
    
    return table;
}

int main() {
    // Read the grammar from a file
    Grammar grammar = readGrammarFromFile("grammar.txt");
    
    cout << "Original Grammar:" << endl;
    grammar.print();
    
    // Apply left factoring
    Grammar leftFactoredGrammar = applyLeftFactoring(grammar);
    
    cout << "\nGrammar after Left Factoring:" << endl;
    leftFactoredGrammar.print();
    
    // Remove left recursion
    Grammar leftRecursionFreeGrammar = removeLeftRecursion(leftFactoredGrammar);
    
    cout << "\nGrammar after Left Recursion Removal:" << endl;
    leftRecursionFreeGrammar.print();
    
    // Compute FIRST sets
    map<string, set<string>> firstSets = computeFirstSets(leftRecursionFreeGrammar);
    
    cout << "\nFIRST Sets:" << endl;
    for (const auto& pair : firstSets) {
        if (leftRecursionFreeGrammar.isNonTerminal(pair.first)) {
            cout << "FIRST(" << pair.first << ") = { ";
            for (const auto& terminal : pair.second) {
                cout << terminal << " ";
            }
            cout << "}" << endl;
        }
    }
    
    // Compute FOLLOW sets
    map<string, set<string>> followSets = computeFollowSets(leftRecursionFreeGrammar, firstSets);
    
    cout << "\nFOLLOW Sets:" << endl;
    for (const auto& pair : followSets) {
        cout << "FOLLOW(" << pair.first << ") = { ";
        for (const auto& terminal : pair.second) {
            cout << terminal << " ";
        }
        cout << "}" << endl;
    }
    
    // Construct LL(1) parsing table
    map<pair<string, string>, Production> parseTable = constructLL1Table(leftRecursionFreeGrammar, firstSets, followSets);
    
    cout << "\nLL(1) Parsing Table:" << endl;
    cout << "-------------------------------------------------------------" << endl;
    cout << "| Non-terminal | Terminal | Production                       |" << endl;
    cout << "-------------------------------------------------------------" << endl;
    
    for (const auto& entry : parseTable) {
        string nonTerminal = entry.first.first;
        string terminal = entry.first.second;
        string production = entry.second.lhs + " -> ";
        
        for (const auto& symbol : entry.second.rhs) {
            production += symbol + " ";
        }
        
        cout << "| " << setw(12) << nonTerminal << " | " << setw(8) << terminal << " | " << setw(32) << production << " |" << endl;
    }
    
    cout << "-------------------------------------------------------------" << endl;
    
    return 0;
}