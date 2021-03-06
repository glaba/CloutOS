#ifndef _WINDOW_MANAGER
#define _WINDOW_MANAGER

#include "../types.h"
#include "../graphics/graphics.h"
#include "../paging.h"
#include "../processes.h"
#include "../spinlock.h"

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

    // The information related to the last mouse event:
    // relative X, relative Y, left button, right button
    int mouse_event[4];

    struct window *next;
    struct window *prev;
} window;

window *head;
window *tail;
uint32_t *back_buffer;
extern int GUI_enabled;

int init_window_manager();

int mouse_clicked_close(window *app, uint32_t mouse_x, uint32_t  mouse_y);

int window_bar_contains_mouse(window *app, uint32_t mouse_x, uint32_t  mouse_y);

void mouse_event(uint32_t x, uint32_t y);

int window_contains_mouse(window *app, uint32_t mouse_x, uint32_t mouse_y);

void draw_client_buffer(window *app);

uint32_t* alloc_window(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t* id, uint32_t pid);

int32_t resize_window(uint32_t width, uint32_t height);

int32_t destroy_windows_by_pid(int pid);

int32_t destroy_window_by_id(int id);

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

