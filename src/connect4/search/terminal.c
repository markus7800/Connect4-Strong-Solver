
/*
Terminal positions: the player who made the last move has four stones in a row, column or diagonal.

The condition "player has four in a line" does not depend on the set of positions it is applied to.
Its BDD is built once per player at start-up and reused for every ply:

    non-terminal positions = current \ four(player)
    terminal positions     = current ∧ four(player)

The BDD is kept as up to four parts (lines in a column, in a row, on ascending and on descending diagonals),
four(player) is the union of the parts. If the union is small enough the parts are merged into one BDD.
(The size of the union grows much faster with the board size than the size of the parts.)

All BDDs use board0 variables.
*/

// The parts are merged if the union does not need more than this fraction of the allocatable nodes.
#ifndef TERMINAL_MERGE_FRACTION
#define TERMINAL_MERGE_FRACTION (1.0 / 64)
#endif

#define TERMINAL_MAX_PARTS 4

typedef struct Terminal {
    nodeindex_t parts[2][TERMINAL_MAX_PARTS]; // parts[player]
    int num_parts[2];
} terminal_t;

// four stones of player in a line that starts at (col, row) and continues in direction (dcol, drow)
static nodeindex_t terminal_line(cellvars_t X, uint32_t height, int col, int row, int dcol, int drow, uint8_t player) {
    nodeindex_t a = ONEINDEX;
    keepalive_ix(a);
    for (int i = 0; i < 4; i++) {
        reassign_and_keepalive(&a, and(a, cell_of_player(X, height, (uint8_t) (col + i * dcol), (uint8_t) (row + i * drow), 0, player)));
    }
    undo_keepalive_ix(a);
    return a;
}

// union of all lines of player in one direction
static nodeindex_t terminal_direction(cellvars_t X, int width, int height, int dcol, int drow, uint8_t player) {
    nodeindex_t part = ZEROINDEX;
    keepalive_ix(part);
    for (int col = 0; col < width; col++) {
        for (int row = 0; row < height; row++) {
            int endcol = col + 3 * dcol;
            int endrow = row + 3 * drow;
            if (endcol < 0 || endcol >= width || endrow < 0 || endrow >= height) {
                continue;
            }
            reassign_and_keepalive(&part, or(part, terminal_line(X, (uint32_t) height, col, row, dcol, drow, player)));
        }
    }
    undo_keepalive_ix(part);
    return part;
}

// builds the BDDs for both players and keeps them alive
void init_terminal(terminal_t* terminal, cellvars_t X, uint32_t width, uint32_t height) {
    const int directions[TERMINAL_MAX_PARTS][2] = {{0, 1}, {1, 0}, {1, 1}, {-1, 1}};
    const char* names[TERMINAL_MAX_PARTS] = {"column", "row", "diagonal /", "diagonal \\"};
    for (uint8_t player = 0; player < 2; player++) {
        int n = 0;
        nodeindex_t merged = ZEROINDEX;
        keepalive_ix(merged);
        printf("Four in a line for player %d:", player);
        for (int d = 0; d < TERMINAL_MAX_PARTS; d++) {
            nodeindex_t part = terminal_direction(X, (int) width, (int) height, directions[d][0], directions[d][1], player);
            if (part == ZEROINDEX) {
                continue; // board is too small for this direction
            }
            keepalive_ix(part);
            terminal->parts[player][n++] = part;
            printf(" %s BDD(%"PRIu64")", names[d], nodecount(part));
        }
        terminal->num_parts[player] = n;

        // try to merge the parts, stop if the union gets too large
        bool merge = n > 1;
        for (int i = 0; i < n && merge; i++) {
            reassign_and_keepalive(&merged, or(merged, terminal->parts[player][i]));
            merge = nodecount(merged) <= TERMINAL_MERGE_FRACTION * memorypool.capacity;
        }
        if (merge) {
            for (int i = 0; i < n; i++) {
                undo_keepalive_ix(terminal->parts[player][i]);
            }
            terminal->parts[player][0] = merged;
            terminal->num_parts[player] = 1;
            printf(", merged BDD(%"PRIu64")\n", nodecount(merged));
        } else {
            undo_keepalive_ix(merged);
            printf(", not merged\n");
        }
    }
}

// all positions in current where player does not have four in a line
nodeindex_t connect4_subtract_term(terminal_t* terminal, nodeindex_t current, uint8_t player) {
    keepalive_ix(current);
    for (int i = 0; i < terminal->num_parts[player]; i++) {
        reassign_and_keepalive(&current, diff(current, terminal->parts[player][i]));
    }
    undo_keepalive_ix(current);
    return current;
}

// all positions in current where player has four in a line
nodeindex_t connect4_intersect_term(terminal_t* terminal, nodeindex_t current, uint8_t player) {
    if (terminal->num_parts[player] == 1) {
        return and(current, terminal->parts[player][0]);
    }
    return diff(current, connect4_subtract_term(terminal, current, player));
}
