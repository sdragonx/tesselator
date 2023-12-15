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

#include "dict.hpp"
#include "geometry.hpp"
#include "mesh.hpp"
#include "mono.hpp"
#include "sweep.hpp"

namespace libtess {

#ifdef FOR_TRITE_TEST_PROGRAM
extern void DebugEvent(TESStesselator *tess);
#else
#define DebugEvent(tess)
#endif

/* longjmp ת�� C++ ���쳣
 */
#ifdef NDEBUG
    //#define LIBTESS_LONGJMP(m) if (m) { longjmp(env, 1) }
    #define LIBTESS_LONGJMP(m) if (m) { throw; }
#else
    //#define LIBTESS_LONGJMP(m) if(m) { LIBTESS_LOG("%s %i, %s", __FILE__, __LINE__, #m); longjmp(env, 1); }
    #define LIBTESS_LONGJMP(m) if(m) { LIBTESS_LOG("%s %i, %s", __FILE__, __LINE__, #m); throw; }
#endif

/*
 * Invariants for the Edge Dictionary.
 * - each pair of adjacent edges e2=Succ(e1) satisfies EdgeLeq(e1,e2)
 *   at any valid location of the sweep event
 * - if EdgeLeq(e2,e1) as well (at any valid sweep event), then e1 and e2
 *   share a common endpoint
 * - for each e, e->Dst has been processed, but not e->Org
 * - each edge e satisfies VertLeq(e->Dst,event) && VertLeq(event,e->Org)
 *   where "event" is the current sweep line event.
 * - no edge e has zero length
 *
 * Invariants for the Mesh (the processed portion).
 * - the portion of the mesh left of the sweep line is a planar graph,
 *   ie. there is *some* way to embed it in the plane
 * - no processed edge has zero length
 * - no two processed vertices have identical coordinates
 * - each "inside" region is monotone, ie. can be broken into two chains
 *   of monotonically increasing vertices according to VertLeq(v1,v2)
 *   - a non-invariant: these chains may intersect (very slightly)
 *
 * Invariants for the Sweep.
 * - if none of the edges incident to the event vertex have an activeRegion
 *   (ie. none of these edges are in the edge dictionary), then the vertex
 *   has only right-going edges.
 * - if an edge is marked "fixUpperEdge" (it is a temporary edge introduced
 *   by ConnectRightVertex), then it is the only right-going edge from
 *   its associated vertex.  (This says that these edges exist only
 *   when it is necessary.)
 */

//static void SweepEvent( TESStesselator *tess, Vertex *vEvent );
//static void WalkDirtyRegions( TESStesselator *tess, ActiveRegion *regUp );
//static int CheckForRightSplice( TESStesselator *tess, ActiveRegion *regUp );

/*
 * Both edges must be directed from right to left (this is the canonical
 * direction for the upper edge of each region).
 *
 * The strategy is to evaluate a "t" value for each edge at the
 * current sweep line position, given by tess->event.  The calculations
 * are designed to be very stable, but of course they are not perfect.
 *
 * Special case: if both edge destinations are at the sweep event,
 * we sort the edges by slope (they would otherwise compare equally).
 *
 * �����߱��������������ÿ�������ϱ�Ե�Ĺ淶���򣩡�
 * ������Ϊ��ǰɨ����λ�õ�ÿ���߼���һ�� "t" ֵ���� tess->event ������
 * ��Щ���㱻��Ƶ÷ǳ��ȶ�������Ȼ���������ġ�
 * ������������������Ŀ�ĵض���ɨ���¼��У����ǽ����¶ȶԱ߽������򣨷������ǽ���ȵؽ��бȽϣ���
 */
LIBTESS_INLINE int Sweep::EdgeLeq(Sweep* sweep, ActiveRegion *reg1, ActiveRegion *reg2)
{
    Vertex *event = sweep->currentEvent;
    HalfEdge *e1, *e2;
    Float t1, t2;

    e1 = reg1->eUp;
    e2 = reg2->eUp;

    if (e1->mirror->vertex == event) {
        if (e2->mirror->vertex == event) {
            /* Two edges right of the sweep line which meet at the sweep event.
             * Sort them by slope.
             */
            if (VertexLessEqual(e1->vertex, e2->vertex)) {
                return EdgeSign(e2->mirror->vertex, e1->vertex, e2->vertex) <= 0;
            }
            return EdgeSign(e1->mirror->vertex, e2->vertex, e1->vertex) >= 0;
        }
        return EdgeSign(e2->mirror->vertex, event, e2->vertex) <= 0;
    }
    if (e2->mirror->vertex == event) {
        return EdgeSign(e1->mirror->vertex, event, e1->vertex) >= 0;
    }

    /* General case - compute signed distance *from* e1, e2 to event */
    t1 = EdgeEval(e1->mirror->vertex, event, e1->vertex);
    t2 = EdgeEval(e2->mirror->vertex, event, e2->vertex);
    return (t1 >= t2);
}

/* ɾ�� ActiveRegion
 */
LIBTESS_INLINE void Sweep::DeleteRegion(ActiveRegion *reg)
{
    if (reg->fixUpperEdge) {
        /* It was created with zero winding number, so it better be
         * deleted with zero winding number (ie. it better not get merged
         * with a real edge).
         *
         * ������������������ģ����������������ɾ������������ò�Ҫ��ʵ�ʱߺϲ�����
         */
        assert(reg->eUp->winding == 0);
    }
    reg->eUp->activeRegion = NULL;
    dict.erase(reg->nodeUp);
    this->deallocate(reg);
}

/* Replace an upper edge which needs fixing (see ConnectRightVertex).
 * ������Ҫ�޸����ϱ�Ե����μ� ConnectRightVertex����
 */
LIBTESS_INLINE int FixUpperEdge(Mesh& mesh, ActiveRegion *reg, HalfEdge *newEdge)
{
    assert(reg->fixUpperEdge);
    if (!mesh.DeleteEdge(reg->eUp)) return 0;
    reg->fixUpperEdge = FALSE;
    reg->eUp = newEdge;
    newEdge->activeRegion = reg;

    return 1;
}

LIBTESS_STATIC ActiveRegion *TopLeftRegion(Mesh& mesh, ActiveRegion *reg)
{
    Vertex *org = reg->eUp->vertex;
    HalfEdge *e;

    /* Find the region above the uppermost edge with the same origin
     * ��������ı����ҵ�ԭ����ͬ������
     */
    do {
        reg = RegionAbove(reg);
    } while (reg->eUp->vertex == org);

    /* If the edge above was a temporary edge introduced by ConnectRightVertex,
     * now is the time to fix it.
     * �������ı���ConnectRightVertex�������ʱ�ߣ��޸�����
     */
    if (reg->fixUpperEdge) {
        e = mesh.Connect(RegionBelow(reg)->eUp->mirror, reg->eUp->Lnext);
        if (e == NULL) return NULL;
        if (!FixUpperEdge(mesh, reg, e)) {
            return NULL;
        }
        reg = RegionAbove(reg);
    }
    return reg;
}

LIBTESS_STATIC ActiveRegion *TopRightRegion(ActiveRegion *reg)
{
    Vertex *dst = reg->eUp->mirror->vertex;

    /* Find the region above the uppermost edge with the same destination */
    do {
        reg = RegionAbove(reg);
    } while (reg->eUp->mirror->vertex == dst);
    return reg;
}

/*
 * Add a new active region to the sweep line, *somewhere* below "regAbove"
 * (according to where the new edge belongs in the sweep-line dictionary).
 * The upper edge of the new region will be "eNewUp".
 * Winding number and "inside" flag are not updated.
 *
 * ��ɨ���������һ���µĻ������ "regAbove" �����ĳ��
 *�������±���ɨ�����ֵ��е�λ�ã���
 * ��������ϱ�Ե���� "eNewUp"��
 * ����ź� "inside" ��־�����¡�
 */
LIBTESS_INLINE ActiveRegion* Sweep::AddRegionBelow(ActiveRegion *regAbove, HalfEdge *eNewUp)
{
    ActiveRegion *regNew = this->allocate();
    //if (regNew == NULL) longjmp(tess->env,1);

    regNew->eUp = eNewUp;
    regNew->nodeUp = dict.insert(regAbove->nodeUp, regNew);
    //if (regNew->nodeUp == NULL) longjmp(tess->env,1);
    regNew->fixUpperEdge = FALSE;
    regNew->sentinel = FALSE;
    regNew->dirty = FALSE;

    eNewUp->activeRegion = regNew;
    return regNew;
}

LIBTESS_INLINE int Sweep::IsWindingInside(int n)
{
    switch (windingRule) {
    case TESS_WINDING_ODD:
        return (n & 1);
    case TESS_WINDING_NONZERO:
        return (n != 0);
    case TESS_WINDING_POSITIVE:
        return (n > 0);
    case TESS_WINDING_NEGATIVE:
        return (n < 0);
    case TESS_WINDING_ABS_GEQ_TWO:
        return (n >= 2) || (n <= -2);
    }
    /*LINTED*/
    assert(FALSE);
    /*NOTREACHED*/

    return(FALSE);
}

LIBTESS_INLINE void Sweep::ComputeWinding(ActiveRegion *reg)
{
    reg->windingNumber = RegionAbove(reg)->windingNumber + reg->eUp->winding;
    reg->inside = IsWindingInside(reg->windingNumber);
}

/*
 * Delete a region from the sweep line.  This happens when the upper
 * and lower chains of a region meet (at a vertex on the sweep line).
 * The "inside" flag is copied to the appropriate mesh face (we could
 * not do this before -- since the structure of the mesh is always
 * changing, this face may not have even existed until now).
 *
 * ��ɨ������ɾ�����򡣵�����������������ཻʱ����ɨ�����ϵĶ��㴦���ᷢ�����������
 * "inside" ��־�����Ƶ��ʵ���������
 *��������ǰ����������������Ϊ����Ľṹ�����ڱ仯����������ֱ�����ڲŴ��ڣ���
 */
LIBTESS_INLINE void Sweep::FinishRegion(ActiveRegion *r)
{
    HalfEdge *e = r->eUp;
    Face *f = e->Lface;

    f->inside = r->inside;
    f->edge = e;   /* optimization for tessMeshTessellateMonoRegion() */
    DeleteRegion(r);
}

/*
 * We are given a vertex with one or more left-going edges.  All affected
 * edges should be in the edge dictionary.  Starting at regFirst->eUp,
 * we walk down deleting all regions where both edges have the same
 * origin vOrg.  At the same time we copy the "inside" flag from the
 * active region to the face, since at this point each face will belong
 * to at most one region (this was not necessarily true until this point
 * in the sweep).  The walk stops at the region above regLast; if regLast
 * is NULL we walk as far as possible.	At the same time we relink the
 * mesh if necessary, so that the ordering of edges around vOrg is the
 * same as in the dictionary.
 *
 * ���ǵõ���һ����һ�����������ߵĶ��㡣
 * ������Ӱ��ı߶�Ӧ���ڱ��ֵ��С��� regFirst->eUp ��ʼ��
 * ������ɾ�����е���������������Ե������ͬ��ԭ���С�
 * ͬʱ�����ǽ� "inside" ��־�ӻ�����Ƶ��棬��Ϊ��ʱ
 * ÿ�����������һ����������ɨ���еĴ˵�֮ǰ��һ������ȷ�ģ���
 * ������ regLast �ϵ�����ֹͣ����� regLast Ϊ�գ����ǽ������ܵ����Ρ�
 * ͬʱ�������Ҫ�����ǽ��������������Ա� vOrg ��Χ�ıߵ�˳�����ֵ��е���ͬ��
 */
LIBTESS_INLINE HalfEdge* Sweep::FinishLeftRegions(Mesh& mesh, ActiveRegion *regFirst, ActiveRegion *regLast)
{
    ActiveRegion *reg, *regPrev;
    HalfEdge *e, *ePrev;

    regPrev = regFirst;
    ePrev = regFirst->eUp;
    while (regPrev != regLast) {
        regPrev->fixUpperEdge = FALSE;    /* placement was OK */
        reg = RegionBelow(regPrev);
        e = reg->eUp;
        if (e->vertex != ePrev->vertex) {
            if (!reg->fixUpperEdge) {
                /* Remove the last left-going edge.  Even though there are no further
                 * edges in the dictionary with this origin, there may be further
                 * such edges in the mesh (if we are adding left edges to a vertex
                 * that has already been processed).  Thus it is important to call
                 * FinishRegion rather than just DeleteRegion.
                 */
                FinishRegion(regPrev);
                break;
            }
            /* If the edge below was a temporary edge introduced by
             * ConnectRightVertex, now is the time to fix it.
             */
            e = mesh.Connect(ePrev->Onext->mirror, e->mirror);
            LIBTESS_LONGJMP(e == NULL);
            LIBTESS_LONGJMP(!FixUpperEdge(mesh, reg, e));
        }

        /* Relink edges so that ePrev->Onext == e */
        if (ePrev->Onext != e) {
            LIBTESS_LONGJMP(!mesh.Splice(e->mirror->Lnext, e));
            LIBTESS_LONGJMP(!mesh.Splice(ePrev, e));
        }
        FinishRegion(regPrev);    /* may change reg->eUp */
        ePrev = reg->eUp;
        regPrev = reg;
    }
    return ePrev;
}

/*
 * Purpose: insert right-going edges into the edge dictionary, and update
 * winding numbers and mesh connectivity appropriately.  All right-going
 * edges share a common origin vOrg.  Edges are inserted CCW starting at
 * eFirst; the last edge inserted is eLast->Oprev.  If vOrg has any
 * left-going edges already processed, then eTopLeft must be the edge
 * such that an imaginary upward vertical segment from vOrg would be
 * contained between eTopLeft->Oprev and eTopLeft; otherwise eTopLeft
 * should be NULL.
 *
 * Ŀ�ģ��ڱ��ֵ��в������бߣ����ʵ����²�������������ͨ�ԡ�
 * �õı߹���һ����ͬ��ԭ���С���Ե�ӵ�һ����ʼ����ʱ�뷽����룻
 * ������ı�Ե�� eLast->Oprev����� vOrg �Ѿ��������κ�����ıߣ�
 * ��ô eTopLeft �����������ıߣ��������� vOrg ��һ���鹹�����ϴ�ֱ��
 * �������� eTopLeft->Oprev �� eTopLeft ֮�䣻���� eTopLeft Ӧ��Ϊ�ա�
 */
LIBTESS_INLINE void Sweep::AddRightEdges(
    Mesh& mesh,
    ActiveRegion *regUp,
    HalfEdge *eFirst,
    HalfEdge *eLast,
    HalfEdge *eTopLeft,
    int cleanUp)
{
    ActiveRegion *reg, *regPrev;
    HalfEdge *e, *ePrev;
    int firstTime = TRUE;

    /* Insert the new right-going edges in the dictionary */
    e = eFirst;
    do {
        assert(VertexLessEqual(e->vertex, e->mirror->vertex));
        AddRegionBelow(regUp, e->mirror);
        e = e->Onext;
    } while (e != eLast);

    /* Walk *all* right-going edges from e->Org, in the dictionary order,
     * updating the winding numbers of each region, and re-linking the mesh
     * edges to match the dictionary ordering (if necessary).
     */
    if (eTopLeft == NULL) {
        eTopLeft = RegionBelow(regUp)->eUp->mirror->Onext;
    }
    regPrev = regUp;
    ePrev = eTopLeft;
    for (;; ) {
        reg = RegionBelow(regPrev);
        e = reg->eUp->mirror;
        if (e->vertex != ePrev->vertex) break;

        if (e->Onext != ePrev) {
            /* Unlink e from its current position, and relink below ePrev */
            LIBTESS_LONGJMP(!mesh.Splice(e->mirror->Lnext, e));
            LIBTESS_LONGJMP(!mesh.Splice(ePrev->mirror->Lnext, e));
        }
        /* Compute the winding number and "inside" flag for the new regions */
        reg->windingNumber = regPrev->windingNumber - e->winding;
        reg->inside = IsWindingInside(reg->windingNumber);

        /* Check for two outgoing edges with same slope -- process these
         * before any intersection tests (see example in tessComputeInterior).
         */
        regPrev->dirty = TRUE;
        if (!firstTime && CheckForRightSplice(mesh, regPrev)) {
            AddWinding(e, ePrev);
            DeleteRegion(regPrev);
            LIBTESS_LONGJMP(!mesh.DeleteEdge(ePrev));
        }
        firstTime = FALSE;
        regPrev = reg;
        ePrev = e;
    }
    regPrev->dirty = TRUE;
    assert(regPrev->windingNumber - e->winding == reg->windingNumber);

    if (cleanUp) {
        /* Check for intersections between newly adjacent edges. */
        WalkDirtyRegions(mesh, regPrev);
    }
}

/*
 * Find some weights which describe how the intersection vertex is
 * a linear combination of "org" and "dest".  Each of the two edges
 * which generated "isect" is allocated 50% of the weight; each edge
 * splits the weight between its org and dst according to the
 * relative distance to "isect".
 * �ҵ�һЩ���� "org" �� "dest" ������ϵĽ��� vertex Ȩ�ء�
 * ���� "isect" ���������е�ÿһ��������50%��Ȩ�أ�
 * ÿ���߸��ݵ� "isect" ����Ծ������� org �� dst ֮��ָ�Ȩ�ء�
 */
LIBTESS_STATIC void VertexWeights(Vertex *isect, Vertex *org, Vertex *dst, Float *weights)
{
    Float t1 = VertexDistance(org, isect);
    Float t2 = VertexDistance(dst, isect);

    weights[0] = Float(0.5) * t2 / (t1 + t2);
    weights[1] = Float(0.5) * t1 / (t1 + t2);
    isect->coords.x += weights[0] * org->coords.x + weights[1] * dst->coords.x;
    isect->coords.y += weights[0] * org->coords.y + weights[1] * dst->coords.y;
    isect->coords.z += weights[0] * org->coords.z + weights[1] * dst->coords.z;
}

/*
 * We've computed a new intersection point, now we need a "data" pointer
 * from the user so that we can refer to this new vertex in the
 * rendering callbacks.
 *
 * �����Ѿ�������һ���µĽ��㣬����������Ҫһ�������û��ġ����ݡ�ָ�룬
 * �Ա����ǿ�������Ⱦ�ص�����������¶��㡣
 * ���ص��Ѿ�ɾ����
 */
LIBTESS_INLINE void Sweep::GetIntersectData(Vertex *isect, Vertex *orgUp, Vertex *dstUp, Vertex *orgLo, Vertex *dstLo)
{
    Float weights[4];
    //TESS_NOTUSED( tess );

    isect->coords.x = isect->coords.y = isect->coords.z = 0;
    isect->idx = INVALID_INDEX;
    VertexWeights(isect, orgUp, dstUp, &weights[0]);
    VertexWeights(isect, orgLo, dstLo, &weights[2]);
}

/*
 * Check the upper and lower edge of "regUp", to make sure that the
 * eUp->Org is above eLo, or eLo->Org is below eUp (depending on which
 * origin is leftmost).
 *
 * The main purpose is to splice right-going edges with the same
 * dest vertex and nearly identical slopes (ie. we can't distinguish
 * the slopes numerically).  However the splicing can also help us
 * to recover from numerical errors.  For example, suppose at one
 * point we checked eUp and eLo, and decided that eUp->Org is barely
 * above eLo.  Then later, we split eLo into two edges (eg. from
 * a splice operation like this one).  This can change the result of
 * our test so that now eUp->Org is incident to eLo, or barely below it.
 * We must correct this condition to maintain the dictionary invariants.
 *
 * One possibility is to check these edges for intersection again
 * (ie. CheckForIntersect).  This is what we do if possible.  However
 * CheckForIntersect requires that tess->event lies between eUp and eLo,
 * so that it has something to fall back on when the intersection
 * calculation gives us an unusable answer.  So, for those cases where
 * we can't check for intersection, this routine fixes the problem
 * by just splicing the offending vertex into the other edge.
 * This is a guaranteed solution, no matter how degenerate things get.
 * Basically this is a combinatorial solution to a numerical problem.
 *
 * ��� "regUp" �����±�Ե��ȷ�� eUp->Org λ�� eLo  ֮�ϣ�
 * �� eLo->Org λ�� eUp ֮�£�ȡ��������ߵ�ԭ�㣩��
 *
 * ����ҪĿ����ƴ�Ӿ�����ͬĿ�Ķ���ͼ�����ͬ�¶ȣ����������޷���
 * ���������¶ȣ������бߡ�
 * Ȼ����ƴ��Ҳ���԰������Ǵ���ֵ����лָ�������
 * ���磬������һ�����Ǽ���� eUp �� eLo����ȷ�� eUp->Org ���������� eLo��
 * Ȼ�����ǽ� eLo �ֳ������ߣ����磬��������ƴ�Ӳ����У���
 * ���ı����ǲ��ԵĽ������������ eUp->Org �� eLo �����˳�ͻ��
 * ���߼������� eLo�����Ǳ���������������Ա����ֵ�Ĳ�������
 *
 * һ�ֿ������ٴμ����Щ���Ƿ��ཻ���� CheckForIntersect����
 * ������ܵĻ������������Ҫ���ġ�
 * Ȼ�� CheckForIntersect Ҫ�� tess->event λ�� eUp �� eLo ֮�䣬
 * ��˵��������������һ�������õĴ�ʱ������һЩ��������������
 * ��ˣ�������Щ�����޷�����ཻ�������
 * �������ͨ����������Ķ���ƴ�ӵ���һ������������⡣
 * ����һ���б�֤�Ľ�����������������ö�ô��⡣
 * ����������һ����ֵ�������Ͻ⡣
 */
LIBTESS_INLINE int Sweep::CheckForRightSplice(Mesh& mesh, ActiveRegion *regUp)
{
    ActiveRegion *regLo = RegionBelow(regUp);
    HalfEdge *eUp = regUp->eUp;
    HalfEdge *eLo = regLo->eUp;

    if (VertexLessEqual(eUp->vertex, eLo->vertex)) {
        if (EdgeSign(eLo->mirror->vertex, eUp->vertex, eLo->vertex) > 0) {
            return FALSE;
        }

        /* eUp->Org appears to be below eLo */
        if (!VertexEqual(eUp->vertex, eLo->vertex)) {
            /* Splice eUp->Org into eLo */
            LIBTESS_LONGJMP(mesh.SplitEdge(eLo->mirror) == NULL);
            LIBTESS_LONGJMP(!mesh.Splice(eUp, eLo->mirror->Lnext));
            regUp->dirty = regLo->dirty = TRUE;

        }
        else if (eUp->vertex != eLo->vertex) {
            /* merge the two vertices, discarding eUp->Org */
            if (pq.find(eUp->vertex)) {
                pq.erase(eUp->vertex);
                LIBTESS_LONGJMP(!mesh.Splice(eLo->mirror->Lnext, eUp));
            }
            else {
                /* ������ʱ������������������κβ���ĿǰҲû���κ�����
                 */
                //LIBTESS_LONGJMP( !mesh.Splice( eLo->mirror->Lnext, eUp ) );
                //LIBTESS_LOG("pq.erase( eUp->vertex ) : error.");
            }
        }
    }
    else {
        if (EdgeSign(eUp->mirror->vertex, eLo->vertex, eUp->vertex) <= 0) {
            return FALSE;
        }

        /* eLo->Org appears to be above eUp, so splice eLo->Org into eUp */
        RegionAbove(regUp)->dirty = regUp->dirty = TRUE;
        LIBTESS_LONGJMP(mesh.SplitEdge(eUp->mirror) == NULL);
        LIBTESS_LONGJMP(!mesh.Splice(eLo->mirror->Lnext, eUp));
    }

    return TRUE;
}

/*
 * Check the upper and lower edge of "regUp", to make sure that the
 * eUp->Dst is above eLo, or eLo->Dst is below eUp (depending on which
 * destination is rightmost).
 *
 * Theoretically, this should always be true.  However, splitting an edge
 * into two pieces can change the results of previous tests.  For example,
 * suppose at one point we checked eUp and eLo, and decided that eUp->Dst
 * is barely above eLo.  Then later, we split eLo into two edges (eg. from
 * a splice operation like this one).  This can change the result of
 * the test so that now eUp->Dst is incident to eLo, or barely below it.
 * We must correct this condition to maintain the dictionary invariants
 * (otherwise new edges might get inserted in the wrong place in the
 * dictionary, and bad stuff will happen).
 *
 * We fix the problem by just splicing the offending vertex into the
 * other edge.
 *
 * ��� "regUp" �����±�Ե��ȷ�� eUp->Dst ���� eLo��
 * �� eLo->Dst ���� eUp��ȡ�����ĸ�Ŀ�ĵ����ұߣ���
 * �������Ͻ�����Ӧ������ġ�
 * ���ǣ���һ���߷ֳ����ο��ܻ������ǰ���ԵĽ����
 * ���磬������һ�����Ǽ���� eUp �� eLo����ȷ�� eUp->Dst ���������� eLo��
 * Ȼ�����ǽ� eLo �ֳ������ߣ����磬��������ƴ�Ӳ����У���
 * ����ܻ�ı���Խ����������� eUp->Dst �� eLo ������ͻ�����߼������� eLo��
 * ���Ǳ��������������Ա����ֵ䲻�����������µı߿��ܻ���뵽�ֵ��е�
 * ����λ�ã����һᷢ����������飩��
 *
 * ����ֻ�轫������Ķ���ƴ�ӵ���һ�����ϾͿ��Խ��������⡣
 */
LIBTESS_INLINE int Sweep::CheckForLeftSplice(Mesh& mesh, ActiveRegion *regUp)
{
    ActiveRegion *regLo = RegionBelow(regUp);
    HalfEdge *eUp = regUp->eUp;
    HalfEdge *eLo = regLo->eUp;
    //HalfEdge *e = NULL;
    HalfEdge *e;

    assert(!VertexEqual(eUp->mirror->vertex, eLo->mirror->vertex));

    if (VertexLessEqual(eUp->mirror->vertex, eLo->mirror->vertex)) {
        if (EdgeSign(eUp->mirror->vertex, eLo->mirror->vertex, eUp->vertex) < 0) {
            return FALSE;
        }

        /* eLo->Dst is above eUp, so splice eLo->Dst into eUp */
        RegionAbove(regUp)->dirty = regUp->dirty = TRUE;
        e = mesh.SplitEdge(eUp);
        LIBTESS_LONGJMP(e == NULL);
        LIBTESS_LONGJMP(!mesh.Splice(eLo->mirror, e));

        e->Lface->inside = regUp->inside;
    }
    else {
        if (EdgeSign(eLo->mirror->vertex, eUp->mirror->vertex, eLo->vertex) > 0) {
            return FALSE;
        }

        /* eUp->Dst is below eLo, so splice eUp->Dst into eLo */
        regUp->dirty = regLo->dirty = TRUE;
        e = mesh.SplitEdge(eLo);
        LIBTESS_LONGJMP(e == NULL);
        LIBTESS_LONGJMP(!mesh.Splice(eUp->Lnext, eLo->mirror));
        e->mirror->Lface->inside = regUp->inside;
    }
    return TRUE;
}

/*
 * Check the upper and lower edges of the given region to see if
 * they intersect.  If so, create the intersection and add it
 * to the data structures.
 *
 * Returns TRUE if adding the new intersection resulted in a recursive
 * call to AddRightEdges(); in this case all "dirty" regions have been
 * checked for intersections, and possibly regUp has been deleted.
 *
 * ������������ϱ�Ե���±�Ե�Ƿ��ཻ��
 * ����ǣ��򴴽�������������ӵ����ݽṹ�С�
 *
 * �������½������µݹ���� AddRightEdges()���򷵻� TRUE��
 * ����������£��Ѽ������ "dirty" ����Ľ�����
 * ���ҿ�����ɾ�� regUp��
 */
LIBTESS_INLINE int Sweep::CheckForIntersect(Mesh& mesh, ActiveRegion *regUp)
{
    ActiveRegion *regLo = RegionBelow(regUp);
    HalfEdge *eUp = regUp->eUp;
    HalfEdge *eLo = regLo->eUp;
    Vertex *orgUp = eUp->vertex;
    Vertex *orgLo = eLo->vertex;
    Vertex *dstUp = eUp->mirror->vertex;
    Vertex *dstLo = eLo->mirror->vertex;
    Float tMinUp, tMaxLo;
    Vertex isect, *orgMin;
    HalfEdge *e;

    assert(!VertexEqual(dstLo, dstUp));
    assert(EdgeSign(dstUp, currentEvent, orgUp) <= 0);
    assert(EdgeSign(dstLo, currentEvent, orgLo) >= 0);
    assert(orgUp != currentEvent && orgLo != currentEvent);
    assert(!regUp->fixUpperEdge && !regLo->fixUpperEdge);

    if (orgUp == orgLo) return FALSE;    /* right endpoints are the same */

    tMinUp = std::min(orgUp->t, dstUp->t);
    tMaxLo = std::max(orgLo->t, dstLo->t);
    if (tMinUp > tMaxLo) return FALSE;    /* t ranges do not overlap */

    if (VertexLessEqual(orgUp, orgLo)) {
        if (EdgeSign(dstLo, orgUp, orgLo) > 0) return FALSE;
    }
    else {
        if (EdgeSign(dstUp, orgLo, orgUp) < 0) return FALSE;
    }

    /* At this point the edges intersect, at least marginally */
    DebugEvent(tess);

    EdgeIntersect(dstUp, orgUp, dstLo, orgLo, &isect);
    /* The following properties are guaranteed: */
    assert(std::min(orgUp->t, dstUp->t) <= isect.t);
    assert(isect.t <= std::max(orgLo->t, dstLo->t));
    assert(std::min(dstLo->s, dstUp->s) <= isect.s);
    assert(isect.s <= std::max(orgLo->s, orgUp->s));

    if (VertexLessEqual(&isect, currentEvent)) {
        /* The intersection point lies slightly to the left of the sweep line,
         * so move it until it''s slightly to the right of the sweep line.
         * (If we had perfect numerical precision, this would never happen
         * in the first place).  The easiest and safest thing to do is
         * replace the intersection by tess->event.
         */
        isect.s = currentEvent->s;
        isect.t = currentEvent->t;
    }
    /* Similarly, if the computed intersection lies to the right of the
     * rightmost origin (which should rarely happen), it can cause
     * unbelievable inefficiency on sufficiently degenerate inputs.
     * (If you have the test program, try running test54.d with the
     * "X zoom" option turned on).
     */
    orgMin = VertexLessEqual(orgUp, orgLo) ? orgUp : orgLo;
    if (VertexLessEqual(orgMin, &isect)) {
        isect.s = orgMin->s;
        isect.t = orgMin->t;
    }

    if (VertexEqual(&isect, orgUp) || VertexEqual(&isect, orgLo)) {
        /* Easy case -- intersection at one of the right endpoints */
        (void) CheckForRightSplice(mesh, regUp);
        return FALSE;
    }

    if ((!VertexEqual(dstUp, currentEvent)
        && EdgeSign(dstUp, currentEvent, &isect) >= 0)
        || (!VertexEqual(dstLo, currentEvent)
            && EdgeSign(dstLo, currentEvent, &isect) <= 0)) {
            /* Very unusual -- the new upper or lower edge would pass on the
             * wrong side of the sweep event, or through it.  This can happen
             * due to very small numerical errors in the intersection calculation.
             */
        if (dstLo == currentEvent) {
            /* Splice dstLo into eUp, and process the new region(s) */
            LIBTESS_LONGJMP(mesh.SplitEdge(eUp->mirror) == NULL);
            LIBTESS_LONGJMP(!mesh.Splice(eLo->mirror, eUp));
            regUp = TopLeftRegion(mesh, regUp);
            LIBTESS_LONGJMP(regUp == NULL);
            eUp = RegionBelow(regUp)->eUp;
            FinishLeftRegions(mesh, RegionBelow(regUp), regLo);
            AddRightEdges(mesh, regUp, eUp->mirror->Lnext, eUp, eUp, TRUE);
            return TRUE;
        }
        if (dstUp == currentEvent) {
            /* Splice dstUp into eLo, and process the new region(s) */
            LIBTESS_LONGJMP(mesh.SplitEdge(eLo->mirror) == NULL);
            LIBTESS_LONGJMP(!mesh.Splice(eUp->Lnext, eLo->mirror->Lnext));
            regLo = regUp;
            regUp = TopRightRegion(regUp);
            e = RegionBelow(regUp)->eUp->mirror->Onext;
            regLo->eUp = eLo->mirror->Lnext;
            eLo = FinishLeftRegions(mesh, regLo, NULL);
            AddRightEdges(mesh, regUp, eLo->Onext, eUp->mirror->Onext, e, TRUE);
            return TRUE;
        }
        /* Special case: called from ConnectRightVertex.  If either
         * edge passes on the wrong side of tess->event, split it
         * (and wait for ConnectRightVertex to splice it appropriately).
         */
        if (EdgeSign(dstUp, currentEvent, &isect) >= 0) {
            RegionAbove(regUp)->dirty = regUp->dirty = TRUE;
            LIBTESS_LONGJMP(mesh.SplitEdge(eUp->mirror) == NULL);
            eUp->vertex->s = currentEvent->s;
            eUp->vertex->t = currentEvent->t;
        }
        if (EdgeSign(dstLo, currentEvent, &isect) <= 0) {
            regUp->dirty = regLo->dirty = TRUE;
            LIBTESS_LONGJMP(mesh.SplitEdge(eLo->mirror) == NULL);
            eLo->vertex->s = currentEvent->s;
            eLo->vertex->t = currentEvent->t;
        }
        /* leave the rest for ConnectRightVertex */
        return FALSE;
    }

    /* General case -- split both edges, splice into new vertex.
     * When we do the splice operation, the order of the arguments is
     * arbitrary as far as correctness goes.  However, when the operation
     * creates a new face, the work done is proportional to the size of
     * the new face.  We expect the faces in the processed part of
     * the mesh (ie. eUp->Lface) to be smaller than the faces in the
     * unprocessed original contours (which will be eLo->Oprev->Lface).
     */
    LIBTESS_LONGJMP(mesh.SplitEdge(eUp->mirror) == NULL);
    LIBTESS_LONGJMP(mesh.SplitEdge(eLo->mirror) == NULL);
    LIBTESS_LONGJMP(!mesh.Splice(eLo->mirror->Lnext, eUp));
    eUp->vertex->s = isect.s;
    eUp->vertex->t = isect.t;
    pq.insert(eUp->vertex);
    /*
    eUp->vertex->pqHandle = tess->pq.insert( eUp->vertex );
    if (eUp->vertex->pqHandle == INV_HANDLE) {
        pqDeletePriorityQ( &tess->alloc, tess->pq );
        tess->pq = NULL;
        longjmp(tess->env,1);
    }
    */
    GetIntersectData(eUp->vertex, orgUp, dstUp, orgLo, dstLo);
    RegionAbove(regUp)->dirty = regUp->dirty = regLo->dirty = TRUE;
    return FALSE;
}

/*
 * When the upper or lower edge of any region changes, the region is
 * marked "dirty".  This routine walks through all the dirty regions
 * and makes sure that the dictionary invariants are satisfied
 * (see the comments at the beginning of this file).  Of course
 * new dirty regions can be created as we make changes to restore
 * the invariants.
 *
 * ���κ�������ϱ�Ե���±�Ե����ʱ�������򽫱��Ϊ "dirty"��
 * �����̱������������򣬲�ȷ�������ֵ䲻������Ҫ������Ĵ��ļ���ͷ��ע�ͣ���
 * ��Ȼ�������ǽ��и����Իָ�������ʱ�����Դ����µ�������
 */
LIBTESS_INLINE void Sweep::WalkDirtyRegions(Mesh& mesh, ActiveRegion *regUp)
{
    ActiveRegion *regLo = RegionBelow(regUp);
    HalfEdge *eUp, *eLo;

    for (;; ) {
        /* Find the lowest dirty region (we walk from the bottom up). */
        while (regLo->dirty) {
            regUp = regLo;
            regLo = RegionBelow(regLo);
        }
        if (!regUp->dirty) {
            regLo = regUp;
            regUp = RegionAbove(regUp);
            if (regUp == NULL || !regUp->dirty) {
                /* We've walked all the dirty regions */
                return;
            }
        }
        regUp->dirty = FALSE;
        eUp = regUp->eUp;
        eLo = regLo->eUp;

        if (eUp->mirror->vertex != eLo->mirror->vertex) {
            /* Check that the edge ordering is obeyed at the Dst vertices. */
            if (CheckForLeftSplice(mesh, regUp)) {

                /* If the upper or lower edge was marked fixUpperEdge, then
                 * we no longer need it (since these edges are needed only for
                 * vertices which otherwise have no right-going edges).
                 */
                if (regLo->fixUpperEdge) {
                    DeleteRegion(regLo);
                    LIBTESS_LONGJMP(!mesh.DeleteEdge(eLo));
                    regLo = RegionBelow(regUp);
                    eLo = regLo->eUp;
                }
                else if (regUp->fixUpperEdge) {
                    DeleteRegion(regUp);
                    LIBTESS_LONGJMP(!mesh.DeleteEdge(eUp));
                    regUp = RegionAbove(regLo);
                    eUp = regUp->eUp;
                }
            }
        }
        if (eUp->vertex != eLo->vertex) {
            if (eUp->mirror->vertex != eLo->mirror->vertex
                && !regUp->fixUpperEdge && !regLo->fixUpperEdge
                && (eUp->mirror->vertex == currentEvent || eLo->mirror->vertex == currentEvent)) {
                /* When all else fails in CheckForIntersect(), it uses tess->event
                 * as the intersection location.  To make this possible, it requires
                 * that tess->event lie between the upper and lower edges, and also
                 * that neither of these is marked fixUpperEdge (since in the worst
                 * case it might splice one of these edges into tess->event, and
                 * violate the invariant that fixable edges are the only right-going
                 * edge from their associated vertex).
                 */
                if (CheckForIntersect(mesh, regUp)) {
                    /* WalkDirtyRegions() was called recursively; we're done */
                    return;
                }
            }
            else {
                /* Even though we can't use CheckForIntersect(), the Org vertices
                * may violate the dictionary edge ordering.  Check and correct this.
                */
                (void) CheckForRightSplice(mesh, regUp);
            }
        }
        if (eUp->vertex == eLo->vertex && eUp->mirror->vertex == eLo->mirror->vertex) {
            /* A degenerate loop consisting of only two edges -- delete it. */
            AddWinding(eLo, eUp);
            DeleteRegion(regUp);
            LIBTESS_LONGJMP(!mesh.DeleteEdge(eUp));
            regUp = RegionAbove(regLo);
        }
    }
}

/*
 * Purpose: connect a "right" vertex vEvent (one where all edges go left)
 * to the unprocessed portion of the mesh.  Since there are no right-going
 * edges, two regions (one above vEvent and one below) are being merged
 * into one.  "regUp" is the upper of these two regions.
 *
 * There are two reasons for doing this (adding a right-going edge):
 *  - if the two regions being merged are "inside", we must add an edge
 *    to keep them separated (the combined region would not be monotone).
 *  - in any case, we must leave some record of vEvent in the dictionary,
 *    so that we can merge vEvent with features that we have not seen yet.
 *    For example, maybe there is a vertical edge which passes just to
 *    the right of vEvent; we would like to splice vEvent into this edge.
 *
 * However, we don't want to connect vEvent to just any vertex.  We don''t
 * want the new edge to cross any other edges; otherwise we will create
 * intersection vertices even when the input data had no self-intersections.
 * (This is a bad thing; if the user's input data has no intersections,
 * we don't want to generate any false intersections ourselves.)
 *
 * Our eventual goal is to connect vEvent to the leftmost unprocessed
 * vertex of the combined region (the union of regUp and regLo).
 * But because of unseen vertices with all right-going edges, and also
 * new vertices which may be created by edge intersections, we don''t
 * know where that leftmost unprocessed vertex is.  In the meantime, we
 * connect vEvent to the closest vertex of either chain, and mark the region
 * as "fixUpperEdge".  This flag says to delete and reconnect this edge
 * to the next processed vertex on the boundary of the combined region.
 * Quite possibly the vertex we connected to will turn out to be the
 * closest one, in which case we won''t need to make any changes.
 */
LIBTESS_INLINE void Sweep::ConnectRightVertex(Mesh& mesh, ActiveRegion *regUp, HalfEdge *eBottomLeft)
{
    HalfEdge *eNew;
    HalfEdge *eTopLeft = eBottomLeft->Onext;
    ActiveRegion *regLo = RegionBelow(regUp);
    HalfEdge *eUp = regUp->eUp;
    HalfEdge *eLo = regLo->eUp;
    int degenerate = FALSE;

    if (eUp->mirror->vertex != eLo->mirror->vertex) {
        (void) CheckForIntersect(mesh, regUp);
    }

    /* Possible new degeneracies: upper or lower edge of regUp may pass
     * through vEvent, or may coincide with new intersection vertex
     */
    if (VertexEqual(eUp->vertex, currentEvent)) {
        LIBTESS_LONGJMP(!mesh.Splice(eTopLeft->mirror->Lnext, eUp));
        regUp = TopLeftRegion(mesh, regUp);
        LIBTESS_LONGJMP(regUp == NULL);
        eTopLeft = RegionBelow(regUp)->eUp;
        FinishLeftRegions(mesh, RegionBelow(regUp), regLo);
        degenerate = TRUE;
    }
    if (VertexEqual(eLo->vertex, currentEvent)) {
        LIBTESS_LONGJMP(!mesh.Splice(eBottomLeft, eLo->mirror->Lnext));
        eBottomLeft = FinishLeftRegions(mesh, regLo, NULL);
        degenerate = TRUE;
    }
    if (degenerate) {
        AddRightEdges(mesh, regUp, eBottomLeft->Onext, eTopLeft, eTopLeft, TRUE);
        return;
    }

    /* Non-degenerate situation -- need to add a temporary, fixable edge.
     * Connect to the closer of eLo->Org, eUp->Org.
     */
    if (VertexLessEqual(eLo->vertex, eUp->vertex)) {
        eNew = eLo->mirror->Lnext;
    }
    else {
        eNew = eUp;
    }
    eNew = mesh.Connect(eBottomLeft->Onext->mirror, eNew);
    LIBTESS_LONGJMP(eNew == NULL);

    /* Prevent cleanup, otherwise eNew might disappear before we've even
     * had a chance to mark it as a temporary edge.
     */
    AddRightEdges(mesh, regUp, eNew, eNew->Onext, eNew->Onext, FALSE);
    eNew->mirror->activeRegion->fixUpperEdge = TRUE;
    WalkDirtyRegions(mesh, regUp);
}

/* Because vertices at exactly the same location are merged together
 * before we process the sweep event, some degenerate cases can't occur.
 * However if someone eventually makes the modifications required to
 * merge features which are close together, the cases below marked
 * TOLERANCE_NONZERO will be useful.  They were debugged before the
 * code to merge identical vertices in the main loop was added.
 */
#define TOLERANCE_NONZERO FALSE

/*
 * The event vertex lies exacty on an already-processed edge or vertex.
 * Adding the new vertex involves splicing it into the already-processed
 * part of the mesh.
 */
LIBTESS_INLINE void Sweep::ConnectLeftDegenerate(Mesh& mesh, ActiveRegion *regUp, Vertex *vEvent)
{
    HalfEdge *e, *eTopLeft, *eTopRight, *eLast;
    ActiveRegion *reg;

    e = regUp->eUp;
    if (VertexEqual(e->vertex, vEvent)) {
        /* e->Org is an unprocessed vertex - just combine them, and wait
         * for e->Org to be pulled from the queue
         */
        assert(TOLERANCE_NONZERO);
        LIBTESS_LONGJMP(!mesh.Splice(e, vEvent->edge));
        return;
    }

    if (!VertexEqual(e->mirror->vertex, vEvent)) {
        /* General case -- splice vEvent into edge e which passes through it */
        LIBTESS_LONGJMP(mesh.SplitEdge(e->mirror) == NULL);
        if (regUp->fixUpperEdge) {
            /* This edge was fixable -- delete unused portion of original edge */
            LIBTESS_LONGJMP(!mesh.DeleteEdge(e->Onext));
            regUp->fixUpperEdge = FALSE;
        }
        LIBTESS_LONGJMP(!mesh.Splice(vEvent->edge, e));
        SweepEvent(mesh, vEvent);    /* recurse */
        return;
    }

    /* vEvent coincides with e->Dst, which has already been processed.
     * Splice in the additional right-going edges.
     */
    assert(TOLERANCE_NONZERO);
    regUp = TopRightRegion(regUp);
    reg = RegionBelow(regUp);
    eTopRight = reg->eUp->mirror;
    eTopLeft = eLast = eTopRight->Onext;
    if (reg->fixUpperEdge) {
        /* Here e->Dst has only a single fixable edge going right.
         * We can delete it since now we have some real right-going edges.
         */
        assert(eTopLeft != eTopRight);   /* there are some left edges too */
        DeleteRegion(reg);
        LIBTESS_LONGJMP(!mesh.DeleteEdge(eTopRight));
        eTopRight = eTopLeft->mirror->Lnext;
    }
    LIBTESS_LONGJMP(!mesh.Splice(vEvent->edge, eTopRight));
    if (!EdgeGoesLeft(eTopLeft)) {
        /* e->Dst had no left-going edges -- indicate this to AddRightEdges() */
        eTopLeft = NULL;
    }
    AddRightEdges(mesh, regUp, eTopRight->Onext, eLast, eTopLeft, TRUE);
}

/*
 * Purpose: connect a "left" vertex (one where both edges go right)
 * to the processed portion of the mesh.  Let R be the active region
 * containing vEvent, and let U and L be the upper and lower edge
 * chains of R.  There are two possibilities:
 *
 * - the normal case: split R into two regions, by connecting vEvent to
 *   the rightmost vertex of U or L lying to the left of the sweep line
 *
 * - the degenerate case: if vEvent is close enough to U or L, we
 *   merge vEvent into that edge chain.  The subcases are:
 *	- merging with the rightmost vertex of U or L
 *	- merging with the active edge of U or L
 *	- merging with an already-processed portion of U or L
 */
LIBTESS_INLINE void Sweep::ConnectLeftVertex(Mesh& mesh, Vertex *vEvent)
{
    ActiveRegion *regUp, *regLo, *reg;
    HalfEdge *eUp, *eLo, *eNew;
    ActiveRegion tmp;

    /* assert( vEvent->anEdge->Onext->Onext == vEvent->anEdge ); */

    /* Get a pointer to the active region containing vEvent */
    tmp.eUp = vEvent->edge->mirror;
    /* __GL_DICTLISTKEY */ /* tessDictListSearch */
    regUp = (ActiveRegion *) dictKey(dict.find(&tmp));
    regLo = RegionBelow(regUp);
    if (!regLo) {
        // This may happen if the input polygon is coplanar.
        return;
    }
    eUp = regUp->eUp;
    eLo = regLo->eUp;

    /* Try merging with U or L first */
    if (EdgeSign(eUp->mirror->vertex, vEvent, eUp->vertex) == 0) {
        ConnectLeftDegenerate(mesh, regUp, vEvent);
        return;
    }

    /* Connect vEvent to rightmost processed vertex of either chain.
     * e->Dst is the vertex that we will connect to vEvent.
     */
    reg = VertexLessEqual(eLo->mirror->vertex, eUp->mirror->vertex) ? regUp : regLo;

    if (regUp->inside || reg->fixUpperEdge) {
        if (reg == regUp) {
            eNew = mesh.Connect(vEvent->edge->mirror, eUp->Lnext);
            LIBTESS_LONGJMP(eNew == NULL);
        }
        else {
            HalfEdge *tempHalfEdge = mesh.Connect(eLo->mirror->Onext->mirror, vEvent->edge);
            LIBTESS_LONGJMP(tempHalfEdge == NULL);

            eNew = tempHalfEdge->mirror;
        }
        if (reg->fixUpperEdge) {
            LIBTESS_LONGJMP(!FixUpperEdge(mesh, reg, eNew));
        }
        else {
            ComputeWinding(AddRegionBelow(regUp, eNew));
        }
        SweepEvent(mesh, vEvent);
    }
    else {
        /* The new vertex is in a region which does not belong to the polygon.
         * We don''t need to connect this vertex to the rest of the mesh.
         */
        AddRightEdges(mesh, regUp, vEvent->edge, vEvent->edge, NULL, TRUE);
    }
}

/*
 * Does everything necessary when the sweep line crosses a vertex.
 * Updates the mesh and the edge dictionary.
 *
 * ��ɨ���ߴ�������ʱִ�����б�Ҫ�Ĳ�����
 * ����ģ�ͺͱ��ֵ䡣
 */
LIBTESS_INLINE void Sweep::SweepEvent(Mesh& mesh, Vertex *vEvent)
{
    ActiveRegion *regUp, *reg;
    HalfEdge *e, *eTopLeft, *eBottomLeft;

    currentEvent = vEvent;        /* for access in EdgeLeq() */
    DebugEvent(tess);

    /* Check if this vertex is the right endpoint of an edge that is
     * already in the dictionary.  In this case we don't need to waste
     * time searching for the location to insert new edges.
     */
    e = vEvent->edge;
    while (e->activeRegion == NULL) {
        e = e->Onext;
        if (e == vEvent->edge) {
            /* All edges go right -- not incident to any processed edges */
            ConnectLeftVertex(mesh, vEvent);
            return;
        }
    }

    /* Processing consists of two phases: first we "finish" all the
     * active regions where both the upper and lower edges terminate
     * at vEvent (ie. vEvent is closing off these regions).
     * We mark these faces "inside" or "outside" the polygon according
     * to their winding number, and delete the edges from the dictionary.
     * This takes care of all the left-going edges from vEvent.
     */
    regUp = TopLeftRegion(mesh, e->activeRegion);
    LIBTESS_LONGJMP(regUp == NULL);
    reg = RegionBelow(regUp);
    eTopLeft = reg->eUp;
    eBottomLeft = FinishLeftRegions(mesh, reg, NULL);

    /* Next we process all the right-going edges from vEvent.  This
     * involves adding the edges to the dictionary, and creating the
     * associated "active regions" which record information about the
     * regions between adjacent dictionary edges.
     */
    if (eBottomLeft->Onext == eTopLeft) {
        /* No right-going edges -- add a temporary "fixable" edge */
        ConnectRightVertex(mesh, regUp, eBottomLeft);
    }
    else {
        AddRightEdges(mesh, regUp, eBottomLeft->Onext, eTopLeft, eTopLeft, TRUE);
    }
}


/* Make the sentinel coordinates big enough that they will never be
 * merged with real input features.
 */
/*
 * We add two sentinel edges above and below all other edges,
 * to avoid special cases at the top and bottom.
 */
LIBTESS_INLINE void Sweep::AddSentinel(Mesh& mesh, Float smin, Float smax, Float t)
{
    HalfEdge *e;
    ActiveRegion *reg = this->allocate();
    //if (reg == NULL) longjmp(env,1);

    e = mesh.MakeEdge();
    //if (e == NULL) longjmp(env,1);

    e->vertex->s = smax;
    e->vertex->t = t;
    e->mirror->vertex->s = smin;
    e->mirror->vertex->t = t;
    currentEvent = e->mirror->vertex;        /* initialize it */

    reg->eUp = e;
    reg->windingNumber = 0;
    reg->inside = FALSE;
    reg->fixUpperEdge = FALSE;
    reg->sentinel = TRUE;
    reg->dirty = FALSE;
    reg->nodeUp = dict.insert(reg);
    //if (reg->nodeUp == NULL) longjmp(env,1);
}

/*
 * We maintain an ordering of edge intersections with the sweep line.
 * This order is maintained in a dynamic dictionary.
 *
 * ��ʼ���ʵ�
 */
LIBTESS_INLINE void Sweep::InitEdgeDict(Mesh& mesh, const AABB& aabb)
{
    Float w, h;
    Float smin, smax, tmin, tmax;

    dict.init(this, (PFN_DICTKEY_COMPARE) EdgeLeq);

    /* If the bbox is empty, ensure that sentinels are not coincident by slightly enlarging it. */
    /* ԭ�����
    w = (tess->bmax[0] - tess->bmin[0]) + (Float)0.01;
    h = (tess->bmax[1] - tess->bmin[1]) + (Float)0.01;

    smin = tess->bmin[0] - w;
    smax = tess->bmax[0] + w;
    tmin = tess->bmin[1] - h;
    tmax = tess->bmax[1] + h;
    */
    w = (aabb.amax - aabb.amin) + (Float)0.01;
    h = (aabb.bmax - aabb.bmin) + (Float)0.01;

    smin = aabb.amin - w;
    smax = aabb.amax + w;
    tmin = aabb.bmin - h;
    tmax = aabb.bmax + h;

    AddSentinel(mesh, smin, smax, tmin);
    AddSentinel(mesh, smin, smax, tmax);
}

/* �رմʵ�
 * ɾ������ ActiveRegion
 */
LIBTESS_INLINE void Sweep::DoneEdgeDict()
{
    ActiveRegion *reg;
    int fixedEdges = 0;

    while ((reg = (ActiveRegion *) dictKey(dict.min())) != NULL) {
        /*
         * At the end of all processing, the dictionary should contain
         * only the two sentinel edges, plus at most one "fixable" edge
         * created by ConnectRightVertex().
         */
        if (!reg->sentinel) {
            assert(reg->fixUpperEdge);
            assert(++fixedEdges == 1);
        }
        assert(reg->windingNumber == 0);
        DeleteRegion(reg);
        /* DeleteEdge( reg->eUp );*/
    }
    dict.dispose();
}

/*
 * Remove zero-length edges, and contours with fewer than 3 vertices.
 */
LIBTESS_INLINE void Sweep::RemoveDegenerateEdges(Mesh& mesh)
{
    HalfEdge *e, *eNext, *eLnext;
    HalfEdge *eHead = &mesh.m_edgeHead;

    /*LINTED*/
    for (e = eHead->next; e != eHead; e = eNext) {
        eNext = e->next;
        eLnext = e->Lnext;

        if (VertexEqual(e->vertex, e->mirror->vertex) && e->Lnext->Lnext != e) {
            /* Zero-length edge, contour has at least 3 edges */

            LIBTESS_LONGJMP(!mesh.Splice(eLnext, e)); /* deletes e->Org */
            LIBTESS_LONGJMP(!mesh.DeleteEdge(e));     /* e is a self-loop */
            e = eLnext;
            eLnext = e->Lnext;
        }
        if (eLnext->Lnext == e) {
            /* Degenerate contour (one or two edges) */

            if (eLnext != e) {
                if (eLnext == eNext || eLnext == eNext->mirror) { eNext = eNext->next; }
                LIBTESS_LONGJMP(!mesh.DeleteEdge(eLnext));
            }
            if (e == eNext || e == eNext->mirror) { eNext = eNext->next; }
            LIBTESS_LONGJMP(!mesh.DeleteEdge(e));
        }
    }
}

/*
 * Insert all vertices into the priority queue which determines the
 * order in which vertices cross the sweep line.
 */
LIBTESS_INLINE int Sweep::InitPriorityQ(Mesh& mesh)
{
    #if 0
    PriorityQ *pq;
    Vertex *v, *vHead;
    int vertexCount = 0;

    vHead = &tess->mesh.m_vtxHead;
    for (v = vHead->next; v != vHead; v = v->next) {
        vertexCount++;
    }
    /* Make sure there is enough space for sentinels. */
    vertexCount += std::max(8, tess->alloc.extraVertices);

    pq = tess->pq = pqNewPriorityQ(&tess->alloc, vertexCount, (int(*)(PQkey, PQkey)) tesvertLeq);
    if (pq == NULL) return 0;

    vHead = &tess->mesh.m_vtxHead;
    for (v = vHead->next; v != vHead; v = v->next) {
        v->pqHandle = pqInsert(&tess->alloc, pq, v);
        if (v->pqHandle == INV_HANDLE)
            break;
    }
    if (v != vHead || !pqInit(&tess->alloc, pq)) {
        pqDeletePriorityQ(&tess->alloc, tess->pq);
        tess->pq = NULL;
        return 0;
    }

    return 1;
    #endif

    #ifdef LIBTESS_USE_PriorityQ
    pq.init(0, (PQ_Comp) VertexLessEqual);
    Vertex* vHead = &mesh.m_vtxHead;
    for (Vertex* v = vHead->next; v != vHead; v = v->next) {
        v->pqHandle = pq.insert(v);
    }
    pq.sort();

    #else
    pq.clear();
    Vertex* vHead = &mesh.m_vtxHead;
    for (Vertex* v = vHead->next; v != vHead; v = v->next) {
        pq.insert(v);
    }
    #endif

    return LIBTESS_OK;
}

/* ���
 */
LIBTESS_INLINE void Sweep::DonePriorityQ()
{
    pq.clear();
}

/*
 * Delete any degenerate faces with only two edges.  WalkDirtyRegions()
 * will catch almost all of these, but it won't catch degenerate faces
 * produced by splice operations on already-processed edges.
 * The two places this can happen are in FinishLeftRegions(), when
 * we splice in a "temporary" edge produced by ConnectRightVertex(),
 * and in CheckForLeftSplice(), where we splice already-processed
 * edges to ensure that our dictionary invariants are not violated
 * by numerical errors.
 *
 * In both these cases it is *very* dangerous to delete the offending
 * edge at the time, since one of the routines further up the stack
 * will sometimes be keeping a pointer to that edge.
 */
LIBTESS_INLINE bool Sweep::RemoveDegenerateFaces(Mesh& mesh)
{
    Face *f, *fNext;
    HalfEdge *e;

    /*LINTED*/
    for (f = mesh.m_faceHead.next; f != &mesh.m_faceHead; f = fNext) {
        fNext = f->next;
        e = f->edge;
        assert(e->Lnext != e);

        if (e->Lnext->Lnext == e) {
            /* A face with only two edges */
            AddWinding(e->Onext, e);
            if (!mesh.DeleteEdge(e)) return false;
        }
    }
    return true;
}

/*
 * __gl_computeInterior( tess ) computes the planar arrangement specified
 * by the given contours, and further subdivides this arrangement
 * into regions.  Each region is marked "inside" if it belongs
 * to the polygon, according to the rule given by tess->windingRule.
 * Each interior region is guaranteed be monotone.
 */
LIBTESS_INLINE int Sweep::ComputeInterior(Mesh& mesh, const AABB& aabb)
{
    Vertex *v, *vNext;

    /* Each vertex defines an event for our sweep line.  Start by inserting
     * all the vertices in a priority queue.  Events are processed in
     * lexicographic order, ie.
     *
     *    e1 < e2  iff  e1.x < e2.x || (e1.x == e2.x && e1.y < e2.y)
     */
    RemoveDegenerateEdges(mesh);
    if (InitPriorityQ(mesh) != LIBTESS_OK) {
        return LIBTESS_ERROR;
    }

    InitEdgeDict(mesh, aabb);

    if (pq.empty()) {
        LIBTESS_LOG("pq is empty()");
        return LIBTESS_ERROR;
    }

    //LIBTESS_LOG("sort begin");
    //int t = std::clock();

    #if 0
    //while( (v = (Vertex *)pqExtractMin( tess->pq )) != NULL ) {

    while ((v = pq.pop()) != NULL) {
        /* �ϲ���ͬ�Ķ���
         */
        for (;; ) {
            vNext = pq.top();// (Vertex *)pqMinimum( tess->pq );
            if (vNext == NULL || !VertexEqual(vNext, v)) {
                break;
            }
            /* Merge together all vertices at exactly the same location.
            * This is more efficient than processing them one at a time,
            * simplifies the code (see ConnectLeftDegenerate), and is also
            * important for correct handling of certain degenerate cases.
            * For example, suppose there are two identical edges A and B
            * that belong to different contours (so without this code they would
            * be processed by separate sweep events).  Suppose another edge C
            * crosses A and B from above.  When A is processed, we split it
            * at its intersection point with C.  However this also splits C,
            * so when we insert B we may compute a slightly different
            * intersection point.  This might leave two edges with a small
            * gap between them.  This kind of error is especially obvious
            * when using boundary extraction (TESS_BOUNDARY_ONLY).
            */
            vNext = pq.pop();// (Vertex *)pqExtractMin( tess->pq );
            /* �������֮�䣬��������
             */
            LIBTESS_LOG("SpliceMergeVertices : begin");
            LIBTESS_LONGJMP(!mesh.splice(v->edge, vNext->edge));
            LIBTESS_LOG("SpliceMergeVertices : end");
            pq.pop();
        }

        SweepEvent(mesh, v);
    }
    #endif

    v = pq.pop();
    while (v) {
        vNext = pq.top();
        while (vNext && VertexEqual(vNext, v)) {
            vNext = pq.pop();
            LIBTESS_LONGJMP(!mesh.Splice(v->edge, vNext->edge));
            vNext = pq.top();
        }
        SweepEvent(mesh, v);
        v = pq.pop();
    };

    //LIBTESS_LOG("sort time : %i", std::clock() - t);

    /* Set tess->event for debugging purposes */
    currentEvent = ((ActiveRegion *) dictKey(dict.min()))->eUp->vertex;
    DebugEvent(tess);
    DoneEdgeDict();
    DonePriorityQ();

    if (!RemoveDegenerateFaces(mesh)) {
        LIBTESS_LOG("Sweep.ComputeInterior() : RemoveDegenerateFaces( mesh ) error.");
        return LIBTESS_ERROR;
    }

    mesh.CheckMesh();

    return LIBTESS_OK;
}

}// end namespace libtess
