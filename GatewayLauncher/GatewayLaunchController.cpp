#include "stdafx.h"
#include "GatewayLauncher.h"
#include "GatewayLaunchController.h"
#include "resource.h"
#include "Miscellaneous.h"
#include <map>
using namespace std;

static VOID CreateErrorMessageMap(VOID);
// アプリケーションの起動に関する関数（ランチャー）
unsigned int __stdcall LaunchApplicationsProc(void *pParam);
unsigned int __stdcall LaunchProgramThreadProc(void *pParam);

namespace {
	static HANDLE g_hStopEvent = INVALID_HANDLE_VALUE;

	// 残ったプロセスを終了するときの終了待ち時間および次のタスクを起動するまでのインターバル
	static int g_nTimeToWait = 1000;
	// エラーメッセージマップ
	static map<int, tstring> g_ErrorMessageMap;
};

extern LPCTSTR g_szConfigurationFileName;

extern TCHAR szTitle[];					// タイトル バーのテキスト

extern GatewayLauncherContext g_context;
extern AnalyzeXML g_analyzer;

extern CRITICAL_SECTION g_lock;
extern CRITICAL_SECTION g_dialogLock;

extern HINSTANCE hInst;
extern HWND g_hWnd;
// ログウィンドウ
extern HWND g_hDlg;
// ログダイアログに表示するテキスト
extern tstring g_statusString;
// プログラム起動に失敗したか
extern volatile BOOL g_bLaunchFailed;

extern HANDLE g_hLauchApplicationThread;

BOOL Initialize(HWND hWnd)
{
	// stopイベントの作成
	g_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (g_hStopEvent == INVALID_HANDLE_VALUE) {
		LogDebugMessage(Log_Error, _T("終了イベントの作成に失敗しました。プログラムを終了します。"));
		MessageBox(hWnd, _T("終了イベントの作成に失敗しました。プログラムを終了します。"), szTitle, MB_OK | MB_ICONERROR | MB_TOPMOST);
		PostMessage(hWnd, WM_CLOSE, 0, 0);
		return FALSE;
	}

	// 設定ファイル解析クラスを初期化
	if (!g_analyzer.LoadXML(g_szConfigurationFileName)) {
		//ReportError(_T("[ERROR] XMLのロードに失敗しています。プログラムを終了します。"));
		LogDebugMessage(Log_Error, _T("XMLのロードに失敗しています。プログラムを終了します。"));
		MessageBox(hWnd, _T("XMLのロードに失敗しています。プログラムを終了します。"), szTitle, MB_OK | MB_ICONERROR | MB_TOPMOST);
		PostMessage(hWnd, WM_CLOSE, 0, 0);
		return FALSE;
	}
	g_context.pAnalyzer = &g_analyzer;

	g_nTimeToWait = _tstoi(g_analyzer.GetSoftValue(szTitle, _T("time_to_wait")));
	g_statusString.append(_T("起動準備中........\r\n"));
	SetDlgItemText(g_hDlg, IDC_EDIT1, g_statusString.c_str());
	InvalidateRect(g_hDlg, NULL, FALSE);

	// アプリケーションを起動
	g_hLauchApplicationThread = (HANDLE)_beginthreadex(NULL, 0, LaunchApplicationsProc, NULL, 0, NULL);
	
	return TRUE;
}

VOID UnInitialize(VOID)
{
	// Launchしたすべてのプロセスを終了させる
	// Launchを担当したすべてのスレッドの終了を確認する
	ExitLauncher();
	EnterCriticalSection(&g_lock);
	WaitForMultipleObjects(g_context.nThreadCount, g_context.hThread, TRUE, INFINITE);
	for (int i = 0; i < g_context.nThreadCount; i++) {
		CloseHandle(g_context.hThread[i]);
		g_context.hThread[i] = INVALID_HANDLE_VALUE;
	}
	g_context.nThreadCount = 0;
	LeaveCriticalSection(&g_lock);

	// ストップイベントの破棄
	CloseHandle(g_hStopEvent);
	g_hStopEvent = INVALID_HANDLE_VALUE;

	if (g_hDlg) {
		PostMessage(g_hDlg, WM_CLOSE, 0, 0);
	}

}

VOID ExitLauncher(VOID)
{
	if (g_hStopEvent != INVALID_HANDLE_VALUE) {
		SetEvent(g_hStopEvent);
	}
}

