/*
 * SGI FREE SOFTWARE LICENSE B (Version 2.0, Sept. 18, 2008)
 * Copyright (C) 1991-2000 Silicon Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice including the dates of first publication and
 * either this permission notice or a reference to
 * http://oss.sgi.com/projects/FreeB/
 * shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * SILICON GRAPHICS, INC. BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of Silicon Graphics, Inc.
 * shall not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization from
 * Silicon Graphics, Inc.
 */
/*
** Author: Eric Veach, July 1994.
**
*/

#ifndef LIBTESS_PUBLIC_H
#define LIBTESS_PUBLIC_H

//#include <assert.h>
//#include <setjmp.h>
//#include <stddef.h>

#include <algorithm>
#include <cassert>
#include <set>
#include <stack>
#include <stdint.h>
#include <vector>

#ifdef _MSC_VER
#undef min
#undef max
#endif

// Config

// whether to calculate
//#define LIBTESS_COMPUTE_NORMAL
//#define TRUE_PROJECT  //error

// use Vec3
#if !defined(LIBTESS_USE_VEC2) && !defined(LIBTESS_USE_VEC3)
    #define LIBTESS_USE_VEC3
#endif

// whether to use pool allocator
#define LIBTESS_USE_POOL

// default pool buffer size
#ifndef LIBTESS_PAGE_SIZE
    #define LIBTESS_PAGE_SIZE 256
#endif

// log
#ifdef CGL_PUBLIC_H
    #define LIBTESS_LOG CGL_LOG
#else
    #include <stdio.h>
    #define LIBTESS_LOG printf
#endif

// unit test
#if defined(LIBTESS_USE_UNIT_TEST) && defined(CGL_PUBLIC_H)
#   define LIBTESS_UNIT_TEST(t) CGL_UNIT_TEST(t)
#else
#   define LIBTESS_UNIT_TEST(t) t
#endif

#ifndef LIBTESS_INLINE
#define LIBTESS_INLINE inline
#define LIBTESS_STATIC static
#endif

namespace libtess {

//
// types
//

// precision
#ifdef LIBTESS_HIGH_PRECISION
    typedef double Float;
#else
    typedef float Float;
#endif
typedef int   Index;
typedef char  Bool;

class Tesselator;

// See OpenGL Red Book for description of the winding rules
// http://www.glprogramming.com/red/chapter11.html
enum TessWindingRule
{
    TESS_WINDING_ODD,           // 环绕数为奇数
    TESS_WINDING_NONZERO,       // 环绕数为非 0 数
    TESS_WINDING_POSITIVE,      // 环绕数是正数
    TESS_WINDING_NEGATIVE,      // 环绕数是负数
    TESS_WINDING_ABS_GEQ_TWO,   // 环绕数绝对值 >= 2
};


enum TessElementType
{
    TESS_TRIANGLES,
    TESS_BOUNDARY_CONTOURS,

//    TESS_POLYGONS,
//    TESS_CONNECTED_POLYGONS,
};

// error code
enum
{
    LIBTESS_OK,
    LIBTESS_ERROR = -1
};


// custom vector
/*
exsample:

#define LIBTESS_CUSTOM_VECTOR
namespace libtess{
    typedef glm::vec2 Vec2;
    typedef glm::vec3 Vec3;
}
*/
#ifndef LIBTESS_CUSTOM_VECTOR

struct Vec2
{
    Float x, y;
};

struct Vec3
{
    Float x, y, z;

    Vec3() : x(), y(), z() {}
    Vec3(Float vx, Float vy, Float vz) : x(vx), y(vy), z(vz) {}

    Float& operator[](int i)
    {
        return (&x)[i];
    }
};

#endif

struct AABB
{
    Float amin, amax;
    Float bmin, bmax;

    AABB() : amin(FLT_MAX), amax(FLT_MIN), bmin(FLT_MAX), bmax(FLT_MIN) {}
};

enum
{
    INVALID_INDEX = (~(Index) 0)
};

#ifndef TRUE
    #define TRUE 1
#endif
#ifndef FALSE
    #define FALSE 0
#endif

LIBTESS_INLINE Float Abs(Float x)
{
    return x < 0 ? -x : x;
}

LIBTESS_INLINE bool IsEqual(Float a, Float b)
{
    using namespace std;
    return fabs(b - a) < 0.000001f;
}

LIBTESS_INLINE Float Dot(Vec3& u, Vec3& v)
{
    return u.x * v.x + u.y * v.y + u.z * v.z;
}

LIBTESS_INLINE Vec3 Cross(Vec3& u, Vec3& v)
{
    return Vec3(
        u.y * v.z - u.z * v.y,
        u.z * v.x - u.x * v.z,
        u.x * v.y - u.y * v.x);
}

#if defined(FOR_TRITE_TEST_PROGRAM) || defined(TRUE_PROJECT)
void Normalize(Vec3& v)
{
    Float len = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];

    assert(len > 0);
    len = sqrt(len);
    v[0] /= len;
    v[1] /= len;
    v[2] /= len;
}
#endif

LIBTESS_INLINE int LongAxis(Vec3& v)
{
    int i = 0;

    if (Abs(v[1]) > Abs(v[0])) { i = 1; }
    if (Abs(v[2]) > Abs(v[i])) { i = 2; }
    return i;
}

