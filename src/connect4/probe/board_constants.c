#if (WIDTH == 1 && HEIGHT == 1)
    #define BOARD_MASK 0x1
    #define BOTTOM_MASK 0x1
    #define STATIC_MOVE_ORDER {0}
#endif
#if (WIDTH == 1 && HEIGHT == 2)
    #define BOARD_MASK 0x3
    #define BOTTOM_MASK 0x1
    #define STATIC_MOVE_ORDER {0}
#endif
#if (WIDTH == 1 && HEIGHT == 3)
    #define BOARD_MASK 0x7
    #define BOTTOM_MASK 0x1
    #define STATIC_MOVE_ORDER {0}
#endif
#if (WIDTH == 1 && HEIGHT == 4)
    #define BOARD_MASK 0xf
    #define BOTTOM_MASK 0x1
    #define STATIC_MOVE_ORDER {0}
#endif
#if (WIDTH == 1 && HEIGHT == 5)
    #define BOARD_MASK 0x1f
    #define BOTTOM_MASK 0x1
    #define STATIC_MOVE_ORDER {0}
#endif
#if (WIDTH == 1 && HEIGHT == 6)
    #define BOARD_MASK 0x3f
    #define BOTTOM_MASK 0x1
    #define STATIC_MOVE_ORDER {0}
#endif
#if (WIDTH == 1 && HEIGHT == 7)
    #define BOARD_MASK 0x7f
    #define BOTTOM_MASK 0x1
    #define STATIC_MOVE_ORDER {0}
#endif
#if (WIDTH == 1 && HEIGHT == 8)
    #define BOARD_MASK 0xff
    #define BOTTOM_MASK 0x1
    #define STATIC_MOVE_ORDER {0}
#endif
#if (WIDTH == 1 && HEIGHT == 9)
    #define BOARD_MASK 0x1ff
    #define BOTTOM_MASK 0x1
    #define STATIC_MOVE_ORDER {0}
#endif
#if (WIDTH == 1 && HEIGHT == 10)
    #define BOARD_MASK 0x3ff
    #define BOTTOM_MASK 0x1
    #define STATIC_MOVE_ORDER {0}
#endif
#if (WIDTH == 1 && HEIGHT == 11)
    #define BOARD_MASK 0x7ff
    #define BOTTOM_MASK 0x1
    #define STATIC_MOVE_ORDER {0}
#endif
#if (WIDTH == 1 && HEIGHT == 12)
    #define BOARD_MASK 0xfff
    #define BOTTOM_MASK 0x1
    #define STATIC_MOVE_ORDER {0}
#endif
#if (WIDTH == 2 && HEIGHT == 1)
    #define BOARD_MASK 0x5
    #define BOTTOM_MASK 0x5
    #define STATIC_MOVE_ORDER {1, 0}
#endif
#if (WIDTH == 2 && HEIGHT == 2)
    #define BOARD_MASK 0x1b
    #define BOTTOM_MASK 0x9
    #define STATIC_MOVE_ORDER {1, 0}
#endif
#if (WIDTH == 2 && HEIGHT == 3)
    #define BOARD_MASK 0x77
    #define BOTTOM_MASK 0x11
    #define STATIC_MOVE_ORDER {1, 0}
#endif
#if (WIDTH == 2 && HEIGHT == 4)
    #define BOARD_MASK 0x1ef
    #define BOTTOM_MASK 0x21
    #define STATIC_MOVE_ORDER {1, 0}
#endif
#if (WIDTH == 2 && HEIGHT == 5)
    #define BOARD_MASK 0x7df
    #define BOTTOM_MASK 0x41
    #define STATIC_MOVE_ORDER {1, 0}
#endif
#if (WIDTH == 2 && HEIGHT == 6)
    #define BOARD_MASK 0x1fbf
    #define BOTTOM_MASK 0x81
    #define STATIC_MOVE_ORDER {1, 0}
#endif
#if (WIDTH == 2 && HEIGHT == 7)
    #define BOARD_MASK 0x7f7f
    #define BOTTOM_MASK 0x101
    #define STATIC_MOVE_ORDER {1, 0}
#endif
#if (WIDTH == 2 && HEIGHT == 8)
    #define BOARD_MASK 0x1feff
    #define BOTTOM_MASK 0x201
    #define STATIC_MOVE_ORDER {1, 0}
#endif
#if (WIDTH == 2 && HEIGHT == 9)
    #define BOARD_MASK 0x7fdff
    #define BOTTOM_MASK 0x401
    #define STATIC_MOVE_ORDER {1, 0}
#endif
#if (WIDTH == 2 && HEIGHT == 10)
    #define BOARD_MASK 0x1ffbff
    #define BOTTOM_MASK 0x801
    #define STATIC_MOVE_ORDER {1, 0}
