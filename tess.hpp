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

#ifndef TESS_H
#define TESS_H

#include "base.hpp"
#include "mesh.hpp"
#include "dict.hpp"
#include "mono.hpp"

namespace libtess{

#ifdef LIBTESS_USE_VEC3
const int LIBTESS_VERTEX_SIZE = 3;
#else
const int LIBTESS_VERTEX_SIZE = 2;
#endif

class Tesselator
{
private:
    TESSmesh mesh;    /* stores the input contours, and eventually the tessellation itself */
    Sweep sweep;

    Vec3 sUnit;       /* unit vector in s-direction (debugging) */
    Vec3 tUnit;       /* unit vector in t-direction (debugging) */

    AABB aabb;

    Index vertexIndexCounter;

public:
    Vec3 normal;    /* user-specified normal (if provided) */

    // If enabled, the initial triagulation is improved with non-robust Constrained Delayney triangulation.
    // default = false
    bool processCDT;         /* option to run Constrained Delayney pass. */

    // If enabled, tessAddContour() will treat CW contours as CCW and vice versa
    // default = false
    bool reverseContours;    /* AddContour() will treat CCW contours as CW and vice versa */

    // outputs
    #ifdef LIBTESS_USE_VEC3
    std::vector<Vec3> vertices;
    #else
    std::vector<Vec2> vertices;
    #endif
    std::vector<int>  indices;
    std::vector<int>  elements;

public:
    Tesselator();
    ~Tesselator();

    int init();
    void dispose();

    int AddContour( int size, const void* pointer, int stride, int count );

    template<typename T>
    int AddContour( std::vector<T> points );

    int Tesselate( TessWindingRule windingRule, TessElementType elementType, int polySize = 3);

private:
    //计算normal
    Vec3 ComputeNormal();
    void CheckOrientation();
    void ProjectPolygon();

    void MeshRefineDelaunay( TESSmesh *mesh );

    Index GetNeighbourFace(HalfEdge* edge);
    int OutputPolymesh( int elementType, int polySize);
    int OutputContours();
};


Tesselator::Tesselator() : mesh(), sweep()
{
    normal = Vec3();
    processCDT = false;
    reverseContours = false;
    vertexIndexCounter = 0;
}

Tesselator::~Tesselator()
{

}

int Tesselator::init()
{
    this->dispose();
    mesh.init();
    return 0;
}

void Tesselator::dispose()
{
    mesh.dispose();
    sweep.dispose();

    this->vertices.clear();
    this->indices.clear();
    this->elements.clear();

    vertexIndexCounter = 0;
}

// AddContour() - Adds a contour to be tesselated.
// The type of the vertex coordinates is assumed to be TESSreal.
// Parameters:
//   tess - pointer to tesselator object.
//   size - number of coordinates per vertex. Must be 2 or 3.
//   pointer - pointer to the first coordinate of the first vertex in the array.
//   stride - defines offset in bytes between consecutive vertices.
//   count - number of vertices in contour.
// Returns:
//   LIBTESS_OK if succeed, LIBTESS_ERROR if failed.
int Tesselator::AddContour( int size, const void* pointer, int stride, int count )
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

template<typename T>
int Tesselator::AddContour( std::vector<T> points )
{

}

// Tesselate() - tesselate contours.
// Parameters:
//   tess - pointer to tesselator object.
//   windingRule - winding rules used for tesselation, must be one of TessWindingRule.
//   elementType - defines the tesselation result element type, must be one of TessElementType.
//   polySize - defines maximum vertices per polygons if output is polygons.
// Returns:
//   LIBTESS_OK if succeed, LIBTESS_ERROR if failed.
int Tesselator::Tesselate( TessWindingRule windingRule, TessElementType elementType, int polySize)
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

    if( elementType == TESS_BOUNDARY_CONTOURS ) {
        /* output contours */
        LIBTESS_UNIT_TEST( errCode = OutputContours() );
    }
    else {
        /* output polygons */
        LIBTESS_UNIT_TEST( errCode = OutputPolymesh( elementType, polySize ) );
    }

