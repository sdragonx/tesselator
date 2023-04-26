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

#ifndef LIBTESS_MONO_HPP
#define LIBTESS_MONO_HPP

#include "geometry.hpp"
#include "mesh.hpp"

namespace libtess {

/* When we merge two edges into one, we need to compute the combined
 * winding of the new edge.
 * 当我们将两条边合并为一条边时，我们需要计算新边的组合卷绕。
 */
LIBTESS_INLINE void AddWinding(HalfEdge* eDst, HalfEdge* eSrc)
{
    eDst->winding += eSrc->winding;
    eDst->mirror->winding += eSrc->mirror->winding;
}

/* __gl_meshTessellateMonoRegion( face ) tessellates a monotone region
 * (what else would it do??)  The region must consist of a single
 * loop of half-edges (see mesh.h) oriented CCW.  "Monotone" in this
 * case means that any vertical line intersects the interior of the
 * region in a single interval.
 *
 * Tessellation consists of adding interior edges (actually pairs of
 * half-edges), to split the region into non-overlapping triangles.
 *
 * The basic idea is explained in Preparata and Shamos (which I don''t
 * have handy right now), although their implementation is more
 * complicated than this one.  The are two edge chains, an upper chain
 * and a lower chain.  We process all vertices from both chains in order,
 * from right to left.
 *
 * The algorithm ensures that the following invariant holds after each
 * vertex is processed: the untessellated region consists of two
 * chains, where one chain (say the upper) is a single edge, and
 * the other chain is concave.  The left vertex of the single edge
 * is always to the left of all vertices in the concave chain.
 *
 * Each step consists of adding the rightmost unprocessed vertex to one
 * of the two chains, and forming a fan of triangles from the rightmost
 * of two chain endpoints.  Determining whether we can add each triangle
 * to the fan is a simple orientation test.  By making the fan as large
 * as possible, we restore the invariant (check it yourself).
 */
LIBTESS_STATIC int TessellateMonoRegion(Mesh *mesh, Face *face)
{
    HalfEdge *up, *lo;

    /* All edges are oriented CCW around the boundary of the region.
     * First, find the half-edge whose origin vertex is rightmost.
     * Since the sweep goes from left to right, face->anEdge should
     * be close to the edge we want.
     */
    up = face->edge;
    assert(up->Lnext != up && up->Lnext->Lnext != up);

    for (; VertexLessEqual(up->mirror->vertex, up->vertex); up = up->Onext->mirror)
        ;
    for (; VertexLessEqual(up->vertex, up->mirror->vertex); up = up->Lnext)
        ;
    lo = up->Onext->mirror;

    while (up->Lnext != lo) {
        if (VertexLessEqual(up->mirror->vertex, lo->vertex)) {
            /* up->Dst is on the left.  It is safe to form triangles from lo->Org.
             * The EdgeGoesLeft test guarantees progress even when some triangles
             * are CW, given that the upper and lower chains are truly monotone.
             */
            while (lo->Lnext != up && (EdgeGoesLeft(lo->Lnext)
                || EdgeSign(lo->vertex, lo->mirror->vertex, lo->Lnext->mirror->vertex) <= 0)) {
                HalfEdge *tempHalfEdge = mesh->Connect(lo->Lnext, lo);
                if (tempHalfEdge == NULL) {
                    return LIBTESS_ERROR;
                }
                lo = tempHalfEdge->mirror;
            }
            lo = lo->Onext->mirror;
        }
        else {
            /* lo->Org is on the left.  We can make CCW triangles from up->Dst. */
            while (lo->Lnext != up && (EdgeGoesRight(up->Onext->mirror)
                || EdgeSign(up->mirror->vertex, up->vertex, up->Onext->mirror->vertex) >= 0)) {
                HalfEdge *tempHalfEdge = mesh->Connect(up, up->Onext->mirror);
                if (tempHalfEdge == NULL) {
                    return LIBTESS_ERROR;
                }
                up = tempHalfEdge->mirror;
            }
            up = up->Lnext;
        }
    }

    /* Now lo->Org == up->Dst == the leftmost vertex.  The remaining region
     * can be tessellated in a fan from this leftmost vertex.
     */
    assert(lo->Lnext != up);
    while (lo->Lnext->Lnext != up) {
        HalfEdge *tempHalfEdge = mesh->Connect(lo->Lnext, lo);
        if (tempHalfEdge == NULL) {
            return LIBTESS_ERROR;
        }
        lo = tempHalfEdge->mirror;
    }

    return LIBTESS_OK;
}

/* __gl_meshTessellateInterior( mesh ) tessellates each region of
 * the mesh which is marked "inside" the polygon.  Each such region
 * must be monotone.
 * 细分网格中标记为多边形“内部”的每个区域。每个这样的区域必须是单调的(多边形)。
 */
LIBTESS_STATIC int TessellateInterior(Mesh *mesh)
{
    Face *f, *next;

    /*LINTED*/
    for (f = mesh->m_faceHead.next; f != &mesh->m_faceHead; f = next) {
        /* Make sure we don''t try to tessellate the new triangles. */
        // 确保我们不尝试对新的三角形进行镶嵌
        next = f->next;
        if (f->inside) {
            if (TessellateMonoRegion(mesh, f) != LIBTESS_OK) {
                return LIBTESS_ERROR;
            }
        }
    }
    return LIBTESS_OK;
}

#if 0
/* __gl_meshDiscardExterior( mesh ) zaps (ie. sets to NULL) all faces
 * which are not marked "inside" the polygon.  Since further mesh operations
 * on NULL faces are not allowed, the main purpose is to clean up the
 * mesh so that exterior loops are not represented in the data structure.
 */
void DiscardExterior(TESSmesh *mesh)
{
    Face *f, *next;

    /*LINTED*/
    for (f = mesh->m_faceHead.next; f != &mesh->m_faceHead; f = next) {
        /* Since f will be destroyed, save its next pointer. */
        next = f->next;
        if (!f->inside) {
            mesh->ZeroAllFace(f);
        }
    }
}
#endif

/* __gl_meshSetWindingNumber( mesh, value, keepOnlyBoundary ) resets the
 * winding numbers on all edges so that regions marked "inside" the
 * polygon have a winding number of "value", and regions outside
 * have a winding number of 0.
 *
 * If keepOnlyBoundary is TRUE, it also deletes all edges which do not
 * separate an interior region from an exterior one.
 */
LIBTESS_STATIC int SetWindingNumber(Mesh *mesh, int value, int keepOnlyBoundary)
{
    HalfEdge *e, *eNext;

    for (e = mesh->m_edgeHead.next; e != &mesh->m_edgeHead; e = eNext) {
        eNext = e->next;
        if (e->mirror->Lface->inside != e->Lface->inside) {
            /* This is a boundary edge (one side is interior, one is exterior). */
            e->winding = (e->Lface->inside) ? value : -value;
        }
        else {
            /* Both regions are interior, or both are exterior. */
            if (!keepOnlyBoundary) {
                e->winding = 0;
            }
            else {
                if (!mesh->DeleteEdge(e)) {
                    return LIBTESS_ERROR;
                }
            }
        }
    }
    return LIBTESS_OK;
}

}// end namespace libtess

#endif// LIBTESS_MONO_HPP