LIBTESS_INLINE int ShortAxis(Vec3& v)
{
    int i = 0;

    if (Abs(v[1]) < Abs(v[0])) { i = 1; }
    if (Abs(v[2]) < Abs(v[i])) { i = 2; }
    return i;
}

//
// memory pool
//

template <typename T, size_t PageSize>
class pool
{
public:
    typedef pool              this_type;
    typedef T                 value_type;
    typedef value_type*       pointer;
    typedef const value_type* const_pointer;
    typedef value_type&       reference;
    typedef const value_type& const_reference;
    typedef std::size_t       size_type;
    typedef std::ptrdiff_t    difference_type;

    const static size_t PAGE_SIZE = PageSize;

private:
    struct node
    {
        node* next;
        value_type value;

        node() : value() {}
    };

    typedef node *page_type;

    node* entry;    // free space entry
    std::vector<page_type> poolbuf;

public:
    pool() : entry()
    {
    }

    pool(const this_type&) : entry()
    {
    }

    ~pool()
    {
        this->dispose();
    }

    this_type& operator=(const this_type&)
    {
        return *this;
    }

    bool operator==(const this_type&)
    {
        return true;
    }

    bool operator!=(const this_type&)
    {
        return false;
    }

    pointer allocate(size_type size = 1, const_pointer = 0)
    {
        assert(size == 1);
        if (!entry) {
            node* buf = allocate_buffer();
            entry = buf;
        }
        node* n = entry;
        entry = entry->next;
        return this->address(n);
    }

    pointer allocate(size_type n, size_type alignment, const_pointer = 0)
    {
        assert(false);
        return nullptr;
    }

    void deallocate(pointer p, size_type = 0)
    {
        node * n = node_pointer(p);
        n->next = entry;
        entry = n;
    }

    pointer reallocate(pointer ptr, size_type n)
    {
        assert(false);
        return nullptr;
    }

    void clear()
    {
        if (poolbuf.empty()) {
            return;
        }

        entry = nullptr;
        for (size_type i = 0; i < poolbuf.size(); ++i) {
            page_type page = poolbuf[i];
            for (size_type j = 0; j < PAGE_SIZE; ++j) {
                page[j] = page + j + 1;
            }
            page[PAGE_SIZE - 1].next = entry;
            entry = page[0];
        }
    }

    void dispose()
    {
        for (size_type i = 0; i < poolbuf.size(); ++i) {
            deallocate_buffer(poolbuf[i]);
        }
        poolbuf.clear();
        entry = nullptr;
    }

    size_type max_size()const
    {
        return static_cast<size_type>(-1) / sizeof(T);
    }

    size_type size()const
    {
        return poolbuf.size() * PAGE_SIZE;
    }

    // 统计自由空间大小
    size_type free_size()const
    {
        size_type n = 0;
        node* p = entry;
        while (p) {
            ++n;
            p = p->next;
        }
        return n;
    }

    void construct(pointer p, const value_type& x)
    {
        new(p) value_type(x);
    }

    void construct(pointer p)
    {
        new (p) value_type();
    }

    void destroy(pointer p)
    {
        p->~T();
    }

private:
    page_type allocate_buffer()
    {
        page_type page = new node[PageSize];
        //init page
        for (size_type i = 0; i < PageSize; ++i) {
            page[i].next = page + i + 1;
        }
        page[PageSize - 1].next = nullptr;
        poolbuf.push_back(page);
        return page;
    }

    void deallocate_buffer(page_type &page)
    {
        delete[]page;
        page = nullptr;
    }

    pointer address(node* n)
    {
        return &n->value;
    }

    node* node_pointer(void* p)
    {
        return reinterpret_cast<node*>(reinterpret_cast<uint8_t*>(p) - sizeof(node*));
    }
};

//
// PriorityQueue
//

struct Vertex;
bool VertexLessEqual(Vertex* u, Vertex* v);

struct VertexLEQ
{
    bool operator()(Vertex* u, Vertex* v)const
    {
        return VertexLessEqual(u, v);
    }
};

class vertex_stack
{
protected:
    std::set<Vertex*, VertexLEQ> heap;

public:
    bool empty()const
    {
        return heap.empty();
    }

    size_t size()const
    {
        return heap.size();
    }

    void insert(Vertex* v)
    {
        if (heap.find(v) == heap.end()) {
            heap.insert(v);
        }
        else {
            LIBTESS_LOG("vertex_stack.insert() : Vertex is exists.");
        }
    }

    void erase(Vertex* v)
    {
        if (heap.find(v) != heap.end()) {
            heap.erase(v);
        }
        else {
            LIBTESS_LOG("vertex_stack.erase() : Vertex is not exists.");
        }
    }

    void clear()
    {
        heap.clear();
    }

    Vertex* find(Vertex* v)
    {
        if (heap.find(v) != heap.end()) {
            return v;
        }
        else {
            return nullptr;
        }
    }

    Vertex* top()
    {
        if (heap.empty()) {
            return nullptr;
        }
        return *heap.begin();
    }

    Vertex* pop()
    {
        if (heap.empty()) {
            return nullptr;
        }
        Vertex *v = *heap.begin();
        heap.erase(heap.begin());
        return v;
    }
};

}// end namespace libtess

#endif// LIBTESS_PUBLIC_H
