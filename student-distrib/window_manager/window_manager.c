#include "window_manager.h"
#include "../images/desktop.h"

// Format: AARRGGBB I think??? lmao
#define BLACK  								0xFF000000
#define RED    								0xFFFF0000
#define GREEN  								0xFF00FF00
#define BLUE   								0xFF0000FF
#define GRAY   								0xFF999999
#define WHITE  								0xFFFFFFFF

int32_t window_id = 0;
uint32_t *back_buffer;

/*
 * This will allocate a window of width and height at the x, y coordinates relative to the screen
 * 
 * INPUTS: x: The x coordinate of window
 *         y: The y coordinate of window
 *         width: Initial width of window
 *         height: Initial height of window
 *         id: Used to tell user program of window id
 * OUTPUTS: pointer to buffer user program can use to construct GUI
 */
int32_t* alloc_window(uint32_t x, uint32_t y, uint32_t width, uint32_t height, int32_t* id) {
    int page_index = get_open_page();
    if (map_containing_region((void *) (page_index * LARGE_PAGE_SIZE), (void*) (page_index * LARGE_PAGE_SIZE), 1, 
        PAGE_SIZE_IS_4M | PAGE_USER_LEVEL | PAGE_READ_WRITE | PAGE_PRESENT) == -1) {
        return NULL;
    }
    window *head = (window*) kmalloc(sizeof(window));
    // The system call will fill out this so that user program will know the window id
    *id = window_id++;
    printf("window id: %d\n", head->id = *id);
    printf("X: %d\n", head->x = x);
    printf("Y: %d\n", head->y = y);
    printf("Width: %d\n", head->width = width);
    printf("Height: %d\n", head->height = height);
    printf("Buffer location: %x\n", head->buffer = (uint32_t*) (page_index * LARGE_PAGE_SIZE));
    head->page_index = page_index;
    head->need_update = 0;
    insert(head);
    // Init the window by drawing border on top
    init_window(head);
    
    return head->buffer;
}

void init_window(window *app) {
    draw_thick_line_horizontal(app->buffer, app->width, 0, 0, app->width, 0, WINDOW_BORDER_WIDTH, WINDOW_BORDER_COLOR);
    fill_rect(app->buffer, app->width, 0, WINDOW_BORDER_WIDTH, app->width, app->height, WHITE);
    fill_circle(app->buffer, app->width, app->width - WINDOW_CLOSE_OFFSET_FROM_LEFT, WINDOW_BORDER_WIDTH / 2, 3, WINDOW_CLOSE_COLOR);
    // app->buffer += WINDOW_BORDER_WIDTH * app->width * BYTES_PER_PIXEL;
}

void draw_client_buffer(window *app) {
    int i, j;
    for (i = 0; i < app->width; i++) {
        for (j = 0; j < app->height; j++) {
            draw_pixel(back_buffer, svga.width, app->x + i, app->y + j, app->buffer[i + j * app->width]);
        }
    }
}

int32_t destroy_window(int id) {
    return 0;
}

/*
 * Insert window struct at beginning of linkedlist
 * 
 * INPUTS: node - node to be inserted in list
 * OUTPUTS: None
 */
void insert(window *node) {
    if (head == NULL) {
        head = node;
        tail = node;
    } else {
        node->next = head;
        head->prev = node;
        head = node;
    }
}

/*
 * Return pointer to node in linked list ith id
 * 
 * INPUTS:  id - the window id to search for
 * OUTPUTS: pointer to node
 */
window* find(uint32_t id) {
    window *temp = head;
    while (temp != NULL) {
        if (temp->id == id) {
            return temp;
        }
        temp = temp->next;
    }
    return NULL;
}

/*
 * Marks the window with id as needing to be updated
 * 
 * INPUTS:  id - the window to update
 * OUTPUTS: None
 */
int32_t redraw_window(uint32_t id) {
    window *app;
    if ((app = find(id)) == NULL) {
        return -1;
    }
    app->need_update = 1;
    printf("redraw window\n");
    compositor();
    return 0;
} 

/*
 * Copies the desktop background to backbuffer and displays on screen
 * 
 * INPUTS:  None
 * OUTPUTS: None
 */
void init_desktop() {
    // Copying desktop to back_buffer for double buffering system
    back_buffer = kmalloc(svga.width * svga.height * 4);
    memcpy(back_buffer, desktop, svga.width * svga.height * 4);
    // Copy the desktop image to actual screen    
    memcpy(svga.frame_buffer, back_buffer, svga.width * svga.height * 4);   
    svga_update(0, 0, SYSTEM_RESOLUTION_WIDTH, SYSTEM_RESOLUTION_HEIGHT);   
}

void mouse_event(uint32_t x, uint32_t y) {
    window *temp = head;
    while (temp) {
        if (window_contains_mouse(temp, x, y)) {
            move_window_to_front(temp->id);
            compositor();
            return;  
        }
        temp = temp->next;
    }
}

void move_window_to_front(int id) {
    if (head->id == id)
        return;
    if (tail->id == id) {
        window *tail_prev = tail->prev;
        tail->next = head;
        head->prev = tail;
        tail->prev->next = NULL;
        tail->prev = NULL;
        head = tail;
        tail = tail_prev;
        return;
    }
    window *node = find(id);
    node->prev->next = node->next;
    node->next->prev = node->prev;

    node->next = head;
    node->prev = NULL;
    head->prev = node;
    head = node;
}

int delete_from_front() {
    window *temp;
    if(head == NULL) {
        return -1;
    }

    temp = head;
    head = head->next; 

    if (head != NULL)
        head->prev = NULL; 

    kfree(temp);
    return 0;
}


int delete_from_end() {
    window *temp;

    if(tail == NULL) {
        return -1;
    }
    temp = tail;

    tail = tail->prev;
    
    if (tail != NULL)
        tail->next = NULL; 

    kfree(temp);       
    return 0;
}

int delete(int id) {
    window *current;

    current = head;

    if (head->id == id) 
        return delete_from_front();
 
    else if (tail->id == id)
        return delete_from_end();
    
    else {
        while (current != NULL && current->id != id) {
            current = current->next;
        }
        current->prev->next = current->next;
        current->next->prev = current->prev;

        kfree(current);

        return 0;
    }
    return -1;
}


int window_contains_mouse(window *app, uint32_t mouse_x, uint32_t mouse_y) {
    if (app->x <= mouse_x && mouse_x <= (app->x + app->width) && app->y <= mouse_y && mouse_y <= (app->y + app->height))
        return 1;
    return 0;
}

void compositor() {
    memcpy(back_buffer, desktop, svga.width * svga.height *4);
    window *temp = tail;
    int i;
     i= 20;
    while (temp) {
        // put_string(back_buffer, svga.width, "compositor strx", 20, i+=10, WHITE);
        draw_client_buffer(temp);
        temp = temp->prev;
    }
    memcpy(svga.frame_buffer, back_buffer, svga.width * svga.height * 4);
    svga_update(0, 0, svga.width, svga.height);
}


























