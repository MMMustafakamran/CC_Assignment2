#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <set>
#include <string>
#include <iostream>
#include <unordered_map>

using namespace std;

// ------------------------------------------------------------------
// Constants
// ------------------------------------------------------------------
#define MAX_NONTERMINALS 52    // 26 letters for normal and 26 for prime versions
#define MAX_PRODUCTIONS 10     // maximum productions per nonterminal
#define MAX_PROD_LENGTH 100    // maximum length of a production string

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
int grammarCount = 0;  // number of grammar entries

// FIRST and FOLLOW sets stored in 52 slots.
set<string> firstSets[MAX_NONTERMINALS];
set<string> followSets[MAX_NONTERMINALS];

// LL(1) parsing table: an unordered_map for each nonterminal index keyed by terminal string.
unordered_map<string, string> parseTable[MAX_NONTERMINALS];

// primeMark array: if true then this grammar entry is a prime version (e.g., E').
bool primeMark[MAX_NONTERMINALS];

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
    if (!isupper((unsigned char)sym[0])) return -1; // terminal
    if (strlen(sym) > 1 && sym[1] == '\'')
        return getNonTerminalIndex(sym[0], true);
    return getNonTerminalIndex(sym[0], false);
}

// ------------------------------------------------------------------
// When printing a nonterminal, print E' if primeMark is true.
// idx is an index into grammar[].
// ------------------------------------------------------------------
void printNonTerminal(int idx) {
    if (primeMark[idx])
        printf("%c'", grammar[idx].nonTerminal);
    else
        printf("%c", grammar[idx].nonTerminal);
}

// ------------------------------------------------------------------
// Trim whitespace from a C-string (in place).
// ------------------------------------------------------------------
void trim(char *str) {
    int i = 0, j = 0;
    while (str[i] && isspace((unsigned char)str[i])) i++;
    while (str[i]) {
        str[j++] = str[i++];
    }
    str[j] = '\0';
    for (i = j - 1; i >= 0 && isspace((unsigned char)str[i]); i--) {
        str[i] = '\0';
    }
}

// ------------------------------------------------------------------
// Helper: Search for an existing grammar entry for nonterminal 'nt' with the given prime flag.
// ------------------------------------------------------------------
int findNonTerminalEntry(char nt, bool isPrime) {
    for (int i = 0; i < grammarCount; i++) {
        if (grammar[i].nonTerminal == nt && primeMark[i] == isPrime) {
            return i;
        }
    }
    return -1;
}

// ------------------------------------------------------------------
// Read grammar from file "grammar.txt"
// Each line should be of the form:
//    S -> if E then S, else S
//    S -> if E then S
//    S -> A
//
// This version merges productions for the same nonterminal.
// ------------------------------------------------------------------
void readGrammar(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("Error opening file %s\n", filename);
        exit(1);
    }
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strlen(line) <= 1)
            continue;
        char *arrow = strstr(line, "->");
        if (!arrow)
            continue;

        // Extract LHS and trim.
        char lhs[10];
        strncpy(lhs, line, arrow - line);
        lhs[arrow - line] = '\0';
        trim(lhs);
        if (strlen(lhs) == 0)
            continue;
        char nt = lhs[0];
        bool isPrime = (strlen(lhs) > 1 && lhs[1] == '\'');

        // Check if this nonterminal already exists.
        int idx = findNonTerminalEntry(nt, isPrime);
        if (idx == -1) {
            // Create a new grammar entry if not found.
            idx = grammarCount;
            grammar[idx].nonTerminal = nt;
            grammar[idx].prodCount = 0;
            primeMark[idx] = isPrime;
            grammarCount++;
        }
        
        // Process RHS productions (separated by '|')
        char *rhs = arrow + 2;
        char *token = strtok(rhs, "|");
        while (token != NULL) {
            trim(token);
            if (grammar[idx].prodCount < MAX_PRODUCTIONS) {
                strcpy(grammar[idx].productions[grammar[idx].prodCount++], token);
            }
            token = strtok(NULL, "|");
        }
    }
    fclose(fp);
}

// ------------------------------------------------------------------
// Print the current grammar.
// ------------------------------------------------------------------
void printGrammar() {
    printf("Grammar:\n");
    for (int i = 0; i < grammarCount; i++) {
        if (grammar[i].prodCount > 0) {
            printNonTerminal(i);
            printf(" -> ");
            for (int j = 0; j < grammar[i].prodCount; j++) {
                printf("%s", grammar[i].productions[j]);
                if (j != grammar[i].prodCount - 1)
                    printf(" | ");
            }
            printf("\n");
        }
    }
}

