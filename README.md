# libtess

this is refactored version of the original libtess which comes with the GLU reference implementation.  
this is C++ version(c++98), Using STL as memory pool.  
重构版本的gluTesselation库，C++98版本，使用了STL容器做内存缓存。

# main class:

class Tesselator  
{  
  int init();  
  void dispose();  
  int AddContour( int size, const void* pointer, int stride, int count );  
  int Tesselate( TessWindingRule windingRule, TessElementType elementType, int polySize = 3);  
};

# Exsample:

struct vec2f  
{  
    float x, y;  
};  
  
libtess::Tesselator tess;  
tess.init();  

std::vector<Vec2> points;  
tess.AddContour( 2, &points[0], sizeof(Vec2), points.size() );  
tess.Tesselate( libtess::TESS_WINDING_ODD, libtess::TESS_POLYGONS );  
  
# OpenGL drawing:

void draw_elements(int shape, const vec2f* vs, const int* indices, int size)  
{  
    glVertexPointer(2, GL_FLOAT, sizeof(vec2f), vs);  
    glEnableClientState(GL_VERTEX_ARRAY);  
    glDrawElements(shape, size, GL_UNSIGNED_INT, indices);  
    glDisableClientState(GL_VERTEX_ARRAY);  
}  


draw_elements(CGL_TRIANGLES, &tess.vertices[0], &tess.indices[0], tess.indices.size());
