#ifndef LIBTESS_CDT_HPP
#define LIBTESS_CDT_HPP

#include "mesh.hpp"

namespace libtess {

/* Starting with a valid triangulation, uses the Edge Flip algorithm to
 * refine the triangulation into a Constrained Delaunay Triangulation.
 */
LIBTESS_INLINE void Tesselator::MeshRefineDelaunay(Mesh *mesh)
{
    /* At this point, we have a valid, but not optimal, triangulation.
     * We refine the triangulation using the Edge Flip algorithm
     *
     * 1) Find all internal edges
     * 2) Mark all dual edges
     * 3) insert all dual edges into a queue
     */

    Face *f;
    std::stack<HalfEdge*> stack;
    HalfEdge *e;
    int maxFaces = 0, maxIter = 0, iter = 0;

    for (f = mesh->m_faceHead.next; f != &mesh->m_faceHead; f = f->next) {
        if (f->inside) {
            e = f->edge;
            do {
                e->mark = EdgeIsInternal(e); // Mark internal edges
                if (e->mark && !e->mirror->mark) stack.push(e); // Insert into queue
                e = e->Lnext;
            } while (e != f->edge);
            maxFaces++;
        }
    }

    /* The algorithm should converge on O(n^2), since the predicate is not robust,
     * we'll save guard against infinite loop.
     */
    maxIter = maxFaces * maxFaces;

    /* Pop stack until we find a reversed edge
     * Flip the reversed edge, and insert any of the four opposite edges
     * which are internal and not already in the stack (!marked)
     */
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
                    stack.push(edges[i]);
                }
            }
        }
        iter++;
    }
}

#if 0

Index Tesselator::GetNeighbourFace(HalfEdge* edge)
{
    if (!edge->mirror->Lface)
        return INVALID_INDEX;
    if (!edge->mirror->Lface->inside)
        return INVALID_INDEX;
    return edge->mirror->Lface->n;
}

int Tesselator::OutputPolymesh(int elementType, int polySize)
{
    Vertex* v = 0;
    Face* f = 0;
    HalfEdge* edge = 0;
    int maxFaceCount = 0;
    int maxVertexCount = 0;
    int faceVerts, i;
    Index *elements = 0;

    /* Assume that the input data is triangles now.
     * Try to merge as many polygons as possible
     */
    if (polySize > 3) {
        if (!mesh.MergeConvexFaces(polySize)) {
            return LIBTESS_ERROR;
        }
    }

    // Mark unused
    for (v = mesh.m_vtxHead.next; v != &mesh.m_vtxHead; v = v->next)
        v->n = INVALID_INDEX;

    // Create unique IDs for all vertices and faces.
    for (f = mesh.m_faceHead.next; f != &mesh.m_faceHead; f = f->next) {
        f->n = INVALID_INDEX;
        if (!f->inside) continue;

        edge = f->edge;
        faceVerts = 0;
        do {
            v = edge->vertex;
            if (v->n == INVALID_INDEX) {
                v->n = maxVertexCount;
                maxVertexCount++;
            }
            faceVerts++;
            edge = edge->Lnext;
        } while (edge != f->edge);

        assert(faceVerts <= polySize);

        f->n = maxFaceCount;
        ++maxFaceCount;
    }

    //int elementCount = maxFaceCount;
    if (elementType == TESS_CONNECTED_POLYGONS)
        maxFaceCount *= 2;
    //tess->elements = (Index*)tess->alloc.memalloc( tess->alloc.userData,sizeof(Index) * maxFaceCount * polySize );
    this->elements.resize(maxFaceCount * polySize);


    int vertexCount = maxVertexCount;
    //tess->vertices = (TESSreal*)tess->alloc.memalloc( tess->alloc.userData, sizeof(TESSreal) * tess->vertexCount * vertexSize );
    this->vertices.resize(vertexCount);

    //tess->vertexIndices = (Index*)tess->alloc.memalloc( tess->alloc.userData, sizeof(Index) * tess->vertexCount );
    this->indices.resize(vertexCount);

    // Output vertices.
    for (v = mesh.m_vtxHead.next; v != &mesh.m_vtxHead; v = v->next) {
        if (v->n != INVALID_INDEX) {
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
    for (f = mesh.m_faceHead.next; f != &mesh.m_faceHead; f = f->next) {
        if (!f->inside) continue;

        // Store polygon
        edge = f->edge;
        faceVerts = 0;
        do {
            v = edge->vertex;
            *elements++ = v->n;
            faceVerts++;
            edge = edge->Lnext;
        } while (edge != f->edge);

        // Fill unused.
        for (i = faceVerts; i < polySize; ++i) {
            *elements++ = INVALID_INDEX;
        }

        // Store polygon connectivity
        if (elementType == TESS_CONNECTED_POLYGONS) {
            edge = f->edge;
            do {
                *elements++ = GetNeighbourFace(edge);
                edge = edge->Lnext;
            } while (edge != f->edge);
            // Fill unused.
            for (i = faceVerts; i < polySize; ++i) {
                *elements++ = INVALID_INDEX;
            }
        }
    }

    return LIBTESS_OK;
}

#endif

}// end namespace libtess

#endif //LIBTESS_CDT_HPP