// ------------------------------------------------------------------
// Helper: Parse the next symbol from a production string.
// It detects if an uppercase letter is immediately followed by a prime marker.
// For terminals starting with a lowercase letter or a digit, it reads the full token (alphanumeric).
// Writes the symbol into outSym and advances pos.
// Returns false if end-of-string is reached.
// ------------------------------------------------------------------
bool nextSymbol(const char *prod, int &pos, char *outSym) {
    // Skip whitespace
    while (prod[pos] && isspace((unsigned char)prod[pos]))
        pos++;
    if (!prod[pos])
        return false;
    
    // Nonterminal: Uppercase letter, possibly followed by an apostrophe.
    if (isupper((unsigned char)prod[pos])) {
        outSym[0] = prod[pos];
        if (prod[pos+1] == '\'') {
            outSym[1] = '\'';
            outSym[2] = '\0';
            pos += 2;
        } else {
            outSym[1] = '\0';
            pos++;
        }
        return true;
    }
    // Terminal: For alphanumeric tokens (like "id"), read the full token.
    else if (isalnum((unsigned char)prod[pos])) {
        int start = pos;
        while (prod[pos] && isalnum((unsigned char)prod[pos])) {
            pos++;
        }
        int len = pos - start;
        strncpy(outSym, prod + start, len);
        outSym[len] = '\0';
        return true;
    }
    // Other terminal symbols (like punctuation), read one character.
    else {
        outSym[0] = prod[pos];
        outSym[1] = '\0';
        pos++;
        return true;
    }
}

// ------------------------------------------------------------------
// LEFT FACTORING
//
// For each grammar entry, if two or more productions share a common prefix,
// factor them out by creating a new prime nonterminal (e.g., E').
// ------------------------------------------------------------------
void leftFactoring() {
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
                strcpy(commonPrefix, grammar[i].productions[j]);
                for (int k = j+1; k < grammar[i].prodCount; k++) {
                    if (grammar[i].productions[k][0] == firstSym) {
                        int idx = 0;
                        while (commonPrefix[idx] && grammar[i].productions[k][idx] &&
                               commonPrefix[idx] == grammar[i].productions[k][idx])
                            idx++;
                        commonPrefix[idx] = '\0';
                    }
                }
                int prefixLen = strlen(commonPrefix);
                if (prefixLen > 0) {
                    factored = true;
                    char APrime[5];
                    sprintf(APrime, "%c'", A);
                    char newProds[MAX_PRODUCTIONS][MAX_PROD_LENGTH];
                    int newCount = 0;
                    char newProdsAPrime[MAX_PRODUCTIONS][MAX_PROD_LENGTH];
                    int newCountAPrime = 0;
                    for (int k = 0; k < grammar[i].prodCount; k++) {
                        if (strncmp(grammar[i].productions[k], commonPrefix, prefixLen) == 0) {
                            char remainder[MAX_PROD_LENGTH];
                            strcpy(remainder, grammar[i].productions[k] + prefixLen);
                            if (strlen(remainder) == 0)
                                strcpy(remainder, "ε");
                            strcpy(newProdsAPrime[newCountAPrime++], remainder);
                        } else {
                            strcpy(newProds[newCount++], grammar[i].productions[k]);
                        }
                    }
                    // Use a larger temporary buffer to avoid truncation.
                    char factoredProd[MAX_PROD_LENGTH * 2];
                    snprintf(factoredProd, MAX_PROD_LENGTH * 2, "%s%s", commonPrefix, APrime);
                    strcpy(newProds[newCount++], factoredProd);
                    grammar[i].prodCount = newCount;
                    for (int k = 0; k < newCount; k++) {
                        strcpy(grammar[i].productions[k], newProds[k]);
                    }
                    // Create a new grammar entry for A'
                    int newIdx = grammarCount;
                    grammar[newIdx].nonTerminal = A; // same letter
                    primeMark[newIdx] = true;        // mark as prime (A')
                    grammar[newIdx].prodCount = 0;
                    for (int k = 0; k < newCountAPrime; k++) {
                        strcpy(grammar[newIdx].productions[grammar[newIdx].prodCount++], newProdsAPrime[k]);
                    }
                    grammarCount++;
                    break;
                }
            }
        }
        if (factored)
            i--; // Re-check this nonterminal if further factoring is possible.
    }
}

