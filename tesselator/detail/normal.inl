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

#ifndef LIBTESS_NORMAL_HPP
#define LIBTESS_NORMAL_HPP

namespace libtess {

LIBTESS_INLINE Vec3 Tesselator::ComputeNormal()
{
    Vertex *v, *v1, *v2;
    Float c, tLen2, maxLen2;
    Vec3 maxVal, minVal, d1, d2, tNorm;
    Vertex *maxVert[3], *minVert[3];
    Vertex *vHead = &this->mesh.m_vtxHead;
    Vec3 norm = this->normal;
    int i;

    v = vHead->next;
    minVal = v->coords;
    maxVal = v->coords;

    /* ��ԭ�治ͬ
     */
    minVert[0] = minVert[1] = minVert[2] = v;
    maxVert[0] = maxVert[1] = maxVert[2] = v;

    for (v = vHead->next; v != vHead; v = v->next) {
        for (i = 0; i < 3; ++i) {
            c = v->coords[i];
            if (c < minVal[i]) { minVal[i] = c; minVert[i] = v; }
            if (c > maxVal[i]) { maxVal[i] = c; maxVert[i] = v; }
        }
    }

    /* Find two vertices separated by at least 1/sqrt(3) of the maximum
     * distance between any two vertices
     */
    i = 0;
    if (maxVal[1] - minVal[1] > maxVal[0] - minVal[0]) { i = 1; }
    if (maxVal[2] - minVal[2] > maxVal[i] - minVal[i]) { i = 2; }
    if (minVal[i] >= maxVal[i]) {
        /* All vertices are the same -- normal doesn't matter */
        norm[0] = 0; norm[1] = 0; norm[2] = 1;
        return norm;
    }

    /* Look for a third vertex which forms the triangle with maximum area
     * (Length of normal == twice the triangle area)
     */
    maxLen2 = 0;
    v1 = minVert[i];
    v2 = maxVert[i];
    d1[0] = v1->coords[0] - v2->coords[0];
    d1[1] = v1->coords[1] - v2->coords[1];
    d1[2] = v1->coords[2] - v2->coords[2];
    for (v = vHead->next; v != vHead; v = v->next) {
        d2[0] = v->coords[0] - v2->coords[0];
        d2[1] = v->coords[1] - v2->coords[1];
        d2[2] = v->coords[2] - v2->coords[2];
        tNorm[0] = d1[1] * d2[2] - d1[2] * d2[1];
        tNorm[1] = d1[2] * d2[0] - d1[0] * d2[2];
        tNorm[2] = d1[0] * d2[1] - d1[1] * d2[0];
        tLen2 = tNorm[0] * tNorm[0] + tNorm[1] * tNorm[1] + tNorm[2] * tNorm[2];
        if (tLen2 > maxLen2) {
            maxLen2 = tLen2;
            norm[0] = tNorm[0];
            norm[1] = tNorm[1];
            norm[2] = tNorm[2];
        }
    }

    if (maxLen2 <= 0) {
        /* All points lie on a single line -- any decent normal will do */
        norm[0] = norm[1] = norm[2] = 0;
        norm[ShortAxis(d1)] = 1;
    }

    return norm;
}

LIBTESS_INLINE void Tesselator::CheckOrientation()
{
    Float area;
    Face *f, *fHead = &mesh.m_faceHead;
    Vertex *v, *vHead = &mesh.m_vtxHead;
    HalfEdge *e;

    /*
     * When we compute the normal automatically, we choose the orientation
     * so that the the sum of the signed areas of all contours is non-negative.
     */
    area = 0;
    for (f = fHead->next; f != fHead; f = f->next) {
        e = f->edge;
        if (e->winding <= 0) continue;
        do {
            area += (e->vertex->s - e->mirror->vertex->s) * (e->vertex->t + e->mirror->vertex->t);
            e = e->Lnext;
        } while (e != f->edge);
    }
    if (area < 0) {
        /* Reverse the orientation by flipping all the t-coordinates */
        for (v = vHead->next; v != vHead; v = v->next) {
            v->t = -v->t;
        }
        tUnit.x = -tUnit.x;
        tUnit.y = -tUnit.y;
        tUnit.z = -tUnit.z;
    }
}

#ifdef FOR_TRITE_TEST_PROGRAM

#include <stdlib.h>
extern int RandomSweep;
const Float S_UNIT_X = (RandomSweep ? (2 * drand48() - 1) : 1.0);
const Float S_UNIT_Y = (RandomSweep ? (2 * drand48() - 1) : 0.0);

#else// FOR_TRITE_TEST_PROGRAM

#if defined(SLANTED_SWEEP)
/* The "feature merging" is not intended to be complete.  There are
 * special cases where edges are nearly parallel to the sweep line
 * which are not implemented.  The algorithm should still behave
 * robustly (ie. produce a reasonable tesselation) in the presence
 * of such edges, however it may miss features which could have been
 * merged.  We could minimize this effect by choosing the sweep line
 * direction to be something unusual (ie. not parallel to one of the
 * coordinate axes).
 */
const Float S_UNIT_X = (Float)0.50941539564955385;    /* Pre-normalized */
const Float S_UNIT_Y = (Float)0.86052074622010633;
#else// SLANTED_SWEEP
const Float S_UNIT_X = (Float)1.0;
const Float S_UNIT_Y = (Float)0.0;
#endif//SLANTED_SWEEP
#endif// FOR_TRITE_TEST_PROGRAM

/*
 * Determine the polygon normal and project vertices onto the plane of the polygon.
 * ȷ������η��߲�������ͶӰ�������ƽ���ϡ�
 */
LIBTESS_INLINE void Tesselator::ProjectPolygon()
{
    Vertex *v, *vHead = &this->mesh.m_vtxHead;

    /* �Ƿ���� normal
     */
    #ifdef LIBTESS_COMPUTE_NORMAL

    Vec3 norm = this->normal;
    bool computedNormal = false;
    int i;

    if (norm[0] == 0 && norm[1] == 0 && norm[2] == 0) {
        norm = this->ComputeNormal();
        computedNormal = true;
    }

    i = LongAxis(norm);

    #if defined(FOR_TRITE_TEST_PROGRAM) || defined(TRUE_PROJECT)
    /* Choose the initial sUnit vector to be approximately perpendicular to the normal.
     * ѡ���ʼ sUnit ������ʹ����ƴ�ֱ�ڷ��ߡ�
     */
    Normalize(norm);

    sUnit[i] = 0;
    sUnit[(i + 1) % 3] = S_UNIT_X;
    sUnit[(i + 2) % 3] = S_UNIT_Y;

    /* Now make it exactly perpendicular
     * ����������ȫ��ֱ
     */
    w = Dot(sUnit, norm);
    sUnit -= w * norm;
    Normalize(sUnit);

    /* Choose tUnit so that (sUnit, tUnit, norm) form a right-handed frame
     * ѡ�� tUnit ���γ����ֿ��(sUnit, tUnit, norm)
     */
    tUnit = cross(norm, sUnit);
    Normalize(tUnit);
    #else
    /* Project perpendicular to a coordinate axis -- better numerically */
    sUnit[i] = 0;
    sUnit[(i + 1) % 3] = S_UNIT_X;
    sUnit[(i + 2) % 3] = S_UNIT_Y;

    tUnit[i] = 0;
    tUnit[(i + 1) % 3] = (norm[i] > 0) ? -S_UNIT_Y : S_UNIT_Y;
    tUnit[(i + 2) % 3] = (norm[i] > 0) ? S_UNIT_X : -S_UNIT_X;
    #endif

    /* Project the vertices onto the sweep plane
     * ������ͶӰ��ɨ��ƽ����
     */
    for (v = vHead->next; v != vHead; v = v->next) {
        v->s = Dot(v->coords, sUnit);
        v->t = Dot(v->coords, tUnit);
        /* ����ԭ��ɨ����Ϊ����
         */
        //v->t = Dot( v->coords, sUnit );
        //v->s = Dot( v->coords, tUnit );
    }
    if (computedNormal) {
        CheckOrientation();
    }

    #else

    /* ����ɨ���߷���Ϊ����
     */
    for (v = vHead->next; v != vHead; v = v->next) {
        v->s = v->coords.y;
        v->t = v->coords.x;
    }

    #endif


    // Compute ST bounds.
    /*
    bool first = true;
    for( v = vHead->next; v != vHead; v = v->next ){
        if (first){
            this->bmin[0] = this->bmax[0] = v->s;
            this->bmin[1] = this->bmax[1] = v->t;
            first = false;
        }
        else{
            if (v->s < this->bmin[0]) this->bmin[0] = v->s;
            if (v->s > this->bmax[0]) this->bmax[0] = v->s;
            if (v->t < this->bmin[1]) this->bmin[1] = v->t;
            if (v->t > this->bmax[1]) this->bmax[1] = v->t;
        }
    }
    */
}

}// end namespace libtess

#endif //LIBTESS_NORMAL_HPP
