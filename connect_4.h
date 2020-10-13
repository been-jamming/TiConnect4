#ifndef CONNECT4_INCLUDED
#define CONNECT4_INCLUDED
#include <stdint.h>

#define HUMAN 0
#define COMPUTER 1

typedef struct board board;

struct board{
	uint64_t state;
	uint64_t placed_mask;
	unsigned char col_heights[7];
};

typedef struct hash_entry hash_entry;

struct hash_entry{
	board hash_board;
	int lower_bound;
	int upper_bound;
	int depth;
};

typedef struct variation variation;

struct variation{
	int moves[42];
	int move_count;
};

typedef struct game_settings game_settings;

struct game_settings{
	board game_board;
	int white_player;
	int black_player;
	int show_evaluation;
	int show_pv;
	int computer_wait;
	unsigned char current_turn;
};
#endif
