#include <tigcclib.h>
#include "extgraph.h"

int menu_select;
int menu_max;

void render_menu(char *title, char **items, unsigned int num_items){
        unsigned int title_width;
        unsigned int option_width;

        FastFillRect_R(GrayGetPlane(LIGHT_PLANE), 1, 35, 158, 75, A_REVERSE);
        FastDrawHLine(GrayGetPlane(LIGHT_PLANE), 0, 159, 34, A_NORMAL);
        FastDrawHLine(GrayGetPlane(LIGHT_PLANE), 0, 159, 44, A_NORMAL);
        FastDrawVLine(GrayGetPlane(LIGHT_PLANE), 0, 34, 76, A_NORMAL);
        FastDrawVLine(GrayGetPlane(LIGHT_PLANE), 159, 34, 76, A_NORMAL);
        FastDrawHLine(GrayGetPlane(LIGHT_PLANE), 0, 159, 76, A_NORMAL);
        FastFillRect_R(GrayGetPlane(DARK_PLANE), 1, 35, 158, 75, A_REVERSE);
        FastDrawHLine(GrayGetPlane(DARK_PLANE), 0, 159, 34, A_NORMAL);
        FastDrawHLine(GrayGetPlane(DARK_PLANE), 0, 159, 44, A_NORMAL);
        FastDrawVLine(GrayGetPlane(DARK_PLANE), 0, 34, 76, A_NORMAL);
        FastDrawVLine(GrayGetPlane(DARK_PLANE), 159, 34, 76, A_NORMAL);
        FastDrawHLine(GrayGetPlane(DARK_PLANE), 0, 159, 76, A_NORMAL);
	PortSet(GrayGetPlane(LIGHT_PLANE),  239, 127);
        title_width = DrawStrWidth(title, F_6x8);
        FontSetSys(F_6x8);
        DrawStr(80 - title_width/2, 36, title, A_NORMAL);
	PortSet(GrayGetPlane(DARK_PLANE), 239, 127);
	DrawStr(80 - title_width/2, 36, title, A_NORMAL);
        if(menu_select > 0){
		option_width = DrawStrWidth(items[menu_select - 1], F_6x8);
		PortSet(GrayGetPlane(LIGHT_PLANE),  239, 127);
		DrawStr(80 - option_width/2, 46, items[menu_select - 1], A_NORMAL);
		PortSet(GrayGetPlane(DARK_PLANE), 239, 127);
		DrawStr(80 - option_width/2, 46, items[menu_select - 1], A_NORMAL);
        }
        option_width = DrawStrWidth(items[menu_select], F_6x8);
        if(option_width < 158){
		FastFillRect_R(GrayGetPlane(LIGHT_PLANE), 79 - option_width/2, 55, 81 + option_width/2, 64, A_NORMAL);
		FastFillRect_R(GrayGetPlane(DARK_PLANE), 79 - option_width/2, 55, 81 + option_width/2, 64, A_NORMAL);
        }
	PortSet(GrayGetPlane(LIGHT_PLANE),  239, 127);
        DrawStr(80 - option_width/2, 56, items[menu_select], A_REVERSE);
	PortSet(GrayGetPlane(DARK_PLANE),  239, 127);
        DrawStr(80 - option_width/2, 56, items[menu_select], A_REVERSE);
        if(menu_select < menu_max){
		option_width = DrawStrWidth(items[menu_select + 1], F_6x8);
		PortSet(GrayGetPlane(LIGHT_PLANE),  239, 127);
		DrawStr(80 - option_width/2, 66, items[menu_select + 1], A_NORMAL);
		PortSet(GrayGetPlane(DARK_PLANE),  239, 127);
		DrawStr(80 - option_width/2, 66, items[menu_select + 1], A_NORMAL);
        }
}

int do_menu(char *title, char **items, unsigned int default_selection, unsigned int num_items){
	unsigned int key = 0;

        menu_select = default_selection;
        menu_max = num_items - 1;

	while(key != KEY_ENTER && key != KEY_ESC){
		render_menu(title, items, num_items);
		key = ngetchx();
		if(key == KEY_UP){
			if(menu_select){
				menu_select--;
			}
		} else if(key == KEY_DOWN){
			if(menu_select < menu_max){
				menu_select++;
			}
		}
	}

        if(key == KEY_ESC){
                return -1;
        }

        return menu_select;
}

char *do_text_entry(char *title){
        char *buffer;
        unsigned char cursor;
        unsigned int key;

        menu_select = 0;
        menu_max = 0;
        buffer = calloc(10, sizeof(char));
        cursor = 0;
        buffer[cursor] = '_';
        key = 0;

        while(key != KEY_ENTER){
                render_menu(title, &buffer, 1);
                key = ngetchx();
                if(key >= ' ' && key <= '~' && cursor < 8){
                        buffer[cursor] = key;
                        cursor++;
                        buffer[cursor] = '_';
                } else if(key == KEY_BACKSPACE && cursor){
                        buffer[cursor] = (char) 0;
                        cursor--;
                        buffer[cursor] = '_';
                } else if(key == KEY_ESC){
                        free(buffer);
                        return (char *) 0;
                }
        }

        buffer[cursor] = (char) 0;
        return buffer;
}
