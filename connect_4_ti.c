#define SAVE_SCREEN
#define USE_TI89
#include <tigcclib.h>
#include "extgraph.h"
#include "graphics.h"
#include "menu.h"
#include "connect_4.h"

#define WHITE 0
#define BLACK 1

#define INFINITY 99

#define NO_SCORE 255

#define HASH_TABLE_SIZE (1024)

hash_entry *hash_table;

uint64_t num_nodes;
const uint64_t pieces_mask = 0b00000000111111011111101111110111111011111101111110111111ULL;

int principal_variations[44][44];
int principal_variation[42];

unsigned char col_orders[7] = {3, 4, 2, 1, 5, 0, 6};
unsigned char col_priorities[7] = {1, 3, 5, 7, 4, 2, 0};

void *light_buffer;
void *dark_buffer;

volatile unsigned char stop_engine;
volatile unsigned char move_made;
volatile unsigned char update_scr;
volatile unsigned char enter_menu;
unsigned char animating = 0;
volatile int animation_y;
volatile int animation_end_y;
volatile int animation_col;
volatile int selection_col;
volatile int computer_col;
unsigned int draw_check = 1;
volatile unsigned char current_turn;
char message_str[28] = {0};

INT_HANDLER old_int_5 = NULL;
void *kbq;
board global_board;
game_settings global_game_settings;

int computer_timer;
int demo_board_moves[] = {3, 3, 3, 3, 3, 2, 4, 4, 4, 3};

char *main_menu_items[] = {
	"New Board",
	"Load Board",
	"Exit"
};

char *options_menu_items[] = {
	"Resume",
	"Game Settings",
	"Save Board",
	"Exit"
};

void print_bin64(uint64_t n){
	uint64_t bit;

	bit = 1ULL<<63;
	while(bit){
		if(bit&n){
			printf("1");
		} else {
			printf("0");
		}
		bit >>= 1;
	}
}

int count_bits(uint64_t n){
	int count = 0;

	while(n){
		n &= n - 1;
		count++;
	}

	return count;
}

uint64_t mix64(uint64_t x){
	x = (x^(x>>30))*0xbf58476d1ce4e5b9ULL;
	x = (x^(x>>27))*0x94d049bb133111ebULL;
	x = x^(x>>31);

	return x;
}

void reset_hash_table(){
	int i;

	for(i = 0; i < HASH_TABLE_SIZE; i++){
		hash_table[i].depth = -1;
	}
}

void initialize_board(board *b){
	int i;

	b->state = 0;
	b->placed_mask = 0;
	for(i = 0; i < 7; i++){
		b->col_heights[i] = 0;
	}

	hash_table = malloc(sizeof(hash_entry)*HASH_TABLE_SIZE);
	if(!hash_table){
		printf("Out of memory!\n");
		ngetchx();
		exit(1);
	}
	for(i = 0; i < HASH_TABLE_SIZE; i++){
		hash_table[i].depth = -1;
	}
	light_buffer = malloc(LCD_SIZE);
	if(!light_buffer){
		free(hash_table);
		printf("Out of memory!\n");
		ngetchx();
		exit(1);
	}
	dark_buffer = malloc(LCD_SIZE);
	if(!dark_buffer){
		free(light_buffer);
		free(hash_table);
		printf("Out of memory!\n");
		ngetchx();
		exit(1);
	}
}

void place_piece(board *b, int x, int color){
	int height;

	b->state |= ((uint64_t) color)<<(x*7 + b->col_heights[x]);
	b->placed_mask |= 1ULL<<(x*7 + b->col_heights[x]);
	b->col_heights[x]++;
}

void remove_piece(board *b, int x){
	if(!b->col_heights[x])
		return;

	b->col_heights[x]--;
	b->state &= ~(1ULL<<(x*7 + b->col_heights[x]));
	b->placed_mask &= ~(1ULL<<(x*7 + b->col_heights[x]));
}

int get_draw(board *b){
	int i;

	for(i = 0; i < 7; i++){
		if(b->col_heights[i] < 6)
			return 0;
	}

	return 1;
}

