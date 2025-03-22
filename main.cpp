#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <set>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <iomanip>

// ------------------------------------------------------------------
// Constants
// ------------------------------------------------------------------
#define MAX_NONTERMINALS 52    // 26 letters + 26 for prime
#define MAX_PRODUCTIONS 10      // maximum productions per nonterminal
#define MAX_PROD_LENGTH 100     // maximum length of a production string
#define MAX_TERM 128            // maximum terminal ASCII codes for parse table

// ------------------------------------------------------------------
// GrammarProduction structure:
// nonTerminal is stored as a single char. The boolean primeMark[] tells
// whether the production represents a prime nonterminal (e.g., E').
struct GrammarProduction {
    char nonTerminal; // e.g. 'E', 'T', etc.
    char productions[MAX_PRODUCTIONS][MAX_PROD_LENGTH];
    int prodCount;
};

GrammarProduction grammar[MAX_NONTERMINALS];
bool primeMark[MAX_NONTERMINALS];    // true if e.g. E'
int grammarCount = 0;                // number of grammar entries

// FIRST and FOLLOW sets stored in 52 slots.
std::set<std::string> firstSets[MAX_NONTERMINALS];
std::set<std::string> followSets[MAX_NONTERMINALS];

// LL(1) parsing table: for each nonterminal index (0..51) and terminal (ASCII)
char parseTable[MAX_NONTERMINALS][MAX_TERM][MAX_PROD_LENGTH];

// ------------------------------------------------------------------
// Helper: Map a nonterminal letter and its prime flag to an index [0..51].
// Normal nonterminals: index = (c - 'A')
// Prime nonterminals:  index = (c - 'A' + 26)
// ------------------------------------------------------------------
int getNonTerminalIndex(char c, bool isPrime) {
    int base = c - 'A';
    return isPrime ? base + 26 : base;
}

// ------------------------------------------------------------------
// Helper: When processing a symbol (from a production’s RHS), detect if it is a nonterminal.
// If it is an uppercase letter possibly followed by an apostrophe, return the proper index.
// Otherwise, return -1 (indicating a terminal).
// ------------------------------------------------------------------
int symbolIndex(const char *sym) {
    if (!std::isupper((unsigned char)sym[0])) return -1; // terminal
    if (std::strlen(sym) > 1 && sym[1] == '\'')
        return getNonTerminalIndex(sym[0], true);
    return getNonTerminalIndex(sym[0], false);
}

// ------------------------------------------------------------------
// Trim whitespace from a C-string (in place).
// ------------------------------------------------------------------
void trim(char *str) {
    int i = 0, j = 0;
    while (str[i] && std::isspace((unsigned char)str[i])) i++;
    while (str[i]) {
        str[j++] = str[i++];
    }
    str[j] = '\0';
    for (i = j - 1; i >= 0 && std::isspace((unsigned char)str[i]); i--) {
        str[i] = '\0';
    }
}

// ------------------------------------------------------------------
// When printing a nonterminal, print E' if primeMark is true.
// idx is an index into grammar[].
// ------------------------------------------------------------------
void printNonTerminal(int idx) {
    if (primeMark[idx])
        std::printf("%c'", grammar[idx].nonTerminal);
    else
        std::printf("%c", grammar[idx].nonTerminal);
}

// ------------------------------------------------------------------
// Helper: Parse the next symbol from a production string.
// It detects if an uppercase letter is immediately followed by a prime marker.
// Writes the symbol into outSym and advances pos.
// Returns false if end-of-string is reached.
// ------------------------------------------------------------------
bool nextSymbol(const char *prod, int &pos, char *outSym) {
    while (prod[pos] && std::isspace((unsigned char)prod[pos])) pos++;
    if (!prod[pos])
        return false;
    
    if (std::isupper((unsigned char)prod[pos])) {
        outSym[0] = prod[pos];
        if (prod[pos+1] == '\'') {
            outSym[1] = '\'';
            outSym[2] = '\0';
            pos += 2;
        } else {
            outSym[1] = '\0';
            pos++;
        }
    } else {
        outSym[0] = prod[pos];
        outSym[1] = '\0';
        pos++;
    }
    return true;
}

