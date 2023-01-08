#pragma once

struct lgc_t;
struct lgc_object_t;

using lgc_alive_func = void(lgc_t* gc, lgc_object_t* obj);
using lgc_dead_func = void(lgc_t* gc, lgc_object_t* obj);
using lgc_scan_func = void(lgc_t* gc, lgc_object_t* obj, lgc_alive_func alive_func);
using lgc_u8 = unsigned char;

struct lgc_object_t {
    lgc_object_t* next{};
    lgc_object_t* prev{};
    lgc_u8 color{};
};

struct lgc_t {
    void* userdata{};
    lgc_object_t white_list{};
    lgc_object_t alive_list{};
    lgc_scan_func* scan_func{};
    lgc_dead_func* dead_func{};
    lgc_u8 white_color{};
};

/// Initialize an instance of a garbage collector.
/// 'scan_func' will be called to query outgoing references from an object.
/// For each reference, the alive_func parameter must be called.
/// If the 'obj' parameter of scan_func is null, scan the roots.
/// 'dead_func' will be called when an object becomes unreachable.
bool lgc_init(lgc_t* gc, lgc_scan_func scan_func, lgc_dead_func dead_func);

/// Add a new object to the gc.
void lgc_register(lgc_t* gc, lgc_object_t* obj);

/// Full collection.
void lgc_collect(lgc_t* gc);
