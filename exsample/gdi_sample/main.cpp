
#include "Window.h"
#include <gl/gl.h>
#include <glm/glm.hpp>          //用到glm库的vec结构，也可以不用

//自定义顶点类型
#define LIBTESS_CUSTOM_VECTOR
namespace libtess
{
typedef glm::vec2 Vec2;
typedef glm::vec3 Vec3;
}

//libtess库
#include <cgl/math/libtess/tess.hpp>

//路径类型
typedef std::vector<glm::vec2> path2f;

path2f temp;                //临时路径轮廓
std::vector<path2f> paths;  //路径轮廓列表，可包含多个轮廓

using namespace libtess;
Tesselator tess;            //三角形切割器

// main函数
int main(int argc, char* argv[])
{
    InitWindow(TEXT("libtess"), 1024, 768);

    RunApp();

    return 0;
}

//窗口缩放事件
void OnSize(int width, int height)
{

}

template<typename T>
void draw_polygon(HDC dc, const std::vector<T>& ls)
{
    if (ls.empty()) {
        return;
    }
    MoveToEx(dc, ls[0].x, ls[0].y, NULL);
    for (size_t i = 1; i<ls.size(); ++i) {
        LineTo(dc, ls[i].x, ls[i].y);
    }
    LineTo(dc, ls[0].x, ls[0].y);
}

//gdi实现的简单绘制图元，只实现绘制三角形
void draw_elements(HDC dc, GLenum shape, const std::vector< glm::vec3 >& vs, const std::vector<int>& ids)
{
    HPEN oldPen = (HPEN) SelectObject(dc, (HPEN) GetStockObject(DC_PEN));
    HBRUSH oldBrush = (HBRUSH) SelectObject(dc, (HBRUSH) GetStockObject(DC_BRUSH));
    size_t begin = 0;
    size_t end;
    int id;
    SetDCPenColor(dc, 0xFF);
    SetDCBrushColor(dc, 0x7F00);

    switch (shape) {
    case GL_POINTS:
        for (size_t i = 0; i<ids.size(); ++i) {
            id = ids[i];
            SetPixelV(dc, vs[id].x, vs[id].y, 0xFF);
        }
        break;
    case GL_LINES:
    case GL_LINE_LOOP:
    case GL_LINE_STRIP:
    case GL_TRIANGLE_STRIP:
        //print("draw", c.command);
        break;
    case GL_TRIANGLES:
        end = ids.size();
        for (begin = 2; begin < end; begin += 3) {
            /*id = ids[begin - 2]; MoveToEx(dc, vs[id].x, vs[id].y, nullptr);
            id = ids[begin - 1]; LineTo(dc, vs[id].x, vs[id].y);
            id = ids[begin];     LineTo(dc, vs[id].x, vs[id].y);
            id = ids[begin - 2]; LineTo(dc, vs[id].x, vs[id].y);*/

            int id1 = ids[begin - 2];
            int id2 = ids[begin - 1];
            int id3 = ids[begin - 0];

            POINT pts[3] = { { vs[id1].x, vs[id1].y }, { vs[id2].x, vs[id2].y }, { vs[id3].x, vs[id3].y } };
            Polygon(dc, pts, 3);

        }
        break;
    case GL_TRIANGLE_FAN:
        //        end = size - 1;
        //        for(begin = 1; begin < end; ++begin){
        //            MoveToEx(dc, vs[0].x, vs[0].y, null);
        //            LineTo(dc, vs[begin].x, vs[begin].y);
        //            LineTo(dc, vs[begin+1].x, vs[begin+1].y);
        //            LineTo(dc, vs[0].x, vs[0].y);
        //        }
        break;
    default:
        break;
    }

    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
}

//窗口绘制事件
void OnPaint(HDC hdc, const RECT& rect)
{
    //绘制切割的三角形
    draw_elements(hdc, GL_TRIANGLES, tess.vertices, tess.elements);

    //绘制多边形轮廓
    SelectObject(hdc, CreatePen(PS_SOLID, 1, 0xFF));
    for (size_t i = 0; i < paths.size(); ++i) {
        draw_polygon(hdc, paths[i]);
    }
    DeleteObject((HPEN)SelectObject(hdc, GetStockObject(BLACK_PEN)));

    //临时多边形轮廓（鼠标操作哪个）
    SelectObject(hdc, CreatePen(PS_SOLID, 1, 0xFF00));
    draw_polygon(hdc, temp);
    DeleteObject((HPEN) SelectObject(hdc, GetStockObject(BLACK_PEN)));
}

//计时器事件（更新窗口）
void OnTimer()
{

}

//键盘事件
void OnKeyDown(int key)
{

}

void OnKeyUp(int key)
{

}

//鼠标事件
void OnMouseDown(int x, int y, int button)
{
    if (button == VK_LBUTTON) {
        //左键添加一个轮廓点
        temp.push_back(glm::vec2(x, y));
    }
    else {
        //右键把轮廓添加到列表中
        paths.push_back(temp);
        temp.clear();

        //切割三角形
        tess.init();
        for (size_t i = 0; i < paths.size(); ++i) {
            tess.add_contour(paths[i]);
        }
        tess.tesselate(TESS_WINDING_ODD, TESS_TRIANGLES);
    }
}

void OnMouseUp(int x, int y, int button)
{

}

void OnMouseMove(int x, int y)
{

}