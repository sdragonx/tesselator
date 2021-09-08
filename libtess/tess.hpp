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

/*

// 更新历史

2021-09-08 11:09:31
header only 完善化

例子：
exsample:

#define LIBTESS_USE_VEC2

#include <gl/glew.h>
#include <libtess/tess.h>

void draw_elements(int shape, const Vec2* vs, const int* indices, int size)
{
    #ifdef LIBTESS_HIGH_PRECISION
    glVertexPointer(2, GL_DOUBLE, sizeof(Vec2), vs);
    #else
    glVertexPointer(2, GL_FLOAT, sizeof(Vec2), vs);
    #endif
    glEnableClientState(GL_VERTEX_ARRAY);
    glDrawElements(shape, size, GL_UNSIGNED_INT, indices);
    glDisableClientState(GL_VERTEX_ARRAY);
}

libtess::Tesselator tess;
std::vector<Vec2> points;
points.push_back(...);      // insert some points

tess.add_contour( points );

// TESS_TRIANGLES:

tess.tesselate( TESS_WINDING_ODD, TESS_TRIANGLES );
draw_elements( GL_TRIANGLES, &tess.vertices[0], &tess.elements[0], tess.elements.size() );

// TESS_BOUNDARY_CONTOURS:

tess.Tesselate( TESS_WINDING_ODD, TESS_BOUNDARY_CONTOURS );
draw_elements( GL_LINES, &tess.vertices[0], &tess.elements[0], tess.elements.size() );

*/

#ifndef LIBTESS_TESSELATOR_HPP
#define LIBTESS_TESSELATOR_HPP

#include "base.hpp"
#include "mesh.hpp"
#include "mono.hpp"
#include "sweep.hpp"

namespace libtess{

#ifdef LIBTESS_USE_VEC3
const int LIBTESS_VERTEX_SIZE = 3;
#else
const int LIBTESS_VERTEX_SIZE = 2;
#endif

class Tesselator
{
private:
    Mesh  mesh;     /* stores the input contours, and eventually the tessellation itself */
    Sweep sweep;

    Vec3 sUnit;     /* unit vector in s-direction (debugging) */
    Vec3 tUnit;     /* unit vector in t-direction (debugging) */

    AABB aabb;      /* mesh bounding rect */

    Index vertexIndexCounter;

public:
    Vec3 normal;          /* user-specified normal (if provided) */

    // If enabled, the initial triagulation is improved with non-robust Constrained Delayney triangulation.
    // default = false
    bool processCDT;      /* option to run Constrained Delayney pass. */

    // If enabled, tessAddContour() will treat CW contours as CCW and vice versa
    // default = false
    bool reverseContours; /* AddContour() will treat CCW contours as CW and vice versa */

    // outputs
    #ifdef LIBTESS_USE_VEC3         // 输出的顶点列表
    std::vector<Vec3> vertices;
    #else
    std::vector<Vec2> vertices;
    #endif
    std::vector<Index>  indices;    // 索引列表
    std::vector<Index>  elements;   // 顶点索引列表

public:
    Tesselator();
    ~Tesselator();

    // 初始化
    int init();

    // 释放
    void dispose();

    // 添加一个轮廓
    int add_contour( int size, const void* pointer, int stride, int count );

    // 添加一个轮廓
    int add_contour( const std::vector<Vec2>& points );
    int add_contour( const std::vector<Vec3>& points );

    // 执行切割三角形
    int tesselate( TessWindingRule windingRule, TessElementType elementType, int polySize = 3);

private:
    // 计算normal
    Vec3 ComputeNormal();
    void CheckOrientation();
    void ProjectPolygon();

    void MeshRefineDelaunay( Mesh *mesh );

    int RenderTriangles();
    int RenderBoundary();