//Returns -1 if nobody has won, otherwise returns the color of the player that won
int get_win(board *b){
	uint64_t current_mask;
	uint64_t first_mask;

	//First we determine if there is a vertical win for black
	current_mask = b->state;

	if(current_mask&(current_mask<<1)&(current_mask<<2)&(current_mask<<3)){
		return BLACK;
	}

	//Next we determine if there is a vertical win for white
	current_mask = ~b->state&b->placed_mask;

	if(current_mask&(current_mask<<1)&(current_mask<<2)&(current_mask<<3)){
		return WHITE;
	}

	//Now for horizontal wins. First for black
	current_mask = b->state;

	if(current_mask&(current_mask<<7)&(current_mask<<14)&(current_mask<<21)){
		return BLACK;
	}

	//Horizontal for white
	current_mask = ~b->state&b->placed_mask;

	if(current_mask&(current_mask<<7)&(current_mask<<14)&(current_mask<<21)){
		return WHITE;
	}

	//Now for positive diagonal, black!
	current_mask = b->state;

	if(current_mask&(current_mask<<8)&(current_mask<<16)&(current_mask<<24)){
		return BLACK;
	}

	//Positive diagonal, white
	current_mask = ~b->state&b->placed_mask;

	if(current_mask&(current_mask<<8)&(current_mask<<16)&(current_mask<<24)){
		return WHITE;
	}

	//Negative diagonal, black
	current_mask = b->state;

	if(current_mask&(current_mask<<6)&(current_mask<<12)&(current_mask<<18)){
		return BLACK;
	}

	//Positive_diagonal, white
	current_mask = ~b->state&b->placed_mask;

	if(current_mask&(current_mask<<6)&(current_mask<<12)&(current_mask<<18)){
		return WHITE;
	}

	//Nobody has won
	return -1;
}

void change_score_traps(int *current_score, uint64_t white_traps, uint64_t black_traps){
	if(black_traps&(black_traps>>1)){
		--*current_score;
		if(black_traps&(black_traps>>1)&(white_traps - 1)){
			*current_score -= 1;
		}
	}

	if(white_traps&(white_traps>>1)){
		++*current_score;
		if(white_traps&(white_traps>>1)&(black_traps - 1)){
			*current_score += 1;
		}
	}
}

int get_score(board *b, int current_color){
	uint64_t current_mask_white;
	uint64_t current_mask_black;
	uint64_t current_traps;
	uint64_t white_traps;
	uint64_t black_traps;
	uint64_t empty_mask;
	int current_score;
	uint64_t column_mask = 0x3F;
	int i;

	current_score = get_win(b);
	if(current_score == WHITE){
		if(current_color == WHITE){
			return 10;
		} else {
			return -10;
		}
	} else if(current_score == BLACK){
		if(current_color == BLACK){
			return 10;
		} else {
			return -10;
		}
	}
	current_score = 0;
	
	//Black vertical threat (there can only be 1)
	current_mask_black = b->state;
	empty_mask = ~b->placed_mask&pieces_mask;
	if(empty_mask&(current_mask_black<<1)&(current_mask_black<<2)&(current_mask_black<<3)){
		current_score = -1;
	}

	//White vertical threat (there can only be 1)
	current_mask_white = ~b->state&b->placed_mask;
	if(empty_mask&(current_mask_white<<1)&(current_mask_white<<2)&(current_mask_white<<3)){
		current_score++;
	}

	for(i = 0; i < 7; i++){
		current_mask_black = (column_mask&empty_mask) | b->state;
		current_mask_white = (column_mask&empty_mask) | (~b->state&b->placed_mask);
		black_traps = 0;
		white_traps = 0;

		//Horizontal for black
		current_traps = current_mask_black&(current_mask_black<<7)&(current_mask_black<<14)&(current_mask_black<<21);
		if(current_traps){
			current_score--;
			black_traps |= ((current_traps>>21)|(current_traps>>14)|(current_traps>>7)|current_traps)&column_mask;
		}

		//Horizontal for white
		current_traps = current_mask_white&(current_mask_white<<7)&(current_mask_white<<14)&(current_mask_white<<21);
		if(current_traps){
			current_score++;
			white_traps |= ((current_traps>>21)|(current_traps>>14)|(current_traps>>7)|current_traps)&column_mask;
		}

		//Now for positive diagonal, black!
		current_traps = current_mask_black&(current_mask_black<<8)&(current_mask_black<<16)&(current_mask_black<<24);
		if(current_traps){
			current_score--;
			black_traps |= ((current_traps>>24)|(current_traps>>16)|(current_traps>>8)|current_traps)&column_mask;
		}

		//Positive diagonal, white
		current_traps = current_mask_white&(current_mask_white<<8)&(current_mask_white<<16)&(current_mask_white<<24);
		if(current_traps){
			current_score++;
			white_traps |= ((current_traps>>24)|(current_traps>>16)|(current_traps>>8)|current_traps)&column_mask;
		}

		//Negative diagonal, black
		current_traps = current_mask_black&(current_mask_black<<6)&(current_mask_black<<12)&(current_mask_black<<18);
		if(current_traps){
			current_score--;
			black_traps |= ((current_traps>>18)|(current_traps>>12)|(current_traps>>6)|current_traps)&column_mask;
		}

		//Positive_diagonal, white
		current_traps = current_mask_white&(current_mask_white<<6)&(current_mask_white<<12)&(current_mask_white<<18);
		if(current_traps){
			current_score++;
			white_traps |= ((current_traps>>18)|(current_traps>>12)|(current_traps>>6)|current_traps)&column_mask;
		}

		change_score_traps(&current_score, white_traps, black_traps);

		column_mask <<= 7;
	}

	if(current_color == WHITE){
		return current_score;
	} else {
		return -current_score;
	}
}

