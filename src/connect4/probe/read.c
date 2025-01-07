
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h> // close
#include <fcntl.h> // open
#include <sys/mman.h> // mmap
#include <sys/stat.h> // file size

// utility to read in BDD .bin files either as mmap or into memory

char* (*mmaps)[3];
off_t (*st_sizes)[3];
bool (*in_memory)[3];

void init_mmaps(uint32_t width, uint32_t height) {
    mmaps = malloc((width*height+1) * sizeof(*mmaps));
    st_sizes = malloc((width*height+1) * sizeof(*st_sizes));
    in_memory = malloc((width*height+1) * sizeof(*in_memory));
    assert(mmaps != NULL);
    for (int ply = 0; ply <= width*height; ply++) {
        for (int i = 0; i < 3; i++) {
            mmaps[ply][i] = NULL;
        }
    }
}

void bin_filename(char filename[], uint32_t width, uint32_t height, int ply, int i, char* suffix) {
    // COMPRESSED_ENCODING = 1, ALLOW_ROW_ORDER = 0
    sprintf(filename, "bdd_w%"PRIu32"_h%"PRIu32"_%d_%s.10.bin", width, height, ply, suffix);
}

void _make_mmap(uint32_t width, uint32_t height, int ply, int i, char* suffix) {
    char filename[50];
    struct stat st;

    bin_filename(filename, width, height, ply, i, suffix);
    stat(filename, &st);

    char* map;

    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        // mmaps[ply][i] = NULL;
        // return;
        printf("Could not open file %s.\n", filename);
        exit(EXIT_FAILURE);
    }
    assert(mmaps[ply][i] == NULL);

    // printf("%s %llu %"PRIu32"\n", filename, st.st_size, nodecount);
    map = (char*) mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        perror("Error mmapping the file");
        exit(EXIT_FAILURE);
    }

    mmaps[ply][i] = map;
    st_sizes[ply][i] = st.st_size;
    in_memory[ply][i] = false;
    // printf("made mmap %u %u %d %d\n", width, height, ply, i);

    close(fd);
}

void make_mmap(uint32_t width, uint32_t height, int ply) {
    char* suffix;

    for (int i = 0; i < 3; i++) {
        if (i==0) {
            suffix = "lost";
        } else if (i==1) {
            suffix = "draw";
            continue;
        } else {
            suffix = "win";
        }
        _make_mmap(width, height, ply, i, suffix);
    }
}

void _read_in_memory(uint32_t width, uint32_t height, int ply, int i, char* suffix) {
    char filename[50];
    struct stat st;

    bin_filename(filename, width, height, ply, i, suffix);
    stat(filename, &st);

    char* map;

    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        // mmaps[ply][i] = NULL;
        // return;
        printf("Could not open file %s.\n", filename);
        exit(EXIT_FAILURE);
    }
    assert(mmaps[ply][i] == NULL);

    map = (char*) malloc(st.st_size);
    if (map == NULL) {
        fclose(file);
        perror("Error making buffer for file");
        exit(EXIT_FAILURE);
    }
    size_t bytes_read = fread(map, sizeof(char), st.st_size, file);
    assert(bytes_read == st.st_size);

    mmaps[ply][i] = map;
    st_sizes[ply][i] = st.st_size;
    in_memory[ply][i] = true;
    // printf("read in memory %u %u %d %d\n", width, height, ply, i);

    fclose(file);
}


void make_mmaps(uint32_t width, uint32_t height) {
    init_mmaps(width, height);
    for (int ply = 0; ply <= width*height; ply++) {
        make_mmap(width, height, ply);
    }       
}

void read_in_memory(uint32_t width, uint32_t height, int ply) {

    char* suffix;

    for (int i = 0; i < 3; i++) {
        if (i==0) {
            suffix = "lost";
        } else if (i==1) {
            suffix = "draw";
            continue;
        } else {
            suffix = "win";
        }

        _read_in_memory(width, height, ply, i, suffix);
    }
}


void make_mmaps_read_in_memory(uint32_t width, uint32_t height) {
    init_mmaps(width, height);
    for (int ply = 0; ply <= width*height; ply++) {
        read_in_memory(width, height, ply);
    }
}

void read_lost_in_memory(uint32_t width, uint32_t height, int ply) {
    char* suffix;

    for (int i = 0; i < 3; i++) {
        if (i==0) {
            suffix = "lost";
            _read_in_memory(width, height, ply, i, suffix);
        } else if (i==1) {
            suffix = "draw";
            continue;
        } else {
            suffix = "win";
            _make_mmap(width, height, ply, i, suffix);
        }
    }
}


void make_mmaps_read_lost_in_memory(uint32_t width, uint32_t height) {
    init_mmaps(width, height);
    for (int ply = 0; ply <= width*height; ply++) {
        read_lost_in_memory(width, height, ply);
    }
}


void free_mmap(uint32_t width, uint32_t height, int ply) {
    for (int i = 0; i < 3; i++) {
        if (mmaps[ply][i] != NULL) {
            if (in_memory[ply][i]) {
                free(mmaps[ply][i]);
            } else {
                // mmap
                off_t size = st_sizes[ply][i];
                int unmap = munmap(mmaps[ply][i], size);
                if (unmap == -1) {
                    perror("Error unmmapping the file");
                    exit(EXIT_FAILURE);
                }
            }
            mmaps[ply][i] = NULL;
            // printf("freed %u %u %d %d\n", width, height, ply, i);
        }
    }
}

void free_mmaps(uint32_t width, uint32_t height) {
    for (int ply = 0; ply <= width*height; ply++) { 
        free_mmap(width, height, ply);
    }       
}