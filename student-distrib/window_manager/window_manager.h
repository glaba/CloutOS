#ifndef _WINDOW_MANAGER
#define _WINDOW_MANAGER

#include "../types.h"
#include "../graphics/graphics.h"
#include "../paging.h"

#define WINDOW_BORDER_WIDTH                             15
#define WINDOW_BORDER_COLOR                             0xFF98989D
#define WINDOW_CLOSE_COLOR                              0xFFFF453A
#define WINDOW_CLOSE_OFFSET_FROM_LEFT                   10
#define WINDOW_CLOSE_RADIUS                             5
// Describes a window. In a doubly linked list the z value is evident from the ordering
typedef struct window {
    uint32_t id;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t *buffer;
    uint32_t pid;
    int page_index;
    int need_update;

    struct window *next;
    struct window *prev;
} window;

window *head;
window *tail;
extern int GUI_enabled;

int mouse_clicked_close(window *app, uint32_t mouse_x, uint32_t  mouse_y);

int window_bar_contains_mouse(window *app, uint32_t mouse_x, uint32_t  mouse_y);

void mouse_event(uint32_t x, uint32_t y);

int window_contains_mouse(window *app, uint32_t mouse_x, uint32_t mouse_y);

void draw_client_buffer(window *app);

uint32_t* alloc_window(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t* id);

int32_t resize_window(uint32_t width, uint32_t height);

int32_t destroy_window(int id);

void init_window(window* app);

void init_desktop();

void insert(window *node);

int delete_from_front();

int delete_from_end();

int delete(int id);

void move_window_to_front(int id);

int32_t redraw_window(uint32_t id);


void compositor();

#endif