VOID CreateErrorMessageMap(VOID)
{
	TCHAR szMessage[BUFFER_SIZE];
	LoadString(hInst, IDS_EXIT_SYSTEM_ERROR, szMessage, _countof(szMessage));
	g_ErrorMessageMap[EXIT_SYSTEM_ERROR] = szMessage;

	LoadString(hInst, IDS_EXIT_BAD_ARGUMENTS, szMessage, _countof(szMessage));
	g_ErrorMessageMap[EXIT_BAD_ARGUMENTS] = szMessage;
	LoadString(hInst, IDS_EXIT_NO_ARGUMENTS, szMessage, _countof(szMessage));
	g_ErrorMessageMap[EXIT_NO_ARGUMENTS] = szMessage;
	
	LoadString(hInst, IDS_EXIT_SOCKET_ERROR, szMessage, _countof(szMessage));
	g_ErrorMessageMap[EXIT_SOCKET_ERROR] = szMessage;
	LoadString(hInst, IDS_EXIT_SOCKET_CONNECT_ERROR, szMessage, _countof(szMessage));
	g_ErrorMessageMap[EXIT_SOCKET_CONNECT_ERROR] = szMessage;
	LoadString(hInst, IDS_EXIT_SOCKET_BIND_LISTEN_ERROR, szMessage, _countof(szMessage));
	g_ErrorMessageMap[EXIT_SOCKET_BIND_LISTEN_ERROR] = szMessage;

	LoadString(hInst, IDS_EXIT_FILE_NOT_FOUND, szMessage, _countof(szMessage));
	g_ErrorMessageMap[EXIT_FILE_NOT_FOUND] = szMessage;
	LoadString(hInst, IDS_EXIT_INVALID_FILE_CONFIGURATION, szMessage, _countof(szMessage));
	g_ErrorMessageMap[EXIT_INVALID_FILE_CONFIGURATION] = szMessage;

	LoadString(hInst, IDS_EXIT_DEVICE_NOT_FOUND, szMessage, _countof(szMessage));
	g_ErrorMessageMap[EXIT_DEVICE_NOT_FOUND] = szMessage;
	LoadString(hInst, IDS_EXIT_GAMEPAD_NOT_FOUND, szMessage, _countof(szMessage));
	g_ErrorMessageMap[EXIT_GAMEPAD_NOT_FOUND] = szMessage;
	LoadString(hInst, IDS_EXIT_GAMEPAD_SETUP_ERROR, szMessage, _countof(szMessage));
	g_ErrorMessageMap[EXIT_GAMEPAD_SETUP_ERROR] = szMessage;

	LoadString(hInst, IDS_EXIT_RTT4EC_CONNECT_ERROR, szMessage, _countof(szMessage));
	g_ErrorMessageMap[EXIT_RTT4EC_CONNECT_ERROR] = szMessage;

	LoadString(hInst, IDS_EXIT_SOME_ERROR, szMessage, _countof(szMessage));
	g_ErrorMessageMap[EXIT_SOME_ERROR] = szMessage;
}

unsigned int __stdcall LaunchApplicationsProc(void *pParam)
{
	// エラーメッセージをStringTableから取得
	CreateErrorMessageMap();

	// Launchする実行ファイル名と起動時の引数を取得
	// ランチャータグの<key name="targetX">起動ファイル名</key>を読み取る
	// <key name="起動ファイル名">引数</key>
	TCHAR szKey[MAX_PATH] = {0};
	PCTSTR szTargetName = NULL, szTargetParams = NULL;
	for (int i = 1; i <= MAX_TARGET_COUNT; i++) {
		_stprintf_s(szKey, _countof(szKey), _T("target%d"), i);
		szTargetName = g_analyzer.GetSoftValue(szTitle, szKey);	// 起動ファイル名取得
		if (szTargetName == NULL) {
			//break;
			continue;
		}
		szTargetParams = g_analyzer.GetSoftValue(szTitle, szTargetName); // 引数取得
		g_context.targetList.push_back(pair<tstring, tstring>(szTargetName, szTargetParams != NULL ? szTargetParams : _T("")));
	}

	// Launch開始
	UINT uThreadId;
	vector<pair<tstring, tstring>>::iterator it = g_context.targetList.begin();
	TCHAR* szCommandLine;

	EnterCriticalSection(&g_lock);
	for (; it != g_context.targetList.end(); it++) {
		ShellExecute(NULL, _T("open"), _T("taskkill"), tstring(_T("/IM ")).append(it->first).c_str(), NULL, SW_HIDE);
		if (WaitForSingleObject(g_hStopEvent, g_nTimeToWait) != WAIT_TIMEOUT) {
			LeaveCriticalSection(&g_lock);
			OutputDebugString(_T("Nyoro\n"));
			return EXIT_FAILURE;
		}

		szCommandLine = new TCHAR[MAX_PATH];
		_stprintf_s(szCommandLine, MAX_PATH, _T("%s %s"), it->first.c_str(), it->second.c_str());
		g_context.hThread[g_context.nThreadCount++] = (HANDLE)_beginthreadex(NULL, 0, LaunchProgramThreadProc, (void*)szCommandLine, 0, &uThreadId);
	}
	LeaveCriticalSection(&g_lock);
	return EXIT_SUCCESS;
}

