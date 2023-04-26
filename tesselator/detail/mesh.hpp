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

#ifndef LIBTESS_MESH_HPP
#define LIBTESS_MESH_HPP

#include "public.h"

/* The mesh operations below have three motivations: completeness,
 * convenience, and efficiency.  The basic mesh operations are MakeEdge,
 * Splice, and Delete.  All the other edge operations can be implemented
 * in terms of these.  The other operations are provided for convenience
 * and/or efficiency.
 *
 * When a face is split or a vertex is added, they are inserted into the
 * global list *before* the existing vertex or face (ie. e->Org or e->Lface).
 * This makes it easier to process all vertices or faces in the global lists
 * without worrying about processing the same data twice.  As a convenience,
 * when a face is split, the "inside" flag is copied from the old face.
 * Other internal data (v->data, v->activeRegion, f->data, f->marked,
 * f->trail, e->winding) is set to zero.
 *
 * ********************** Basic Edge Operations **************************
 *
 * __gl_meshMakeEdge( mesh ) creates one edge, two vertices, and a loop.
 * The loop (face) consists of the two new half-edges.
 *
 * __gl_meshSplice( eOrg, eDst ) is the basic operation for changing the
 * mesh connectivity and topology.  It changes the mesh so that
 *    eOrg->Onext <- OLD( eDst->Onext )
 *    eDst->Onext <- OLD( eOrg->Onext )
 * where OLD(...) means the value before the meshSplice operation.
 *
 * This can have two effects on the vertex structure:
 *  - if eOrg->Org != eDst->Org, the two vertices are merged together
 *  - if eOrg->Org == eDst->Org, the origin is split into two vertices
 * In both cases, eDst->Org is changed and eOrg->Org is untouched.
 *
 * Similarly (and independently) for the face structure,
 *  - if eOrg->Lface == eDst->Lface, one loop is split into two
 *  - if eOrg->Lface != eDst->Lface, two distinct loops are joined into one
 * In both cases, eDst->Lface is changed and eOrg->Lface is unaffected.
 *
 * __gl_meshDelete( eDel ) removes the edge eDel.  There are several cases:
 * if (eDel->Lface != eDel->Rface), we join two loops into one; the loop
 * eDel->Lface is deleted.  Otherwise, we are splitting one loop into two;
 * the newly created loop will contain eDel->Dst.  If the deletion of eDel
 * would create isolated vertices, those are deleted as well.
 *
 * ********************** Other Edge Operations **************************
 *
 * __gl_meshAddEdgeVertex( eOrg ) creates a new edge eNew such that
 * eNew == eOrg->Lnext, and eNew->Dst is a newly created vertex.
 * eOrg and eNew will have the same left face.
 *
 * __gl_meshSplitEdge( eOrg ) splits eOrg into two edges eOrg and eNew,
 * such that eNew == eOrg->Lnext.  The new vertex is eOrg->Dst == eNew->Org.
 * eOrg and eNew will have the same left face.
 *
 * __gl_meshConnect( eOrg, eDst ) creates a new edge from eOrg->Dst
 * to eDst->Org, and returns the corresponding half-edge eNew.
 * If eOrg->Lface == eDst->Lface, this splits one loop into two,
 * and the newly created loop is eNew->Lface.  Otherwise, two disjoint
 * loops are merged into one, and the loop eDst->Lface is destroyed.
 *
 * ************************ Other Operations *****************************
 *
 * __gl_meshNewMesh() creates a new mesh with no edges, no vertices,
 * and no loops (what we usually call a "face").
 *
 * __gl_meshUnion( mesh1, mesh2 ) forms the union of all structures in
 * both meshes, and returns the new mesh (the old meshes are destroyed).
 *
 * __gl_meshDeleteMesh( mesh ) will free all storage for any valid mesh.
 *
 * __gl_meshZapFace( fZap ) destroys a face and removes it from the
 * global face list.  All edges of fZap will have a NULL pointer as their
 * left face.  Any edges which also have a NULL pointer as their right face
 * are deleted entirely (along with any isolated vertices this produces).
 * An entire mesh can be deleted by zapping its faces, one at a time,
 * in any order.  Zapped faces cannot be used in further mesh operations!
 *
 * __gl_meshCheckMesh( mesh ) checks a mesh for self-consistency.
 */

namespace libtess {

struct Vertex;
struct Face;
struct HalfEdge;
struct ActiveRegion;

int VertexIsCCW(Vertex *u, Vertex *v, Vertex *w);
int CountFaceVertices(Face* face);
bool EdgeIsInternal(HalfEdge* e);

struct Vertex
{
    Vertex   *next;     /* next vertex (never NULL) */
    Vertex   *prev;     /* previous vertex (never NULL) */
    HalfEdge *edge;     /* a half-edge with this origin */