uint64_t get_hash_index(board *b){
	return mix64(b->state^(b->placed_mask<<11))%HASH_TABLE_SIZE;
}

int negamax(board b, int color, int orig_alpha, int orig_beta, int depth, int *old_pv, int old_pv_length, variation *pline){
	variation line;
	board saved_board;
	hash_entry entry;
	uint64_t hash_index;
	int score;
	int best_score;
	int i;
	int move;
	int alpha;
	int beta;
	int is_draw = 1;

	hash_index = get_hash_index(&b);
	entry = hash_table[hash_index];
	if(entry.hash_board.state == b.state && entry.hash_board.placed_mask == b.placed_mask && entry.depth >= depth){
		if(entry.lower_bound >= orig_beta){
			return entry.lower_bound;
		}
		if(entry.upper_bound <= orig_alpha){
			return entry.upper_bound;
		}
		if(entry.lower_bound > orig_alpha){
			orig_alpha = entry.lower_bound;
		}
		if(entry.upper_bound < orig_beta){
			orig_beta = entry.upper_bound;
		}
		if(entry.lower_bound == entry.upper_bound)
			return entry.lower_bound;
	}
	if(!depth || stop_engine){
		pline->move_count = 0;
		return get_score(&b, color);
	}

	alpha = orig_alpha;
	beta = orig_beta;
	line.move_count = 0;
	saved_board = b;
	best_score = -INFINITY;
	for(i = 0; i < 8; i++){
		if(i == 0){
			if(old_pv_length){
				move = old_pv[0];
			} else {
				continue;
			}
		} else {
			move = col_orders[i - 1];
			if((old_pv_length && old_pv[0] == move)){
				continue;
			}
		}
		if(b.col_heights[move] < 6){
			is_draw = 0;
			place_piece(&b, move, color);
			if(get_win(&b) == color){
				score = INFINITY;
			} else {
				if(old_pv_length > 1 && i == 0){
					score = -negamax(b, !color, -beta, -alpha, depth - 1, old_pv + 1, old_pv_length - 1, &line);
				} else {
					score = -negamax(b, !color, -beta, -alpha, depth - 1, NULL, 0, &line);
				}
			}
			if(score > best_score){
				best_score = score;
				if(best_score > alpha){
					alpha = score;
					pline->moves[0] = move;
					if(line.move_count)
						memcpy(pline->moves + 1, line.moves, line.move_count*sizeof(int));
					pline->move_count = line.move_count + 1;
				}
				if(best_score >= beta){
					break;
				}
			}
		}
		b = saved_board;
	}

	if(best_score <= orig_alpha){
		entry.hash_board = saved_board;
		entry.upper_bound = best_score;
		entry.lower_bound = -INFINITY;
		entry.depth = depth;
		hash_index = get_hash_index(&saved_board);
		hash_table[hash_index] = entry;
	} else if(best_score > orig_alpha && best_score < orig_beta){
		entry.hash_board = saved_board;
		entry.upper_bound = best_score;
		entry.lower_bound = best_score;
		entry.depth = depth;
		hash_index = get_hash_index(&saved_board);
		hash_table[hash_index] = entry;
	} else if(best_score >= orig_beta){
		entry.hash_board = saved_board;
		entry.upper_bound = INFINITY;
		entry.lower_bound = best_score;
		entry.depth = depth;
		hash_index = get_hash_index(&saved_board);
		hash_table[hash_index] = entry;
	}

	if(is_draw){
		return 0;
	} else {
		return best_score;
	}
}

