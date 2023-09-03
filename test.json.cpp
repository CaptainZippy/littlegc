#include <cassert>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "littlegc.h"

namespace ns_json {
struct json_value;

// Json values which involve dynamic allocations - strings, objects, arrays
struct json_compound {
    enum class CompoundKind { invalid = 0, string, object, array };
    lgc_object_t gc_obj;
    CompoundKind kind;

    json_compound(CompoundKind k) : kind(k) {}

    static json_compound* from_gcobj(lgc_object_t* obj) {
        constexpr size_t off = offsetof(json_compound, gc_obj);
        auto addr = reinterpret_cast<size_t>(obj) - off;
        return reinterpret_cast<json_compound*>(addr);
    }

    void destroy();
    json_value toValue();
};

struct json_string : json_compound {
    json_string() : json_compound(CompoundKind::string) {}
    std::string str;
};

struct json_object : json_compound {
    json_object() : json_compound(CompoundKind::object) {}
    std::unordered_map<std::string, json_value> pairs;
};

struct json_array : json_compound {
    json_array() : json_compound(CompoundKind::array) {}
    std::vector<json_value> items;
};

// Json values such as numbers/bools/null can be stored without any allocations.
// strings, objects, arrays point to an external json_compound object.
struct json_value {
    enum class ValueKind { invalid = 0, compound, number, boolean, null };
    union Value {
        json_compound* c;
        double d;
        bool b;
    };
    ValueKind kind;
    Value value;
};

json_value json_compound::toValue() {
    return {json_value::ValueKind::compound, this};
}

void json_compound::destroy() {
    // or we could add a vtable
    switch (kind) {
        case CompoundKind::string: {
            delete (json_string*)this;
            break;
        }
        case CompoundKind::object: {
            delete (json_object*)this;
            break;
        }
        case CompoundKind::array: {
            delete (json_array*)this;
            break;
        }
        default: {
            assert(false);
        }
    }
}

struct test_json_fixture {
    test_json_fixture() { lgc_init(&gc, json_scan, json_free); }

    static void json_scan(lgc_t* gc, lgc_object_t* obj, lgc_alive_func alive_func) {
        if (obj) {
            json_compound* o = json_compound::from_gcobj(obj);
            switch (o->kind) {
                case json_compound::CompoundKind::array: {
                    json_array* a = static_cast<json_array*>(o);
                    for (auto&& i : a->items) {
                        if (i.kind == json_value::ValueKind::compound) {
                            alive_func(gc, &i.value.c->gc_obj);
                        }
                    }
                    break;
                }
                case json_compound::CompoundKind::object: {
                    json_object* a = static_cast<json_object*>(o);
                    for (auto&& i : a->pairs) {
                        if (i.second.kind == json_value::ValueKind::compound) {
                            alive_func(gc, &i.second.value.c->gc_obj);
                        }
                    }
                    break;
                }
                case json_compound::CompoundKind::string: {
                    break;
                }
                default: {
                    assert(false);
                }
            }
        } else {
            test_json_fixture* self = test_json_fixture::from_gc(gc);
            for (auto&& i : self->roots) {
                alive_func(gc, &i->gc_obj);
            }
        }
    }

    json_array* newArray() {
        auto* a = new json_array();
        lgc_register(&gc, &a->gc_obj);
        return a;
    }
    json_string* newString(const char* s) {
        auto* a = new json_string();
        a->str = s;
        lgc_register(&gc, &a->gc_obj);
        return a;
    }

    static void json_free(lgc_t* gc, lgc_object_t* obj) {
        auto c = json_compound::from_gcobj(obj);
        auto fixture = test_json_fixture::from_gc(gc);
        auto it = fixture->shouldBeGarbage.find(c);
        assert(it != fixture->shouldBeGarbage.end());
        fixture->shouldBeGarbage.erase(it);
        c->destroy();
    }

    void collect() {
        lgc_collect(&gc);
        assert(shouldBeGarbage.size() == 0);
    }

    static test_json_fixture* from_gc(lgc_t* gc) {
        constexpr size_t off = offsetof(test_json_fixture, gc);
        auto addr = reinterpret_cast<size_t>(gc) - off;
        return reinterpret_cast<test_json_fixture*>(addr);
    }

    void addRoot(json_compound* c) { roots.emplace_back(c); }

    void addGarbage(json_compound* c) { shouldBeGarbage.emplace(c); }

    lgc_t gc;
    std::vector<json_compound*> roots;
    std::unordered_set<json_compound*> shouldBeGarbage;
};
}  // namespace ns_json

int test_json() {
    using namespace ns_json;
    {
        test_json_fixture fixture;
        auto arr = fixture.newArray();
        auto hello = fixture.newString("hello");
        arr->items.push_back(hello->toValue());

        fixture.addRoot(arr);
        fixture.collect();

        auto bye = fixture.newString("goodbye");
        fixture.addGarbage(bye);
        fixture.collect();

        fixture.addGarbage(hello);
        arr->items.pop_back();
        fixture.collect();
    }

    return 0;
}
