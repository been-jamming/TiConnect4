#include <tigcclib.h>
#include "extgraph.h"
#include "connect_4.h"

extern void *light_buffer;
extern void *dark_buffer;

unsigned short int grid[14] = {
	0b1111111111111100,
	0b1111100001111100,
	0b1110000000011100,
	0b1100000000001100,
	0b1100000000001100,
	0b1000000000000100,
	0b1000000000000100,
	0b1000000000000100,
	0b1000000000000100,
	0b1100000000001100,
	0b1100000000001100,
	0b1110000000011100,
	0b1111100001111100,
	0b1111111111111100
};

unsigned short int white_piece[14] = {
	0b0000000000000000,
	0b0000011110000000,
	0b0001111111100000,
	0b0011100001110000,
	0b0011000000110000,
	0b0111000000111000,
	0b0110001100011000,
	0b0110001100011000,
	0b0111000000111000,
	0b0011000000110000,
	0b0011100001110000,
	0b0001111111100000,
	0b0000011110000000,
	0b0000000000000000
};

unsigned short int black_piece[14] = {
	0b0000000000000000,
	0b0000011110000000,
	0b0001111111100000,
	0b0011111111110000,
	0b0011111111110000,
	0b0111111111111000,
	0b0111110011111000,
	0b0111110011111000,
	0b0111111111111000,
	0b0011111111110000,
	0b0011111111110000,
	0b0001111111100000,
	0b0000011110000000,
	0b0000000000000000
};

unsigned short int piece_sprt_mask[14] = {
	0b0000000000000000,
	0b0000011110000000,
	0b0001111111100000,
	0b0011111111110000,
	0b0011111111110000,
	0b0111111111111000,
	0b0111111111111000,
	0b0111111111111000,
	0b0111111111111000,
	0b0011111111110000,
	0b0011111111110000,
	0b0001111111100000,
	0b0000011110000000,
	0b0000000000000000
};

unsigned short int player_selection_arrow[6] = {
	0b0000011110000000,
	0b0000011110000000,
	0b0000011110000000,
	0b0011111111110000,
	0b0000111111000000,
	0b0000001100000000,
};

unsigned short int checkmark[5] = {
	0b0000000001000000,
	0b0000000011000000,
	0b0000000110000000,
	0b0000111100000000,
	0b0000001000000000
};

void draw_selection_live(int col){
	Sprite16(col*14 + 31, 10, 6, player_selection_arrow, GrayGetPlane(LIGHT_PLANE), SPRT_XOR);
	Sprite16(col*14 + 31, 10, 6, player_selection_arrow, GrayGetPlane(DARK_PLANE), SPRT_XOR);
}

void draw_checkmark_live(int col){
	Sprite16(col*14 + 31, 10, 5, checkmark, GrayGetPlane(LIGHT_PLANE), SPRT_XOR);
	Sprite16(col*14 + 31, 10, 5, checkmark, GrayGetPlane(DARK_PLANE), SPRT_XOR);
}

void draw_selection(int col){
	Sprite16(col*14 + 31, 10, 6, player_selection_arrow, light_buffer, SPRT_XOR);
	Sprite16(col*14 + 31, 10, 6, player_selection_arrow, dark_buffer, SPRT_XOR);
}

void draw_checkmark(int col){
	Sprite16(col*14 + 31, 10, 5, checkmark, light_buffer, SPRT_XOR);
	Sprite16(col*14 + 31, 10, 5, checkmark, dark_buffer, SPRT_XOR);
}

void render_board(board *b){
	unsigned int x;
	unsigned int y;

	for(x = 0; x < 7; x++){
		for(y = 0; y < 6; y++){
			Sprite16(x*14 + 31, y*14 + 16, 14, piece_sprt_mask, light_buffer, SPRT_AND);
			Sprite16(x*14 + 31, y*14 + 16, 14, grid, dark_buffer, SPRT_OR);
			if(b->placed_mask&(1ULL<<(x*7 + 5 - y))){
				if(b->state&(1ULL<<(x*7 + 5 - y))){
					Sprite16(x*14 + 31, y*14 + 16, 14, black_piece, light_buffer, SPRT_OR);
					Sprite16(x*14 + 31, y*14 + 16, 14, black_piece, dark_buffer, SPRT_OR);
				} else {
					Sprite16(x*14 + 31, y*14 + 16, 14, white_piece, light_buffer, SPRT_OR);
					Sprite16(x*14 + 31, y*14 + 16, 14, white_piece, dark_buffer, SPRT_OR);
				}
			}
		}
	}
}