void update_msg(char *message){
	GraySetAMSPlane(LIGHT_PLANE);
	FastFillRect(GrayGetPlane(LIGHT_PLANE), 0, 0, 159, 7, A_NORMAL);
	DrawStr(0, 0, message, A_REVERSE);
	GraySetAMSPlane(DARK_PLANE);
	FastFillRect(GrayGetPlane(DARK_PLANE), 0, 0, 159, 7, A_NORMAL);
	DrawStr(0, 0, message, A_REVERSE);
}

void render_screen(){
	unsigned int i;

	ClearGrayScreen2B(light_buffer, dark_buffer);
	if(animating && current_turn == WHITE){
		Sprite16(animation_col*14 + 31, animation_y, 14, white_piece, light_buffer, SPRT_OR);
		Sprite16(animation_col*14 + 31, animation_y, 14, white_piece, dark_buffer, SPRT_OR);
	} else if(animating && current_turn == BLACK){
		Sprite16(animation_col*14 + 31, animation_y, 14, black_piece, light_buffer, SPRT_OR);
		Sprite16(animation_col*14 + 31, animation_y, 14, black_piece, dark_buffer, SPRT_OR);
	}
	render_board(&global_board);
	draw_selection(selection_col);
	if(global_game_settings.show_pv){
		draw_checkmark(computer_col);
	}
	PortSet(light_buffer,  239, 127);
	FastFillRect(light_buffer, 0, 0, 159, 7, A_NORMAL);
	DrawStr(0, 0, message_str, A_REVERSE);
	PortSet(dark_buffer,  239, 127);
	FastFillRect(dark_buffer, 0, 0, 159, 7, A_NORMAL);
	DrawStr(0, 0, message_str, A_REVERSE);
	memcpy(GrayGetPlane(LIGHT_PLANE), light_buffer, LCD_SIZE);
	memcpy(GrayGetPlane(DARK_PLANE), dark_buffer, LCD_SIZE);
}

int get_player(int color){
	if(color == WHITE){
		return global_game_settings.white_player;
	} else {
		return global_game_settings.black_player;
	}
}

DEFINE_INT_HANDLER (time_update){
	unsigned int key;
	while(!OSdequeue(&key, kbq)){
		if(key == KEY_ESC){
			stop_engine = 1;
			enter_menu = 1;
		} else if(key == KEY_LEFT){
			draw_selection_live(selection_col);
			if(global_game_settings.show_pv){
				draw_checkmark_live(computer_col);
			}
			selection_col -= 1;
			if(selection_col < 0)
				selection_col = 6;
			draw_selection_live(selection_col);
			if(global_game_settings.show_pv){
				draw_checkmark_live(computer_col);
			}
		} else if(key == KEY_RIGHT){
			draw_selection_live(selection_col);
			if(global_game_settings.show_pv){
				draw_checkmark_live(computer_col);
			}
			selection_col = (selection_col + 1)%7;
			draw_selection_live(selection_col);
			if(global_game_settings.show_pv){
				draw_checkmark_live(computer_col);
			}
		} else if(key == KEY_ENTER && !animating && global_board.col_heights[selection_col] < 6 && get_player(current_turn) == HUMAN){
			animating = 1;
			animation_y = 3;
			animation_end_y = 16 + 14*(5 - global_board.col_heights[selection_col]);
			animation_col = selection_col;
			stop_engine = 1;
		}
	}

	if(animating){
		if(animation_y != animation_end_y && animation_y + 6 > animation_end_y){
			animation_y = animation_end_y;
			update_scr = 1;
		} else if(animation_y < animation_end_y){
			animation_y += 6;
			update_scr = 1;
		}
	} else {
		if(get_player(current_turn) == COMPUTER){
			if(computer_timer > 0){
				computer_timer--;
			} else {
				animating = 1;
				animation_y = 3;
				animation_end_y = 16 + 14*(5 - global_board.col_heights[computer_col]);
				animation_col = computer_col;
				stop_engine = 1;
			}
		}
	}
	ExecuteHandler(old_int_5);
}

