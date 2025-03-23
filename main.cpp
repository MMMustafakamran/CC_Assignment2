#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <set>
#include <string>
#include <iostream>
#include <unordered_map>
#include <iomanip>   //for setw and such

using namespace std;

namespace CFG {

//constants - quick numbers
const int MAX_NONTERMINALS = 52;   //26 letters and 26 primes
const int MAX_PRODUCTIONS   = 10;    //max prods per nonterminal
const int MAX_PROD_LENGTH   = 100;   //max length of a prod

//structure to hold a grammar production
struct GrammarProduction {
    char nonTerminal;                        //letter for nonterminal
    char productions[MAX_PRODUCTIONS][MAX_PROD_LENGTH]; //prod strings
    int prodCount;                           //number of prods
};

//global storage for grammar and sets
GrammarProduction grammar[MAX_NONTERMINALS];
int grammarCount = 0;  //count of grammar entries

set<string> firstSets[MAX_NONTERMINALS];   //first sets
set<string> followSets[MAX_NONTERMINALS];  //follow sets

//ll(1) table - key is terminal, value is production string
unordered_map<string, string> parseTable[MAX_NONTERMINALS];

bool primeMark[MAX_NONTERMINALS];  //mark if nonterminal is prime (like A')

//get index for a nonterminal
int getNonTerminalIndex(char c, bool isPrime) {
    int base = c - 'A';
    return isPrime ? base + 26 : base;
}

//get index for a symbol - if uppercase then nonterminal else -1
int symbolIndex(const char *sym) {
    if (!isupper((unsigned char)sym[0])) return -1; //not a nonterm
    if (strlen(sym) > 1 && sym[1] == '\'')
        return getNonTerminalIndex(sym[0], true);
    return getNonTerminalIndex(sym[0], false);
}

//print a nonterminal (with prime if needed)
void printNonTerminal(int idx) {
    if (primeMark[idx])
        printf("%c'", grammar[idx].nonTerminal);
    else
        printf("%c", grammar[idx].nonTerminal);
}

//trim spaces from a c-string
void trim(char *str) {
    int i = 0, j = 0;
    //skip leading spaces
    while (str[i] && isspace((unsigned char)str[i]))
        i++;
    //copy rest
    while (str[i])
        str[j++] = str[i++];
    str[j] = '\0';
    //remove trailing spaces
    for (i = j - 1; i >= 0 && isspace((unsigned char)str[i]); i--)
        str[i] = '\0';
}

//find an entry for a nonterminal
int findNonTerminalEntry(char nt, bool isPrime) {
    for (int i = 0; i < grammarCount; i++) {
        if (grammar[i].nonTerminal == nt && primeMark[i] == isPrime)
            return i;
    }
    return -1;
}

//read grammar from file "grammar.txt"
void readGrammar(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("error opening file %s\n", filename);
        exit(1);
    }
    char line[256];
    //go line by line
    while (fgets(line, sizeof(line), fp)) {
        if (strlen(line) <= 1)
            continue;
        //find arrow separator
        char *arrow = strstr(line, "->");
        if (!arrow)
            continue;
        //get lhs and trim it
        char lhs[10];
        strncpy(lhs, line, arrow - line);
        lhs[arrow - line] = '\0';
        trim(lhs);
        if (strlen(lhs) == 0)
            continue;
        //get nonterminal and check for prime
        char nt = lhs[0];
        bool isPrime = (strlen(lhs) > 1 && lhs[1] == '\'');
        //see if exists
        int idx = findNonTerminalEntry(nt, isPrime);
        if (idx == -1) {
            idx = grammarCount;
            grammar[idx].nonTerminal = nt;
            grammar[idx].prodCount = 0;
            primeMark[idx] = isPrime;
            grammarCount++;
        }
        //process rhs split by |
        char *rhs = arrow + 2;
        char *token = strtok(rhs, "|");
        while (token != NULL) {
            trim(token);
            if (grammar[idx].prodCount < MAX_PRODUCTIONS)
                strcpy(grammar[idx].productions[grammar[idx].prodCount++], token);
            token = strtok(NULL, "|");
        }
    }
    fclose(fp);
}