// ------------------------------------------------------------------
// LEFT RECURSION REMOVAL
//
// For grammar entry A, partition productions into left-recursive and non-recursive,
// then rewrite as:
//    A -> nonRecProd A'
//    A' -> recProd A' | ε
// ------------------------------------------------------------------
void removeLeftRecursionFor(int idx) {
    char nt = grammar[idx].nonTerminal;
    char ntPrime[5];
    sprintf(ntPrime, "%c'", nt);

    char nonRecProds[MAX_PRODUCTIONS][MAX_PROD_LENGTH];
    int nonRecCount = 0;
    char recProds[MAX_PRODUCTIONS][MAX_PROD_LENGTH];
    int recCount = 0;
    
    for (int i = 0; i < grammar[idx].prodCount; i++) {
        char *prod = grammar[idx].productions[i];
        if (prod[0] == nt) {
            if (strlen(prod) > 1)
                strcpy(recProds[recCount++], prod + 1);
            else
                strcpy(recProds[recCount++], "");
        } else {
            strcpy(nonRecProds[nonRecCount++], prod);
        }
    }
    
    if (recCount > 0) {
        for (int i = 0; i < nonRecCount; i++) {
            // Use an enlarged temporary buffer to avoid truncation.
            char temp[MAX_PROD_LENGTH * 2];
            snprintf(temp, MAX_PROD_LENGTH * 2, "%s%s", nonRecProds[i], ntPrime);
            strcpy(nonRecProds[i], temp);
        }
        for (int i = 0; i < nonRecCount; i++) {
            strcpy(grammar[idx].productions[i], nonRecProds[i]);
        }
        grammar[idx].prodCount = nonRecCount;
        
        // Create new entry for A'
        int newIdx = grammarCount;
        grammar[newIdx].nonTerminal = nt;  // same letter
        primeMark[newIdx] = true;          // mark as prime
        grammar[newIdx].prodCount = 0;
        for (int i = 0; i < recCount; i++) {
            char temp[MAX_PROD_LENGTH * 2];
            snprintf(temp, MAX_PROD_LENGTH * 2, "%s%s", recProds[i], ntPrime);
            strcpy(grammar[newIdx].productions[grammar[newIdx].prodCount++], temp);
        }
        strcpy(grammar[newIdx].productions[grammar[newIdx].prodCount++], "ε");
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
// Compute FIRST sets for each grammar entry.
// ------------------------------------------------------------------
void computeFirst() {
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < grammarCount; i++) {
            int Aidx = getNonTerminalIndex(grammar[i].nonTerminal, primeMark[i]);
            for (int j = 0; j < grammar[i].prodCount; j++) {
                char *prod = grammar[i].productions[j];
                if (strcmp(prod, "ε") == 0) {
                    if (firstSets[Aidx].insert("ε").second)
                        changed = true;
                    continue;
                }
                bool allEpsilon = true;
                int pos = 0;
                while (true) {
                    char symbol[10];
                    if (!nextSymbol(prod, pos, symbol))
                        break;
                    int Bidx = symbolIndex(symbol);
                    if (Bidx < 0) {
                        if (firstSets[Aidx].insert(string(symbol)).second)
                            changed = true;
                        allEpsilon = false;
                        break;
                    } else {
                        bool hasEpsilon = false;
                        for (auto &s : firstSets[Bidx]) {
                            if (s == "ε")
                                hasEpsilon = true;
                            else {
                                if (firstSets[Aidx].insert(s).second)
                                    changed = true;
                            }
                        }
                        if (!hasEpsilon) {
                            allEpsilon = false;
                            break;
                        }
                    }
                }
                if (allEpsilon) {
                    if (firstSets[Aidx].insert("ε").second)
                        changed = true;
                }
            }
        }
    }
}

// ------------------------------------------------------------------
// Compute FOLLOW sets for each grammar entry.
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
                int pos = 0;
                // Collect symbols from the production.
                char symbols[50][10];
                int symCount = 0;
                int tempPos = 0;
                while (nextSymbol(prod, tempPos, symbols[symCount])) {
                    symCount++;
                }
                for (int k = 0; k < symCount; k++) {
                    int Bidx = symbolIndex(symbols[k]);
                    if (Bidx < 0)
                        continue;
                    bool allEpsilon = true;
                    for (int x = k + 1; x < symCount; x++) {
                        int nextIdx = symbolIndex(symbols[x]);
                        if (nextIdx < 0) {
                            if (followSets[Bidx].insert(string(symbols[x])).second)
                                changed = true;
                            allEpsilon = false;
                            break;
                        } else {
                            bool hasEps = false;
                            for (auto &fs : firstSets[nextIdx]) {
                                if (fs == "ε")
                                    hasEps = true;
                                else {
                                    if (followSets[Bidx].insert(fs).second)
                                        changed = true;
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
                            if (followSets[Bidx].insert(fa).second)
                                changed = true;
                        }
                    }
                }
            }
        }
    }
}