int update_computer(){
	int d;
	int score;
	int moves[42];
	variation pv;
	int pv_length;

	pv_length = 0;
	reset_hash_table();
	for(d = 0; d < 42; d++){
		score = negamax(global_board, current_turn, -INFINITY, INFINITY, d, moves, pv_length, &pv);
		if(stop_engine){
			return;
		}
		pv_length = pv.move_count;
		if(global_game_settings.show_pv){
			draw_checkmark_live(computer_col);
		}
		if(pv_length){
			computer_col = pv.moves[0];
		}
		if(global_game_settings.show_pv){
			draw_checkmark_live(computer_col);
		}
		if(global_game_settings.show_evaluation && global_game_settings.show_pv){
			memset(message_str, 0, sizeof(message_str));
			if(pv_length == 0){
				snprintf(message_str, 28, "%d d: %d", score, d);
			} else if(pv_length == 1){
				snprintf(message_str, 28, "%d d: %d PV: %d", score, d, pv.moves[0]);
			} else if(pv_length == 2){
				snprintf(message_str, 28, "%d d: %d PV: %d%d", score, d, pv.moves[0], pv.moves[1]);
			} else if(pv_length == 3){
				snprintf(message_str, 28, "%d d: %d PV: %d%d%d", score, d, pv.moves[0], pv.moves[1], pv.moves[2]);
			} else if(pv_length == 4){
				snprintf(message_str, 28, "%d d: %d PV: %d%d%d%d", score, d, pv.moves[0], pv.moves[1], pv.moves[2], pv.moves[3]);
			} else if(pv_length == 5){
				snprintf(message_str, 28, "%d d: %d PV: %d%d%d%d%d", score, d, pv.moves[0], pv.moves[1], pv.moves[2], pv.moves[3], pv.moves[4]);
			} else if(pv_length == 6){
				snprintf(message_str, 28, "%d d: %d PV: %d%d%d%d%d%d", score, d, pv.moves[0], pv.moves[1], pv.moves[2], pv.moves[3], pv.moves[4], pv.moves[5]);
			} else if(pv_length == 7){
				snprintf(message_str, 28, "%d d: %d PV: %d%d%d%d%d%d%d", score, d, pv.moves[0], pv.moves[1], pv.moves[2], pv.moves[3], pv.moves[4], pv.moves[5], pv.moves[6]);
			} else if(pv_length == 8){
				snprintf(message_str, 28, "%d d: %d PV: %d%d%d%d%d%d%d%d", score, d, pv.moves[0], pv.moves[1], pv.moves[2], pv.moves[3], pv.moves[4], pv.moves[5], pv.moves[6], pv.moves[7]);
			} else if(pv_length == 9){
				snprintf(message_str, 28, "%d d: %d PV: %d%d%d%d%d%d%d%d%d", score, d, pv.moves[0], pv.moves[1], pv.moves[2], pv.moves[3], pv.moves[4], pv.moves[5], pv.moves[6], pv.moves[7], pv.moves[8]);
			} else if(pv_length == 10){
				snprintf(message_str, 28, "%d d: %d PV: %d%d%d%d%d%d%d%d%d%d", score, d, pv.moves[0], pv.moves[1], pv.moves[2], pv.moves[3], pv.moves[4], pv.moves[5], pv.moves[6], pv.moves[7], pv.moves[8], pv.moves[9]);
			} else if(pv_length > 10){
				snprintf(message_str, 28, "%d d: %d PV: %d%d%d%d%d%d%d%d%d%d...", score, d, pv.moves[0], pv.moves[1], pv.moves[2], pv.moves[3], pv.moves[4], pv.moves[5], pv.moves[6], pv.moves[7], pv.moves[8], pv.moves[9]);
			}
			update_msg(message_str);
		} else if(global_game_settings.show_evaluation){
			memset(message_str, 0, sizeof(message_str));
			snprintf(message_str, 28, "%d d: %d", score, d);
			update_msg(message_str);
		}
		memcpy(moves, pv.moves, pv_length*sizeof(int));
		if(score == INFINITY || score == -INFINITY){
			stop_engine = 1;
			return;
		}
	}
}

