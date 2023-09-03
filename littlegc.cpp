#include "littlegc.h"
#include <cassert>

enum lgc_state {
    grey = 2,  // alive, needs scanning
};

using lgc_object_p = lgc_object_t*;

static void list_push_front(lgc_object_t* list, lgc_object_t* obj) {
    assert(obj->next == nullptr);
    assert(obj->prev == nullptr);
    obj->next = list->next;
    obj->prev = list;
    list->next->prev = obj;
    list->next = obj;
}

static void list_remove(lgc_object_t* obj) {
    obj->prev->next = obj->next;
    obj->next->prev = obj->prev;
    obj->next = nullptr;
    obj->prev = nullptr;
}

bool lgc_init(lgc_t* gc, lgc_scan_func scan, lgc_dead_func dead) {
    gc->white_list.next = &gc->white_list;  // circular list with sentinel element
    gc->white_list.prev = &gc->white_list;
    gc->white_list.color = 100;             // magic
    gc->alive_list.next = &gc->alive_list;  // circular list with sentinel element
    gc->alive_list.prev = &gc->alive_list;
    gc->alive_list.color = 101;  // magic

    gc->scan_func = scan;
    gc->dead_func = dead;
    gc->white_color = 0;  // 0 or 1. So black = !white. Grey is always 2.
    return true;
}

void lgc_register(lgc_t* gc, lgc_object_t* obj) {
    assert(obj->color == 0);
    assert(obj->next == nullptr);
    assert(obj->prev == nullptr);
    // insert after head of list:
    // white -> first
    // white -> obj -> first
    list_push_front(&gc->white_list, obj);
    obj->color = gc->white_color;
}

static void lgc_mark_alive(lgc_t* gc, lgc_object_t* obj) {
    // obj should be on one of the lists already
    assert(obj->next != nullptr);
    assert(obj->prev != nullptr);
    if (obj->color == gc->white_color) {
        list_remove(obj);
        list_push_front(&gc->alive_list, obj);
        obj->color = grey;
    }
}

static void lgc_mark(lgc_t* gc) {
    // First scan the roots
    (*gc->scan_func)(gc, nullptr, &lgc_mark_alive);
    // Run backwards through this list - new additions from scan are pushed at the front.
    // So marking is complete when we reach the list alive_list sentinel.
    lgc_u8 black_color = gc->white_color ? 0 : 1;
    for (lgc_object_p cur = gc->alive_list.prev, end = &gc->alive_list; cur != end;
         cur = cur->prev) {
        assert(cur->color == grey);
        cur->color = black_color;
        (*gc->scan_func)(gc, cur, &lgc_mark_alive);
    }
}

static void lgc_sweep(lgc_t* gc) {
    // Everything remaining on the white list is dead.
    for (lgc_object_p cur = gc->white_list.next, end = &gc->white_list; cur != end;) {
        assert(cur->color == gc->white_color);
        lgc_object_t* dead = cur;
        cur = dead->next;
        (*gc->dead_func)(gc, dead);
    }
    // Move all survivors to white list
    if (gc->alive_list.next != &gc->alive_list) {
        gc->white_list.next = gc->alive_list.next;
        gc->white_list.prev = gc->alive_list.prev;
        gc->alive_list.next = &gc->alive_list;
        gc->alive_list.prev = &gc->alive_list;
        gc->white_list.next->prev = &gc->white_list;
        gc->white_list.prev->next = &gc->white_list;
    } else {
        gc->white_list.next = &gc->white_list;
        gc->white_list.prev = &gc->white_list;
    }
    // Flip color
    gc->white_color = gc->white_color ? 0 : 1;
}

void lgc_collect(lgc_t* gc) {
    // check preconditions
    assert(gc->alive_list.next == gc->alive_list.prev);  // Alive list initially empty
    for (lgc_object_t *cur = gc->white_list.next, *end = &gc->white_list; cur != end;
         cur = cur->next) {
        assert(cur->color == gc->white_color);
    }

    // mark everything which is alive
    lgc_mark(gc);

    // cleanup
    lgc_sweep(gc);
}