// ------------------------------------------------------------------
// Read grammar from file "grammar.txt"
// Each line should be of the form:
//    E -> T E'
//    E' -> + T E' | ε
// ------------------------------------------------------------------
void readGrammar(const char *filename) {
    FILE *fp = std::fopen(filename, "r");
    if (!fp) {
        std::printf("Error opening file %s\n", filename);
        std::exit(1);
    }
    char line[256];
    while (std::fgets(line, sizeof(line), fp)) {
        if (std::strlen(line) <= 1)
            continue;
        char *arrow = std::strstr(line, "->");
        if (!arrow)
            continue;

        char lhs[10];
        std::strncpy(lhs, line, arrow - line);
        lhs[arrow - line] = '\0';
        trim(lhs);
        if (std::strlen(lhs) == 0)
            continue;
        char nt = lhs[0];
        bool isPrime = false;
        if (std::strlen(lhs) > 1 && lhs[1] == '\'')
            isPrime = true;

        int idx = grammarCount;
        grammar[idx].nonTerminal = nt;
        grammar[idx].prodCount = 0;
        primeMark[idx] = isPrime;
        grammarCount++;

        char *rhs = arrow + 2;
        char *token = std::strtok(rhs, "|");
        while (token != NULL) {
            trim(token);
            if (grammar[idx].prodCount < MAX_PRODUCTIONS) {
                std::strcpy(grammar[idx].productions[grammar[idx].prodCount++], token);
            }
            token = std::strtok(NULL, "|");
        }
    }
    std::fclose(fp);
}

// ------------------------------------------------------------------
// Print the current grammar.
// ------------------------------------------------------------------
void printGrammar() {
    for (int i = 0; i < grammarCount; i++) {
        if (grammar[i].prodCount > 0) {
            printNonTerminal(i);
            std::printf(" -> ");
            for (int j = 0; j < grammar[i].prodCount; j++) {
                std::printf("%s", grammar[i].productions[j]);
                if (j != grammar[i].prodCount - 1)
                    std::printf(" | ");
            }
            std::printf("\n");
        }
    }
}

// ------------------------------------------------------------------
// LEFT FACTORING
// ------------------------------------------------------------------
void leftFactoring() {
    bool repeat = true;
    while (repeat) {
        repeat = false;
        for (int i = 0; i < grammarCount; i++) {
            char A = grammar[i].nonTerminal;
            bool factored = false;
            for (int j = 0; j < grammar[i].prodCount; j++) {
                char firstSym = grammar[i].productions[j][0];
                int count = 0;
                for (int k = 0; k < grammar[i].prodCount; k++) {
                    if (grammar[i].productions[k][0] == firstSym)
                        count++;
                }
                if (count > 1) {
                    char commonPrefix[MAX_PROD_LENGTH];
                    std::strcpy(commonPrefix, grammar[i].productions[j]);
                    for (int k = j+1; k < grammar[i].prodCount; k++) {
                        if (grammar[i].productions[k][0] == firstSym) {
                            int idx = 0;
                            while (commonPrefix[idx] && grammar[i].productions[k][idx] &&
                                   commonPrefix[idx] == grammar[i].productions[k][idx])
                                idx++;
                            commonPrefix[idx] = '\0';
                        }
                    }
                    int prefixLen = (int)std::strlen(commonPrefix);
                    if (prefixLen > 0) {
                        factored = true;
                        char APrime[5];
                        std::sprintf(APrime, "%c'", A);
                        char newProds[MAX_PRODUCTIONS][MAX_PROD_LENGTH];
                        int newCount = 0;
                        char newProdsAPrime[MAX_PRODUCTIONS][MAX_PROD_LENGTH];
                        int newCountAPrime = 0;
                        for (int k = 0; k < grammar[i].prodCount; k++) {
                            if (std::strncmp(grammar[i].productions[k], commonPrefix, prefixLen) == 0) {
                                char remainder[MAX_PROD_LENGTH];
                                std::strcpy(remainder, grammar[i].productions[k] + prefixLen);
                                if (std::strlen(remainder) == 0)
                                    std::strcpy(remainder, "ε");
                                std::strcpy(newProdsAPrime[newCountAPrime++], remainder);
                            } else {
                                std::strcpy(newProds[newCount++], grammar[i].productions[k]);
                            }
                        }
                        char factoredProd[MAX_PROD_LENGTH];
                        // Limit commonPrefix so that commonPrefix + APrime fits in MAX_PROD_LENGTH
                        int avail = MAX_PROD_LENGTH - 1 - (int)std::strlen(APrime);
                        std::snprintf(factoredProd, MAX_PROD_LENGTH, "%.*s%s", avail, commonPrefix, APrime);
                        std::strcpy(newProds[newCount++], factoredProd);
                        grammar[i].prodCount = newCount;
                        for (int k = 0; k < newCount; k++) {
                            std::strcpy(grammar[i].productions[k], newProds[k]);
                        }
                        // Create new grammar entry for A'
                        int newIdx = grammarCount;
                        grammar[newIdx].nonTerminal = A; // same letter
                        primeMark[newIdx] = true;
                        grammar[newIdx].prodCount = 0;
                        for (int k = 0; k < newCountAPrime; k++) {
                            std::strcpy(grammar[newIdx].productions[grammar[newIdx].prodCount++], newProdsAPrime[k]);
                        }
                        grammarCount++;
                        break;
                    }
                }
            }
            if (factored) {
                repeat = true;
                break; // re-check from start
            }
        }
    }
}

