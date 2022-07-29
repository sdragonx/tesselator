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

#ifndef LIBTESS_DICT_HPP
#define LIBTESS_DICT_HPP

#include "base.hpp"

namespace libtess {

//
// Dist 排序链表
//

typedef void *DictKey;

typedef int(*DictKeyComp)(void *frame, DictKey key1, DictKey key2);

struct DictNode
{
    DictKey    key;
    DictNode *next;
    DictNode *prev;

    DictNode() : key(), next(), prev() {}
};

class Dict
{
protected:
    //head.next == first node;
    //head.prev == last node
    DictNode head;
    void *frame;
    DictKeyComp comp;

    pool<DictNode, LIBTESS_PAGE_SIZE> poolbuf;

public:
    Dict();
    ~Dict();

    void init(void *_frame, DictKeyComp pfn);
    void dispose();

    DictNode* insert(DictNode *node, DictKey key);
    DictNode* insert(DictKey key) { return this->insert(&head, key); }
    void erase(DictNode *node);
    DictNode* find(DictKey key);
    DictNode* min();
    DictNode* max();

protected:
    DictNode* allocate()
    {
        #ifdef LIBTESS_USE_POOL
        return poolbuf.allocate();
        #else
        return new DictNode;
        #endif
    }

    void deallocate(DictNode* n)
    {
        #ifdef LIBTESS_USE_POOL
        poolbuf.deallocate(n);
        #else
        delete n;
        #endif
    }
};

//
// source
//

// DictKey

LIBTESS_INLINE DictKey dictKey(DictNode* n)
{
    return n->key;
}

// Dict

LIBTESS_INLINE Dict::Dict() : head(), frame(), comp()
{

}

LIBTESS_INLINE Dict::~Dict()
{
    this->dispose();
}

LIBTESS_INLINE void Dict::init(void *_frame, DictKeyComp pfn)
{
    head.key = NULL;
    head.next = &head;
    head.prev = &head;

    frame = _frame;
    comp = pfn;
}

LIBTESS_INLINE void Dict::dispose()
{
    #ifdef LIBTESS_USE_POOL
    poolbuf.dispose();
    #else
    DictNode* node = head.next;
    DictNode* next;
    while (node && node != &head) {
        next = node->next;
        delete node;
        node = next;
    }
    #endif
}

LIBTESS_INLINE DictNode * Dict::insert(DictNode *node, DictKey key)
{
    DictNode *newNode;

    do {
        node = node->prev;
    } while (node->key != NULL && !(*comp)(frame, node->key, key));

    newNode = this->allocate();
    if (newNode == NULL) return NULL;

    newNode->key = key;
    newNode->next = node->next;
    node->next->prev = newNode;
    newNode->prev = node;
    node->next = newNode;

    return newNode;
}

LIBTESS_INLINE void Dict::erase(DictNode *node)
{
    node->next->prev = node->prev;
    node->prev->next = node->next;
    this->deallocate(node);
}

LIBTESS_INLINE DictNode * Dict::find(DictKey key)
{
    DictNode *node = &head;

    do {
        node = node->next;
    } while (node->key != NULL && !(*comp)(frame, key, node->key));

    return node;
}

LIBTESS_INLINE DictNode* Dict::min()
{
    return head.next;
}

LIBTESS_INLINE DictNode* Dict::max()
{
    return head.prev;
}

}// end namespace libtess

#endif// LIBTESS_DICT_HPP
