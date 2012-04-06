#include "stdafx.h"
#include "GatewayLauncher.h"
#include "GatewayLaunchController.h"
#include "AnalyzeXML.h"
#include "resource.h"

#include <map>
using namespace std;

#if UNICODE || _UNICODE
static LPCTSTR g_FILE = __FILEW__;
#else
static LPCTSTR g_FILE = __FILE__;
#endif

static VOID CreateErrorMessageMap(VOID);
static VOID CloseProcess(LPCTSTR szApplicationName, HANDLE hProcess, DWORD dwProcessId);
static VOID GetFileNameWithNoExtension(LPCTSTR szAppTitleWithExtension, LPTSTR szAppTitle, int length);

// アプリケーションの起動に関する関数（ランチャー）
unsigned int __stdcall LaunchApplicationsProc(void *pParam);
unsigned int __stdcall LaunchProgramThreadProc(void *pParam);

namespace {
	static HANDLE g_hStopEvent = INVALID_HANDLE_VALUE;

	// 残ったプロセスを終了するときの終了待ち時間および次のタスクを起動するまでのインターバル
	static int g_nTimeToWait = 1000;
	// エラーメッセージマップ
	static map<int, tstring> g_ErrorMessageMap;

	const PCTSTR TAG_TIME_TO_WAIT	= _T("time_to_wait");
	const tstring CRNL				= _T("\r\n");
	const UINT g_uErrorDialogType = MB_OK | MB_ICONERROR | MB_TOPMOST;
};

extern TCHAR szTitle[];					// タイトル バーのテキスト

extern GatewayLauncherContext g_context;
extern AnalyzeXML g_analyzer;

extern CRITICAL_SECTION g_lock;
extern CRITICAL_SECTION g_dialogLock;

extern HINSTANCE hInst;
extern HWND g_hWnd;
// ダイアログウィンドウ
extern HWND g_hDlg;

extern HANDLE g_hLauchApplicationThread;

BOOL Initialize(HWND hWnd)
{
	// エラーメッセージをStringTableから取得
	CreateErrorMessageMap();

	// stopイベントの作成
	g_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (g_hStopEvent == INVALID_HANDLE_VALUE) {
		LoggingMessage(Log_Error, _T(MESSAGE_ERROR_HANDLE_INVALID), GetLastError(), g_FILE, __LINE__);
		MessageBox(hWnd, g_ErrorMessageMap[EXIT_FAILURE].c_str(), szTitle, g_uErrorDialogType);
		PostMessage(hWnd, WM_CLOSE, 0, 0);
		return FALSE;
	}

	// 設定ファイル解析クラスを初期化
	BOOL bFileExist = TRUE;
	if (!g_analyzer.LoadXML(SHARED_XML_FILE, &bFileExist)) {
		if (bFileExist) {
			LoggingMessage(Log_Error, g_ErrorMessageMap[EXIT_INVALID_FILE_CONFIGURATION].c_str(), GetLastError(), __FILEW__, __LINE__);
			MessageBox(hWnd, g_ErrorMessageMap[EXIT_INVALID_FILE_CONFIGURATION].c_str(), szTitle, g_uErrorDialogType);
		} else {
			LoggingMessage(Log_Error, _T(MESSAGE_ERROR_XML_LOAD), GetLastError(), __FILEW__, __LINE__);
			MessageBox(hWnd, g_ErrorMessageMap[EXIT_FILE_NOT_FOUND].c_str(), szTitle, g_uErrorDialogType);
		}
		PostMessage(hWnd, WM_CLOSE, 0, 0);
		return FALSE;
	}
	g_context.pAnalyzer = &g_analyzer;

	g_nTimeToWait = _tstoi(g_analyzer.GetSoftValue(szTitle, _T("time_to_wait")));

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

	LoadString(hInst, IDS_EXIT_RTTEC_CONNECT_ERROR, szMessage, _countof(szMessage));
	g_ErrorMessageMap[EXIT_RTTEC_CONNECT_ERROR] = szMessage;
	
	LoadString(hInst, IDS_EXIT_CORE_CONNECT_ERROR, szMessage, _countof(szMessage));
	g_ErrorMessageMap[EXIT_CORE_CONNECT_ERROR] = szMessage;

	// ライセンス系
	LoadString(hInst, IDS_EXIT_CERT_UNINITIALIZED, szMessage, _countof(szMessage));
	g_ErrorMessageMap[EXIT_CERT_UNINITIALIZED] = szMessage;
	LoadString(hInst, IDS_EXIT_CERT_INVALID_MACADDRESS, szMessage, _countof(szMessage));
	g_ErrorMessageMap[EXIT_CERT_INVALID_MACADDRESS] = szMessage;
	LoadString(hInst, IDS_EXIT_CERT_INVALID_EXPIRE_DATE, szMessage, _countof(szMessage));
	g_ErrorMessageMap[EXIT_CERT_INVALID_EXPIRE_DATE] = szMessage;
	LoadString(hInst, IDS_EXIT_CERT_FILE_NOT_FOUND, szMessage, _countof(szMessage));
	g_ErrorMessageMap[EXIT_CERT_FILE_NOT_FOUND] = szMessage;
	LoadString(hInst, IDS_EXIT_CERT_SYSTEM_ERROR, szMessage, _countof(szMessage));
	g_ErrorMessageMap[EXIT_CERT_SYSTEM_ERROR] = szMessage;

	LoadString(hInst, IDS_EXIT_NOT_EXECUTABLE, szMessage, _countof(szMessage));
	g_ErrorMessageMap[EXIT_NOT_EXECUTABLE] = szMessage;
	LoadString(hInst, IDS_EXIT_SOME_ERROR, szMessage, _countof(szMessage));
	g_ErrorMessageMap[EXIT_SOME_ERROR] = szMessage;
}