    Index GetNeighbourFace(HalfEdge* edge);
    int OutputPolymesh( int elementType, int polySize);
};

//
// source
//

LIBTESS_INLINE Tesselator::Tesselator() : mesh(), sweep()
{
    normal = Vec3();
    processCDT = false;
    reverseContours = false;
    vertexIndexCounter = 0;
}

LIBTESS_INLINE Tesselator::~Tesselator()
{

}

LIBTESS_INLINE int Tesselator::init()
{
    this->dispose();
    mesh.init();
    return 0;
}

LIBTESS_INLINE void Tesselator::dispose()
{
    mesh.dispose();
    sweep.dispose();

    this->vertices.clear();
    this->indices.clear();
    this->elements.clear();

    vertexIndexCounter = 0;
}

// AddContour() - Adds a contour to be tesselated.
// The type of the vertex coordinates is assumed to be Float.
// Parameters:
//   tess    - pointer to tesselator object.
//   size    - number of coordinates per vertex. Must be 2 or 3.
//   pointer - pointer to the first coordinate of the first vertex in the array.
//   stride  - defines offset in bytes between consecutive vertices.
//   count   - number of vertices in contour.
// Returns:
//   LIBTESS_OK if succeed, LIBTESS_ERROR if failed.
LIBTESS_INLINE int Tesselator::add_contour( int size, const void* pointer, int stride, int count )
{
    const unsigned char *src = (const unsigned char*)pointer;
    HalfEdge *e = NULL;

    if ( size < 2 )
        size = 2;
    if ( size > 3 )
        size = 3;

    //处理顶点
    for(int i = 0; i < count; ++i )
    {
        const Float* coords = (const Float*)src;
        src += stride;

        if( e == NULL ) {
            /* Make a self-loop (one vertex, one edge). */
            e = mesh.MakeEdge();
            if ( e == NULL ) {
                return LIBTESS_ERROR;
            }

            if ( !this->mesh.Splice( e, e->mirror ) ) {
                return LIBTESS_ERROR;
            }
        }
        else {
            /* Create a new vertex and edge which immediately follow e
             * in the ordering around the left face.
             */
            if ( mesh.SplitEdge( e ) == NULL ) {
                return LIBTESS_ERROR;
            }
            e = e->Lnext;
        }

        /* The new vertex is now e->Org. */
        e->vertex->coords.x = coords[0];
        e->vertex->coords.y = coords[1];
        if ( size > 2 )
            e->vertex->coords.z = coords[2];
        else
            e->vertex->coords.z = 0;
        /* Store the insertion number so that the vertex can be later recognized. */
        e->vertex->idx = this->vertexIndexCounter++;

        /* The winding of an edge says how the winding number changes as we
         * cross from the edge''s right face to its left face.  We add the
         * vertices in such an order that a CCW contour will add +1 to
         * the winding number of the region inside the contour.
         */
        e->winding = this->reverseContours ? -1 : 1;
        e->mirror->winding = this->reverseContours ? 1 : -1;
    }

    return LIBTESS_OK;
}

LIBTESS_INLINE int Tesselator::add_contour(const std::vector<Vec2>& points)
{
    return this->add_contour(2, points.data(), sizeof(Vec2), points.size());
}

LIBTESS_INLINE int Tesselator::add_contour(const std::vector<Vec3>& points)
{
    return this->add_contour(3, points.data(), sizeof(Vec3), points.size());
}

// Tesselate() - tesselate contours.
// Parameters:
//   tess        - pointer to tesselator object.
//   windingRule - winding rules used for tesselation, must be one of TessWindingRule.
//   elementType - defines the tesselation result element type, must be one of TessElementType.
//   polySize    - defines maximum vertices per polygons if output is polygons.
// Returns:
//   LIBTESS_OK if succeed, LIBTESS_ERROR if failed.
LIBTESS_INLINE int Tesselator::tesselate( TessWindingRule windingRule, TessElementType elementType, int polySize)
{
    int errCode;

    this->vertices.clear();
    this->indices.clear();
    this->elements.clear();

    if( mesh.empty() ){
        LIBTESS_LOG("Tesselator.Tesselate() : mesh is empty.");
        return LIBTESS_ERROR;
    }

    /* Determine the polygon normal and project vertices onto the plane
     * of the polygon.
     */
    LIBTESS_UNIT_TEST( ProjectPolygon() );

    /* ComputeInterior( tess ) computes the planar arrangement specified
     * by the given contours, and further subdivides this arrangement
     * into regions.  Each region is marked "inside" if it belongs
     * to the polygon, according to the rule given by tess->windingRule.
     * Each interior region is guaranteed be monotone.
     */

    aabb = mesh.ComputeAABB();
    try{
        sweep.init( windingRule );
        LIBTESS_UNIT_TEST( errCode = sweep.ComputeInterior( mesh, aabb ) );
        if ( errCode != LIBTESS_OK ) {
            LIBTESS_LOG("Tesselator.Tesselate() : Sweep.ComputeInterior() error.");
            return LIBTESS_ERROR;
        }
    }
    catch(...){
        return LIBTESS_ERROR;
    }

    /* If the user wants only the boundary contours, we throw away all edges
     * except those which separate the interior from the exterior.
     * Otherwise we tessellate all the regions marked "inside".
     */
    if( elementType == TESS_BOUNDARY_CONTOURS ) {
        LIBTESS_UNIT_TEST( errCode = SetWindingNumber( &this->mesh, 1, TRUE ) );
    }
    else {
        LIBTESS_UNIT_TEST( errCode = TessellateInterior( &this->mesh ) );
        // This process is very time consuming !!!
        if ( errCode == LIBTESS_OK && this->processCDT ) {
            LIBTESS_UNIT_TEST( MeshRefineDelaunay( &this->mesh ) );
        }
    }

    if( errCode != LIBTESS_OK ) {
        LIBTESS_LOG("Tesselator.Tesselate() : tessellate error.");
        return LIBTESS_ERROR;
    }

    this->mesh.CheckMesh();

    switch( elementType ) {
    case TESS_TRIANGLES:         /* output trianlges */
        LIBTESS_UNIT_TEST( errCode = RenderTriangles() );
        break;
    case TESS_BOUNDARY_CONTOURS: /* output contours */
        LIBTESS_UNIT_TEST( errCode = RenderBoundary() );
        break;
    default:
        errCode = LIBTESS_ERROR;
        // OutputPolymesh( elementType, polySize ) );
        break;
    }

    if( errCode != LIBTESS_OK ) {
        LIBTESS_LOG("Tesselator.Tesselate() : output error.");
        return LIBTESS_ERROR;
    }

    mesh.dispose();
    sweep.dispose();

    return LIBTESS_OK;
}

// element == GL_TRIANGLES
LIBTESS_INLINE int Tesselator::RenderTriangles()
{
    Vertex *v;
    Face *f;
    HalfEdge *edge;
    int faceVerts;
    int maxFaceCount = 0;
    int maxVertexCount = 0;

    // Mark unused
    for ( v = mesh.m_vtxHead.next; v != &mesh.m_vtxHead; v = v->next )
        v->n = INVALID_INDEX;

    // Create unique IDs for all vertices and faces.
    for ( f = mesh.m_faceHead.next; f != &mesh.m_faceHead; f = f->next )
    {
        f->n = INVALID_INDEX;
        if( !f->inside ) continue;

        edge = f->edge;
        faceVerts = 0;
        do{
            v = edge->vertex;
            if ( v->n == INVALID_INDEX ){
                v->n = maxVertexCount;

                #ifdef LIBTESS_USE_VEC3
                this->vertices.push_back( Vec3(v->coords.x, v->coords.y, v->coords.z) );
                #else
                this->vertices.push_back( Vec2(v->coords.x, v->coords.y) );
                #endif

                // Store vertex index.
                this->indices.push_back( maxVertexCount );

                maxVertexCount++;
            }
            this->elements.push_back( v->n );
            faceVerts++;
            edge = edge->Lnext;
        }while (edge != f->edge);

        assert( faceVerts <= 3 );

        f->n = maxFaceCount;
        ++maxFaceCount;
    }

    return LIBTESS_OK;
}

// element == GL_LINES
LIBTESS_INLINE int Tesselator::RenderBoundary()
{
    HalfEdge *edge;
    Vec3 *v;
    int first;
    int last = 0;
    //int face_vertex_count;

    for (Face *f = mesh.m_faceHead.next; f != &mesh.m_faceHead; f = f->next ) {
        if ( !f->inside ) {
            continue;
        }
        edge = f->edge;
        first = last;
        do {
            v = &edge->vertex->coords;
            #ifdef LIBTESS_USE_VEC3
            this->vertices.push_back( Vec3(v->x, v->y, v->z) );
            #else
            this->vertices.push_back( Vec2(v->x, v->y) );
            #endif
            indices.push_back( last );
            elements.push_back( last );
            ++last;
            elements.push_back( last );
            //++face_vertex_count;
            edge = edge->Lnext;
        } while ( edge != f->edge );

        if( first < last ){
            elements.back() = first;
        }

        //CGL_LOG("face vertices : %i", face_vertex_count);
    }

    return LIBTESS_OK;
}

}// end namespace libtess

#include "cdt.inl"
#include "normal.inl"

#endif// LIBTESS_TESSELATOR_HPP
