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

#ifndef LIBTESS_SWEEP_HPP
#define LIBTESS_SWEEP_HPP

#include "base.hpp"
#include "dict.hpp"
#include "mesh.hpp"

namespace libtess {

/* The following is here *only* for access by debugging routines */

/* For each pair of adjacent edges crossing the sweep line, there is
* an ActiveRegion to represent the region between them.  The active
* regions are kept in sorted order in a dynamic dictionary.  As the
* sweep line crosses each vertex, we update the affected regions.
*/

struct ActiveRegion
{
    HalfEdge *eUp;      /* upper edge, directed right to left */
    DictNode *nodeUp;   /* dictionary node corresponding to eUp. 与 eUp 对应的 dict 节点 */
    int windingNumber;  /* used to determine which regions are inside the polygon */
    Bool inside;        /* is this region inside the polygon? */
    Bool sentinel;      /* marks fake edges at t = +/-infinity */
    Bool dirty;         /* marks regions where the upper or lower
                         * edge has changed, but we haven't checked
                         * whether they intersect yet */
    Bool fixUpperEdge;  /* marks temporary edges introduced when
                         * we process a "right vertex" (one without
                         * any edges leaving to the right) */
};

// 下一个面
LIBTESS_INLINE ActiveRegion* RegionBelow(ActiveRegion* r)
{
    return (ActiveRegion *) ((r)->nodeUp)->prev->key;
}

// 上一个面
LIBTESS_INLINE ActiveRegion* RegionAbove(ActiveRegion* r)
{
    return (ActiveRegion *) ((r)->nodeUp)->next->key;
}

//#define LIBTESS_USE_PriorityQ

class Sweep
{
protected:
    int windingRule;      /* rule for determining polygon interior */
    Dict dict;            /* edge dictionary for sweep line */
    vertex_stack pq;      /* priority queue of vertex events */
    Vertex *currentEvent; /* current sweep event being processed */

    pool<ActiveRegion, LIBTESS_PAGE_SIZE> regionbuf;

public:
    //jmp_buf env;          /* place to jump to when memAllocs fail */

public:
    Sweep();
    int init(int rule);
    void dispose();

    int ComputeInterior(Mesh& mesh, const AABB& aabb);

private:
    ActiveRegion* allocate();
    void deallocate(ActiveRegion*);


    int IsWindingInside(int n);
    void ComputeWinding(ActiveRegion *reg);

    void InitEdgeDict(Mesh& mesh, const AABB& aabb);
    void DoneEdgeDict();

    int InitPriorityQ(Mesh& mesh);
    void DonePriorityQ();

    void AddSentinel(Mesh& mesh, Float smin, Float smax, Float t);
    ActiveRegion* AddRegionBelow(ActiveRegion *regAbove, HalfEdge *eNewUp);
    void AddRightEdges(Mesh& mesh, ActiveRegion *regUp, HalfEdge *eFirst, HalfEdge *eLast, HalfEdge *eTopLeft, int cleanUp);
    void DeleteRegion(ActiveRegion* r);
    void FinishRegion(ActiveRegion* r);
    HalfEdge* FinishLeftRegions(Mesh& mesh, ActiveRegion *regFirst, ActiveRegion *regLast);

    int CheckForLeftSplice(Mesh& mesh, ActiveRegion *regUp);
    int CheckForRightSplice(Mesh& mesh, ActiveRegion *regUp);

    int CheckForIntersect(Mesh& mesh, ActiveRegion *regUp);
    void GetIntersectData(Vertex *isect, Vertex *orgUp, Vertex *dstUp, Vertex *orgLo, Vertex *dstLo);

    void WalkDirtyRegions(Mesh& mesh, ActiveRegion *regUp);

    void ConnectLeftDegenerate(Mesh& mesh, ActiveRegion *regUp, Vertex *vEvent);
    void ConnectLeftVertex(Mesh& mesh, Vertex *vEvent);
    void ConnectRightVertex(Mesh& mesh, ActiveRegion *regUp, HalfEdge *eBottomLeft);

    void SweepEvent(Mesh& mesh, Vertex *vEvent);

    void RemoveDegenerateEdges(Mesh& mesh);
    bool RemoveDegenerateFaces(Mesh& mesh);

    static int EdgeLeq(Sweep* sweep, ActiveRegion *reg1, ActiveRegion *reg2);
};

//
// source
//

LIBTESS_INLINE Sweep::Sweep()
{
    windingRule = TESS_WINDING_ODD;
    currentEvent = NULL;
}

LIBTESS_INLINE int Sweep::init(int value)
{
    windingRule = value;
    currentEvent = NULL;
    return LIBTESS_OK;
}

LIBTESS_INLINE void Sweep::dispose()
{
    dict.dispose();
    pq.clear();
    regionbuf.dispose();
    currentEvent = NULL;
}

LIBTESS_INLINE ActiveRegion* Sweep::allocate()
{
    #ifdef LIBTESS_USE_POOL
    return regionbuf.allocate();
    #else
    return new ActiveRegion;
    #endif
}

LIBTESS_INLINE void Sweep::deallocate(ActiveRegion* r)
{
    #ifdef LIBTESS_USE_POOL
    return regionbuf.deallocate(r);
    #else
    delete r;
    #endif
}

}// end namespace libtess

#include "sweep.inl"

#endif// LIBTESS_SWEEP_HPP
