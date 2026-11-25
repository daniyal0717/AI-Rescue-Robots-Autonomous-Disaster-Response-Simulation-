// containers.h
// Own data structures for the AI Rescue Robots project. Nothing from the STL is used.
// Only C headers (stdio, stdlib, string) are included, for file output and memory.
//
//   Vector<T>      growable array, doubles when full         push O(1) amortised, index O(1)
//   String         growable character buffer                  append O(length)
//   OutBuffer      buffered text file writer                  write O(length)
//   HashMap        int -> int, open addressing, linear probe  put/get O(1) expected
//   MinHeap<T>     binary min-heap (uses operator<)           push/pop O(log n)
//   IndexedHeap    min-heap over items 0..n-1, two keys,      insert/remove/update O(log n)
//                  position array so a key can change in place
//   mergeSort      stable merge sort                          O(n log n)
#define _CRT_SECURE_NO_WARNINGS
#ifndef CONTAINERS_H
#define CONTAINERS_H

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ds {

    // ---------------------------------------------------------------- Vector
    template <typename T>
    class Vector {
        T* data_;
        int size_;
        int cap_;

        void reallocate(int newCap) {
            T* nd = new T[newCap];
            for (int i = 0; i < size_; i++) nd[i] = data_[i];
            delete[] data_;
            data_ = nd;
            cap_ = newCap;
        }
        void copyFrom(const Vector& o) {
            if (o.size_ > 0) {
                data_ = new T[o.size_];
                for (int i = 0; i < o.size_; i++) data_[i] = o.data_[i];
                cap_ = o.size_;
            }
            size_ = o.size_;
        }

    public:
        Vector() : data_(0), size_(0), cap_(0) {}
        Vector(const Vector& o) : data_(0), size_(0), cap_(0) { copyFrom(o); }
        Vector& operator=(const Vector& o) {
            if (this != &o) {
                delete[] data_;
                data_ = 0; size_ = 0; cap_ = 0;
                copyFrom(o);
            }
            return *this;
        }
        ~Vector() { delete[] data_; }

        void push_back(const T& v) {
            T copy = v;  // v may live inside this vector, which reallocation would free
            if (size_ == cap_) reallocate(cap_ == 0 ? 4 : cap_ * 2);
            data_[size_++] = copy;
        }
        void pop_back() { if (size_ > 0) size_--; }
        T& back() { return data_[size_ - 1]; }
        const T& back() const { return data_[size_ - 1]; }

        T& operator[](int i) { return data_[i]; }
        const T& operator[](int i) const { return data_[i]; }

        int size() const { return size_; }
        bool empty() const { return size_ == 0; }
        void clear() { size_ = 0; }

        void reserve(int n) { if (n > cap_) reallocate(n); }

        // make the vector exactly n elements, all equal to value
        void assign(int n, const T& value) {
            T copy = value;
            size_ = 0;
            reserve(n);
            for (int i = 0; i < n; i++) data_[i] = copy;
            size_ = n;
        }
    };

    // ---------------------------------------------------------------- String
    class String {
        char* s_;
        int len_;
        int cap_;

        void ensure(int extra) {
            int need = len_ + extra + 1;
            if (need <= cap_) return;
            int nc = cap_;
            while (nc < need) nc *= 2;
            char* ns = new char[nc];
            memcpy(ns, s_, len_ + 1);
            delete[] s_;
            s_ = ns;
            cap_ = nc;
        }

    public:
        String() : s_(new char[16]), len_(0), cap_(16) { s_[0] = '\0'; }
        String(const char* c) : s_(0), len_(0), cap_(16) {
            int n = (int)strlen(c);
            while (cap_ < n + 1) cap_ *= 2;
            s_ = new char[cap_];
            memcpy(s_, c, n + 1);
            len_ = n;
        }
        String(const String& o) : s_(new char[o.cap_]), len_(o.len_), cap_(o.cap_) {
            memcpy(s_, o.s_, len_ + 1);
        }
        String& operator=(const String& o) {
            if (this != &o) {
                char* ns = new char[o.cap_];
                memcpy(ns, o.s_, o.len_ + 1);
                delete[] s_;
                s_ = ns; len_ = o.len_; cap_ = o.cap_;
            }
            return *this;
        }
        ~String() { delete[] s_; }

        const char* c_str() const { return s_; }
        int length() const { return len_; }
        bool empty() const { return len_ == 0; }
        void clear() { len_ = 0; s_[0] = '\0'; }
        char operator[](int i) const { return s_[i]; }

        String& append(const char* c) {
            int n = (int)strlen(c);
            ensure(n);
            memcpy(s_ + len_, c, n + 1);
            len_ += n;
            return *this;
        }
        String& append(const String& o) { return append(o.s_); }
        String& append(char ch) {
            ensure(1);
            s_[len_++] = ch;
            s_[len_] = '\0';
            return *this;
        }
        String& appendInt(long long v) {
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "%lld", v);
            return append(tmp);
        }
        String& appendDouble(double v, int decimals) {
            char tmp[64];
            snprintf(tmp, sizeof(tmp), "%.*f", decimals, v);
            return append(tmp);
        }

        int compare(const String& o) const { return strcmp(s_, o.s_); }
        bool operator==(const String& o) const { return strcmp(s_, o.s_) == 0; }
        bool operator==(const char* c) const { return strcmp(s_, c) == 0; }
        bool operator!=(const char* c) const { return strcmp(s_, c) != 0; }
        bool operator<(const String& o) const { return strcmp(s_, o.s_) < 0; }
    };

    // ---------------------------------------------------------------- OutBuffer
    // Collects text and writes it to a file in large blocks (C stdio only).
    class OutBuffer {
        FILE* f_;
        String buf_;
        static const int FLUSH_AT = 1 << 16;

        OutBuffer(const OutBuffer&);             // not copyable
        OutBuffer& operator=(const OutBuffer&);

    public:
        OutBuffer() : f_(0) {}
        ~OutBuffer() { close(); }

        bool open(const char* path) {
            close();
            f_ = fopen(path, "wb");
            return f_ != 0;
        }
        bool isOpen() const { return f_ != 0; }

        void write(const String& s) {
            buf_.append(s);
            if (buf_.length() >= FLUSH_AT) flush();
        }
        void write(const char* s) {
            buf_.append(s);
            if (buf_.length() >= FLUSH_AT) flush();
        }
        void flush() {
            if (f_ && buf_.length() > 0) fwrite(buf_.c_str(), 1, buf_.length(), f_);
            buf_.clear();
        }
        void close() {
            if (f_) {
                flush();
                fclose(f_);
                f_ = 0;
            }
        }
    };

    // ---------------------------------------------------------------- HashMap
    // int key -> int value. Open addressing with linear probing. Capacity is a power
    // of two and the table is kept at most half full. No deletion is needed by the
    // simulator, so none is provided.
    class HashMap {
        int* keys_;
        int* vals_;
        unsigned char* used_;
        int cap_;
        int size_;

        static unsigned hashKey(int k) {
            unsigned x = (unsigned)k;
            x ^= x >> 16;
            x *= 0x45d9f3bu;
            x ^= x >> 16;
            x *= 0x45d9f3bu;
            x ^= x >> 16;
            return x;
        }
        void allocate(int cap) {
            cap_ = cap;
            keys_ = new int[cap];
            vals_ = new int[cap];
            used_ = new unsigned char[cap];
            for (int i = 0; i < cap; i++) used_[i] = 0;
        }
        void release() {
            delete[] keys_;
            delete[] vals_;
            delete[] used_;
        }
        int slotFor(int key) const {  // slot holding key, or the empty slot where it belongs
            int i = (int)(hashKey(key) & (unsigned)(cap_ - 1));
            while (used_[i] && keys_[i] != key) i = (i + 1) & (cap_ - 1);
            return i;
        }
        void rehash(int newCap) {
            int* ok = keys_;
            int* ov = vals_;
            unsigned char* ou = used_;
            int oc = cap_;
            allocate(newCap);
            for (int i = 0; i < oc; i++) {
                if (ou[i]) {
                    int s = slotFor(ok[i]);
                    used_[s] = 1;
                    keys_[s] = ok[i];
                    vals_[s] = ov[i];
                }
            }
            delete[] ok;
            delete[] ov;
            delete[] ou;
        }

    public:
        HashMap() : keys_(0), vals_(0), used_(0), cap_(0), size_(0) { allocate(16); }
        HashMap(const HashMap& o) : keys_(0), vals_(0), used_(0), cap_(0), size_(o.size_) {
            allocate(o.cap_);
            for (int i = 0; i < cap_; i++) {
                used_[i] = o.used_[i];
                keys_[i] = o.keys_[i];
                vals_[i] = o.vals_[i];
            }
        }
        HashMap& operator=(const HashMap& o) {
            if (this != &o) {
                release();
                size_ = o.size_;
                allocate(o.cap_);
                for (int i = 0; i < cap_; i++) {
                    used_[i] = o.used_[i];
                    keys_[i] = o.keys_[i];
                    vals_[i] = o.vals_[i];
                }
            }
            return *this;
        }
        ~HashMap() { release(); }

        // returns true if the key was new, false if an existing value was overwritten
        bool put(int key, int value) {
            if ((size_ + 1) * 2 > cap_) rehash(cap_ * 2);
            int s = slotFor(key);
            if (used_[s]) {
                vals_[s] = value;
                return false;
            }
            used_[s] = 1;
            keys_[s] = key;
            vals_[s] = value;
            size_++;
            return true;
        }
        bool get(int key, int& out) const {
            int s = slotFor(key);
            if (!used_[s]) return false;
            out = vals_[s];
            return true;
        }
        bool contains(int key) const { return used_[slotFor(key)] != 0; }
        int size() const { return size_; }
    };

    // ---------------------------------------------------------------- MinHeap
    // Binary min-heap. T must be copyable and provide operator<.
    template <typename T>
    class MinHeap {
        Vector<T> a_;

        void siftUp(int i) {
            T x = a_[i];
            while (i > 0) {
                int p = (i - 1) / 2;
                if (!(x < a_[p])) break;
                a_[i] = a_[p];
                i = p;
            }
            a_[i] = x;
        }
        void siftDown(int i) {
            int n = a_.size();
            T x = a_[i];
            while (true) {
                int c = 2 * i + 1;
                if (c >= n) break;
                if (c + 1 < n && a_[c + 1] < a_[c]) c++;
                if (!(a_[c] < x)) break;
                a_[i] = a_[c];
                i = c;
            }
            a_[i] = x;
        }

    public:
        void push(const T& v) {
            a_.push_back(v);
            siftUp(a_.size() - 1);
        }
        const T& top() const { return a_[0]; }
        void pop() {
            int n = a_.size();
            if (n == 0) return;
            a_[0] = a_[n - 1];
            a_.pop_back();
            if (a_.size() > 0) siftDown(0);
        }
        bool empty() const { return a_.empty(); }
        int size() const { return a_.size(); }
        void clear() { a_.clear(); }
    };

    // ---------------------------------------------------------------- IndexedHeap
    // Min-heap over items numbered 0..n-1. Each item has two keys; the smaller
    // (key1, key2) pair is served first. pos_[item] tells where the item sits in the
    // heap, so an item can be removed or have its keys changed in O(log n).
    class IndexedHeap {
        Vector<int> heap_;       // heap_[i] = item stored at heap position i
        Vector<int> pos_;        // pos_[item] = position in heap_, or -1 if absent
        Vector<long long> k1_;
        Vector<long long> k2_;

        bool before(int a, int b) const {
            if (k1_[a] != k1_[b]) return k1_[a] < k1_[b];
            return k2_[a] < k2_[b];
        }
        void place(int item, int position) {
            heap_[position] = item;
            pos_[item] = position;
        }
        void siftUp(int i) {
            int item = heap_[i];
            while (i > 0) {
                int p = (i - 1) / 2;
                if (!before(item, heap_[p])) break;
                place(heap_[p], i);
                i = p;
            }
            place(item, i);
        }
        void siftDown(int i) {
            int n = heap_.size();
            int item = heap_[i];
            while (true) {
                int c = 2 * i + 1;
                if (c >= n) break;
                if (c + 1 < n && before(heap_[c + 1], heap_[c])) c++;
                if (!before(heap_[c], item)) break;
                place(heap_[c], i);
                i = c;
            }
            place(item, i);
        }

    public:
        // prepare for items 0..n-1, all absent
        void init(int n) {
            heap_.clear();
            pos_.assign(n, -1);
            k1_.assign(n, 0);
            k2_.assign(n, 0);
        }
        bool contains(int item) const { return item >= 0 && item < pos_.size() && pos_[item] >= 0; }
        int size() const { return heap_.size(); }
        bool empty() const { return heap_.empty(); }
        long long key1(int item) const { return k1_[item]; }
        long long key2(int item) const { return k2_[item]; }

        void insert(int item, long long k1, long long k2) {
            if (contains(item)) { update(item, k1, k2); return; }
            k1_[item] = k1;
            k2_[item] = k2;
            heap_.push_back(item);
            pos_[item] = heap_.size() - 1;
            siftUp(heap_.size() - 1);
        }
        void update(int item, long long k1, long long k2) {
            if (!contains(item)) return;
            k1_[item] = k1;
            k2_[item] = k2;
            int i = pos_[item];
            siftUp(i);
            siftDown(pos_[item]);
        }
        void remove(int item) {
            if (!contains(item)) return;
            int i = pos_[item];
            int last = heap_.back();
            heap_.pop_back();
            pos_[item] = -1;
            if (last == item) return;       // it was the last element
            heap_[i] = last;
            pos_[last] = i;
            siftUp(i);
            siftDown(pos_[last]);
        }
        int top() const { return heap_[0]; }
        int pop() {
            int t = heap_[0];
            remove(t);
            return t;
        }

        // All items in priority order, without changing this heap. O(n log n).
        void sortedItems(Vector<int>& out) const {
            out.clear();
            IndexedHeap copy(*this);
            while (!copy.empty()) out.push_back(copy.pop());
        }
    };

    // ---------------------------------------------------------------- merge sort
    // Stable. 'less' is a function object or function pointer: less(a, b) is true when a goes first.
    template <typename T, typename Less>
    void mergeSortRange(Vector<T>& v, Vector<T>& tmp, int lo, int hi, Less less) {
        if (hi - lo < 2) return;
        int mid = lo + (hi - lo) / 2;
        mergeSortRange(v, tmp, lo, mid, less);
        mergeSortRange(v, tmp, mid, hi, less);
        int i = lo, j = mid, k = lo;
        while (i < mid && j < hi) {
            if (less(v[j], v[i])) tmp[k++] = v[j++];   // take from the left on ties: stable
            else tmp[k++] = v[i++];
        }
        while (i < mid) tmp[k++] = v[i++];
        while (j < hi) tmp[k++] = v[j++];
        for (int x = lo; x < hi; x++) v[x] = tmp[x];
    }

    template <typename T, typename Less>
    void mergeSort(Vector<T>& v, Less less) {
        int n = v.size();
        if (n < 2) return;
        Vector<T> tmp;
        tmp.assign(n, v[0]);
        mergeSortRange(v, tmp, 0, n, less);
    }

}  // namespace ds

#endif