// ------------------------------------------------------------------
// Build the LL(1) Parsing Table.
// ------------------------------------------------------------------
void buildParseTable() {
    // Clear the parse table.
    for (int i = 0; i < MAX_NONTERMINALS; i++) {
        parseTable[i].clear();
    }
    for (int i = 0; i < grammarCount; i++) {
        int Aidx = getNonTerminalIndex(grammar[i].nonTerminal, primeMark[i]);
        for (int j = 0; j < grammar[i].prodCount; j++) {
            char *prod = grammar[i].productions[j];
            set<string> firstProd;
            int pos = 0;
            if (strcmp(prod, "ε") == 0) {
                firstProd.insert("ε");
            } else {
                bool allEps = true;
                int pos2 = 0;
                while (true) {
                    char sym[10];
                    if (!nextSymbol(prod, pos2, sym))
                        break;
                    int idxSym = symbolIndex(sym);
                    if (idxSym < 0) {
                        firstProd.insert(string(sym));
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
                if (allEps)
                    firstProd.insert("ε");
            }
            // For each terminal in FIRST(prod) except ε, add production.
            for (auto &term : firstProd) {
                if (term != "ε") {
                    parseTable[Aidx][term] = string(prod);
                }
            }
            // If ε is in FIRST(prod), then for every terminal in FOLLOW(A) add production,
            // but only if there isn't already a production entered.
            if (firstProd.find("ε") != firstProd.end()) {
                for (auto &fw : followSets[Aidx]) {
                    if (parseTable[Aidx].find(fw) == parseTable[Aidx].end()) {
                        parseTable[Aidx][fw] = string(prod);
                    }
                }
            }
        }
    }
}

// ------------------------------------------------------------------
// Print FIRST and FOLLOW sets.
// ------------------------------------------------------------------
void printSets() {
    printf("\nFIRST Sets:\n");
    for (int i = 0; i < grammarCount; i++) {
        int idx = getNonTerminalIndex(grammar[i].nonTerminal, primeMark[i]);
        printf("FIRST(");
        printNonTerminal(i);
        printf(") = { ");
        for (auto &s : firstSets[idx]) {
            printf("%s ", s.c_str());
        }
        printf("}\n");
    }
    
    printf("\nFOLLOW Sets:\n");
    for (int i = 0; i < grammarCount; i++) {
        int idx = getNonTerminalIndex(grammar[i].nonTerminal, primeMark[i]);
        printf("FOLLOW(");
        printNonTerminal(i);
        printf(") = { ");
        for (auto &s : followSets[idx]) {
            printf("%s ", s.c_str());
        }
        printf("}\n");
    }
}
#include <iomanip>  // Include for setw, left, etc.

// ------------------------------------------------------------------
// Print the LL(1) Parsing Table in a formatted table.
// ------------------------------------------------------------------
void printParseTable() {
    cout << "\nLL(1) Parsing Table:\n";
    // Print header row with fixed width columns.
    cout << left << setw(15) << "Nonterminal"
         << left << setw(15) << "Terminal"
         << "Production" << "\n";
    cout << string(50, '-') << "\n";
    
    for (int i = 0; i < MAX_NONTERMINALS; i++) {
        for (auto &entry : parseTable[i]) {
            string terminal = entry.first;
            string production = entry.second;
            string nonterminal;
            // Determine proper nonterminal label (e.g., A or A')
            if (i >= 26)
                nonterminal = string(1, 'A' + i - 26) + "'";
            else
                nonterminal = string(1, 'A' + i);
            cout << left << setw(15) << nonterminal
                 << left << setw(15) << terminal
                 << production << "\n";
        }
    }
}


// ------------------------------------------------------------------
// MAIN
// ------------------------------------------------------------------
int main() {
    // 1. Read the CFG from "grammar.txt"
    readGrammar("grammar.txt");
    printf("Original Grammar:\n");
    printGrammar();

    // 2. Apply Left Factoring
    leftFactoring();
    printf("\nGrammar after Left Factoring:\n");
    printGrammar();

    // 3. Apply Left Recursion Removal
    removeLeftRecursion();
    printf("\nGrammar after Left Recursion Removal:\n");
    printGrammar();

    // 4. Compute FIRST sets
    computeFirst();

    // 5. Compute FOLLOW sets
    computeFollow();

    // Print FIRST and FOLLOW sets
    printSets();

    // 6. Build LL(1) Parsing Table and print it.
    buildParseTable();
    printParseTable();

    return 0;
}