    if( errCode != LIBTESS_OK ) {
        LIBTESS_LOG("Tesselator.Tesselate() : output error.");
        return LIBTESS_ERROR;
    }

    mesh.dispose();
    sweep.dispose();

    return LIBTESS_OK;
}

// Starting with a valid triangulation, uses the Edge Flip algorithm to
// refine the triangulation into a Constrained Delaunay Triangulation.
void Tesselator::MeshRefineDelaunay( TESSmesh *mesh )
{
    // At this point, we have a valid, but not optimal, triangulation.
    // We refine the triangulation using the Edge Flip algorithm
    //
    // 1) Find all internal edges
    // 2) Mark all dual edges
    // 3) insert all dual edges into a queue

    Face *f;
    std::stack<HalfEdge*> stack;
    HalfEdge *e;
    int maxFaces = 0, maxIter = 0, iter = 0;

    for( f = mesh->m_faceHead.next; f != &mesh->m_faceHead; f = f->next ) {
        if ( f->inside) {
            e = f->edge;
            do {
                e->mark = EdgeIsInternal(e); // Mark internal edges
                if (e->mark && !e->mirror->mark) stack.push( e ); // Insert into queue
                e = e->Lnext;
            } while (e != f->edge);
            maxFaces++;
        }
    }

    // The algorithm should converge on O(n^2), since the predicate is not robust,
    // we'll save guard against infinite loop.
    maxIter = maxFaces * maxFaces;

    // Pop stack until we find a reversed edge
    // Flip the reversed edge, and insert any of the four opposite edges
    // which are internal and not already in the stack (!marked)
    while (!stack.empty() && iter < maxIter) {
        e = stack.top();
        stack.pop();
        e->mark = e->mirror->mark = 0;
        if (!EdgeIsLocallyDelaunay(e)) {
            HalfEdge *edges[4];
            int i;
            mesh->FlipEdge(e);
            // for each opposite edge
            edges[0] = e->Lnext;
            edges[1] = e->Onext->mirror;
            edges[2] = e->mirror->Lnext;
            edges[3] = e->mirror->Onext->mirror;
            for (i = 0; i < 4; i++) {
                if (!edges[i]->mark && EdgeIsInternal(edges[i])) {
                    edges[i]->mark = edges[i]->mirror->mark = 1;
                    stack.push( edges[i] );
                }
            }
        }
        iter++;
    }
}

Index Tesselator::GetNeighbourFace(HalfEdge* edge)
{
    if (!edge->mirror->Lface)
        return INVALID_INDEX;
    if (!edge->mirror->Lface->inside)
        return INVALID_INDEX;
    return edge->mirror->Lface->n;
}

int Tesselator::OutputPolymesh( int elementType, int polySize )
{
    Vertex* v = 0;
    Face* f = 0;
    HalfEdge* edge = 0;
    int maxFaceCount = 0;
    int maxVertexCount = 0;
    int faceVerts, i;
    Index *elements = 0;

    // Assume that the input data is triangles now.
    // Try to merge as many polygons as possible
    if (polySize > 3) {
        if (!mesh.MergeConvexFaces( polySize )) {
            return LIBTESS_ERROR;
        }
    }

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
                maxVertexCount++;
            }
            faceVerts++;
            edge = edge->Lnext;
        }while (edge != f->edge);

        assert( faceVerts <= polySize );

        f->n = maxFaceCount;
        ++maxFaceCount;
    }

    //int elementCount = maxFaceCount;
    if (elementType == TESS_CONNECTED_POLYGONS)
        maxFaceCount *= 2;
//    tess->elements = (Index*)tess->alloc.memalloc( tess->alloc.userData,sizeof(Index) * maxFaceCount * polySize );
    this->elements.resize(maxFaceCount * polySize);


    int vertexCount = maxVertexCount;
//    tess->vertices = (TESSreal*)tess->alloc.memalloc( tess->alloc.userData, sizeof(TESSreal) * tess->vertexCount * vertexSize );
    this->vertices.resize(vertexCount);

