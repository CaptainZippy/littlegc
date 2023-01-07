#include "littlegc.h"
#include <cassert>

enum lgc_state {
	white = 0, // uknown set
	black = 1, // alive & scanned
	grey = 2,  // alive, to scan
};

using lgc_object_p = lgc_object_t*;

static void list_push_front(lgc_object_t* list, lgc_object_t* obj) {
	assert(obj->next == nullptr);
	assert(obj->prev == nullptr);
	assert(obj->color == 0);
	obj->next = list->next;
	obj->prev = list;
	obj->color = white;
	list->next->prev = obj;
	list->next = obj;
}

static void list_remove(lgc_object_t* obj) {
	obj->prev->next = obj->next;
	obj->next->prev = obj->prev;
	obj->next = nullptr;
	obj->prev = nullptr;
	obj->color = white;
}

bool lgc_init(lgc_t* gc, lgc_scan_func scan, lgc_dead_func dead) {
	gc->white.next = &gc->white; // circular list with sentinel element
	gc->white.prev = &gc->white;
	gc->white.color = 100; //magic
	gc->alive.next = &gc->alive; // circular list with sentinel element
	gc->alive.prev = &gc->alive;
	gc->alive.color = 101; //magic

	gc->scan_func = scan;
	gc->dead_func = dead;
	return true;
}


void lgc_register(lgc_t* gc, lgc_object_t* obj) {
	// insert after head of list:
	// white -> first
	// white -> obj -> first
	list_push_front(&gc->white, obj);
}

static void lgc_is_alive(lgc_t* gc, lgc_object_t* object) {
	if (object->color == white) {
		list_remove(object);
		list_push_front(&gc->alive, object);
		object->color = grey;
	}
}

static void lgc_mark(lgc_t* gc) {
	// first scan the roots
	(*gc->scan_func)(gc, nullptr, &lgc_is_alive);
	// run backwards through this list - new additions from scan are pushed at the front
	for (lgc_object_p cur = gc->alive.prev, end = &gc->alive; cur != end; cur = cur->prev) {
		assert(cur->color == grey);
		cur->color = black;
		(*gc->scan_func)(gc, cur, &lgc_is_alive);
	}
}

static void lgc_sweep(lgc_t* gc) {
	for (lgc_object_p cur = gc->white.next, end = &gc->white; cur != end; ) {
		assert(cur->color == white);
		lgc_object_t* dead = cur;
		cur = dead->next;
		(*gc->dead_func)(gc, dead);
	}
	// move all survivors to white list + reset color
	if (gc->alive.next != &gc->alive ) {
		gc->white.next = gc->alive.next;
		gc->white.prev = gc->alive.prev;
		gc->alive.next = &gc->alive;
		gc->alive.prev = &gc->alive;
		gc->white.next->prev = &gc->white;
		gc->white.prev->next = &gc->white;

		for (lgc_object_p cur = gc->white.next, end = &gc->white; cur != end; cur = cur->next) {
			assert(cur->color == black);
			cur->color = white;
		}
	}
	else {
		gc->white.next = &gc->white;
		gc->white.prev = &gc->white;
	}
}

void lgc_collect(lgc_t* gc) {
	// check preconditions
	assert(gc->alive.next == gc->alive.prev); // is empty
	for (lgc_object_t* cur = gc->white.next, *end = &gc->white; cur != end; cur = cur->next) {
		assert(cur->color == white);
	}

	// mark everything which is alive
	lgc_mark(gc);

	// cleanup
	lgc_sweep(gc);
}