    /* Internal data (keep hidden) */
    Vec3 coords;        /* vertex location in 3D */
    Float s, t;         /* projection onto the sweep plane */
    int pqHandle;       /* to allow deletion from priority queue */
    Index n;            /* to allow identify unique vertices */
    Index idx;          /* to allow map result to original verts */

    Vertex()
    {
        memset(this, 0, sizeof(*this));
    }
};

struct Face
{
    Face     *next;     /* next face (never NULL) */
    Face     *prev;     /* previous face (never NULL) */
    HalfEdge *edge;     /* a half edge with this left face */

    /* Internal data (keep hidden) */
    Face *trail;        /* "stack" for conversion to strips */
    Index n;            /* to allow identiy unique faces */
    Bool marked;        /* flag for conversion to strips */
    Bool inside;        /* this face is in the polygon interior */

    Face()
    {
        memset(this, 0, sizeof(*this));
    }
};

struct HalfEdge
{
    HalfEdge *next;     /* doubly-linked list (prev==Sym->next) */
    HalfEdge *mirror;   /* same edge, opposite direction */
    HalfEdge *Onext;    /* next edge CCW around origin */
    HalfEdge *Lnext;    /* next edge CCW around left face */
    Vertex   *vertex;   /* origin vertex (Overtex too long) */
    Face     *Lface;    /* left face */

    /* Internal data (keep hidden) */
    ActiveRegion *activeRegion; /* a region with this upper edge (sweep.c) */
    int winding;        /* change in winding number when crossing
                           from the right face to the left face */
    int mark;           /* Used by the Edge Flip algorithm */

    HalfEdge()
    {
        memset(this, 0, sizeof(*this));
    }
};

typedef std::pair<HalfEdge, HalfEdge> EdgePair;

//#define Rface   Sym->Lface
//#define Dst     Sym->Org

//#define Oprev   Sym->Lnext
//#define Lprev   Onext->Sym
//#define Dprev   Lnext->Sym --
//#define Rprev   Sym->Onext
//#define Dnext   Rprev->Sym  /* 3 pointers */
//#define Rnext   Oprev->Sym  /* 3 pointers -- */

class Mesh
{
public:
    Vertex   m_vtxHead;     /* dummy header for vertex list  */
    Face     m_faceHead;    /* dummy header for face list    */
    HalfEdge m_edgeHead;    /* dummy header for edge list    */
    HalfEdge m_edgeHeadst;  /* and its symmetric counterpart */

    pool<Vertex, LIBTESS_PAGE_SIZE> vtxbuf;
    pool<Face, LIBTESS_PAGE_SIZE> facebuf;
    pool<EdgePair, LIBTESS_PAGE_SIZE> edgebuf;

public:
    Mesh();
    ~Mesh();
    int init();
    void dispose();

    bool empty()const { return m_vtxHead.next == &m_vtxHead && m_vtxHead.prev == &m_vtxHead; }
    AABB ComputeAABB();

    HalfEdge * MakeEdge();
    HalfEdge * MakeEdge(HalfEdge *eNext);

    void KillVertex(Vertex *vDel, Vertex *newOrg);
    void KillFace(Face *fDel, Face *newLface);
    void KillEdge(HalfEdge *eDel);

    int Splice(HalfEdge *eOrg, HalfEdge *eDst);
    int DeleteEdge(HalfEdge *eDel);

    HalfEdge * AddEdgeVertex(HalfEdge *eOrg);
    HalfEdge * SplitEdge(HalfEdge *eOrg);
    HalfEdge * Connect(HalfEdge *eOrg, HalfEdge *eDst);

    void ZeroAllFace(Face *fZap);

    // libtess2
    bool MergeConvexFaces(int maxVertsPerFace);
    void FlipEdge(HalfEdge *edge);

