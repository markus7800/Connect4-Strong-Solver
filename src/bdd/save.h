#ifndef SAVE
#define SAVE

#include "node.h"
#include "uniquetable.h"
#include "counts.h"
#include "nodeindexmap.h"

uint64_t _collect_nodes_into_map(nodeindex_t ix, nodeindexmap_t* map);
uint64_t _safe_to_file_with_varmap(nodeindex_t root, char* filename, nodeindexmap_t* map, variable_t varmap[256]);
uint64_t _safe_to_file(nodeindex_t root, char* filename, nodeindexmap_t* map);
nodeindex_t _read_from_file(char* filename, nodeindexmap_t* map);

#endif