#endif
#if (WIDTH == 2 && HEIGHT == 11)
    #define BOARD_MASK 0x7ff7ff
    #define BOTTOM_MASK 0x1001
    #define STATIC_MOVE_ORDER {1, 0}
#endif
#if (WIDTH == 3 && HEIGHT == 1)
    #define BOARD_MASK 0x15
    #define BOTTOM_MASK 0x15
    #define STATIC_MOVE_ORDER {1, 0, 2}
#endif
#if (WIDTH == 3 && HEIGHT == 2)
    #define BOARD_MASK 0xdb
    #define BOTTOM_MASK 0x49
    #define STATIC_MOVE_ORDER {1, 0, 2}
#endif
#if (WIDTH == 3 && HEIGHT == 3)
    #define BOARD_MASK 0x777
    #define BOTTOM_MASK 0x111
    #define STATIC_MOVE_ORDER {1, 0, 2}
#endif
#if (WIDTH == 3 && HEIGHT == 4)
    #define BOARD_MASK 0x3def
    #define BOTTOM_MASK 0x421
    #define STATIC_MOVE_ORDER {1, 0, 2}
#endif
#if (WIDTH == 3 && HEIGHT == 5)
    #define BOARD_MASK 0x1f7df
    #define BOTTOM_MASK 0x1041
    #define STATIC_MOVE_ORDER {1, 0, 2}
#endif
#if (WIDTH == 3 && HEIGHT == 6)
    #define BOARD_MASK 0xfdfbf
    #define BOTTOM_MASK 0x4081
    #define STATIC_MOVE_ORDER {1, 0, 2}
#endif
#if (WIDTH == 3 && HEIGHT == 7)
    #define BOARD_MASK 0x7f7f7f
    #define BOTTOM_MASK 0x10101
    #define STATIC_MOVE_ORDER {1, 0, 2}
#endif
#if (WIDTH == 3 && HEIGHT == 8)
    #define BOARD_MASK 0x3fdfeff
    #define BOTTOM_MASK 0x40201
    #define STATIC_MOVE_ORDER {1, 0, 2}
#endif
#if (WIDTH == 3 && HEIGHT == 9)
    #define BOARD_MASK 0x1ff7fdff
    #define BOTTOM_MASK 0x100401
    #define STATIC_MOVE_ORDER {1, 0, 2}
#endif
#if (WIDTH == 3 && HEIGHT == 10)
    #define BOARD_MASK 0xffdffbff
    #define BOTTOM_MASK 0x400801
    #define STATIC_MOVE_ORDER {1, 0, 2}
#endif
#if (WIDTH == 4 && HEIGHT == 1)
    #define BOARD_MASK 0x55
    #define BOTTOM_MASK 0x55
    #define STATIC_MOVE_ORDER {2, 1, 3, 0}
#endif
#if (WIDTH == 4 && HEIGHT == 2)
    #define BOARD_MASK 0x6db
    #define BOTTOM_MASK 0x249
    #define STATIC_MOVE_ORDER {2, 1, 3, 0}
#endif
#if (WIDTH == 4 && HEIGHT == 3)
    #define BOARD_MASK 0x7777
    #define BOTTOM_MASK 0x1111
    #define STATIC_MOVE_ORDER {2, 1, 3, 0}
#endif
#if (WIDTH == 4 && HEIGHT == 4)
    #define BOARD_MASK 0x7bdef
    #define BOTTOM_MASK 0x8421
    #define STATIC_MOVE_ORDER {2, 1, 3, 0}
#endif
#if (WIDTH == 4 && HEIGHT == 5)
    #define BOARD_MASK 0x7df7df
    #define BOTTOM_MASK 0x41041
    #define STATIC_MOVE_ORDER {2, 1, 3, 0}
#endif
#if (WIDTH == 4 && HEIGHT == 6)
    #define BOARD_MASK 0x7efdfbf
    #define BOTTOM_MASK 0x204081
    #define STATIC_MOVE_ORDER {2, 1, 3, 0}
#endif
#if (WIDTH == 4 && HEIGHT == 7)
    #define BOARD_MASK 0x7f7f7f7f
    #define BOTTOM_MASK 0x1010101
    #define STATIC_MOVE_ORDER {2, 1, 3, 0}
#endif
#if (WIDTH == 4 && HEIGHT == 8)
    #define BOARD_MASK 0x7fbfdfeff
    #define BOTTOM_MASK 0x8040201
    #define STATIC_MOVE_ORDER {2, 1, 3, 0}
#endif
#if (WIDTH == 4 && HEIGHT == 9)
    #define BOARD_MASK 0x7fdff7fdff
    #define BOTTOM_MASK 0x40100401
    #define STATIC_MOVE_ORDER {2, 1, 3, 0}
#endif
#if (WIDTH == 5 && HEIGHT == 1)
    #define BOARD_MASK 0x155
    #define BOTTOM_MASK 0x155
    #define STATIC_MOVE_ORDER {2, 1, 3, 0, 4}