    void CheckMesh();

private:
    void MakeVertex(Vertex *newVertex, HalfEdge *eOrig, Vertex *vNext);
    void MakeFace(Face *newFace, HalfEdge *eOrig, Face *fNext);
    void SpliceEdge(HalfEdge *a, HalfEdge *b);

};

//
// source
//

LIBTESS_INLINE Mesh::Mesh()
{
    this->init();
}

LIBTESS_INLINE Mesh::~Mesh()
{
    this->dispose();
}

LIBTESS_INLINE int Mesh::init()
{
    Vertex *v;
    Face *f;
    HalfEdge *e;
    HalfEdge *eSym;

    v = &this->m_vtxHead;
    f = &this->m_faceHead;
    e = &this->m_edgeHead;
    eSym = &this->m_edgeHeadst;

    v->next = v->prev = v;
    v->edge = NULL;

    f->next = f->prev = f;
    f->edge = NULL;
    f->trail = NULL;
    f->marked = FALSE;
    f->inside = FALSE;

    e->next = e;
    e->mirror = eSym;
    e->Onext = NULL;
    e->Lnext = NULL;
    e->vertex = NULL;
    e->Lface = NULL;
    e->winding = 0;
    e->activeRegion = NULL;

    eSym->next = eSym;
    eSym->mirror = e;
    eSym->Onext = NULL;
    eSym->Lnext = NULL;
    eSym->vertex = NULL;
    eSym->Lface = NULL;
    eSym->winding = 0;
    eSym->activeRegion = NULL;

    return 0;
}

LIBTESS_INLINE void Mesh::dispose()
{
    vtxbuf.dispose();
    facebuf.dispose();
    edgebuf.dispose();
    this->init();
}

LIBTESS_INLINE AABB Mesh::ComputeAABB()
{
    if (this->empty()) {
        return AABB();
    }

    AABB aabb;
    Vertex *v, *vHead = &m_vtxHead;
    for (v = vHead->next; v != vHead; v = v->next) {
        if (v->s < aabb.amin) aabb.amin = v->s;
        if (v->s > aabb.amax) aabb.amax = v->s;
        if (v->t < aabb.bmin) aabb.bmin = v->t;
        if (v->t > aabb.bmax) aabb.bmax = v->t;
    }
    return aabb;
}


/* MakeVertex( newVertex, eOrig, vNext ) attaches a new vertex and makes it the
 * origin of all edges in the vertex loop to which eOrig belongs. "vNext" gives
 * a place to insert the new vertex in the global vertex list.  We insert
 * the new vertex *before* vNext so that algorithms which walk the vertex
 * list will not see the newly created vertices.
 */
LIBTESS_INLINE void Mesh::MakeVertex(Vertex *newVertex, HalfEdge *eOrig, Vertex *vNext)
{
    HalfEdge *e;
    Vertex *vPrev;
    Vertex *vNew = newVertex;

    assert(vNew != NULL);

    /* insert in circular doubly-linked list before vNext */
    vPrev = vNext->prev;
    vNew->prev = vPrev;
    vPrev->next = vNew;
    vNew->next = vNext;
    vNext->prev = vNew;

    vNew->edge = eOrig;
    /* leave coords, s, t undefined */

    /* fix other edges on this vertex loop */
    e = eOrig;
    do {
        e->vertex = vNew;
        e = e->Onext;
    } while (e != eOrig);
}

/* MakeFace( newFace, eOrig, fNext ) attaches a new face and makes it the left
 * face of all edges in the face loop to which eOrig belongs.  "fNext" gives
 * a place to insert the new face in the global face list.  We insert
 * the new face *before* fNext so that algorithms which walk the face
 * list will not see the newly created faces.
 */
LIBTESS_INLINE void Mesh::MakeFace(Face *newFace, HalfEdge *eOrig, Face *fNext)
{
    HalfEdge *e;
    Face *fPrev;
    Face *fNew = newFace;

    assert(fNew != NULL);

    /* insert in circular doubly-linked list before fNext */
    fPrev = fNext->prev;
    fNew->prev = fPrev;
    fPrev->next = fNew;
    fNew->next = fNext;
    fNext->prev = fNew;

    fNew->edge = eOrig;
    fNew->trail = NULL;
    fNew->marked = FALSE;

    /* The new face is marked "inside" if the old one was.  This is a
     * convenience for the common case where a face has been split in two.
     */
    fNew->inside = fNext->inside;

    /* fix other edges on this face loop */
    e = eOrig;
    do {
        e->Lface = fNew;
        e = e->Lnext;
    } while (e != eOrig);
}

/* __gl_meshMakeEdge creates one edge, two vertices, and a loop (face).
 * The loop consists of the two new half-edges.
 */
LIBTESS_INLINE HalfEdge * Mesh::MakeEdge()
{
    Vertex *newVertex1 = vtxbuf.allocate();
    Vertex *newVertex2 = vtxbuf.allocate();
    Face *newFace = facebuf.allocate();
    HalfEdge *e;

    /* if any one is null then all get freed */
    if (newVertex1 == NULL || newVertex2 == NULL || newFace == NULL) {
        if (newVertex1 != NULL)vtxbuf.deallocate(newVertex1);
        if (newVertex2 != NULL)vtxbuf.deallocate(newVertex2);
        if (newFace != NULL) facebuf.deallocate(newFace);
        return NULL;
    }

    e = this->MakeEdge(&m_edgeHead);
    if (e == NULL) {
        vtxbuf.deallocate(newVertex1);
        vtxbuf.deallocate(newVertex2);
        facebuf.deallocate(newFace);
        return NULL;
    }

    MakeVertex(newVertex1, e, &m_vtxHead);
    MakeVertex(newVertex2, e->mirror, &m_vtxHead);
    MakeFace(newFace, e, &m_faceHead);
    return e;
}

/* MakeEdge creates a new pair of half-edges which form their own loop.
 * No vertex or face structures are allocated, but these must be assigned
 * before the current edge operation is completed.
 */
LIBTESS_INLINE HalfEdge * Mesh::MakeEdge(HalfEdge *eNext)
{
    HalfEdge *e;
    HalfEdge *eMirror;
    HalfEdge *ePrev;

    EdgePair *pair = edgebuf.allocate();// (EdgePair *)bucketAlloc( this->edgeBucket );
    if (pair == NULL) {
        return NULL;
    }

    e = &pair->first;
    eMirror = &pair->second;

    /* Make sure eNext points to the first edge of the edge pair */
    if (eNext->mirror < eNext) { eNext = eNext->mirror; }

    /* Insert in circular doubly-linked list before eNext.
     * Note that the prev pointer is stored in Sym->next.
     */
    ePrev = eNext->mirror->next;
    eMirror->next = ePrev;
    ePrev->mirror->next = e;
    e->next = eNext;
    eNext->mirror->next = eMirror;

    e->mirror = eMirror;
    e->Onext = e;
    e->Lnext = eMirror;
    e->vertex = NULL;
    e->Lface = NULL;
    e->winding = 0;
    e->activeRegion = NULL;
    e->mark = 0;

    eMirror->mirror = e;
    eMirror->Onext = eMirror;
    eMirror->Lnext = e;
    eMirror->vertex = NULL;
    eMirror->Lface = NULL;
    eMirror->winding = 0;
    eMirror->activeRegion = NULL;
    eMirror->mark = 0;

    return e;
}

/*
 * KillVertex( vDel )
 * destroys a vertex and removes it from the global vertex list.
 * It updates the vertex loop to point to a given new vertex.
 * 销毁顶点并将其从全局顶点列表中删除。
 * 更新顶点循环以指向给定的新顶点。
 */
LIBTESS_INLINE void Mesh::KillVertex(Vertex *vDel, Vertex *newOrg)
{
    HalfEdge *e, *eStart = vDel->edge;
    Vertex *vPrev, *vNext;

    /* change the origin of all affected edges */
    e = eStart;
    do {
        e->vertex = newOrg;
        e = e->Onext;
    } while (e != eStart);

    /* delete from circular doubly-linked list */
    vPrev = vDel->prev;
    vNext = vDel->next;
    vNext->prev = vPrev;
    vPrev->next = vNext;

    vtxbuf.deallocate(vDel);
}

/* KillFace( fDel ) destroys a face and removes it from the global face
 * list.  It updates the face loop to point to a given new face.
 */
LIBTESS_INLINE void Mesh::KillFace(Face *fDel, Face *newLface)
{
    HalfEdge *e, *eStart = fDel->edge;
    Face *fPrev, *fNext;

    /* change the left face of all affected edges */
    e = eStart;
    do {
        e->Lface = newLface;
        e = e->Lnext;
    } while (e != eStart);

    /* delete from circular doubly-linked list */
    fPrev = fDel->prev;
    fNext = fDel->next;
    fNext->prev = fPrev;
    fPrev->next = fNext;

    facebuf.deallocate(fDel);
}

/* KillEdge( eDel ) destroys an edge (the half-edges eDel and eDel->Sym),
 * and removes from the global edge list.
 */
LIBTESS_INLINE void Mesh::KillEdge(HalfEdge *eDel)
{
    HalfEdge *ePrev, *eNext;

    /* Half-edges are allocated in pairs, see EdgePair above */
    if (eDel->mirror < eDel) { eDel = eDel->mirror; }

    /* delete from circular doubly-linked list */
    eNext = eDel->next;
    ePrev = eDel->mirror->next;
    eNext->mirror->next = ePrev;
    ePrev->mirror->next = eNext;

    edgebuf.deallocate((EdgePair*) eDel);
}

/* Splice( a, b ) is best described by the Guibas/Stolfi paper or the
 * CS348a notes (see mesh.h).  Basically it modifies the mesh so that
 * a->Onext and b->Onext are exchanged.  This can have various effects
 * depending on whether a and b belong to different face or vertex rings.
 * For more explanation see __gl_meshSplice() below.
 */
LIBTESS_INLINE void Mesh::SpliceEdge(HalfEdge *a, HalfEdge *b)
{
    HalfEdge *aOnext = a->Onext;
    HalfEdge *bOnext = b->Onext;

    aOnext->mirror->Lnext = b;
    bOnext->mirror->Lnext = a;
    a->Onext = bOnext;
    b->Onext = aOnext;
}

/* __gl_meshSplice( eOrg, eDst ) is the basic operation for changing the
 * mesh connectivity and topology.  It changes the mesh so that
 *    eOrg->Onext <- OLD( eDst->Onext )
 *    eDst->Onext <- OLD( eOrg->Onext )
 * where OLD(...) means the value before the meshSplice operation.
 *
 * This can have two effects on the vertex structure:
 *  - if eOrg->Org != eDst->Org, the two vertices are merged together
 *  - if eOrg->Org == eDst->Org, the origin is split into two vertices
 * In both cases, eDst->Org is changed and eOrg->Org is untouched.
 *
 * Similarly (and independently) for the face structure,
 *  - if eOrg->Lface == eDst->Lface, one loop is split into two
 *  - if eOrg->Lface != eDst->Lface, two distinct loops are joined into one
 * In both cases, eDst->Lface is changed and eOrg->Lface is unaffected.
 *
 * Some special cases:
 * If eDst == eOrg, the operation has no effect.
 * If eDst == eOrg->Lnext, the new face will have a single edge.
 * If eDst == eOrg->Lprev, the old face will have a single edge.
 * If eDst == eOrg->Onext, the new vertex will have a single edge.
 * If eDst == eOrg->Oprev, the old vertex will have a single edge.
 */
LIBTESS_INLINE int Mesh::Splice(HalfEdge *eOrg, HalfEdge *eDst)
{
    int joiningLoops = FALSE;
    int joiningVertices = FALSE;

    if (eOrg == eDst) return 1;

    if (eDst->vertex != eOrg->vertex) {
        /* We are merging two disjoint vertices -- destroy eDst->Org */
        joiningVertices = TRUE;
        this->KillVertex(eDst->vertex, eOrg->vertex);
    }

    if (eDst->Lface != eOrg->Lface) {
        /* We are connecting two disjoint loops -- destroy eDst->Lface */
        joiningLoops = TRUE;
        this->KillFace(eDst->Lface, eOrg->Lface);
    }

    /* Change the edge structure */
    SpliceEdge(eDst, eOrg);

    if (!joiningVertices) {
        Vertex *newVertex = vtxbuf.allocate();
        if (newVertex == NULL) {
            return 0;
        }

        /* We split one vertex into two -- the new vertex is eDst->Org.
        * Make sure the old vertex points to a valid half-edge.
        */
        MakeVertex(newVertex, eDst, eOrg->vertex);
        eOrg->vertex->edge = eOrg;
    }
    if (!joiningLoops) {
        Face *newFace = facebuf.allocate();
        if (newFace == NULL) {
            return 0;
        }

        /* We split one loop into two -- the new loop is eDst->Lface.
        * Make sure the old face points to a valid half-edge.
        */
        MakeFace(newFace, eDst, eOrg->Lface);
        eOrg->Lface->edge = eOrg;
    }

    return 1;
}

/* __gl_meshDelete( eDel ) removes the edge eDel.  There are several cases:
 * if (eDel->Lface != eDel->Rface), we join two loops into one; the loop
 * eDel->Lface is deleted.  Otherwise, we are splitting one loop into two;
 * the newly created loop will contain eDel->Dst.  If the deletion of eDel
 * would create isolated vertices, those are deleted as well.
 *
 * This function could be implemented as two calls to __gl_meshSplice
 * plus a few calls to memFree, but this would allocate and delete
 * unnecessary vertices and faces.
 */
LIBTESS_INLINE int Mesh::DeleteEdge(HalfEdge *eDel)
{
    HalfEdge *eDelSym = eDel->mirror;
    int joiningLoops = FALSE;

    /* First step: disconnect the origin vertex eDel->Org.  We make all
      * changes to get a consistent mesh in this "intermediate" state.
       */
    if (eDel->Lface != eDel->mirror->Lface) {
        /* We are joining two loops into one -- remove the left face */
        joiningLoops = TRUE;
        this->KillFace(eDel->Lface, eDel->mirror->Lface);
    }

    if (eDel->Onext == eDel) {
        this->KillVertex(eDel->vertex, NULL);
    }
    else {
        /* Make sure that eDel->Org and eDel->Rface point to valid half-edges */
        eDel->mirror->Lface->edge = eDel->mirror->Lnext;
        eDel->vertex->edge = eDel->Onext;

        SpliceEdge(eDel, eDel->mirror->Lnext);
        if (!joiningLoops) {
            Face *newFace = facebuf.allocate();//allocate()
            if (newFace == NULL) {
                return 0;
            }

            /* We are splitting one loop into two -- create a new loop for eDel. */
            MakeFace(newFace, eDel, eDel->Lface);
        }
    }

    /* Claim: the mesh is now in a consistent state, except that eDel->Org
     * may have been deleted.  Now we disconnect eDel->Dst.
     */
    if (eDelSym->Onext == eDelSym) {
        this->KillVertex(eDelSym->vertex, NULL);
        this->KillFace(eDelSym->Lface, NULL);
    }
    else {
     /* Make sure that eDel->Dst and eDel->Lface point to valid half-edges */
        eDel->Lface->edge = eDelSym->mirror->Lnext;
        eDelSym->vertex->edge = eDelSym->Onext;
        SpliceEdge(eDelSym, eDelSym->mirror->Lnext);
    }

    /* Any isolated vertices or faces have already been freed. */
    //KillEdge( mesh, eDel );
    this->KillEdge(eDel);

    return 1;
}


/* __gl_meshAddEdgeVertex( eOrg ) creates a new edge eNew such that
 * eNew == eOrg->Lnext, and eNew->Dst is a newly created vertex.
 * eOrg and eNew will have the same left face.
 */
LIBTESS_INLINE HalfEdge * Mesh::AddEdgeVertex(HalfEdge *eOrg)
{
    HalfEdge *eNewSym;
    HalfEdge *eNew = this->MakeEdge(eOrg);
    if (eNew == NULL) {
        return NULL;
    }

    eNewSym = eNew->mirror;

    /* Connect the new edge appropriately */
    SpliceEdge(eNew, eOrg->Lnext);

    /* Set the vertex and face information */
    eNew->vertex = eOrg->mirror->vertex;
    {
        Vertex *newVertex = vtxbuf.allocate();
        if (newVertex == NULL) {
            return NULL;
        }

        MakeVertex(newVertex, eNewSym, eNew->vertex);
    }
    eNew->Lface = eNewSym->Lface = eOrg->Lface;

    return eNew;
}

/*
 * __gl_meshSplitEdge( eOrg )
 * splits eOrg into two edges eOrg and eNew,
 * such that eNew == eOrg->Lnext.  The new vertex is eOrg->Dst == eNew->Org.
 * eOrg and eNew will have the same left face.
 *
 * 将 eOrg 分为两个边 eOrg 和 eNew，
 * 设 eNew == eOrg->Lnext。新顶点是 eOrg->Dst == eNew->Org。
 * eOrg 和 eNew 拥有相同的左面。
 */
LIBTESS_INLINE HalfEdge * Mesh::SplitEdge(HalfEdge *eOrg)
{
    HalfEdge *eNew;
    HalfEdge *tempHalfEdge = this->AddEdgeVertex(eOrg);
    if (tempHalfEdge == NULL) {
        return NULL;
    }

    eNew = tempHalfEdge->mirror;

    /* Disconnect eOrg from eOrg->Dst and connect it to eNew->Org */
    SpliceEdge(eOrg->mirror, eOrg->mirror->mirror->Lnext);
    SpliceEdge(eOrg->mirror, eNew);

    /* Set the vertex and face information */
    eOrg->mirror->vertex = eNew->vertex;
    eNew->mirror->vertex->edge = eNew->mirror;    /* may have pointed to eOrg->Sym */
    eNew->mirror->Lface = eOrg->mirror->Lface;
    eNew->winding = eOrg->winding;    /* copy old winding information */
    eNew->mirror->winding = eOrg->mirror->winding;

    return eNew;
}


/* __gl_meshConnect( eOrg, eDst ) creates a new edge from eOrg->Dst
 * to eDst->Org, and returns the corresponding half-edge eNew.
 * If eOrg->Lface == eDst->Lface, this splits one loop into two,
 * and the newly created loop is eNew->Lface.  Otherwise, two disjoint
 * loops are merged into one, and the loop eDst->Lface is destroyed.
 *
 * If (eOrg == eDst), the new face will have only two edges.
 * If (eOrg->Lnext == eDst), the old face is reduced to a single edge.
 * If (eOrg->Lnext->Lnext == eDst), the old face is reduced to two edges.
 */
LIBTESS_INLINE HalfEdge * Mesh::Connect(HalfEdge *eOrg, HalfEdge *eDst)
{
    HalfEdge *eNewSym;
    int joiningLoops = FALSE;
    HalfEdge *eNew = this->MakeEdge(eOrg);
    if (eNew == NULL) {
        return NULL;
    }

    eNewSym = eNew->mirror;

    if (eDst->Lface != eOrg->Lface) {
        /* We are connecting two disjoint loops -- destroy eDst->Lface */
        joiningLoops = TRUE;
        this->KillFace(eDst->Lface, eOrg->Lface);
    }

    /* Connect the new edge appropriately */
    SpliceEdge(eNew, eOrg->Lnext);
    SpliceEdge(eNewSym, eDst);

    /* Set the vertex and face information */
    eNew->vertex = eOrg->mirror->vertex;
    eNewSym->vertex = eDst->vertex;
    eNew->Lface = eNewSym->Lface = eOrg->Lface;

    /* Make sure the old face points to a valid half-edge */
    eOrg->Lface->edge = eNewSym;

    if (!joiningLoops) {
        Face *newFace = facebuf.allocate();// (Face*)bucketAlloc( this->faceBucket );
        if (newFace == NULL) {
            return NULL;
        }

        /* We split one loop into two -- the new loop is eNew->Lface */
        MakeFace(newFace, eNew, eOrg->Lface);
    }
    return eNew;
}

/* __gl_meshZapFace( fZap ) destroys a face and removes it from the
 * global face list.  All edges of fZap will have a NULL pointer as their
 * left face.  Any edges which also have a NULL pointer as their right face
 * are deleted entirely (along with any isolated vertices this produces).
 * An entire mesh can be deleted by zapping its faces, one at a time,
 * in any order.  Zapped faces cannot be used in further mesh operations!
 * 销毁面并将其从全局面列表中移除。fZap 的所有边都将空指针作为其左面。
 * 任何也具有空指针作为其右面的边缘都将完全删除（以及由此产生的任何隔离顶点）。
 * 可以通过以任何顺序一次一个地 zapping 其面来删除整个网格。ZAAPPED faces 不能用于其他网格操作！
 */
LIBTESS_INLINE void Mesh::ZeroAllFace(Face *fZap)
{
    HalfEdge *eStart = fZap->edge;
    HalfEdge *e, *eNext, *eSym;
    Face *fPrev, *fNext;

    /* walk around face, deleting edges whose right face is also NULL */
    eNext = eStart->Lnext;
    do {
        e = eNext;
        eNext = e->Lnext;

        e->Lface = NULL;
        if (e->mirror->Lface == NULL) {
            /* delete the edge -- see MeshDelete above */

            if (e->Onext == e) {
                this->KillVertex(e->vertex, NULL);
            }
            else {
             /* Make sure that e->Org points to a valid half-edge */
                e->vertex->edge = e->Onext;
                SpliceEdge(e, e->mirror->Lnext);
            }
            eSym = e->mirror;
            if (eSym->Onext == eSym) {
                this->KillVertex(eSym->vertex, NULL);
            }
            else {
             /* Make sure that eSym->Org points to a valid half-edge */
                eSym->vertex->edge = eSym->Onext;
                SpliceEdge(eSym, eSym->mirror->Lnext);
            }
            this->KillEdge(e);
        }
    } while (e != eStart);

    /* delete from circular doubly-linked list */
    fPrev = fZap->prev;
    fNext = fZap->next;
    fNext->prev = fPrev;
    fPrev->next = fNext;

    facebuf.deallocate(fZap);
}

// libtess2
LIBTESS_INLINE bool Mesh::MergeConvexFaces(int maxVertsPerFace)
{
    HalfEdge *e, *eNext, *eSym;
    //HalfEdge *eHead = eHead; 2020-11-6
    HalfEdge *eHead = &m_edgeHead;
    Vertex *va, *vb, *vc, *vd, *ve, *vf;
    int leftNv, rightNv;

    for (e = eHead->next; e != eHead; e = eNext) {
        eNext = e->next;
        eSym = e->mirror;
        if (!eSym)
            continue;

        // Both faces must be inside
        if (!e->Lface || !e->Lface->inside)
            continue;
        if (!eSym->Lface || !eSym->Lface->inside)
            continue;

        leftNv = CountFaceVertices(e->Lface);
        rightNv = CountFaceVertices(eSym->Lface);
        if ((leftNv + rightNv - 2) > maxVertsPerFace)
            continue;

        // Merge if the resulting poly is convex.
        //
        //      vf--ve--vd
        //          ^|
        // left   e ||   right
        //          |v
        //      va--vb--vc

        va = e->Onext->mirror->vertex;
        vb = e->vertex;
        vc = e->mirror->Lnext->mirror->vertex;

        vd = e->mirror->Onext->mirror->vertex;
        ve = e->mirror->vertex;
        vf = e->Lnext->mirror->vertex;

        if (VertexIsCCW(va, vb, vc) && VertexIsCCW(vd, ve, vf)) {
            if (e == eNext || e == eNext->mirror) {
                eNext = eNext->next;
            }
            if (!this->DeleteEdge(e)) {
                return false;
            }
        }
    }

    return true;
}

// libtess2
LIBTESS_INLINE void Mesh::FlipEdge(HalfEdge *edge)
{
    HalfEdge *a0 = edge;
    HalfEdge *a1 = a0->Lnext;
    HalfEdge *a2 = a1->Lnext;
    HalfEdge *b0 = edge->mirror;
    HalfEdge *b1 = b0->Lnext;
    HalfEdge *b2 = b1->Lnext;

    Vertex *aOrg = a0->vertex;
    Vertex *aOpp = a2->vertex;
    Vertex *bOrg = b0->vertex;
    Vertex *bOpp = b2->vertex;

    Face *fa = a0->Lface;
    Face *fb = b0->Lface;

    assert(EdgeIsInternal(edge));
    assert(a2->Lnext == a0);
    assert(b2->Lnext == b0);

    a0->vertex = bOpp;
    a0->Onext = b1->mirror;
    b0->vertex = aOpp;
    b0->Onext = a1->mirror;
    a2->Onext = b0;
    b2->Onext = a0;
    b1->Onext = a2->mirror;
    a1->Onext = b2->mirror;

    a0->Lnext = a2;
    a2->Lnext = b1;
    b1->Lnext = a0;

    b0->Lnext = b2;
    b2->Lnext = a1;
    a1->Lnext = b0;

    a1->Lface = fb;
    b1->Lface = fa;

    fa->edge = a0;
    fb->edge = b0;

    if (aOrg->edge == a0) aOrg->edge = b1;
    if (bOrg->edge == b0) bOrg->edge = a1;

    assert(a0->Lnext->Onext->mirror == a0);
    assert(a0->Onext->mirror->Lnext == a0);
    assert(a0->vertex->edge->vertex == a0->vertex);


    assert(a1->Lnext->Onext->mirror == a1);
    assert(a1->Onext->mirror->Lnext == a1);
    assert(a1->vertex->edge->vertex == a1->vertex);

    assert(a2->Lnext->Onext->mirror == a2);
    assert(a2->Onext->mirror->Lnext == a2);
    assert(a2->vertex->edge->vertex == a2->vertex);

    assert(b0->Lnext->Onext->mirror == b0);
    assert(b0->Onext->mirror->Lnext == b0);
    assert(b0->vertex->edge->vertex == b0->vertex);

    assert(b1->Lnext->Onext->mirror == b1);
    assert(b1->Onext->mirror->Lnext == b1);
    assert(b1->vertex->edge->vertex == b1->vertex);

    assert(b2->Lnext->Onext->mirror == b2);
    assert(b2->Onext->mirror->Lnext == b2);
    assert(b2->vertex->edge->vertex == b2->vertex);

    assert(aOrg->edge->vertex == aOrg);
    assert(bOrg->edge->vertex == bOrg);

    assert(a0->mirror->Lnext->Onext->vertex == a0->vertex);
}

#ifdef NDEBUG

LIBTESS_INLINE void Mesh::CheckMesh()
{
}

#else

/* __gl_meshCheckMesh( mesh ) checks a mesh for self-consistency.
 */
LIBTESS_INLINE void Mesh::CheckMesh()
{
    Face *fHead = &m_faceHead;
    Vertex *vHead = &m_vtxHead;
    HalfEdge *eHead = &m_edgeHead;
    Face *f, *fPrev;
    Vertex *v, *vPrev;
    HalfEdge *e, *ePrev;

    for (fPrev = fHead; (f = fPrev->next) != fHead; fPrev = f) {
        assert(f->prev == fPrev);
        e = f->edge;
        do {
            assert(e->mirror != e);
            assert(e->mirror->mirror == e);
            assert(e->Lnext->Onext->mirror == e);
            assert(e->Onext->mirror->Lnext == e);
            assert(e->Lface == f);
            e = e->Lnext;
        } while (e != f->edge);
    }
    assert(f->prev == fPrev && f->edge == NULL);

    for (vPrev = vHead; (v = vPrev->next) != vHead; vPrev = v) {
        assert(v->prev == vPrev);
        e = v->edge;
        do {
            assert(e->mirror != e);
            assert(e->mirror->mirror == e);
            assert(e->Lnext->Onext->mirror == e);
            assert(e->Onext->mirror->Lnext == e);
            assert(e->vertex == v);
            e = e->Onext;
        } while (e != v->edge);
    }
    assert(v->prev == vPrev && v->edge == NULL);

    for (ePrev = eHead; (e = ePrev->next) != eHead; ePrev = e) {
        assert(e->mirror->next == ePrev->mirror);
        assert(e->mirror != e);
        assert(e->mirror->mirror == e);
        assert(e->vertex != NULL);
        assert(e->mirror->vertex != NULL);
        assert(e->Lnext->Onext->mirror == e);
        assert(e->Onext->mirror->Lnext == e);
    }
    assert(e->mirror->next == ePrev->mirror
        && e->mirror == &m_edgeHeadst
        && e->mirror->mirror == e
        && e->vertex == NULL && e->mirror->vertex == NULL
        && e->Lface == NULL && e->mirror->Lface == NULL);
}

#endif

#if 0
/* __gl_meshUnion( mesh1, mesh2 ) forms the union of all structures in
 * both meshes, and returns the new mesh (the old meshes are destroyed).
 */
GLUmesh *__gl_meshUnion(GLUmesh *mesh1, GLUmesh *mesh2)
{
    GLUface *f1 = &mesh1->fHead;
    Vertex *v1 = &mesh1->vHead;
    GLUhalfEdge *e1 = &mesh1->eHead;
    GLUface *f2 = &mesh2->fHead;
    Vertex *v2 = &mesh2->vHead;
    GLUhalfEdge *e2 = &mesh2->eHead;

    /* Add the faces, vertices, and edges of mesh2 to those of mesh1 */
    if (f2->next != f2) {
        f1->prev->next = f2->next;
        f2->next->prev = f1->prev;
        f2->prev->next = f1;
        f1->prev = f2->prev;
    }

    if (v2->next != v2) {
        v1->prev->next = v2->next;
        v2->next->prev = v1->prev;
        v2->prev->next = v1;
        v1->prev = v2->prev;
    }

    if (e2->next != e2) {
        e1->Sym->next->Sym->next = e2->next;
        e2->next->Sym->next = e1->Sym->next;
        e2->Sym->next->Sym->next = e1;
        e1->Sym->next = e2->Sym->next;
    }

    memFree(mesh2);
    return mesh1;
}
#endif

}// end namespace libtess

#endif// LIBTESS_MESH_HPP
