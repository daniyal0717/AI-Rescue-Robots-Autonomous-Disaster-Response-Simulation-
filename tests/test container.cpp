// Container tests. Build and run from the project root:
//   g++ -std=c++11 -Wall -Wextra -O2 -Isrc tests/test_containers.cpp -o test_containers && ./test_containers
#define _CRT_SECURE_NO_WARNINGS
#include "container.h"
using namespace ds;

static int g_checks = 0, g_failed = 0;
#define CHECK(cond) do { g_checks++; if (!(cond)) { g_failed++; \
    printf("FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

static unsigned long long g_seed = 12345;
static int rnd(int n) {  // small deterministic generator
    g_seed = g_seed * 6364136223846793005ULL + 1442695040888963407ULL;
    return (int)((g_seed >> 33) % (unsigned long long)n);
}

struct Item { int key, order; bool operator<(const Item& o) const { return key < o.key; } };
struct ByKey { bool operator()(const Item& a, const Item& b) const { return a.key < b.key; } };

static void testVector() {
    Vector<int> v;
    for (int i = 0; i < 1000; i++) v.push_back(i);
    CHECK(v.size() == 1000);
    CHECK(v[0] == 0 && v[999] == 999);
    v.push_back(v[5]);                       // push an element of itself across a growth
    CHECK(v[1000] == 5);
    Vector<int> c(v);
    v[0] = -1;
    CHECK(c[0] == 0 && c.size() == 1001);    // deep copy
    Vector<int> d;
    d = c;
    CHECK(d.size() == 1001 && d[1000] == 5);
    d.assign(7, 3);
    CHECK(d.size() == 7 && d[6] == 3);
    d.clear();
    CHECK(d.empty());
    Vector<Vector<int> > nested;             // used for adjacency lists
    nested.assign(5, Vector<int>());
    nested[2].push_back(9);
    nested[2].push_back(8);
    CHECK(nested[2].size() == 2 && nested[1].size() == 0);
}

static void testString() {
    String s;
    s.append("step=").appendInt(42).append(',').appendDouble(3.14159, 2);
    CHECK(s == "step=42,3.14");
    String big;
    for (int i = 0; i < 1000; i++) big.append("ab");
    CHECK(big.length() == 2000);
    String t = s;
    t.append("x");
    CHECK(s == "step=42,3.14" && t == "step=42,3.14x");
    CHECK(String("a") < String("b"));
}

static void testHashMap() {
    HashMap m;
    for (int i = 0; i < 20000; i++) CHECK(m.put(i * 7 - 5000, i));   // sparse and negative keys
    CHECK(m.size() == 20000);
    int out = -1;
    bool allFound = true;
    for (int i = 0; i < 20000; i++) {
        if (!m.get(i * 7 - 5000, out) || out != i) allFound = false;
    }
    CHECK(allFound);
    CHECK(!m.contains(1));
    CHECK(!m.put(7 - 5000, 99));             // overwrite, not a new key
    m.get(7 - 5000, out);
    CHECK(out == 99 && m.size() == 20000);
    HashMap copy(m);
    copy.put(123456789, 1);
    CHECK(!m.contains(123456789) && copy.contains(123456789));
    // many keys that collide in the low bits
    HashMap h;
    for (int i = 0; i < 2000; i++) h.put(i * 1024, i);
    bool ok = true;
    for (int i = 0; i < 2000; i++) if (!h.get(i * 1024, out) || out != i) ok = false;
    CHECK(ok);
}

static void testMinHeap() {
    MinHeap<Item> h;
    Vector<int> keys;
    for (int i = 0; i < 5000; i++) {
        Item it;
        it.key = rnd(100000);
        it.order = i;
        h.push(it);
        keys.push_back(it.key);
    }
    int prev = -1;
    bool sorted = true;
    int count = 0;
    while (!h.empty()) {
        if (h.top().key < prev) sorted = false;
        prev = h.top().key;
        h.pop();
        count++;
    }
    CHECK(sorted && count == 5000);
}

static void testIndexedHeap() {
    const int N = 300;
    IndexedHeap h;
    h.init(N);
    long long k1[N], k2[N];
    bool in[N];
    for (int i = 0; i < N; i++) {
        k1[i] = rnd(50); k2[i] = i; in[i] = true;
        h.insert(i, k1[i], k2[i]);
    }
    CHECK(h.size() == N);
    // change keys and remove some items
    for (int r = 0; r < 400; r++) {
        int item = rnd(N);
        if (!in[item]) continue;
        if (r % 3 == 0) { h.remove(item); in[item] = false; }
        else { k1[item] = rnd(50); h.update(item, k1[item], k2[item]); }
    }
    int remaining = 0;
    for (int i = 0; i < N; i++) if (in[i]) remaining++;
    CHECK(h.size() == remaining);
    Vector<int> order;
    h.sortedItems(order);
    CHECK(order.size() == remaining);
    CHECK(h.size() == remaining);            // sortedItems must not change the heap
    bool good = true;
    for (int i = 1; i < order.size(); i++) {
        int a = order[i - 1], b = order[i];
        if (k1[a] > k1[b] || (k1[a] == k1[b] && k2[a] > k2[b])) good = false;
    }
    CHECK(good);
    // pop everything in order and check membership flags
    int popped = 0;
    while (!h.empty()) {
        int t = h.pop();
        CHECK(in[t] && !h.contains(t));
        popped++;
    }
    CHECK(popped == remaining);
    h.insert(5, 1, 1);                       // reuse after emptying
    h.insert(5, 0, 1);                       // insert of an existing item acts as update
    CHECK(h.size() == 1 && h.key1(5) == 0);
}

static void testMergeSort() {
    Vector<Item> v;
    for (int i = 0; i < 2000; i++) {
        Item it;
        it.key = rnd(20);                    // many equal keys to test stability
        it.order = i;
        v.push_back(it);
    }
    mergeSort(v, ByKey());
    bool good = true;
    for (int i = 1; i < v.size(); i++) {
        if (v[i - 1].key > v[i].key) good = false;
        if (v[i - 1].key == v[i].key && v[i - 1].order > v[i].order) good = false;
    }
    CHECK(good);
    Vector<Item> one;
    mergeSort(one, ByKey());                 // empty and single element must not crash
    Item x; x.key = 1; x.order = 0;
    one.push_back(x);
    mergeSort(one, ByKey());
    CHECK(one.size() == 1);
}

static void testOutBuffer() {
    OutBuffer ob;
    CHECK(ob.open("test_out.tmp"));
    for (int i = 0; i < 20000; i++) {        // large enough to trigger several flushes
        String line;
        line.appendInt(i).append(",line\n");
        ob.write(line);
    }
    ob.close();
    FILE* f = fopen("test_out.tmp", "rb");
    CHECK(f != 0);
    int lines = 0, ch;
    while ((ch = fgetc(f)) != EOF) if (ch == '\n') lines++;
    fclose(f);
    remove("test_out.tmp");
    CHECK(lines == 20000);
}

int main() {
    testVector();
    testString();
    testHashMap();
    testMinHeap();
    testIndexedHeap();
    testMergeSort();
    testOutBuffer();
    printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}