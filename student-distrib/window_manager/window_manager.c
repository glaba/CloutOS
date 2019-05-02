#include "window_manager.h"
#include "../images/desktop.h"
#include "../processes.h"

// Format: AARRGGBB I think??? lmao
#define BLACK  								0xFF000000
#define RED    								0xFFFF0000
#define GREEN  								0xFF00FF00
#define BLUE   								0xFF0000FF
#define GRAY   								0xFF999999
#define WHITE  								0xFFFFFFFF

#define abs(n) ((n < 0) ? (-n) : (n))


int32_t window_id = 0;


int GUI_enabled = 0;
struct spinlock_t window_lock = SPIN_LOCK_UNLOCKED;

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
uint32_t* alloc_window(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t* id, uint32_t pid) {
    spin_lock_irqsave(window_lock);
    
    pcb_t *pcb = get_pcb();

    int page_index = get_open_page();

    // Add a mapping to the large_page_mappings array
    page_mapping mapping;
    mapping.virt_index = page_index;
    mapping.phys_index = page_index;
    DYN_ARR_PUSH(page_mapping, pcb->large_page_mappings, mapping);

    // Map in the page for the window
    if (map_containing_region((void*)(page_index * LARGE_PAGE_SIZE), (void*)(page_index * LARGE_PAGE_SIZE), 1, 
            PAGE_SIZE_IS_4M | PAGE_USER_LEVEL | PAGE_READ_WRITE | PAGE_PRESENT) == -1) {
        return NULL;
    }

    window *new_window = (window*)kmalloc(sizeof(window));

    // The system call will fill out this so that user program will know the window id
    *id = window_id++;
    printf("window id: %d\n", new_window->id = *id);
    printf("X: %d\n", new_window->x = x);
    printf("Y: %d\n", new_window->y = y);
    printf("Width: %d\n", new_window->width = width);
    printf("Height: %d\n", new_window->height = height);
    printf("Buffer location: %x\n", new_window->buffer = (uint32_t*) (page_index * LARGE_PAGE_SIZE));
    printf("Process PID: %d\n", new_window->pid = pid);

    // int i;
    // for (i = 0; i < 4; i++)
        // new_window->mouse_event[i] = -1;

    new_window->page_index = page_index;
    new_window->need_update = 0;
    insert(new_window);

    // Init the window by drawing border on top
    init_window(new_window);
    spin_unlock_irqsave(window_lock);
    return new_window->buffer;
}

void init_window(window *app) {
    fill_rect(app->buffer, app->width, 0, 0, app->width, WINDOW_BORDER_WIDTH, WINDOW_BORDER_COLOR);
    fill_rect(app->buffer, app->width, 0, WINDOW_BORDER_WIDTH, app->width, app->height, WHITE);
    fill_circle(app->buffer, app->width, app->width - WINDOW_CLOSE_OFFSET_FROM_LEFT, WINDOW_BORDER_WIDTH / 2, WINDOW_CLOSE_RADIUS, WINDOW_CLOSE_COLOR);
    // app->buffer += WINDOW_BORDER_WIDTH * app->width * BYTES_PER_PIXEL;
}

void draw_client_buffer(window *app) {
    int i, j;
    for (i = 0; i < app->width; i++) {
        for (j = 0; j < app->height; j++) {
            draw_pixel_fast(back_buffer, app->x + i, app->y + j, app->buffer[i + j * app->width]);
        }
    }
}
int32_t destroy_windows_by_pid(int pid) {
    spin_lock_irqsave(window_lock);

    window *cur, *next;
    for (cur = head; cur != NULL; cur = next) {
        next = cur->next;

        if (cur->pid == pid) {
            // Unlink this node from the other nodes
            (cur == head) ? (head = cur->next) : (cur->prev->next = cur->next);
            (cur == tail) ? (tail = cur->prev) : (cur->next->prev = cur->prev);
            // Free the memory for this node
            kfree(cur);
        }
    }

    spin_unlock_irqsave(window_lock);
    return 0;
}

int32_t destroy_window_by_id(int id) {
    spin_lock_irqsave(window_lock);

    window *cur, *next;
    int32_t pid = -1;
    for (cur = head; cur != NULL; cur = next) {
        next = cur->next;

        if (cur->id == id) {
            // Unlink this node from the other nodes
            (cur == head) ? (head = cur->next) : (cur->prev->next = cur->next);
            (cur == tail) ? (tail = cur->prev) : (cur->next->prev = cur->prev);
            // Free the memory for this node
            kfree(cur);

            pid = cur->pid;
            break;
        }
    }

    if (pid == -1) {
        spin_unlock_irqsave(window_lock);
        return -1;
    }

    // Check if there are any windows remaining that belong to the process
    int remaining_windows = 0;
    for (cur = head; cur != NULL; cur = next) {
        next = cur->next;

        if (cur->pid == pid) {
            remaining_windows = 1;
            break;
        }
    }

    // If there are no remaining windows, send the process the SIGINT signal
    if (!remaining_windows) {
        send_signal(pid, SIGNAL_INTERRUPT, 0);
    }

    spin_unlock_irqsave(window_lock);
    return -1;
}

/*
 * Insert window struct at beginning of linkedlist
 * 
 * INPUTS: node - node to be inserted in list
 * OUTPUTS: None
 */
void insert(window *node) {
    node->next = NULL;
    node->prev = NULL;
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
    spin_lock_irqsave(window_lock);
    window *app;
    if ((app = find(id)) == NULL) {
        spin_unlock_irqsave(window_lock);
        return -1;
    }
    app->need_update = 1;
    // printf("redraw window\n");
    compositor();
    spin_unlock_irqsave(window_lock);
    return 0;
} 