#endif
#if (WIDTH == 5 && HEIGHT == 2)
    #define BOARD_MASK 0x36db
    #define BOTTOM_MASK 0x1249
    #define STATIC_MOVE_ORDER {2, 1, 3, 0, 4}
#endif
#if (WIDTH == 5 && HEIGHT == 3)
    #define BOARD_MASK 0x77777
    #define BOTTOM_MASK 0x11111
    #define STATIC_MOVE_ORDER {2, 1, 3, 0, 4}
#endif
#if (WIDTH == 5 && HEIGHT == 4)
    #define BOARD_MASK 0xf7bdef
    #define BOTTOM_MASK 0x108421
    #define STATIC_MOVE_ORDER {2, 1, 3, 0, 4}
#endif
#if (WIDTH == 5 && HEIGHT == 5)
    #define BOARD_MASK 0x1f7df7df
    #define BOTTOM_MASK 0x1041041
    #define STATIC_MOVE_ORDER {2, 1, 3, 0, 4}
#endif
#if (WIDTH == 5 && HEIGHT == 6)
    #define BOARD_MASK 0x3f7efdfbf
    #define BOTTOM_MASK 0x10204081
    #define STATIC_MOVE_ORDER {2, 1, 3, 0, 4}
#endif
#if (WIDTH == 5 && HEIGHT == 7)
    #define BOARD_MASK 0x7f7f7f7f7f
    #define BOTTOM_MASK 0x101010101
    #define STATIC_MOVE_ORDER {2, 1, 3, 0, 4}
#endif
#if (WIDTH == 5 && HEIGHT == 8)
    #define BOARD_MASK 0xff7fbfdfeff
    #define BOTTOM_MASK 0x1008040201
    #define STATIC_MOVE_ORDER {2, 1, 3, 0, 4}
#endif
#if (WIDTH == 6 && HEIGHT == 1)
    #define BOARD_MASK 0x555
    #define BOTTOM_MASK 0x555
    #define STATIC_MOVE_ORDER {3, 2, 4, 1, 5, 0}
#endif
#if (WIDTH == 6 && HEIGHT == 2)
    #define BOARD_MASK 0x1b6db
    #define BOTTOM_MASK 0x9249
    #define STATIC_MOVE_ORDER {3, 2, 4, 1, 5, 0}
#endif
#if (WIDTH == 6 && HEIGHT == 3)
    #define BOARD_MASK 0x777777
    #define BOTTOM_MASK 0x111111
    #define STATIC_MOVE_ORDER {3, 2, 4, 1, 5, 0}
#endif
#if (WIDTH == 6 && HEIGHT == 4)
    #define BOARD_MASK 0x1ef7bdef
    #define BOTTOM_MASK 0x2108421
    #define STATIC_MOVE_ORDER {3, 2, 4, 1, 5, 0}
#endif
#if (WIDTH == 6 && HEIGHT == 5)
    #define BOARD_MASK 0x7df7df7df
    #define BOTTOM_MASK 0x41041041
    #define STATIC_MOVE_ORDER {3, 2, 4, 1, 5, 0}
#endif
#if (WIDTH == 6 && HEIGHT == 6)
    #define BOARD_MASK 0x1fbf7efdfbf
    #define BOTTOM_MASK 0x810204081
    #define STATIC_MOVE_ORDER {3, 2, 4, 1, 5, 0}
#endif
#if (WIDTH == 6 && HEIGHT == 7)
    #define BOARD_MASK 0x7f7f7f7f7f7f
    #define BOTTOM_MASK 0x10101010101
    #define STATIC_MOVE_ORDER {3, 2, 4, 1, 5, 0}
#endif
#if (WIDTH == 7 && HEIGHT == 1)
    #define BOARD_MASK 0x1555
    #define BOTTOM_MASK 0x1555
    #define STATIC_MOVE_ORDER {3, 2, 4, 1, 5, 0, 6}
#endif
#if (WIDTH == 7 && HEIGHT == 2)
    #define BOARD_MASK 0xdb6db
    #define BOTTOM_MASK 0x49249
    #define STATIC_MOVE_ORDER {3, 2, 4, 1, 5, 0, 6}
#endif
#if (WIDTH == 7 && HEIGHT == 3)
    #define BOARD_MASK 0x7777777
    #define BOTTOM_MASK 0x1111111
    #define STATIC_MOVE_ORDER {3, 2, 4, 1, 5, 0, 6}
#endif
#if (WIDTH == 7 && HEIGHT == 4)
    #define BOARD_MASK 0x3def7bdef
    #define BOTTOM_MASK 0x42108421
    #define STATIC_MOVE_ORDER {3, 2, 4, 1, 5, 0, 6}
