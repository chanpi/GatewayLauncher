// GatewayLauncher.cpp : �A�v���P�[�V�����̃G���g�� �|�C���g���`���܂��B
//

#include "stdafx.h"
#include "GatewayLauncher.h"
#include "GatewayLaunchController.h"
#include "AnalyzeXML.h"

#include <cstdlib>	// �K�v

#if _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

#define new  ::new( _NORMAL_BLOCK, __FILE__, __LINE__ )
#endif

#if UNICODE || _UNICODE
static LPCTSTR g_FILE = __FILEW__;
#else
static LPCTSTR g_FILE = __FILE__;
#endif

// �O���[�o���ϐ�:
HINSTANCE hInst;								// ���݂̃C���^�[�t�F�C�X
TCHAR szTitle[MAX_LOADSTRING];					// �^�C�g�� �o�[�̃e�L�X�g
TCHAR szWindowClass[MAX_LOADSTRING];			// ���C�� �E�B���h�E �N���X��
HWND g_hWnd = NULL;

// ���̃R�[�h ���W���[���Ɋ܂܂��֐��̐錾��]�����܂�:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

// ���O�_�C�A���O�p�̊֐�
LRESULT CALLBACK DlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// �����ŗ^����ꂽ�v���O���������s���A�I���܂ŊĎ����܂��B

namespace {
	// �^�X�N�g���C�A�C�R��
	HICON g_hMiniIcon = NULL;
	typedef enum { EDIT_MENU, RELOAD_MENU, EXIT_MENU } MENU_ITEMS;

	// �ғ����Ƀ��O�_�C�A���O��\������
	static BOOL g_noDialog = FALSE;

	static LPCTSTR g_szNoDialogOption = _T("-nodialog");
}

GatewayLauncherContext g_context = {0};
AnalyzeXML g_analyzer;

// �r������A�v���O�������s�̐���Ɏg�p
CRITICAL_SECTION g_lock;
CRITICAL_SECTION g_dialogLock;

// ���O�_�C�A���O
HWND g_hDlg = NULL;
// ���O�_�C�A���O�ɕ\������e�L�X�g
tstring g_statusString = _T("");

HANDLE g_hLauchApplicationThread = INVALID_HANDLE_VALUE;

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

#if DEBUG || _DEBUG
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	
	// TODO: �����ɃR�[�h��}�����Ă��������B
	MSG msg;
	HACCEL hAccelTable;

	// �O���[�o������������������Ă��܂��B
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_LAUNCHER, szWindowClass, MAX_LOADSTRING);

	if (!ExecuteOnce(szTitle)) {
		return EXIT_SUCCESS;
	}

	MyRegisterClass(hInstance);

	int argc = 0;
	LPTSTR *argv = NULL;
	argv = CommandLineToArgvW(GetCommandLine(), &argc);
	if (argc > 1) {
		if (0 == _tcsicmp(argv[1], g_szNoDialogOption)) {
			g_noDialog = TRUE;
		}
	}
	LocalFree(argv);

	LOG_LEVEL logLevel = Log_Error;
#if _DEBUG || DEBUG
	logLevel = Log_Debug;
#else
	logLevel = Log_Error;
#endif
	if (!LogFileOpenW(SHARED_LOG_FILE_NAME, logLevel)) {
		//ReportError(_T("GatewayLauncher�̃��O�͏o�͂���܂���B"));
	}

	// �N���e�B�J���Z�N�V�����̏�����
	InitializeCriticalSection(&g_lock);
	InitializeCriticalSection(&g_dialogLock);

	// �A�v���P�[�V�����̏����������s���܂�:
	if (!InitInstance (hInstance, nCmdShow))
	{
		DeleteCriticalSection(&g_lock);
		DeleteCriticalSection(&g_dialogLock);
		CleanupMutex();
		LogFileCloseW();
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_LAUNCHER));

	// ���C�� ���b�Z�[�W ���[�v:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (g_hDlg != NULL && IsDialogMessage(g_hDlg, &msg)) {
			continue;
		}

		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	// �N���e�B�J���Z�N�V�����̔j��
	DeleteCriticalSection(&g_lock);
	DeleteCriticalSection(&g_dialogLock);

	CleanupMutex();
	LogFileCloseW();
	return (int) msg.wParam;
}