VOID CloseProcess(LPCTSTR szApplicationName, HANDLE hProcess, DWORD dwProcessId)
{
	LPCTSTR szAppTitleWithExtension = PathFindFileName(szApplicationName);
	TCHAR szAppTitle[MAX_PATH] = {0};
	DWORD dwExitCode;
	GetFileNameWithNoExtension(szAppTitleWithExtension, szAppTitle, _countof(szAppTitle));
	// HWNDを取得できるならWM_CLOSEメッセージを送信
	if (szAppTitleWithExtension) {
		HWND hWnd = FindWindow(szAppTitle, NULL);
		if (hWnd) {
			PostMessage(hWnd, WM_CLOSE, 0, 0);
		}
	}

	// プロセス情報が与えられていれば、プロセスが終了しているか確認
	if (hProcess != NULL && hProcess != INVALID_HANDLE_VALUE) {
		GetExitCodeProcess(hProcess, &dwExitCode);
		if (dwExitCode != STILL_ACTIVE) {
			return;
		}
	}

	// taskkillを試す
	for (int i = 0; i < 5; i++) {
		ShellExecute(NULL, _T("open"), _T("taskkill"), tstring(_T("/IM ")).append(szAppTitleWithExtension).c_str(), NULL, SW_HIDE);
	}

	// プロセス情報が与えられていれば、プロセスが終了しているか確認
	if (hProcess != NULL && hProcess != INVALID_HANDLE_VALUE) {
		GetExitCodeProcess(hProcess, &dwExitCode);
		if (dwExitCode != STILL_ACTIVE) {
			return;
		}
	}

	// プロセスIDが与えられているならtaskkill pid xxxx、与えられていなければTerminatePorcess
	if (dwProcessId > 0) {
		TCHAR szPID[8];
		_stprintf_s(szPID, _countof(szPID), _T("%d"), dwProcessId);
		for (int i = 0; i < 5; i++) {
			ShellExecute(NULL, _T("open"), _T("taskkill"), tstring(_T("/pid ")).append(szPID).c_str(), NULL, SW_HIDE);
		}
	}
	if (hProcess) {
		for (int i = 0; i < 5; i++) {
			TerminateProcess(hProcess, EXIT_SUCCESS);
		}
	}
}

VOID GetFileNameWithNoExtension(LPCTSTR szAppTitleWithExtension, LPTSTR szAppTitle, int length)
{
	LPCTSTR szFileName = PathFindFileName(szAppTitleWithExtension);
	if (szFileName && szAppTitle) {
		_tcscpy_s(szAppTitle, length, szFileName);

		LPTSTR pExtension = _tcschr(szAppTitle, _T('.'));
		if (pExtension) {
			*pExtension = _T('\0');
		}
	}
}

