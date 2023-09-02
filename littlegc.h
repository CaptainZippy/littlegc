#pragma once

struct lgc_t;
struct lgc_object_t;

using lgc_alive_func = void(lgc_t* gc, lgc_object_t* obj);
using lgc_dead_func = void(lgc_t* gc, lgc_object_t* obj);
using lgc_scan_func = void(lgc_t* gc, lgc_object_t* obj, lgc_alive_func alive_func);
using lgc_u8 = unsigned char;

/// Garbage collectable object.
/// Add this as a field of the object you want to be garbage collected.
/// You can cast between the gc and containing object with offsetof + casts.
struct lgc_object_t {
    lgc_object_t* next{};
    lgc_object_t* prev{};
    lgc_u8 color{};
};

/// The garbage collection object.
/// Add this as a field of the object you want to control garbage collection.
/// Consider all fields private.
struct lgc_t {
    lgc_object_t white_list{};
    lgc_object_t alive_list{};
    lgc_scan_func* scan_func{};
    lgc_dead_func* dead_func{};
    lgc_u8 white_color{};
};

/// Initialize an instance of a garbage collector.
/// Provide two functions:
/// * 'scan_func' which is called to discover outgoing references from an object.
/// * 'dead_func' which is called when an object is detected as unreachable.
///
/// For 'scan_func' the first parameter is the object to scan, or null meaning the roots.
/// For each outgoing reference, the callee must call the 'alive_func' parameter.
bool lgc_init(lgc_t* gc, lgc_scan_func scan_func, lgc_dead_func dead_func);

/// Add a new object to the gc.
void lgc_register(lgc_t* gc, lgc_object_t* obj);

/// Perform full collection.
void lgc_collect(lgc_t* gc);
