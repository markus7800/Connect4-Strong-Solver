#ifndef SAVE
#define SAVE

#include "node.h"
#include "uniquetable.h"
#include "counts.h"
#include "nodeindexmap.h"

void _safe_to_file_with_varmap(nodeindex_t root, char* filename, nodeindexmap_t* map, variable_t varmap[256]);
void _safe_to_file(nodeindex_t root, char* filename, nodeindexmap_t* map);
nodeindex_t _read_from_file(char* filename, nodeindexmap_t* map);

#endif