// ------------------------------------------------------------------
// REMOVE LEFT RECURSION
// ------------------------------------------------------------------
void removeLeftRecursionFor(int idx) {
    char nt = grammar[idx].nonTerminal;
    char ntPrime[5];
    std::sprintf(ntPrime, "%c'", nt);
    char nonRecProds[MAX_PRODUCTIONS][MAX_PROD_LENGTH];
    int nonRecCount = 0;
    char recProds[MAX_PRODUCTIONS][MAX_PROD_LENGTH];
    int recCount = 0;
    for (int i = 0; i < grammar[idx].prodCount; i++) {
        char *prod = grammar[idx].productions[i];
        if (prod[0] == nt) {
            if (std::strlen(prod) > 1)
                std::strcpy(recProds[recCount++], prod + 1);
            else
                std::strcpy(recProds[recCount++], "");
        } else {
            std::strcpy(nonRecProds[nonRecCount++], prod);
        }
    }
    if (recCount > 0) {
        for (int i = 0; i < nonRecCount; i++) {
            char temp[MAX_PROD_LENGTH];
            int avail = MAX_PROD_LENGTH - 1 - (int)std::strlen(ntPrime);
            std::snprintf(temp, MAX_PROD_LENGTH, "%.*s%s", avail, nonRecProds[i], ntPrime);
            std::strcpy(nonRecProds[i], temp);
        }
        grammar[idx].prodCount = nonRecCount;
        for (int i = 0; i < nonRecCount; i++) {
            std::strcpy(grammar[idx].productions[i], nonRecProds[i]);
        }
        int newIdx = grammarCount;
        grammar[newIdx].nonTerminal = nt;
        primeMark[newIdx] = true;
        grammar[newIdx].prodCount = 0;
        for (int i = 0; i < recCount; i++) {
            char temp[MAX_PROD_LENGTH];
            int avail = MAX_PROD_LENGTH - 1 - (int)std::strlen(ntPrime);
            std::snprintf(temp, MAX_PROD_LENGTH, "%.*s%s", avail, recProds[i], ntPrime);
            std::strcpy(grammar[newIdx].productions[grammar[newIdx].prodCount++], temp);
        }
        std::strcpy(grammar[newIdx].productions[grammar[newIdx].prodCount++], "ε");
        grammarCount++;
    }
}

void removeLeftRecursion() {
    for (int i = 0; i < grammarCount; i++) {
        if (grammar[i].prodCount > 0)
            removeLeftRecursionFor(i);
    }
}

// ------------------------------------------------------------------
// COMPUTE FIRST SETS
// ------------------------------------------------------------------
void computeFirst() {
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < grammarCount; i++) {
            int Aidx = getNonTerminalIndex(grammar[i].nonTerminal, primeMark[i]);
            for (int j = 0; j < grammar[i].prodCount; j++) {
                char *prod = grammar[i].productions[j];
                if (std::strcmp(prod, "ε") == 0) {
                    if (firstSets[Aidx].insert("ε").second) {
                        changed = true;
                    }
                    continue;
                }
                bool allEpsilon = true;
                int pos = 0;
                while (true) {
                    char symbol[10];
                    if (!nextSymbol(prod, pos, symbol)) break;
                    int Bidx = symbolIndex(symbol);
                    if (Bidx < 0) {
                        if (firstSets[Aidx].insert(symbol).second) {
                            changed = true;
                        }
                        allEpsilon = false;
                        break;
                    } else {
                        bool hasEpsilon = false;
                        for (auto &s : firstSets[Bidx]) {
                            if (s == "ε")
                                hasEpsilon = true;
                            else {
                                if (firstSets[Aidx].insert(s).second) {
                                    changed = true;
                                }
                            }
                        }
                        if (!hasEpsilon) {
                            allEpsilon = false;
                            break;
                        }
                    }
                }
                if (allEpsilon) {
                    if (firstSets[Aidx].insert("ε").second) {
                        changed = true;
                    }
                }
            }
        }
    }
}

