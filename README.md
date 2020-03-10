# libtess

this is refactored version of the original libtess which comes with the GLU reference implementation.  
this is C++ version(c++98), Using STL as memory pool.  
Independent version without any third-party libraries, header files only.

重构版本的gluTesselation库，C++98版本，使用了STL容器做内存缓存。  
不依赖任何三方库，独立版本，只需要引用头文件就能使用。  

琢磨了几个月，这次心血来潮，重构了代码，用了十来天时间（记不清了）。  
原版本是个C库，到处是跳转和#define，要把这些分散的函数封装成类，难度颇大，改动一处错误百出。  
libtess2作者对原代码重构过一次，但还是C版本，而且整体结构变动不大。  
这次重构，看着libtess2的代码，又看着原版代码，为了查找错误，前后对照了几遍！吐血！  
这次重构成C++版本，是第一步，之后会进行后续优化。  

也很纳闷，这么多年了，没有人重做一个OpenGL的三角形分解库，一直是SGI的这个libtess。  
GPC简单易用，但是开源协议不友好；poly2tri比libtess快，但效果不如libtess好，有时候很不稳定。  
希望我这个代码做个好的开头，有人能构建出更好更快的库，同时开源协议要友好。  

For OpenGL, For Open source !!!  

# main class:
<pre><code>
class Tesselator  
{  
  int init();  
  void dispose();  
  int AddContour( int size, const void* pointer, int stride, int count );  
  int Tesselate( TessWindingRule windingRule, TessElementType elementType, int polySize = 3);  
};
</code></pre>
# Exsample:
<pre><code>
struct vec2f  
{  
    float x, y;  
};  
  
//tesselate polygons
libtess::Tesselator tess;  
tess.init();  

std::vector<Vec2> points;  
//points.push_back(...)//add some points  
tess.AddContour( 2, &points[0], sizeof(Vec2), points.size() );  
//tess.AddContour( ... );  
tess.Tesselate( libtess::TESS_WINDING_ODD, libtess::TESS_TRIANGLES );  
  
//OpenGL drawing:
void draw_elements(int shape, const vec2f* vs, const int* indices, int size)  
{  
    glVertexPointer(2, GL_FLOAT, sizeof(vec2f), vs);  
    glEnableClientState(GL_VERTEX_ARRAY);  
    glDrawElements(shape, size, GL_UNSIGNED_INT, indices);  
    glDisableClientState(GL_VERTEX_ARRAY);  
}  
draw_elements(CGL_TRIANGLES, &tess.vertices[0], &tess.indices[0], tess.indices.size());
</code></pre>
