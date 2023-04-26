//---------------------------------------------------------------------------

#ifndef WIN32_WINDOW_H
#define WIN32_WINDOW_H

#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <tchar.h>

#define CLASSNAME TEXT("win32.window")

// 返回窗口句柄
HWND Window();

// 窗口创建函数
HWND InitWindow(LPCTSTR title, int width, int height, bool scalable = true);

// 重绘窗口
void Repaint(HWND hWnd);

// 判断程序是否运行
bool Running();

// 执行消息循环
void DoEvents();

// 运行 APP
int RunApp();

//
// 窗口事件
//

// 窗口大小改变
void OnSize(int width, int height);

// 按键按下
void OnKeyDown(int key);

// 按键弹起
void OnKeyUp(int key);

// 鼠标按下
void OnMouseDown(int x, int y, int button);

// 鼠标弹起
void OnMouseUp(int x, int y, int button);

// 鼠标移动
void OnMouseMove(int x, int y);

// 计时器事件
void OnTimer();

// 窗口绘制事件
void OnPaint(HDC hdc, const RECT& rect);

//
// 辅助函数
//

// 显示消息对话框
void msgbox(PCTSTR param, ...);

//---------------------------------------------------------------------------
#endif //WIN32_WINDOW_H
