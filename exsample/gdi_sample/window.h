//---------------------------------------------------------------------------

#ifndef WindowH
#define WindowH

#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <tchar.h>

#define CLASSNAME TEXT("Win32Window")

//窗口创建函数
HWND InitWindow(LPCTSTR title, int width, int height, bool scalable = true);

//重绘窗口
void Repaint(HWND hWnd);

//执行消息循环
void DoEvents();

//运行APP
void RunApp();

//
// 窗口事件
//

//窗口缩放事件
void OnSize(int width, int height);

//窗口绘制事件
void OnPaint(HDC hdc, const RECT& rect);

//计时器事件（更新窗口）
void OnTimer();

//键盘事件
void OnKeyDown(int key);
void OnKeyUp(int key);

//鼠标事件
void OnMouseDown(int x, int y, int button);
void OnMouseUp(int x, int y, int button);
void OnMouseMove(int x, int y);

//
// 辅助函数
//

// 显示消息对话框
void msgbox(PCTSTR param, ...);

//---------------------------------------------------------------------------
#endif
