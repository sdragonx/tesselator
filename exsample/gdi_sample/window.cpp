//---------------------------------------------------------------------------

#include "Window.h"

//全局变量
HWND hWindow;           //主窗口
UINT uTimerID = 1;      //计时器ID
HDC hMemDC;             //绘图设备
HBITMAP hBackBuffer;    //背景缓冲图片
HFONT hFont;            //字体

//主消息处理函数
LRESULT CALLBACK WindowProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);

//窗口创建函数
HWND InitWindow(LPCTSTR title, int width, int height, bool scalable)
{
	HWND hwnd;
	HINSTANCE hInstance = GetModuleHandle(NULL);//进程实例句柄

	WNDCLASSEX wc = { 0 };
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIconW(hInstance, L"ICO_MAIN");
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = CLASSNAME;
	wc.hIconSm = LoadIconW(hInstance, L"ICO_MAIN");

	if (!RegisterClassEx(&wc)) {
		return NULL;
	}

	DWORD style;
	if (scalable) {
		//窗口可以缩放
		style = WS_OVERLAPPEDWINDOW;
	}
	else {
		//固定大小窗口
		style = WS_BORDER | WS_MINIMIZEBOX | WS_SYSMENU | WS_CAPTION;
	}

	RECT rect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
	AdjustWindowRectEx(&rect, style, FALSE, WS_EX_CLIENTEDGE);
	width = static_cast<int>(rect.right - rect.left);
	height = static_cast<int>(rect.bottom - rect.top);

	hwnd = CreateWindowEx(WS_EX_CLIENTEDGE, CLASSNAME, title, style,
		CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, hInstance, NULL);

	if (hwnd) {
		ShowWindow(hwnd, SW_SHOW);
		hWindow = hwnd;
	}

	return hwnd;
}

//主消息处理函数
LRESULT CALLBACK WindowProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch(Message){
    case WM_CREATE:
        //创建内存DC和背景缓冲位图
        hMemDC = CreateCompatibleDC(NULL);
        hBackBuffer = CreateBitmap(1024, 768, 1, 32, NULL);
        SelectObject(hMemDC, hBackBuffer);

        //创建字体
        hFont = CreateFont(32, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, GB2312_CHARSET, 0, 0, 0, DEFAULT_PITCH, TEXT("msyh"));
        
        //选择字体
        SelectObject(hMemDC, hFont);

        //字体透明
        SetBkMode(hMemDC, TRANSPARENT);

        //设置计时器
        SetTimer(hWnd, uTimerID, 40, NULL);
        break;
    case WM_DESTROY:
        //删除双缓冲
        DeleteObject(hBackBuffer);
        DeleteDC(hMemDC);
        //删除字体
        DeleteObject(hFont);

        //退出消息
        PostQuitMessage(0);
        break;

    case WM_ERASEBKGND://不擦除背景
        return TRUE;

    case WM_SIZE://窗口大小改变
        //OnSize(LOWORD(lParam), HIWORD(lParam));
        break;
    case WM_PAINT:{
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: 在此处添加使用 hdc 的任何绘图代码...
            RECT rect;
            GetClientRect(hWnd, &rect);//获取窗口内部区域大小

            HBRUSH brush = CreateSolidBrush(0xFF8000);//创建背景画刷
            FillRect(hMemDC, &rect, brush);//填充背景
            DeleteObject(brush);//删除画刷

            //绘制游戏，用背景缓冲的HDC
            OnPaint(hMemDC, rect);
            //背景缓冲绘制到窗口dc
            BitBlt(hdc, 0, 0, 1024, 800, hMemDC, 0, 0, SRCCOPY);

            ReleaseDC(hWnd, hdc);//释放窗口dc

            EndPaint(hWnd, &ps);
        }
        break;

    case WM_TIMER://计时器事件
        //OnTimer();
        Repaint(hWnd);
        break;

    case WM_KEYDOWN:
        if(wParam == VK_ESCAPE)PostQuitMessage(0);
        if(OnKeyDown)OnKeyDown(int(wParam));
        break;
    case WM_KEYUP:
        if(OnKeyUp)OnKeyUp(int(wParam));
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

//重绘窗口
void Repaint(HWND hWnd)
{
    RECT rect;
    GetClientRect(hWnd, &rect);
    RedrawWindow(hWnd, &rect, 0, RDW_INVALIDATE|RDW_NOERASE|RDW_UPDATENOW);
}

//执行消息循环
void DoEvents()
{
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

//运行APP
void RunApp()
{
    MSG msg;

    // 主消息循环:
    while (GetMessage(&msg, NULL, 0, 0)){
        if (!TranslateAccelerator(msg.hwnd, NULL, &msg)){
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}


// 显示消息对话框
void msgbox(PCTSTR param, ...)
{
    TCHAR buf[1024] = { 0 };
    va_list body;
    va_start(body, param);
    _vsntprintf(buf, 1024, param, body);
    va_end(body);

    MessageBox(hWindow, buf, TEXT("消息"), MB_OK);
}