//print the grammar
void printGrammar() {
    printf("grammar:\n");
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

//get next symbol from a production string
bool nextSymbol(const char *prod, int &pos, char *outSym) {
    //skip spaces
    while (prod[pos] && isspace((unsigned char)prod[pos]))
        pos++;
    if (!prod[pos])
        return false;
    
    //if uppercase then nonterminal check for prime
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
    //if alnum token then read full token
    else if (isalnum((unsigned char)prod[pos])) {
        int start = pos;
        while (prod[pos] && isalnum((unsigned char)prod[pos]))
            pos++;
        int len = pos - start;
        strncpy(outSym, prod + start, len);
        outSym[len] = '\0';
        return true;
    }
    //else just one char (like punctuation)
    else {
        outSym[0] = prod[pos];
        outSym[1] = '\0';
        pos++;
        return true;
    }
}

//left factoring - factor common prefixes
void leftFactoring() {
    //loop through each grammar entry
    for (int i = 0; i < grammarCount; i++) {
        char A = grammar[i].nonTerminal;
        bool factored = false;
        //check each production for common start
        for (int j = 0; j < grammar[i].prodCount; j++) {
            char firstSym = grammar[i].productions[j][0];
            int count = 0;
            //count prods starting with same symbol
            for (int k = 0; k < grammar[i].prodCount; k++) {
                if (grammar[i].productions[k][0] == firstSym)
                    count++;
            }
            if (count > 1) {
                char commonPrefix[MAX_PROD_LENGTH];
                strcpy(commonPrefix, grammar[i].productions[j]);
                //find common part with other prods
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
                    //make new nonterminal like A'
                    char APrime[5];
                    sprintf(APrime, "%c'", A);
                    char newProds[MAX_PROD_LENGTH][MAX_PROD_LENGTH];
                    int newCount = 0;
                    char newProdsAPrime[MAX_PRODUCTIONS][MAX_PROD_LENGTH];
                    int newCountAPrime = 0;
                    //split prods based on common prefix
                    for (int k = 0; k < grammar[i].prodCount; k++) {
                        if (strncmp(grammar[i].productions[k], commonPrefix, prefixLen) == 0) {
                            char remainder[MAX_PROD_LENGTH];
                            strcpy(remainder, grammar[i].productions[k] + prefixLen);
                            if (strlen(remainder) == 0)
                                strcpy(remainder, "ε"); //empty prod
                            strcpy(newProdsAPrime[newCountAPrime++], remainder);
                        } else {
                            strcpy(newProds[newCount++], grammar[i].productions[k]);
                        }
                    }
                    //new factored production
                    char factoredProd[MAX_PROD_LENGTH * 2];
                    snprintf(factoredProd, MAX_PROD_LENGTH * 2, "%s%s", commonPrefix, APrime);
                    strcpy(newProds[newCount++], factoredProd);
                    //update current entry with factored prods
                    grammar[i].prodCount = newCount;
                    for (int k = 0; k < newCount; k++)
                        strcpy(grammar[i].productions[k], newProds[k]);
                    //add new entry for A'
                    int newIdx = grammarCount;
                    grammar[newIdx].nonTerminal = A;
                    primeMark[newIdx] = true;
                    grammar[newIdx].prodCount = 0;
                    for (int k = 0; k < newCountAPrime; k++)
                        strcpy(grammar[newIdx].productions[grammar[newIdx].prodCount++], newProdsAPrime[k]);
                    grammarCount++;
                    break;
                }
            }
        }
        if (factored)
            i--; //recheck if more factoring needed
    }
}

//remove left recursion for a specific entry
void removeLeftRecursionFor(int idx) {
    char nt = grammar[idx].nonTerminal;
    char ntPrime[5];
    sprintf(ntPrime, "%c'", nt);

    char nonRecProds[MAX_PRODUCTIONS][MAX_PROD_LENGTH];
    int nonRecCount = 0;
    char recProds[MAX_PRODUCTIONS][MAX_PROD_LENGTH];
    int recCount = 0;
    
    //split prods into left rec and non-left rec
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
    
    //if left recursion exists
    if (recCount > 0) {
        //add A' to non-recursive prods
        for (int i = 0; i < nonRecCount; i++) {
            char temp[MAX_PROD_LENGTH * 2];
            snprintf(temp, MAX_PROD_LENGTH * 2, "%s%s", nonRecProds[i], ntPrime);
            strcpy(nonRecProds[i], temp);
        }
        //update original prods with non-recursive ones
        for (int i = 0; i < nonRecCount; i++)
            strcpy(grammar[idx].productions[i], nonRecProds[i]);
        grammar[idx].prodCount = nonRecCount;
        
        //create new entry for A'
        int newIdx = grammarCount;
        grammar[newIdx].nonTerminal = nt;
        primeMark[newIdx] = true;
        grammar[newIdx].prodCount = 0;
        //add recursive prods followed by A'
        for (int i = 0; i < recCount; i++) {
            char temp[MAX_PROD_LENGTH * 2];
            snprintf(temp, MAX_PROD_LENGTH * 2, "%s%s", recProds[i], ntPrime);
            strcpy(grammar[newIdx].productions[grammar[newIdx].prodCount++], temp);
        }
        //add epsilon prod
        strcpy(grammar[newIdx].productions[grammar[newIdx].prodCount++], "ε");
        grammarCount++;
    }
}

//remove left recursion for all entries
void removeLeftRecursion() {
    for (int i = 0; i < grammarCount; i++) {
        if (grammar[i].prodCount > 0)
            removeLeftRecursionFor(i);
    }
}

