#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_SYMBOLS 100
#define MAX_PRODUCTIONS 100
#define MAX_RHS_LENGTH 50
#define MAX_SYMBOL_LENGTH 20
#define EPSILON "ε"

// Structure to represent a production
typedef struct {
    char lhs[MAX_SYMBOL_LENGTH];
    char rhs[MAX_RHS_LENGTH][MAX_SYMBOL_LENGTH];
    int rhs_count;
} Production;

// Structure to represent a grammar
typedef struct {
    char non_terminals[MAX_SYMBOLS][MAX_SYMBOL_LENGTH];
    int non_terminal_count;
    char terminals[MAX_SYMBOLS][MAX_SYMBOL_LENGTH];
    int terminal_count;
    Production productions[MAX_PRODUCTIONS];
    int production_count;
    char start_symbol[MAX_SYMBOL_LENGTH];
} Grammar;

// Structure for FIRST and FOLLOW sets
typedef struct {
    char symbol[MAX_SYMBOL_LENGTH];
    char set[MAX_SYMBOLS][MAX_SYMBOL_LENGTH];
    int count;
} SymbolSet;

// Structure for LL(1) parsing table entry
typedef struct {
    int production_index;  // Index of production to use (-1 for error)
} TableEntry;



// Check if a symbol is a non-terminal
bool is_non_terminal(Grammar* g, char* symbol) {
    for (int i = 0; i < g->non_terminal_count; i++) {
        if (strcmp(g->non_terminals[i], symbol) == 0)
            return true;
    }
    return false;
}

// Add symbol to set if not already present
bool add_to_set(SymbolSet* set, char* symbol) {
    for (int i = 0; i < set->count; i++) {
        if (strcmp(set->set[i], symbol) == 0)
            return false;  // Already in set
    }
    strcpy(set->set[set->count++], symbol);
    return true;  // Added to set
}

// Read grammar from file
Grammar read_grammar(const char* filename) {
    Grammar g = {0};
    FILE* file = fopen(filename, "r");
    
    if (!file) {
        fprintf(stderr, "Error opening file: %s\n", filename);
        exit(1);
    }
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Remove newline character
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n')
            line[len-1] = '\0';
            
        // Skip empty lines
        if (strlen(line) == 0)
            continue;
            
        // Parse production rule (LHS -> RHS1 | RHS2 | ...)
        char* arrow = strstr(line, "->");
        if (!arrow) {
            fprintf(stderr, "Invalid production format: %s\n", line);
            continue;
        }
        
        // Extract LHS
        char lhs[MAX_SYMBOL_LENGTH] = {0};
        strncpy(lhs, line, arrow - line);
        
        // Trim whitespace
        char* p = lhs;
        while (*p == ' ') p++;
        memmove(lhs, p, strlen(p) + 1);
        p = lhs + strlen(lhs) - 1;
        while (p >= lhs && *p == ' ') *p-- = '\0';
        
        // Add LHS to non-terminals if new
        bool found = false;
        for (int i = 0; i < g.non_terminal_count; i++) {
            if (strcmp(g.non_terminals[i], lhs) == 0) {
                found = true;
                break;
            }
        }
        if (!found)
            strcpy(g.non_terminals[g.non_terminal_count++], lhs);
        
        // Set start symbol if this is the first production
        if (g.production_count == 0)
            strcpy(g.start_symbol, lhs);
        
        // Process RHS parts (split by |)
        char* rhs = arrow + 2;  // Skip ->
        char* token = strtok(rhs, "|");
        
        while (token) {
            // Trim whitespace
            while (*token == ' ') token++;
            p = token + strlen(token) - 1;
            while (p >= token && *p == ' ') *p-- = '\0';
            
            // Create new production
            Production prod;
            strcpy(prod.lhs, lhs);
            prod.rhs_count = 0;
            
            // Split RHS into symbols
            char* symbol_start = token;
            char symbol[MAX_SYMBOL_LENGTH];
            
            while (*symbol_start) {
                // Extract one symbol
                int i = 0;
                while (*symbol_start && *symbol_start != ' ')
                    symbol[i++] = *symbol_start++;
                symbol[i] = '\0';
                
                // Skip whitespace
                while (*symbol_start == ' ') symbol_start++;
                
                // Add to production RHS
                strcpy(prod.rhs[prod.rhs_count++], symbol);
                
                // Check if this is a terminal (not in non-terminals and not epsilon)
                if (!is_non_terminal(&g, symbol) && strcmp(symbol, EPSILON) != 0) {
                    bool terminal_exists = false;
                    for (int i = 0; i < g.terminal_count; i++) {
                        if (strcmp(g.terminals[i], symbol) == 0) {
                            terminal_exists = true;
                            break;
                        }
                    }
                    if (!terminal_exists)
                        strcpy(g.terminals[g.terminal_count++], symbol);
                }
            }
            
            // Add production to grammar
            g.productions[g.production_count++] = prod;
            
            // Get next token
            token = strtok(NULL, "|");
        }
    }
    
    fclose(file);
    return g;
}