void print_settings_buffer(char str_buffers[6][32], char *menu_entries[6], game_settings settings){
	int i;

	memset(str_buffers, 0, sizeof(str_buffers[0])*6);

	if(settings.white_player == HUMAN){
		snprintf(str_buffers[0], 32, "white: human");
	} else {
		snprintf(str_buffers[0], 32, "white: cpu");
	}
	if(settings.black_player == HUMAN){
		snprintf(str_buffers[1], 32, "black: human");
	} else {
		snprintf(str_buffers[1], 32, "black: cpu");
	}
	if(settings.show_evaluation){
		snprintf(str_buffers[2], 32, "show eval: true");
	} else {
		snprintf(str_buffers[2], 32, "show eval: false");
	}
	if(settings.show_pv){
		snprintf(str_buffers[3], 32, "show pv: true");
	} else {
		snprintf(str_buffers[3], 32, "show pv: false");
	}
	snprintf(str_buffers[4], 32, "cpu wait: %dsec", settings.computer_wait);
	snprintf(str_buffers[5], 32, "Continue");

	for(i = 0; i < 6; i++){
		menu_entries[i] = str_buffers[i];
	}
}

int choose_game_settings(){
	char str_buffers[6][32];
	char *menu_entries[6];
	int selection;
	char *invalid_message = "Invalid wait time";
	char *entry;
	int secs;
	game_settings old_settings;

	old_settings = global_game_settings;

	global_game_settings.game_board = global_board;
	global_game_settings.white_player = HUMAN;
	global_game_settings.black_player = HUMAN;
	global_game_settings.show_evaluation = 1;
	global_game_settings.show_pv = 1;
	global_game_settings.computer_wait = 30;
	print_settings_buffer(str_buffers, menu_entries, global_game_settings);

	while((selection = do_menu("Game Options", menu_entries, sizeof(menu_entries)/sizeof(menu_entries[0]))) != 5){
		if(selection == -1){
			global_game_settings = old_settings;
			return 0;
		}
		if(selection == 0){
			global_game_settings.white_player = !global_game_settings.white_player;
		} else if(selection == 1){
			global_game_settings.black_player = !global_game_settings.black_player;
		} else if(selection == 2){
			global_game_settings.show_evaluation = !global_game_settings.show_evaluation;
		} else if(selection == 3){
			global_game_settings.show_pv = !global_game_settings.show_pv;
		} else if(selection == 4){
			entry = do_text_entry("Enter wait seconds");
			if(entry){
				secs = atoi(entry);
				free(entry);
				if(secs <= 0 || secs >= 1000){
					do_menu("Error", &invalid_message, 1);
				} else {
					global_game_settings.computer_wait = secs;
				}
			}
		}
		print_settings_buffer(str_buffers, menu_entries, global_game_settings);
	}

	return 1;
};

int do_load_board(){
	FILE *fp;
	char *entry;
	char *error_message = "Failed to load file";
	game_settings old_settings;

	entry = do_text_entry("Enter file name");
	if(entry){
		fp = fopen(entry, "r");
		free(entry);
	}
	if(!entry || !fp){
		do_menu("Error", &error_message, 1);
		return 0;
	}
	old_settings = global_game_settings;
	if(!fread(&global_game_settings, 1, sizeof(game_settings), fp)){
		fclose(fp);
		do_menu("Error", &error_message, 1);
		return 0;
	}

	fclose(fp);

	global_board = global_game_settings.game_board;
	return 1;
}