inline void SetTaskTrayIcon(HWND hWnd) {
	HMENU hMenu = NULL;
	MENUITEMINFO menuItem_edit, menuItem_reload, menuItem_exit;
	POINT pos;
	ZeroMemory(&menuItem_edit, sizeof(menuItem_edit));
	ZeroMemory(&menuItem_reload, sizeof(menuItem_reload));
	ZeroMemory(&menuItem_exit, sizeof(menuItem_exit));

	menuItem_edit.cbSize		= sizeof(menuItem_edit);
	menuItem_edit.fMask			= MIIM_STRING | MIIM_ID;
	menuItem_edit.wID			= EDIT_MENU;
	menuItem_edit.dwTypeData	= _T("�ݒ�t�@�C����ҏW");
	menuItem_edit.cch			= _tcslen(menuItem_edit.dwTypeData);

	menuItem_reload.cbSize		= sizeof(menuItem_reload);
	menuItem_reload.fMask		= MIIM_STRING | MIIM_ID;
	menuItem_reload.wID			= RELOAD_MENU;
	menuItem_reload.dwTypeData	= _T("�����[�h");
	menuItem_reload.cch			= _tcslen(menuItem_reload.dwTypeData);

	menuItem_exit.cbSize		= sizeof(menuItem_exit);
	menuItem_exit.fMask			= MIIM_STRING | MIIM_ID;
	menuItem_exit.wID			= EXIT_MENU;
	menuItem_exit.dwTypeData	= _T("�I��");
	menuItem_exit.cch			= _tcslen(menuItem_exit.dwTypeData);

	GetCursorPos(&pos);
	SetForegroundWindow(hWnd);
	hMenu = CreatePopupMenu();
	InsertMenuItem(hMenu, EDIT_MENU, TRUE, &menuItem_edit);
	//InsertMenuItem(hMenu, RELOAD_MENU, TRUE, &menuItem_reload);
	InsertMenuItem(hMenu, EXIT_MENU, TRUE, &menuItem_exit);
	TrackPopupMenuEx(hMenu, TPM_LEFTALIGN, pos.x, pos.y, hWnd, 0);
	DestroyMenu(hMenu);
}

//
//  �֐�: MyRegisterClass()
//
//  �ړI: �E�B���h�E �N���X��o�^���܂��B
//
//  �R�����g:
//
//    ���̊֐�����юg�����́A'RegisterClassEx' �֐����ǉ����ꂽ
//    Windows 95 ���O�� Win32 �V�X�e���ƌ݊�������ꍇ�ɂ̂ݕK�v�ł��B
//    �A�v���P�[�V�������A�֘A�t����ꂽ
//    �������`���̏������A�C�R�����擾�ł���悤�ɂ���ɂ́A
//    ���̊֐����Ăяo���Ă��������B
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LAUNCHER));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_LAUNCHER);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//   �֐�: InitInstance(HINSTANCE, int)
//
//   �ړI: �C���X�^���X �n���h����ۑ����āA���C�� �E�B���h�E���쐬���܂��B
//
//   �R�����g:
//
//        ���̊֐��ŁA�O���[�o���ϐ��ŃC���X�^���X �n���h����ۑ����A
//        ���C�� �v���O���� �E�B���h�E���쐬����ѕ\�����܂��B
//
BOOL InitInstance(HINSTANCE hInstance, int /*nCmdShow*/)
{
   HWND hWnd;

   hInst = hInstance; // �O���[�o���ϐ��ɃC���X�^���X�������i�[���܂��B

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   //ShowWindow(hWnd, nCmdShow);
   //UpdateWindow(hWnd);
   g_hWnd = hWnd;

   return TRUE;
}