//compute first sets
void computeFirst() {
    bool changed = true;
    //loop until nothing changes
    while (changed) {
        changed = false;
        for (int i = 0; i < grammarCount; i++) {
            int Aidx = getNonTerminalIndex(grammar[i].nonTerminal, primeMark[i]);
            //check each prod
            for (int j = 0; j < grammar[i].prodCount; j++) {
                char *prod = grammar[i].productions[j];
                //if prod is epsilon, add it
                if (strcmp(prod, "ε") == 0) {
                    if (firstSets[Aidx].insert("ε").second)
                        changed = true;
                    continue;
                }
                bool allEpsilon = true;
                int pos = 0;
                //go through symbols in prod
                while (true) {
                    char symbol[10];
                    if (!nextSymbol(prod, pos, symbol))
                        break;
                    int Bidx = symbolIndex(symbol);
                    //if terminal, add and break
                    if (Bidx < 0) {
                        if (firstSets[Aidx].insert(string(symbol)).second)
                            changed = true;
                        allEpsilon = false;
                        break;
                    } else {
                        //for nonterminal add its firsts (skip epsilon)
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

//compute follow sets
void computeFollow() {
    //add $ to start symbol follow
    if (grammarCount > 0) {
        int startIdx = getNonTerminalIndex(grammar[0].nonTerminal, primeMark[0]);
        followSets[startIdx].insert("$");
    }
    bool changed = true;
    //loop until no changes
    while (changed) {
        changed = false;
        for (int i = 0; i < grammarCount; i++) {
            int Aidx = getNonTerminalIndex(grammar[i].nonTerminal, primeMark[i]);
            //for each prod in entry
            for (int j = 0; j < grammar[i].prodCount; j++) {
                char *prod = grammar[i].productions[j];
                int pos = 0;
                //split prod into symbols
                char symbols[50][10];
                int symCount = 0;
                int tempPos = 0;
                while (nextSymbol(prod, tempPos, symbols[symCount]))
                    symCount++;
                //for each symbol update follow
                for (int k = 0; k < symCount; k++) {
                    int Bidx = symbolIndex(symbols[k]);
                    if (Bidx < 0)
                        continue;
                    bool allEpsilon = true;
                    //look at symbols after current one
                    for (int x = k + 1; x < symCount; x++) {
                        int nextIdx = symbolIndex(symbols[x]);
                        if (nextIdx < 0) { //terminal
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
                    //if everything after can be epsilon add follow of current entry
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

//build ll(1) parse table
void buildParseTable() {
    //clear table
    for (int i = 0; i < MAX_NONTERMINALS; i++)
        parseTable[i].clear();
    
    //for each grammar entry
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
                //get first set for prod
                while (true) {
                    char sym[10];
                    if (!nextSymbol(prod, pos2, sym))
                        break;
                    int idxSym = symbolIndex(sym);
                    if (idxSym < 0) {  //terminal
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
            //for each terminal in first set (skip epsilon)
            for (auto &term : firstProd) {
                if (term != "ε")
                    parseTable[Aidx][term] = string(prod);
            }
            //if epsilon in first add prod for each follow symbol
            if (firstProd.find("ε") != firstProd.end()) {
                for (auto &fw : followSets[Aidx]) {
                    if (parseTable[Aidx].find(fw) == parseTable[Aidx].end())
                        parseTable[Aidx][fw] = string(prod);
                }
            }
        }
    }
}

//print first and follow sets
void printSets() {
    printf("\nfirst sets:\n");
    for (int i = 0; i < grammarCount; i++) {
        int idx = getNonTerminalIndex(grammar[i].nonTerminal, primeMark[i]);
        printf("first(");
        printNonTerminal(i);
        printf(") = { ");
        for (auto &s : firstSets[idx])
            printf("%s ", s.c_str());
        printf("}\n");
    }
    
    printf("\nfollow sets:\n");
    for (int i = 0; i < grammarCount; i++) {
        int idx = getNonTerminalIndex(grammar[i].nonTerminal, primeMark[i]);
        printf("follow(");
        printNonTerminal(i);
        printf(") = { ");
        for (auto &s : followSets[idx])
            printf("%s ", s.c_str());
        printf("}\n");
    }
}

//print the ll(1) parse table
void printParseTable() {
    cout << "\nll(1) parsing table:\n";
    //header row
    cout << left << setw(15) << "nonterminal"
         << left << setw(15) << "terminal"
         << "production" << "\n";
    cout << string(50, '-') << "\n";
    
    //loop through table entries
    for (int i = 0; i < MAX_NONTERMINALS; i++) {
        for (auto &entry : parseTable[i]) {
            string terminal = entry.first;
            string production = entry.second;
            string nonterminal;
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

} //end namespace CFG

//main function - runs all steps
int main() {
    //read grammar from file
    CFG::readGrammar("grammar.txt");
    printf("original grammar:\n");
    CFG::printGrammar();

    //left factoring - quick common prefix fix
    CFG::leftFactoring();
    printf("\ngrammar after left factoring:\n");
    CFG::printGrammar();

    //remove left recursion - makes it ll(1)
    CFG::removeLeftRecursion();
    printf("\ngrammar after left recursion removal:\n");
    CFG::printGrammar();

    //compute first and follow sets
    CFG::computeFirst();
    CFG::computeFollow();
    CFG::printSets();

    //build and print parse table
    CFG::buildParseTable();
    CFG::printParseTable();

    return 0;
}