void do_save_board(){
	FILE *fp;
	char *entry;
	char *error_message = "Failed to save file";

	global_game_settings.game_board = global_board;
	entry = do_text_entry("Enter file name");
	if(entry){
		fp = fopen(entry, "w");
		free(entry);
	}
	if(!entry || !fp){
		do_menu("Error", &error_message, 1);
		return;
	}
	if(!fwrite(&global_game_settings, 1, sizeof(game_settings), fp)){
		fclose(fp);
		do_menu("Error", &error_message, 1);
		return;
	}

	fclose(fp);
}

void _main(){
	int win_color;
	int draw = 0;
	int menu_selection;
	unsigned char success = 0;
	board demo_board;
	int i;

	clrscr();
	initialize_board(&global_board);
	if(!GrayOn()){
		printf("Error initializeing grayscale!\n");
		ngetchx();
		free(hash_table);
		free(light_buffer);
		free(dark_buffer);
		return;
	}

	demo_board.state = 0;
	demo_board.placed_mask = 0;
	for(i = 0; i < 7; i++){
		demo_board.col_heights[i] = 0;
	}
	for(i = 0; i < sizeof(demo_board_moves)/sizeof(demo_board_moves[0]); i++){
		place_piece(&demo_board, demo_board_moves[i], i%2);
	}
	ClearGrayScreen2B(light_buffer, dark_buffer);
	render_board(&demo_board);
	memcpy(GrayGetPlane(LIGHT_PLANE), light_buffer, LCD_SIZE);
	memcpy(GrayGetPlane(DARK_PLANE), dark_buffer, LCD_SIZE);

	while(!success){
		menu_selection = do_menu("Ti89 Connect 4", main_menu_items, sizeof(main_menu_items)/sizeof(main_menu_items[0]));
		if(menu_selection == 0){
			success = choose_game_settings();
		} else if(menu_selection == 1){
			success = do_load_board();
		} else if(menu_selection == 2){
			free(hash_table);

			GrayOff();
			free(light_buffer);
			free(dark_buffer);
			return;
		}
	}

	selection_col = 3;
	computer_col = 3;
	current_turn = WHITE;

	stop_engine = 0;
	move_made = 0;
	update_scr = 1;
	enter_menu = 0;

	kbq = kbd_queue();
	computer_timer = global_game_settings.computer_wait*20;
	old_int_5 = GetIntVec(AUTO_INT_5);
	SetIntVec(AUTO_INT_5, time_update);

	while(1){
		if(update_scr){
			render_screen();
			update_scr = 0;
		}
		if(enter_menu){
			SetIntVec(AUTO_INT_5, old_int_5);
			menu_selection = do_menu("Options", options_menu_items, sizeof(options_menu_items)/sizeof(options_menu_items[0]));
			if(menu_selection == 3){
				break;
			}
			if(menu_selection == 2){
				do_save_board();
			}
			if(menu_selection == 1){
				choose_game_settings();
			}
			enter_menu = 0;
			render_screen();
			stop_engine = 0;
			computer_timer = global_game_settings.computer_wait*20;
			SetIntVec(AUTO_INT_5, time_update);
		}
		if(animating && animation_y == animation_end_y){
			animating = 0;
			place_piece(&global_board, animation_col, current_turn);
			current_turn = !current_turn;
			stop_engine = 0;
			update_scr = 1;
			if(get_player(current_turn) == COMPUTER){
				computer_timer = global_game_settings.computer_wait*20;
			}
		}
		win_color = get_win(&global_board);
		if(win_color != -1){
			break;
		}
		if(get_draw(&global_board)){
			draw = 1;
			break;
		}
		if(!stop_engine && (global_game_settings.show_evaluation || get_player(current_turn) == COMPUTER)){
			update_computer();
		}
	}

	SetIntVec(AUTO_INT_5, old_int_5);

	memset(message_str, 0, sizeof(message_str));
	render_screen();

	if(win_color == WHITE){
		snprintf(message_str, 28, "White wins!");
		update_msg(message_str);
		ngetchx();
	} else if(win_color == BLACK){
		snprintf(message_str, 28, "Black wins!");
		update_msg(message_str);
		ngetchx();
	}
	if(draw){
		snprintf(message_str, 28, "Draw!");
		update_msg(message_str);
		ngetchx();
	}

	free(hash_table);

	GrayOff();
	free(light_buffer);
	free(dark_buffer);
}
