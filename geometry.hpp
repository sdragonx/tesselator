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

#ifndef LIBTESS_GEOMETRY_HPP
#define LIBTESS_GEOMETRY_HPP

#include "mesh.hpp"

namespace libtess{

int VertexIsCCW( Vertex *u, Vertex *v, Vertex *w )
{
    /* For almost-degenerate situations, the results are not reliable.
     * Unless the floating-point arithmetic can be performed without
     * rounding errors, *any* implementation will give incorrect results
     * on some degenerate inputs, so the client must have some way to
     * handle this situation.
     */
    return (u->s*(v->t - w->t) + v->s*(w->t - u->t) + w->s*(u->t - v->t)) >= 0;
}

inline bool VertexEqual(Vertex* u, Vertex* v)
{
//    return ((u)->s == (v)->s && (u)->t == (v)->t);
    return IsEqual(u->s, v->s) && IsEqual(u->t, v->t);
}

inline bool VertexLessEqual(Vertex* u, Vertex* v)
{
//    return ( u->s < v->s ) || //((u)->s == (v)->s && (u)->t <= (v)->t));
//        ( IsEqual( u->s, v->s ) && ( u->t < v->t || IsEqual( u->t, v->t ) ) );

    return ( u->s < v->s ) || //((u)->s == (v)->s && (u)->t <= (v)->t));
        ( IsEqual( u->s, v->s ) && ( u->t < v->t || IsEqual( u->t, v->t ) ) );

}

int CountFaceVertices( Face* face )
{
    HalfEdge *e = face->edge;
    int n = 0;
    do{
        n++;
        e = e->Lnext;
    }while (e != face->edge);
    return n;
}

inline bool EdgeGoesLeft(HalfEdge* e)
{
    return VertexLessEqual( (e)->mirror->vertex, (e)->vertex );
}

inline bool EdgeGoesRight(HalfEdge* e)
{
    return VertexLessEqual( (e)->vertex, (e)->mirror->vertex );
}

inline bool EdgeIsInternal(HalfEdge* e)
{
    return e->mirror->Lface && e->mirror->Lface->inside;
}

/* Versions of VertLeq, EdgeSign, EdgeEval with s and t transposed. */
inline bool VertexTransLEQ(Vertex* u, Vertex* v)
{
     return (((u)->t < (v)->t) || ((u)->t == (v)->t && (u)->s <= (v)->s));
}

//Manhattan
Float VertexDistance(Vertex* u, Vertex* v)
{
    return std::fabs(u->s - v->s) + std::fabs(u->t - v->t);
}

Float EdgeEval( Vertex *u, Vertex *v, Vertex *w )
{
    /* Given three vertices u,v,w such that VertLeq(u,v) && VertLeq(v,w),
     * evaluates the t-coord of the edge uw at the s-coord of the vertex v.
     * Returns v->t - (uw)(v->s), ie. the signed distance from uw to v.
     * If uw is vertical (and thus passes thru v), the result is zero.
     *
     * The calculation is extremely accurate and stable, even when v
     * is very close to u or w.  In particular if we set v->t = 0 and
     * let r be the negated result (this evaluates (uw)(v->s)), then
     * r is guaranteed to satisfy MIN(u->t,w->t) <= r <= MAX(u->t,w->t).

     * 给定三个顶点u，v，w，使 VertLeq(u,v) && VertLeq(v,w)
     * 在顶点v的s坐标处计算边uw的t坐标。
     * 返回v->t - (uw)(v->s)，即从uw到v的有符号距离。
     * 如果你是垂直的（因此通过v），结果是零。
     *
     * 即使当v非常接近u或w时，计算也非常精确和稳定。
     * 特别是如果我们设置v->t=0并让r为求反结果（这计算（uw）（v->s）），
     * 则r保证满足 MIN（u->t，w->t）<= r <= MAX（u->t，w->t）。
     */
    Float gapL, gapR;

    assert( VertexLessEqual( u, v ) && VertexLessEqual( v, w ));

    gapL = v->s - u->s;
    gapR = w->s - v->s;

    if( gapL + gapR > 0 ) {
        if( gapL < gapR ) {
            return (v->t - u->t) + (u->t - w->t) * (gapL / (gapL + gapR));
        } else {
            return (v->t - w->t) + (w->t - u->t) * (gapR / (gapL + gapR));
        }
    }
    /* vertical line */
    return 0;
}

Float EdgeSign( Vertex *u, Vertex *v, Vertex *w )
{
    /* Returns a number whose sign matches EdgeEval(u,v,w) but which
     * is cheaper to evaluate.  Returns > 0, == 0 , or < 0
     * as v is above, on, or below the edge uw.
     *
     * 返回一个其符号与EdgeEval(u,v,w)匹配但计算成本较低的数字。
     * 当v在边缘uw的上方、上方或下方时，返回>0、==0或<0。
     */
    Float gapL, gapR;

    assert( VertexLessEqual( u, v ) && VertexLessEqual( v, w ) );

    gapL = v->s - u->s;
    gapR = w->s - v->s;

    if( gapL + gapR > 0 ) {
        return (v->t - w->t) * gapL + (v->t - u->t) * gapR;
    }
    /* vertical line */
    return 0;
}


/***********************************************************************
 * Define versions of EdgeSign, EdgeEval with s and t transposed.
 */

Float EdgeTransEval( Vertex *u, Vertex *v, Vertex *w )
{
    /* Given three vertices u,v,w such that VertexTransLEQ(u,v) && VertexTransLEQ(v,w),
     * evaluates the t-coord of the edge uw at the s-coord of the vertex v.
     * Returns v->s - (uw)(v->t), ie. the signed distance from uw to v.
     * If uw is vertical (and thus passes thru v), the result is zero.
     *
     * The calculation is extremely accurate and stable, even when v
     * is very close to u or w.  In particular if we set v->s = 0 and
     * let r be the negated result (this evaluates (uw)(v->t)), then
     * r is guaranteed to satisfy MIN(u->s,w->s) <= r <= MAX(u->s,w->s).
     */
    Float gapL, gapR;

    assert( VertexTransLEQ( u, v ) && VertexTransLEQ( v, w ));

    gapL = v->t - u->t;
    gapR = w->t - v->t;

    if( gapL + gapR > 0 ) {
        if( gapL < gapR ) {
            return (v->s - u->s) + (u->s - w->s) * (gapL / (gapL + gapR));
        } else {
            return (v->s - w->s) + (w->s - u->s) * (gapR / (gapL + gapR));
        }
    }
    /* vertical line */
    return 0;
}

Float EdgeTransSign( Vertex *u, Vertex *v, Vertex *w )
{
    /* Returns a number whose sign matches TransEval(u,v,w) but which
     * is cheaper to evaluate.  Returns > 0, == 0 , or < 0
     * as v is above, on, or below the edge uw.
     */
    Float gapL, gapR;

    assert( VertexTransLEQ( u, v ) && VertexTransLEQ( v, w ));

    gapL = v->t - u->t;
    gapR = w->t - v->t;

    if( gapL + gapR > 0 ) {
        return (v->s - w->s) * gapL + (v->s - u->s) * gapR;
    }
    /* vertical line */
    return 0;
}

/* Given parameters a,x,b,y returns the value (b*x+a*y)/(a+b),
 * or (x+y)/2 if a==b==0.  It requires that a,b >= 0, and enforces
 * this in the rare case that one argument is slightly negative.
 * The implementation is extremely stable numerically.
 * In particular it guarantees that the result r satisfies
 * MIN(x,y) <= r <= MAX(x,y), and the results are very accurate
 * even when a and b differ greatly in magnitude.
 *
 * 给定参数 a，x，b，y
 * 返回(b*x+a*y)/(a+b)；如果a==b==0，则返回(x+y)/2。
 * 它要求a，b>=0，并在一个参数稍微为负的罕见情况下强制执行。
 * 在数值上实现是非常稳定的。
 * 特别地，它保证了结果r满足MIN(x,y)<=r<=MAX(x,y)，
 * 并且即使a和b在数量级上相差很大，结果也非常精确。
 */

//#define RealInterpolate(a,x,b,y)
//    (a = (a < 0) ? 0 : a, b = (b < 0) ? 0 : b,
//    ((a <= b) ? ((b == 0) ? ((x+y) / 2)
//    : (x + (y-x) * (a/(a+b))))
//    : (y + (x-y) * (b/(a+b)))))

inline Float Interpolate( Float a, Float x, Float b, Float y )
{
    a = (a < 0) ? 0 : a;
    b = (b < 0) ? 0 : b;
    return (a <= b) ? ((b == 0) ? ((x+y) / 2) : (x + (y-x) * (a/(a+b)))) : (y + (x-y) * (b/(a+b)));
}

/* Given edges (o1,d1) and (o2,d2), compute their point of intersection.
 * The computed point is guaranteed to lie in the intersection of the
 * bounding rectangles defined by each edge.
 * 给定边 (o1,d1) 和 (o2,d2)，计算它们的交点。
 * 计算点保证位于由每条边定义的边界矩形的交点处。
 */
void EdgeIntersect( Vertex *o1, Vertex *d1, Vertex *o2, Vertex *d2, Vertex *v )
{
    Float z1, z2;

    /* This is certainly not the most efficient way to find the intersection
     * of two line segments, but it is very numerically stable.
     *
     * Strategy: find the two middle vertices in the VertLeq ordering,
     * and interpolate the intersection s-value from these.  Then repeat
     * using the VertexTransLEQ ordering to find the intersection t-value.
     *
     * 这当然不是找到两条线段相交的最有效方法，但它在数值上非常稳定。
     * 策略：在顶点排序中找到两个中间顶点，
     * 并从中插入交点s值。然后重复使用VertexTransLEQ顺序来查找交集t值。
     */

    if( ! VertexLessEqual( o1, d1 )) { std::swap( o1, d1 ); }
    if( ! VertexLessEqual( o2, d2 )) { std::swap( o2, d2 ); }
    if( ! VertexLessEqual( o1, o2 )) { std::swap( o1, o2 ); std::swap( d1, d2 ); }

    if( ! VertexLessEqual( o2, d1 )) {
        /* Technically, no intersection -- do our best */
        v->s = (o2->s + d1->s) / 2;
    } else if( VertexLessEqual( d1, d2 )) {
        /* Interpolate between o2 and d1 */
        z1 = EdgeEval( o1, o2, d1 );
        z2 = EdgeEval( o2, d1, d2 );
        if( z1+z2 < 0 ) { z1 = -z1; z2 = -z2; }
        v->s = Interpolate( z1, o2->s, z2, d1->s );
    } else {
        /* Interpolate between o2 and d2 */
        z1 = EdgeSign( o1, o2, d1 );
        z2 = -EdgeSign( o1, d2, d1 );
        if( z1+z2 < 0 ) { z1 = -z1; z2 = -z2; }
        v->s = Interpolate( z1, o2->s, z2, d2->s );
    }

    /* Now repeat the process for t */

    if( ! VertexTransLEQ( o1, d1 )) { std::swap( o1, d1 ); }
    if( ! VertexTransLEQ( o2, d2 )) { std::swap( o2, d2 ); }
    if( ! VertexTransLEQ( o1, o2 )) { std::swap( o1, o2 ); std::swap( d1, d2 ); }

    if( ! VertexTransLEQ( o2, d1 )) {
        /* Technically, no intersection -- do our best */
        v->t = (o2->t + d1->t) / 2;
    } else if( VertexTransLEQ( d1, d2 )) {
        /* Interpolate between o2 and d1 */
        z1 = EdgeTransEval( o1, o2, d1 );
        z2 = EdgeTransEval( o2, d1, d2 );
        if( z1+z2 < 0 ) { z1 = -z1; z2 = -z2; }
        v->t = Interpolate( z1, o2->t, z2, d1->t );
    } else {
        /* Interpolate between o2 and d2 */
        z1 = EdgeTransSign( o1, o2, d1 );
        z2 = -EdgeTransSign( o1, d2, d1 );
        if( z1+z2 < 0 ) { z1 = -z1; z2 = -z2; }
        v->t = Interpolate( z1, o2->t, z2, d2->t );
    }
}

//libtess2
Float inCircle( Vertex *v, Vertex *v0, Vertex *v1, Vertex *v2 )
{
    Float adx, ady, bdx, bdy, cdx, cdy;
    Float abdet, bcdet, cadet;
    Float alift, blift, clift;

    adx = v0->s - v->s;
    ady = v0->t - v->t;
    bdx = v1->s - v->s;
    bdy = v1->t - v->t;
    cdx = v2->s - v->s;
    cdy = v2->t - v->t;

    abdet = adx * bdy - bdx * ady;
    bcdet = bdx * cdy - cdx * bdy;
    cadet = cdx * ady - adx * cdy;

    alift = adx * adx + ady * ady;
    blift = bdx * bdx + bdy * bdy;
    clift = cdx * cdx + cdy * cdy;

    return alift * bcdet + blift * cadet + clift * abdet;
}

/*
    Returns 1 is edge is locally delaunay
 */
int EdgeIsLocallyDelaunay( HalfEdge *e )
{
    return inCircle(e->mirror->Lnext->Lnext->vertex, e->Lnext->vertex, e->Lnext->Lnext->vertex, e->vertex) < 0;
}

}// end namespace libtess

#endif// LIBTESS_GEOMETRY_HPP