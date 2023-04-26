//---------------------------------------------------------------------------

#include "Window.h"

static HWND hWindow;            // 主窗口
static UINT uTimerID = 0;       // 计时器 ID
static HDC hMemDC;              // 绘图设备
static HBITMAP hBackBuffer;     // 背景缓冲图片
static HFONT hFont;             // 字体

// 主消息处理函数
LRESULT CALLBACK WindowProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);

// 初始化函数
void OnWMCreate(HWND hWnd);

// 释放函数
void OnWMDestroy(HWND hWnd);

// 窗口大小改变
void OnWMResize(HWND hWnd, int width, int height);

// 内部绘制函数
void OnWMPaint(HWND hWnd);

// 返回窗口句柄
HWND Window()
{
    return hWindow;
}

// 窗口创建函数
HWND InitWindow(LPCTSTR title, int width, int height, bool scalable)
{
    HWND hWnd;
    HINSTANCE hInstance = GetModuleHandle(NULL);

    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, L"ICO_MAIN");
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH) (COLOR_WINDOW);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = CLASSNAME;
    wc.hIconSm = LoadIconW(hInstance, L"ICO_MAIN");

    if (!RegisterClassEx(&wc)) {
        return NULL;
    }

    DWORD style;
    if (scalable) {
        // 窗口可以缩放
        style = WS_OVERLAPPEDWINDOW;
    }
    else {
        // 固定大小窗口
        style = WS_BORDER | WS_MINIMIZEBOX | WS_SYSMENU | WS_CAPTION;
    }

    RECT rect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    AdjustWindowRectEx(&rect, style, FALSE, WS_EX_CLIENTEDGE);
    width = static_cast<int>(rect.right - rect.left);
    height = static_cast<int>(rect.bottom - rect.top);

    hWnd = CreateWindowEx(WS_EX_CLIENTEDGE, CLASSNAME, title, style,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, hInstance, NULL);

    if (hWnd) {
        ShowWindow(hWnd, SW_SHOW);
    }

    return hWnd;
}

// 主消息处理函数
LRESULT CALLBACK WindowProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch (Message) {
    case WM_CREATE:
        OnWMCreate(hWnd);
        break;
    case WM_DESTROY:
        OnWMDestroy(hWnd);
        break;
    case WM_CLOSE:
        //OutputDebugStringA("WM_CLOSE\n");
        PostQuitMessage(0);
        break;
    case WM_QUIT:
        //OutputDebugStringA("WM_QUIT\n");
        break;
    case WM_ERASEBKGND:// 不擦除背景
        return TRUE;
    case WM_SIZE:// 窗口大小改变
        OnWMResize(hWnd, LOWORD(lParam), HIWORD(lParam));
        break;
    case WM_PAINT:// 窗口绘制事件
        OnWMPaint(hWnd);
        break;
    case WM_TIMER:// 计时器事件
        OnTimer();
        Repaint(hWnd);
        break;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)PostQuitMessage(0);
        if (OnKeyDown)OnKeyDown(int(wParam));
        break;
    case WM_KEYUP:
        if (OnKeyUp)OnKeyUp(int(wParam));
        break;

    case WM_MOUSEMOVE:
        OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        break;
    case WM_LBUTTONDOWN:
        OnMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), VK_LBUTTON);
        break;
    case WM_LBUTTONUP:
        OnMouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), VK_LBUTTON);
        break;
    case WM_RBUTTONDOWN:
        OnMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), VK_RBUTTON);
        break;
    case WM_RBUTTONUP:
        OnMouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), VK_RBUTTON);
        break;

    default:
        break;
    }
    return DefWindowProc(hWnd, Message, wParam, lParam);
}

// 重绘窗口
void Repaint(HWND hwnd)
{
    RECT rect;
    GetClientRect(hwnd, &rect);
    RedrawWindow(hwnd, &rect, 0, RDW_INVALIDATE | RDW_NOERASE | RDW_UPDATENOW);
}

// 执行消息循环
void DoEvents()
{
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (!TranslateAccelerator(msg.hwnd, NULL, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

// 判断程序是否运行
bool Running()
{
    return hWindow != NULL;
}

// 运行 APP
int RunApp()
{
    MSG msg;

    // 主消息循环:
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!TranslateAccelerator(msg.hwnd, NULL, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return 0;
}

// 初始化函数
void OnWMCreate(HWND hWnd)
{
    hWindow = hWnd;

    // 获取窗口客户区大小
    RECT rect;
    GetClientRect(hWnd, &rect);

    // 创建内存 DC 和背景缓冲位图
    hMemDC = CreateCompatibleDC(NULL);
    hBackBuffer = CreateBitmap(rect.right - rect.left, rect.bottom - rect.top, 1, 32, NULL);
    SelectObject(hMemDC, hBackBuffer);

    // 创建字体
    hFont = CreateFont(32, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, GB2312_CHARSET, 0, 0, 0, DEFAULT_PITCH, TEXT("msyh"));

    // 选择字体
    SelectObject(hMemDC, hFont);

    // 字体透明
    SetBkMode(hMemDC, TRANSPARENT);

    // 设置计时器
    uTimerID = SetTimer(hWnd, 0, 40, NULL);
}

// 释放函数
void OnWMDestroy(HWND hWnd)
{
    if (uTimerID) {
        KillTimer(hWnd, uTimerID);
        uTimerID = 0;
    }

    // 删除双缓冲
    DeleteObject(hBackBuffer);
    DeleteDC(hMemDC);
    // 删除字体
    DeleteObject(hFont);

    // 退出消息
    //PostQuitMessage(0);

    hWindow = NULL;
}

// 窗口大小改变
void OnWMResize(HWND hWnd, int width, int height)
{
    // 重建绘图缓冲区
    RECT rect;
    GetClientRect(hWnd, &rect);

    // 创建内存 DC 和背景缓冲位图
    if (hBackBuffer) {
        DeleteObject(hBackBuffer);
    }
    hBackBuffer = CreateBitmap(rect.right - rect.left, rect.bottom - rect.top, 1, 32, NULL);
    SelectObject(hMemDC, hBackBuffer);
}

// 内部绘制函数
void OnWMPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    RECT rect;

    // 获取窗口内部区域大小
    GetClientRect(hWnd, &rect);

    // 创建背景画刷
    HBRUSH brush = CreateSolidBrush(0xFF8000);

    // 填充背景
    FillRect(hMemDC, &rect, brush);

    // 删除画刷
    DeleteObject(brush);

    // 绘制游戏，用背景缓冲的 HDC
    OnPaint(hMemDC, rect);

    // 背景缓冲绘制到窗口 dc
    BitBlt(hdc, 0, 0, rect.right - rect.left, rect.bottom - rect.top, hMemDC, 0, 0, SRCCOPY);

    // 释放窗口 dc
    ReleaseDC(hWnd, hdc);

    EndPaint(hWnd, &ps);
}

// 显示消息对话框
void msgbox(PCTSTR param, ...)
{
    TCHAR buf[1024] = {};
    va_list body;
    va_start(body, param);
    _vsntprintf(buf, 1024, param, body);
    va_end(body);

    MessageBox(hWindow, buf, TEXT("消息"), MB_OK);
}