#endif
#if (WIDTH == 7 && HEIGHT == 5)
    #define BOARD_MASK 0x1f7df7df7df
    #define BOTTOM_MASK 0x1041041041
    #define STATIC_MOVE_ORDER {3, 2, 4, 1, 5, 0, 6}
#endif
#if (WIDTH == 7 && HEIGHT == 6)
    #define BOARD_MASK 0xfdfbf7efdfbf
    #define BOTTOM_MASK 0x40810204081
    #define STATIC_MOVE_ORDER {3, 2, 4, 1, 5, 0, 6}
#endif
#if (WIDTH == 8 && HEIGHT == 1)
    #define BOARD_MASK 0x5555
    #define BOTTOM_MASK 0x5555
    #define STATIC_MOVE_ORDER {4, 3, 5, 2, 6, 1, 7, 0}
#endif
#if (WIDTH == 8 && HEIGHT == 2)
    #define BOARD_MASK 0x6db6db
    #define BOTTOM_MASK 0x249249
    #define STATIC_MOVE_ORDER {4, 3, 5, 2, 6, 1, 7, 0}
#endif
#if (WIDTH == 8 && HEIGHT == 3)
    #define BOARD_MASK 0x77777777
    #define BOTTOM_MASK 0x11111111
    #define STATIC_MOVE_ORDER {4, 3, 5, 2, 6, 1, 7, 0}
#endif
#if (WIDTH == 8 && HEIGHT == 4)
    #define BOARD_MASK 0x7bdef7bdef
    #define BOTTOM_MASK 0x842108421
    #define STATIC_MOVE_ORDER {4, 3, 5, 2, 6, 1, 7, 0}
#endif
#if (WIDTH == 8 && HEIGHT == 5)
    #define BOARD_MASK 0x7df7df7df7df
    #define BOTTOM_MASK 0x41041041041
    #define STATIC_MOVE_ORDER {4, 3, 5, 2, 6, 1, 7, 0}
#endif
#if (WIDTH == 9 && HEIGHT == 1)
    #define BOARD_MASK 0x15555
    #define BOTTOM_MASK 0x15555
    #define STATIC_MOVE_ORDER {4, 3, 5, 2, 6, 1, 7, 0, 8}
#endif
#if (WIDTH == 9 && HEIGHT == 2)
    #define BOARD_MASK 0x36db6db
    #define BOTTOM_MASK 0x1249249
    #define STATIC_MOVE_ORDER {4, 3, 5, 2, 6, 1, 7, 0, 8}
#endif
#if (WIDTH == 9 && HEIGHT == 3)
    #define BOARD_MASK 0x777777777
    #define BOTTOM_MASK 0x111111111
    #define STATIC_MOVE_ORDER {4, 3, 5, 2, 6, 1, 7, 0, 8}
#endif
#if (WIDTH == 9 && HEIGHT == 4)
    #define BOARD_MASK 0xf7bdef7bdef
    #define BOTTOM_MASK 0x10842108421
    #define STATIC_MOVE_ORDER {4, 3, 5, 2, 6, 1, 7, 0, 8}
#endif
#if (WIDTH == 10 && HEIGHT == 1)
    #define BOARD_MASK 0x55555
    #define BOTTOM_MASK 0x55555
    #define STATIC_MOVE_ORDER {5, 4, 6, 3, 7, 2, 8, 1, 9, 0}
#endif
#if (WIDTH == 10 && HEIGHT == 2)
    #define BOARD_MASK 0x1b6db6db
    #define BOTTOM_MASK 0x9249249
    #define STATIC_MOVE_ORDER {5, 4, 6, 3, 7, 2, 8, 1, 9, 0}
#endif
#if (WIDTH == 10 && HEIGHT == 3)
    #define BOARD_MASK 0x7777777777
    #define BOTTOM_MASK 0x1111111111
    #define STATIC_MOVE_ORDER {5, 4, 6, 3, 7, 2, 8, 1, 9, 0}
#endif
#if (WIDTH == 11 && HEIGHT == 1)
    #define BOARD_MASK 0x155555
    #define BOTTOM_MASK 0x155555
    #define STATIC_MOVE_ORDER {5, 4, 6, 3, 7, 2, 8, 1, 9, 0, 10}
#endif
#if (WIDTH == 11 && HEIGHT == 2)
    #define BOARD_MASK 0xdb6db6db
    #define BOTTOM_MASK 0x49249249
    #define STATIC_MOVE_ORDER {5, 4, 6, 3, 7, 2, 8, 1, 9, 0, 10}
#endif
#if (WIDTH == 12 && HEIGHT == 1)
    #define BOARD_MASK 0x555555
    #define BOTTOM_MASK 0x555555
    #define STATIC_MOVE_ORDER {6, 5, 7, 4, 8, 3, 9, 2, 10, 1, 11, 0}
#endif
