#ifndef SAVE
#define SAVE

#include "node.h"
#include "uniquetable.h"
#include "counts.h"

// Writes the BDD with the given root to a binary file.
// Each node takes 9 bytes: variable (1 byte), index of low node (4 bytes), index of high node (4 bytes).
// The index of a node is its position in the file. Children are stored before their parents, the root is last.
// The constants 0 and 1 are stored at index 0 and 1 (the file for the constant 0 only contains the constant 0).
// varmap maps the variable of each node to the variable that is written to the file (has to be at most 255).
// Returns the number of nodes written.
//
// No additional memory is needed: the file index of a node is temporarily stored in its `next` field.
// This destroys the bucket lists of the unique table, which are rebuilt before the function returns.
// (With COMPACT_NODES the nodes are written in the order of their index instead, the root last,
// which needs 1.5 bits per slot of the node array as temporary memory.)
uint64_t _safe_to_file_with_varmap(nodeindex_t root, char* filename, variable_t varmap[MAX_VARIABLES + 1]);
uint64_t _safe_to_file(nodeindex_t root, char* filename);

// With INDEX_BITS=40, a BDD with 2^32 or more nodes is written with 5 instead of 4 bytes per index (11 bytes per node)
// to a file with the additional suffix ".wide". The probing programs cannot read these files.

// removes the file of a BDD (whichever of the two formats it has)
void _remove_bdd_file(char* filename);

// Reads a BDD from a file and returns its root.
// varmap maps the variable stored in the file to the variable of the node that is created.
// The returned node is not kept alive.
// The nodes may be stored in any order, as long as the root is last.
// Needs 4 bytes per node in the file as temporary memory.
nodeindex_t _read_from_file_with_varmap(char* filename, variable_t varmap[256]);
nodeindex_t _read_from_file(char* filename);

#endif