//
//  �֐�: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  �ړI:  ���C�� �E�B���h�E�̃��b�Z�[�W���������܂��B
//
//  WM_COMMAND	- �A�v���P�[�V���� ���j���[�̏���
//  WM_PAINT	- ���C�� �E�B���h�E�̕`��
//  WM_DESTROY	- ���~���b�Z�[�W��\�����Ė߂�
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	TCHAR szFileName[MAX_PATH] = {0};
	static NOTIFYICONDATA nIcon = {0};

	switch (message)
	{
	case WM_CREATE:
		g_hDlg = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, (DLGPROC)DlgProc);
		if (!g_noDialog) {
			ShowWindow(g_hDlg, SW_SHOW);
		}
		if (!Initialize(hWnd)) {
			PostMessage(hWnd, WM_CLOSE, 0, 0);
			return 0;
		}

		// ���W���[�������擾�i�j
		if (!GetModuleFileName(NULL, szFileName, _countof(szFileName))) {
			LoggingMessage(Log_Error, _T("���s���W���[�����̎擾�Ɏ��s���܂����B�I�����܂��B"), GetLastError(), g_FILE, __LINE__);
			PostMessage(hWnd, WM_CLOSE, 0, 0);
			return 0;
		}

		// �^�X�N�o�[�ɃA�C�R���ݒ�
		ExtractIconEx(szFileName, 0, NULL, &g_hMiniIcon, 1);
		nIcon.cbSize = sizeof(g_hMiniIcon);
		nIcon.uID = 1;
		nIcon.hWnd = hWnd;
		nIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
		nIcon.uCallbackMessage = MY_NOTIFYICON;
		nIcon.hIcon = g_hMiniIcon;
		_tcscpy_s(nIcon.szTip, szTitle);
		Shell_NotifyIcon(NIM_ADD, &nIcon);

		break;

	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// �I�����ꂽ���j���[�̉��:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		case EDIT_MENU:
			ShellExecute(NULL, _T("open"), _T("notepad.exe"), SHARED_XML_FILE, NULL, SW_SHOW);
			break;
		case RELOAD_MENU:
			// TODO Stop
			//LaunchApplications(hWnd, showLogDialog);
			break;
		case EXIT_MENU:
			DestroyWindow(hWnd);
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;

	case MY_NOTIFYICON:
		switch (lParam) {
		case WM_LBUTTONDBLCLK:
			if (g_hDlg == NULL) {
				g_hDlg = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, (DLGPROC)DlgProc);
			}
			ShowWindow(g_hDlg, SW_SHOW);
			SetForegroundWindow(g_hDlg);
			break;

		case WM_RBUTTONDOWN:
			SetTaskTrayIcon(hWnd);
			break;
		}
		break;

	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: �`��R�[�h�������ɒǉ����Ă�������...
		EndPaint(hWnd, &ps);
		break;

	case MY_LAUNCHFAILED:
		ExitLauncher();
		if (g_hLauchApplicationThread != INVALID_HANDLE_VALUE) {
			WaitForSingleObject(g_hLauchApplicationThread, INFINITE);
			CloseHandle(g_hLauchApplicationThread);
			g_hLauchApplicationThread = INVALID_HANDLE_VALUE;
		}
		break;

	case WM_CLOSE:
	case WM_DESTROY:
		ExitLauncher();
		if (g_hLauchApplicationThread != INVALID_HANDLE_VALUE) {
			WaitForSingleObject(g_hLauchApplicationThread, INFINITE);
			CloseHandle(g_hLauchApplicationThread);
			g_hLauchApplicationThread = INVALID_HANDLE_VALUE;
		}

		UnInitialize();
		if (g_hMiniIcon != NULL) {
			Shell_NotifyIcon(NIM_DELETE, &nIcon);
			DestroyIcon(g_hMiniIcon);
		}
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

LRESULT CALLBACK DlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	HICON hIcon;

	switch (message)
	{
	case WM_INITDIALOG:
		hIcon = (HICON)LoadImage(hInst,
			MAKEINTRESOURCE(IDI_SMALL),
            IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON),
            GetSystemMetrics(SM_CYSMICON),
            0);
		SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
		SetDlgItemText(hDlg, IDC_EDIT1, g_statusString.c_str());
		SetForegroundWindow(hDlg);
		return (LRESULT)TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			// �^�X�N�g���C��
			break;

		case IDCANCEL:
			EndDialog(hDlg, LOWORD(wParam));
			g_hDlg = NULL;
			break;

		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hDlg, About);
			break;

		case IDM_EXIT:
			PostMessage(hDlg, WM_CLOSE, 0, 0);
			break;
		}
		return (LRESULT)TRUE;

	case WM_CLOSE:
		EndDialog(hDlg, LOWORD(wParam));
		g_hDlg = NULL;
		PostMessage(g_hWnd, WM_CLOSE, 0, 0);
		return (LRESULT)TRUE;
	}
	return (LRESULT)FALSE;
}

// �o�[�W�������{�b�N�X�̃��b�Z�[�W �n���h���[�ł��B
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