int init_window_manager() {
    // Copying desktop to back_buffer for double buffering system
    // back_buffer = kmalloc(svga.width * svga.height * 4);
    // memcpy(back_buffer, desktop, svga.width * svga.height * 4);
    back_buffer = vid_mem_buffers[3];
    head = NULL;
    tail = NULL;
    memcpy((uint32_t*) back_buffer, desktop, svga.width * svga.height * 4);
    return 1;
}

/*
 * Copies the desktop background to backbuffer and displays on screen
 * 
 * INPUTS:  None
 * OUTPUTS: None
 */
void init_desktop() {
    if (GUI_enabled) {
        // Copy the desktop image to actual screen    
        memcpy(svga.frame_buffer, back_buffer, svga.width * svga.height * 4);   
        svga_update(0, 0, SYSTEM_RESOLUTION_WIDTH, SYSTEM_RESOLUTION_HEIGHT);   
    }
}

void mouse_event(uint32_t x, uint32_t y) {
    // Sets mouse to not currently holding window when left button not pressed
    spin_lock_irqsave(window_lock);
    if (head == NULL) {
        spin_unlock_irqsave(window_lock);
        return;
    }
    if (GUI_enabled && head != NULL) {
        if (!mouse.left_click && mouse.holding_window)
            mouse.holding_window = 0;
        
        window *temp = head;
        if (mouse.left_click && mouse_clicked_close(temp, x, y)) {
            destroy_window_by_id(temp->id);
            compositor();
            return;
        }
        // Used to focus the window clicked on
        if (mouse.left_click && !mouse.holding_window) {
            while (temp != NULL) {
                if (window_contains_mouse(temp, x, y)) {
                    move_window_to_front(temp->id);
                    break;  
                }
                temp = temp->next;
            }
        }

        // Rely on shortcutting to insure mouse clips on to window even when out of window bounds when moving
        if (mouse.holding_window || (mouse.left_click && window_bar_contains_mouse(head, x, y))) {
            mouse.holding_window = 1;
            temp->x += mouse.x - mouse.old_x;
            temp->y += mouse.y - mouse.old_y;
            if (temp->x > svga.width)    temp->x = 0;
            if (temp->y > svga.height)   temp->y = 0;
        }
        // Calls compositor which puts everything together
        compositor();
    }
    spin_unlock_irqsave(window_lock);
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

// int delete_from_front() {
//     window *temp;
//     if(head == NULL) {
//         return -1;
//     }

//     temp = head;
//     head = head->next; 

//     if (head != NULL)
//         head->prev = NULL; 

//     kfree(temp);
//     return 0;
// }


// int delete_from_end() {
//     window *temp;

//     if(tail == NULL) {
//         return -1;
//     }
//     temp = tail;

//     tail = tail->prev;
    
//     if (tail != NULL)
//         tail->next = NULL; 

//     kfree(temp);       
//     return 0;
// }

// int delete(int id) {
//     window *current;

//     current = head;

//     if (head->id == id) 
//         return delete_from_front();
 
//     else if (tail->id == id)
//         return delete_from_end();
    
//     else {
//         while (current != NULL && current->id != id) {
//             current = current->next;
//         }
//         current->prev->next = current->next;
//         current->next->prev = current->prev;

//         kfree(current);

//         return 0;
//     }
//     return -1;
// }


int mouse_clicked_close(window *app, uint32_t mouse_x, uint32_t  mouse_y) {
    if (mouse_x >= (app->x + app->width - WINDOW_CLOSE_OFFSET_FROM_LEFT) && mouse_y >= app->y && mouse_x <= (app->x + app->width) && mouse_y <= (app->y + WINDOW_BORDER_WIDTH)){
        return 1;
    }
    return 0;
}

int window_bar_contains_mouse(window *app, uint32_t mouse_x, uint32_t  mouse_y) {
    if (mouse_x >= app->x && mouse_y >= app->y && mouse_x <= (app->x + app->width) && mouse_y <= (app->y + WINDOW_BORDER_WIDTH)){
        return 1;
    }
    return 0;
}

int window_contains_mouse(window *app, uint32_t mouse_x, uint32_t mouse_y) {
    if (app->x <= mouse_x && mouse_x <= (app->x + app->width) && app->y <= mouse_y && mouse_y <= (app->y + app->height)) {
        return 1;
    }
    return 0;
}

void compositor() {
    spin_lock_irqsave(window_lock);

    // Unmap the memory for the current process
    unmap_process(get_pid());

    // Map in the memory for all the windows
    window *cur;
    for (cur = head; cur != NULL; cur = cur->next) {
        map_containing_region((void*)(cur->page_index * LARGE_PAGE_SIZE), (void*)(cur->page_index * LARGE_PAGE_SIZE), 1, 
            PAGE_SIZE_IS_4M | PAGE_USER_LEVEL | PAGE_READ_WRITE | PAGE_PRESENT);
    }

    if (GUI_enabled) {
        memcpy(back_buffer, desktop, svga.width * svga.height *4);
        window *temp = tail;
        while (temp) {
            // put_string(back_buffer, svga.width, "compositor strx", 20, i+=10, WHITE);
            draw_client_buffer(temp);
            temp = temp->prev;
        }
        memcpy(svga.frame_buffer, back_buffer, svga.width * svga.height * 4);
        svga_update(0, 0, svga.width, svga.height);
    }

    // Unmap out the memory for all the windows
    for (cur = head; cur != NULL; cur = cur->next) {
        unmap_containing_region((void*)(cur->page_index * LARGE_PAGE_SIZE), 1);
    }

    // Remap the current process
    map_process(get_pid());

    spin_unlock_irqsave(window_lock);
}


























