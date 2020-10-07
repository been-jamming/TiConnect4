#ifndef GRAPHICS_INCLUDED
#define GRAPHICS_INCLUDED
#include "connect_4.h"

void draw_selection_live(int col);
void draw_checkmark_live(int col);
void draw_selection(int col);
void draw_checkmark(int col);
void render_board(board *b);
extern unsigned short int grid[14];
extern unsigned short int white_piece[14];
extern unsigned short int black_piece[14];
extern unsigned short int piece_sprt_mask[14];
#endif
