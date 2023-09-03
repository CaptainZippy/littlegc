#include <cassert>
#include <cstddef>
#include <vector>
#include "littlegc.h"

struct test_object {
    test_object(lgc_t* gc, char n) {
        name = n;
        lgc_register(gc, &lgc);
    }
    void refs(test_object* o) { children.push_back(o); }

    test_object(const test_object&) = delete;
    lgc_object_t lgc;
    char name{};
    bool shouldDie{false};
    bool isDead{false};
    std::vector<test_object*> children;

    static test_object* from_gcobj(lgc_object_t* obj) {
        constexpr size_t off = offsetof(test_object, lgc);
        auto addr = reinterpret_cast<size_t>(obj) - off;
        return reinterpret_cast<test_object*>(addr);
    }
};

struct test_fixture {
    test_fixture() { lgc_init(&gc, testscan, testfree); }

    static void testscan(lgc_t* gc, lgc_object_t* obj, lgc_alive_func alive_func) {
        if (obj) {
            test_object* o = test_object::from_gcobj(obj);
            for (auto&& c : o->children) {
                (*alive_func)(gc, &c->lgc);
            }
        } else {
            test_fixture* self = test_fixture::from_gc(gc);
            for (auto&& c : self->roots) {
                (*alive_func)(gc, &c->lgc);
            }
        }
    }
    static void testfree(lgc_t* gc, lgc_object_t* obj) {
        test_object* o = test_object::from_gcobj(obj);
        assert(o->shouldDie);
        o->isDead = true;
    }

    test_object* newobj(char c) {
        auto o = new test_object(&gc, c);
        objects.push_back(o);
        return o;
    }

    void isRoot(test_object* o) { roots.push_back(o); }

    void collect() {
        lgc_collect(&gc);
        for (auto o : objects) {
            assert(o->isDead == o->shouldDie);
        }
        lgc_collect(&gc);
    }

    static test_fixture* from_gc(lgc_t* gc) {
        constexpr size_t off = offsetof(test_fixture, gc);
        auto addr = reinterpret_cast<size_t>(gc) - off;
        return reinterpret_cast<test_fixture*>(addr);
    }

    lgc_t gc;
    std::vector<test_object*> objects;
    std::vector<test_object*> roots;
};

int test_basic() {
    // no references
    {
        test_fixture fixture;
        test_object* a = fixture.newobj('a');
        test_object* b = fixture.newobj('b');

        a->shouldDie = true;
        b->shouldDie = true;

        fixture.collect();
    }

    // circular, no refs from root
    {
        test_fixture fixture;
        test_object* a = fixture.newobj('a');
        test_object* b = fixture.newobj('b');

        a->refs(b);
        b->refs(a);
        a->shouldDie = true;
        b->shouldDie = true;

        fixture.collect();
    }

    // circular, refs from root
    {
        test_fixture fixture;
        test_object* a = fixture.newobj('a');
        test_object* b = fixture.newobj('b');

        a->refs(b);
        b->refs(a);
        fixture.isRoot(a);

        fixture.collect();
    }

    // obj1 garbage
    {
        test_fixture fixture;
        test_object* a = fixture.newobj('a');
        test_object* b = fixture.newobj('b');

        a->refs(b);
        a->shouldDie = true;
        fixture.isRoot(b);

        fixture.collect();
    }

    return 0;
}