// ------------------------------------------------------------------
// COMPUTE FOLLOW SETS
// ------------------------------------------------------------------
void computeFollow() {
    if (grammarCount > 0) {
        int startIdx = getNonTerminalIndex(grammar[0].nonTerminal, primeMark[0]);
        followSets[startIdx].insert("$");
    }
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < grammarCount; i++) {
            int Aidx = getNonTerminalIndex(grammar[i].nonTerminal, primeMark[i]);
            for (int j = 0; j < grammar[i].prodCount; j++) {
                char *prod = grammar[i].productions[j];
                char symbols[50][10];
                int symCount = 0;
                int tempPos = 0;
                while (true) {
                    char s[10];
                    if (!nextSymbol(prod, tempPos, s)) break;
                    std::strcpy(symbols[symCount++], s);
                }
                for (int k = 0; k < symCount; k++) {
                    int Bidx = symbolIndex(symbols[k]);
                    if (Bidx < 0)
                        continue;
                    bool allEpsilon = true;
                    for (int x = k+1; x < symCount; x++) {
                        int nxt = symbolIndex(symbols[x]);
                        if (nxt < 0) {
                            if (followSets[Bidx].insert(symbols[x]).second) {
                                changed = true;
                            }
                            allEpsilon = false;
                            break;
                        } else {
                            bool hasEps = false;
                            for (auto &fs : firstSets[nxt]) {
                                if (fs == "ε")
                                    hasEps = true;
                                else {
                                    if (followSets[Bidx].insert(fs).second) {
                                        changed = true;
                                    }
                                }
                            }
                            if (!hasEps) {
                                allEpsilon = false;
                                break;
                            }
                        }
                    }
                    if (allEpsilon) {
                        for (auto &fa : followSets[Aidx]) {
                            if (followSets[Bidx].insert(fa).second) {
                                changed = true;
                            }
                        }
                    }
                }
            }
        }
    }
}

// ------------------------------------------------------------------
// BUILD LL(1) PARSE TABLE
// ------------------------------------------------------------------
void buildParseTable() {
    for (int i = 0; i < MAX_NONTERMINALS; i++) {
        for (int j = 0; j < MAX_TERM; j++) {
            parseTable[i][j][0] = '\0';
        }
    }
    for (int i = 0; i < grammarCount; i++) {
        int Aidx = getNonTerminalIndex(grammar[i].nonTerminal, primeMark[i]);
        for (int j = 0; j < grammar[i].prodCount; j++) {
            char *prod = grammar[i].productions[j];
            std::set<std::string> firstProd;
            bool allEps = true;
            int pos = 0;
            if (std::strcmp(prod, "ε") == 0) {
                firstProd.insert("ε");
            } else {
                while (true) {
                    char sym[10];
                    if (!nextSymbol(prod, pos, sym)) break;
                    int idxSym = symbolIndex(sym);
                    if (idxSym < 0) {
                        firstProd.insert(std::string(sym));
                        allEps = false;
                        break;
                    } else {
                        bool hasEps = false;
                        for (auto &x : firstSets[idxSym]) {
                            if (x == "ε")
                                hasEps = true;
                            else
                                firstProd.insert(x);
                        }
                        if (!hasEps) {
                            allEps = false;
                            break;
                        }
                    }
                }
                if (allEps) {
                    firstProd.insert("ε");
                }
            }
            for (auto &term : firstProd) {
                if (term != "ε") {
                    unsigned char c = (unsigned char)term[0];
                    std::strcpy(parseTable[Aidx][c], prod);
                }
            }
            if (firstProd.find("ε") != firstProd.end()) {
                for (auto &fw : followSets[Aidx]) {
                    unsigned char c = (unsigned char)fw[0];
                    std::strcpy(parseTable[Aidx][c], prod);
                }
            }
        }
    }
}