//    tess->vertexIndices = (Index*)tess->alloc.memalloc( tess->alloc.userData, sizeof(Index) * tess->vertexCount );
    this->indices.resize(vertexCount);

    // Output vertices.
    for ( v = mesh.m_vtxHead.next; v != &mesh.m_vtxHead; v = v->next )
    {
        if ( v->n != INVALID_INDEX ){
            // Store coordinate
            #ifdef LIBTESS_USE_VEC3
            this->vertices[v->n].x = v->coords.x;
            this->vertices[v->n].y = v->coords.y;
            this->vertices[v->n].z = v->coords.z;
            #else
            this->vertices[v->n].x = v->coords.x;
            this->vertices[v->n].y = v->coords.y;
            #endif

            // Store vertex index.
            this->indices[v->n] = v->idx;
        }
    }

    // Output indices.
    elements = &this->elements[0];
    for ( f = mesh.m_faceHead.next; f != &mesh.m_faceHead; f = f->next )
    {
        if ( !f->inside ) continue;

        // Store polygon
        edge = f->edge;
        faceVerts = 0;
        do{
            v = edge->vertex;
            *elements++ = v->n;
            faceVerts++;
            edge = edge->Lnext;
        }while (edge != f->edge);

        // Fill unused.
        for (i = faceVerts; i < polySize; ++i) {
            *elements++ = INVALID_INDEX;
        }

        // Store polygon connectivity
        if ( elementType == TESS_CONNECTED_POLYGONS )
        {
            edge = f->edge;
            do{
                *elements++ = GetNeighbourFace( edge );
                edge = edge->Lnext;
            }while (edge != f->edge);
            // Fill unused.
            for (i = faceVerts; i < polySize; ++i) {
                *elements++ = INVALID_INDEX;
            }
        }
    }

    return LIBTESS_OK;
}

int Tesselator::OutputContours()
{
    Face *f = 0;
    HalfEdge *edge = 0;
    HalfEdge *start = 0;
    Vec3 *v = 0;
    Index *elements = 0;
    Index *vertInds = 0;
    int startVert = 0;
    int vertCount = 0;

    int vertexCount = 0;
    int elementCount = 0;

    for ( f = mesh.m_faceHead.next; f != &mesh.m_faceHead; f = f->next )
    {
        if ( !f->inside ) continue;

        start = edge = f->edge;
        do{
            ++vertexCount;
            edge = edge->Lnext;
        }while ( edge != start );

        ++elementCount;
    }

//    tess->elements = (Index*)tess->alloc.memalloc( tess->alloc.userData, sizeof(Index) * tess->elementCount * 2 );
    this->elements.resize(elementCount * 2);

//    tess->vertices = (Float*)tess->alloc.memalloc( tess->alloc.userData, sizeof(Float) * tess->vertexCount * vertexSize );
    this->vertices.resize(vertexCount);

//    tess->vertexIndices = (Index*)tess->alloc.memalloc( tess->alloc.userData, sizeof(Index) * tess->vertexCount );
    this->indices.resize(vertexCount);

    elements = &this->elements[0];
    vertInds = &this->indices[0];

    startVert = 0;

    for ( f = mesh.m_faceHead.next; f != &mesh.m_faceHead; f = f->next )
    {
        if ( !f->inside ) continue;

        vertCount = 0;
        start = edge = f->edge;
        do{
            v = &edge->vertex->coords;

            #ifdef LIBTESS_USE_VEC3
            this->vertices[vertCount].x = v->x;
            this->vertices[vertCount].y = v->y;
            this->vertices[vertCount].z = v->z;
            #else
            this->vertices[vertCount].x = v->x;
            this->vertices[vertCount].y = v->y;
            #endif

            *vertInds++ = edge->vertex->idx;
            ++vertCount;
            edge = edge->Lnext;
        }while ( edge != start );

        elements[0] = startVert;
        elements[1] = vertCount;
        elements += 2;

        startVert += vertCount;
    }

    return LIBTESS_OK;
}


#include "normal.inl"

}// end namespace libtess

#endif// end