unsigned int __stdcall LaunchApplicationsProc(void *pParam)
{
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
		CloseProcess(it->first.c_str(), NULL, 0);
		if (WaitForSingleObject(g_hStopEvent, g_nTimeToWait) != WAIT_TIMEOUT) {
			LeaveCriticalSection(&g_lock);
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
	tstring szDialogMessage;

	LPTSTR szCommandLine = (LPTSTR)pParam;
	TCHAR szApplicationName[MAX_PATH] = {0};
	LPCTSTR space = _tcschr((LPCTSTR)pParam, _T(' '));
	if (space != NULL) {
		_tcsncpy_s(szApplicationName, _countof(szApplicationName), szCommandLine, space - szCommandLine);
	} else {
		_tcscpy_s(szApplicationName, _countof(szApplicationName), szCommandLine);
	}
	szDialogMessage = szApplicationName;
	szDialogMessage.append(_T(": "));

	if (!PathFileExists(szApplicationName)) {
		PostMessage(g_hWnd, MY_LAUNCHFAILED, 0, 0);
		szDialogMessage.append(g_ErrorMessageMap[EXIT_FILE_NOT_FOUND]);
		LoggingMessage(Log_Error, szDialogMessage.c_str(), GetLastError(), g_FILE, __LINE__);
		MessageBox(g_hWnd, g_ErrorMessageMap[EXIT_FILE_NOT_FOUND].c_str(), szTitle, g_uErrorDialogType);
		PostMessage(g_hWnd, WM_CLOSE, 0, 0);
		delete pParam;
		return EXIT_FAILURE;
	}

	isCreateProcessSucceeded = CreateProcess(NULL, szCommandLine, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi);

	CloseHandle(pi.hThread);

	waitHandle[0] = g_hStopEvent;
	waitHandle[1] = pi.hProcess;

	WaitForMultipleObjects(2, waitHandle, FALSE, INFINITE);
	GetExitCodeProcess(pi.hProcess, &dwExitCode);
	if (dwExitCode == STILL_ACTIVE) {	// 終了イベントがONになった
		// プロセスを終了させる
		if (pi.hProcess != 0 && pi.hProcess != INVALID_HANDLE_VALUE) {
			CloseProcess(szApplicationName, pi.hProcess, pi.dwProcessId);
		}

	} else if (!isCreateProcessSucceeded) {
		PostMessage(g_hWnd, MY_LAUNCHFAILED, 0, 0);

		// すべてのプログラムの実行を中止
		szDialogMessage.append(_T(MESSAGE_ERROR_SYSTEM_INIT));
		LoggingMessage(Log_Error, szDialogMessage.c_str(), GetLastError(), g_FILE, __LINE__);
		MessageBox(g_hWnd, _T(MESSAGE_ERROR_SYSTEM_INIT), szTitle, g_uErrorDialogType);

		PostMessage(g_hWnd, WM_CLOSE, 0, 0);

	} else if (dwExitCode != EXIT_SUCCESS) {
		PostMessage(g_hWnd, MY_LAUNCHFAILED, 0, 0);

		szErrorMessage = g_ErrorMessageMap[dwExitCode];
		if (szErrorMessage.empty() || szErrorMessage == _T("") ) {
			szErrorMessage = g_ErrorMessageMap[EXIT_SOME_ERROR];
		}

		// すべてのプログラムの実行を中止
		szDialogMessage.append(szErrorMessage);
		LoggingMessage(Log_Error, szDialogMessage.c_str(), GetLastError(), g_FILE, __LINE__);
		MessageBox(g_hWnd, szErrorMessage.c_str(), szTitle, g_uErrorDialogType);
		PostMessage(g_hWnd, WM_CLOSE, 0, 0);
	}

	CloseHandle(pi.hProcess);
	EnterCriticalSection(&g_dialogLock);
	if (++g_context.nDiedThreadCount >= g_context.nThreadCount) {
		PostMessage(g_hWnd, WM_CLOSE, 0, 0);
	}
	LeaveCriticalSection(&g_dialogLock);


	delete pParam;
	return TRUE;
}