unsigned int __stdcall LaunchProgramThreadProc(void *pParam)
{
	DWORD dwExitCode = EXIT_SUCCESS;
	STARTUPINFO si = {0};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi = {0};
	HANDLE waitHandle[2];
	BOOL isCreateProcessSucceeded;
	tstring szErrorMessage;

	LPTSTR szCommandLine = (LPTSTR)pParam;
	TCHAR szApplicationName[MAX_PATH] = {0};
	LPCTSTR space = _tcschr((LPCTSTR)pParam, _T(' '));
	if (space != NULL) {
		_tcsncpy_s(szApplicationName, _countof(szApplicationName), szCommandLine, space - szCommandLine);
	} else {
		_tcscpy_s(szApplicationName, _countof(szApplicationName), szCommandLine);
	}

	isCreateProcessSucceeded = CreateProcess(NULL, szCommandLine, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi);

	EnterCriticalSection(&g_dialogLock);
	g_statusString.append(szApplicationName);
	g_statusString.append(_T("を起動しています........\r\n"));
	LeaveCriticalSection(&g_dialogLock);

	SetDlgItemText(g_hDlg, IDC_EDIT1, g_statusString.c_str());
	InvalidateRect(g_hDlg, NULL, FALSE);

	CloseHandle(pi.hThread);

	waitHandle[0] = g_hStopEvent;
	waitHandle[1] = pi.hProcess;

	WaitForMultipleObjects(2, waitHandle, FALSE, INFINITE);
	GetExitCodeProcess(pi.hProcess, &dwExitCode);
	if (dwExitCode == STILL_ACTIVE) {	// 終了イベントがONになった
		// プロセスを終了させる
		if (pi.hProcess != 0 && pi.hProcess != INVALID_HANDLE_VALUE) {
			ShellExecute(NULL, _T("open"), _T("taskkill"), tstring(_T("/IM ")).append(szApplicationName).c_str(), NULL, SW_HIDE);
		}

	} else if (!isCreateProcessSucceeded) {
		// すべてのプログラムの実行を中止
		EnterCriticalSection(&g_dialogLock);
		g_statusString.append(szApplicationName);
		g_statusString.append(_T("を起動できませんでした。\r\n"));
		LeaveCriticalSection(&g_dialogLock);

		SetDlgItemText(g_hDlg, IDC_EDIT1, g_statusString.c_str());
		InvalidateRect(g_hDlg, NULL, FALSE);

		g_bLaunchFailed = TRUE;
		PostMessage(g_hWnd, MY_LAUNCHFAILED, 0, 0);

	} else if (dwExitCode != EXIT_SUCCESS) {
		szErrorMessage = g_ErrorMessageMap[dwExitCode];
		if (szErrorMessage.empty() || szErrorMessage == _T("") ) {
			szErrorMessage = g_ErrorMessageMap[EXIT_SOME_ERROR];
		}

		// すべてのプログラムの実行を中止
		EnterCriticalSection(&g_dialogLock);
		g_statusString.append(szApplicationName);
		g_statusString.append(_T("にエラーが発生しました。"));
		g_statusString.append(szErrorMessage);
		LeaveCriticalSection(&g_dialogLock);

		SetDlgItemText(g_hDlg, IDC_EDIT1, g_statusString.c_str());
		InvalidateRect(g_hDlg, NULL, FALSE);

		g_bLaunchFailed = TRUE;
		PostMessage(g_hWnd, MY_LAUNCHFAILED, 0, 0);
	}

	CloseHandle(pi.hProcess);

	//EnterCriticalSection(&g_lock);
	//if (++g_context.nDiedThreadCount == g_context.nThreadCount) {
	//	PostMessage(g_hWnd, MY_LAUNCHFAILED, 0, 0);
	//}
	//LeaveCriticalSection(&g_lock);
	delete pParam;
	return TRUE;
}