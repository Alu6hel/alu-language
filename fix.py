import sys
text = open('std/collections.alu').read()
rest = text[text.find('struct HashEntry<K, V>'):]

new_content = """// ALU Native Standard Library: collections
// Provides generic dynamic Vectors and HashMaps.

import "std/mem.alu";
import "std/string.alu";

// ==========================================
// Vector<T>
// Dynamic array of elements of type T
// ==========================================

struct Vector<T> {
    ptr<T> data;
    int length;
    int capacity;
}

routine vec_new<T>() -> ptr<Vector<T> > {
    ptr<Vector<T> > v = new Vector<T>;
    v.capacity = 16;
    v.length = 0;
    // Allocate max possible size per element (8 bytes) to accommodate any Alu type (pointer or primitive).
    v.data = alu_alloc(16 * 8); 
    return v;
}

routine vec_push<T>(ptr<Vector<T> > v, T item) -> void {
    if (v.length == v.capacity) {
        v.capacity = v.capacity * 2;
        v.data = alu_realloc(v.data, v.capacity * 8);
    }
    ptr<T> d = v.data;
    d[v.length] = item;
    v.length = v.length + 1;
}

routine vec_get<T>(ptr<Vector<T> > v, int index) -> T {
    ptr<T> d = v.data;
    return d[index];
}

routine vec_set<T>(ptr<Vector<T> > v, int index, T item) -> void {
    ptr<T> d = v.data;
    d[index] = item;
}

routine vec_len<T>(ptr<Vector<T> > v) -> int {
    return v.length;
}

// ==========================================
// HashMap<K, V>
// Simple Hash Map implementation
// ==========================================

""" + rest

with open('std/collections.alu', 'w') as f:
    f.write(new_content)