// ------------------------------------------------------------------
// ASCII Printing for FIRST and FOLLOW sets
// ------------------------------------------------------------------
void printFirstAndFollowSets() {
    using namespace std;
    int colWidthNT = 6;
    int colWidthFirst = 10;
    int colWidthFollow = 10;
    vector<string> firstStr(grammarCount), followStr(grammarCount);
    for (int i = 0; i < grammarCount; i++) {
        char label[5];
        label[0] = grammar[i].nonTerminal;
        int p = 1;
        if (primeMark[i]) {
            label[p++] = '\'';
        }
        label[p] = '\0';
        string fs = "{ ";
        bool firstItem = true;
        for (auto &s : firstSets[getNonTerminalIndex(grammar[i].nonTerminal, primeMark[i])]) {
            if (!firstItem) fs += ", ";
            fs += s;
            firstItem = false;
        }
        fs += " }";
        string fws = "{ ";
        firstItem = true;
        for (auto &s : followSets[getNonTerminalIndex(grammar[i].nonTerminal, primeMark[i])]) {
            if (!firstItem) fws += ", ";
            fws += s;
            firstItem = false;
        }
        fws += " }";
        firstStr[i] = fs;
        followStr[i] = fws;
        int lenF = (int)fs.size();
        int lenW = (int)fws.size();
        if (lenF > colWidthFirst) colWidthFirst = lenF;
        if (lenW > colWidthFollow) colWidthFollow = lenW;
    }
    int totalWidth = colWidthNT + colWidthFirst + colWidthFollow + 10;
    cout << "\nFIRST and FOLLOW Sets:\n\n";
    for (int i = 0; i < totalWidth; i++) cout << "-";
    cout << "\n";
    cout << "| " << setw(colWidthNT) << left << "NT"
         << " | " << setw(colWidthFirst) << left << "FIRST"
         << " | " << setw(colWidthFollow) << left << "FOLLOW"
         << " |\n";
    for (int i = 0; i < totalWidth; i++) cout << "-";
    cout << "\n";
    for (int i = 0; i < grammarCount; i++) {
        char label[5];
        label[0] = grammar[i].nonTerminal;
        int p = 1;
        if (primeMark[i]) {
            label[p++] = '\'';
        }
        label[p] = '\0';
        cout << "| " << setw(colWidthNT) << left << label
             << " | " << setw(colWidthFirst) << left << firstStr[i]
             << " | " << setw(colWidthFollow) << left << followStr[i]
             << " |\n";
        for (int x = 0; x < totalWidth; x++) cout << "-";
        cout << "\n";
    }
    cout << "\n";
}

// ------------------------------------------------------------------
// Print the LL(1) Parsing Table in an ASCII grid
// ------------------------------------------------------------------
void printLL1ParseTable() {
    using namespace std;
    set<char> terminalsUsed;
    for (int i = 0; i < MAX_NONTERMINALS; i++) {
        for (int t = 0; t < MAX_TERM; t++) {
            if (parseTable[i][t][0] != '\0') {
                terminalsUsed.insert((char)t);
            }
        }
    }
    vector<char> termVec(terminalsUsed.begin(), terminalsUsed.end());
    const int NT_COL_WIDTH = 6;
    const int CELL_WIDTH = 15;
    int totalCols = 1 + termVec.size();
    ostringstream borderStream;
    borderStream << "+";
    borderStream << string(NT_COL_WIDTH, '-') << "+";
    for (size_t i = 0; i < termVec.size(); i++) {
        borderStream << string(CELL_WIDTH, '-') << "+";
    }
    string horizontalBorder = borderStream.str();
    cout << "\nLL(1) Parsing Table:\n";
    cout << horizontalBorder << "\n";
    cout << "| " << setw(NT_COL_WIDTH - 2) << left << "NT" << " |";
    for (auto c : termVec) {
        string colLabel(1, c);
        cout << " " << setw(CELL_WIDTH - 2) << left << colLabel << " |";
    }
    cout << "\n" << horizontalBorder << "\n";
    for (int i = 0; i < grammarCount; i++) {
        char rowLabel[5];
        rowLabel[0] = grammar[i].nonTerminal;
        int pos = 1;
        if (primeMark[i]) {
            rowLabel[pos++] = '\'';
        }
        rowLabel[pos] = '\0';
        cout << "| " << setw(NT_COL_WIDTH - 2) << left << rowLabel << " |";
        int rowIdx = getNonTerminalIndex(grammar[i].nonTerminal, primeMark[i]);
        for (auto c : termVec) {
            if (parseTable[rowIdx][(unsigned char)c][0] != '\0') {
                char cell[2 * MAX_PROD_LENGTH];
                snprintf(cell, sizeof(cell), "%s->%s", rowLabel, parseTable[rowIdx][(unsigned char)c]);
                cout << " " << setw(CELL_WIDTH - 2) << left << cell << " |";
            } else {
                cout << " " << setw(CELL_WIDTH - 2) << left << " " << " |";
            }
        }
        cout << "\n" << horizontalBorder << "\n";
    }
}

// ------------------------------------------------------------------
// MAIN
// ------------------------------------------------------------------
int main() {
    readGrammar("grammar.txt");
    std::cout << "Original Grammar:\n";
    printGrammar();
    std::cout << "\nGrammar after Left Factoring:\n";
    leftFactoring();
    printGrammar();
    std::cout << "\nGrammar after Left Recursion Removal:\n";
    removeLeftRecursion();
    printGrammar();
    computeFirst();
    computeFollow();
    printFirstAndFollowSets();
    buildParseTable();
    printLL1ParseTable();
    return 0;
}
