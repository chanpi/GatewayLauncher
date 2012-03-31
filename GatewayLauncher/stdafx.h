// stdafx.h : 標準のシステム インクルード ファイルのインクルード ファイル、または
// 参照回数が多く、かつあまり変更されない、プロジェクト専用のインクルード ファイル
// を記述します。
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Windows ヘッダーから使用されていない部分を除外します。
// Windows ヘッダー ファイル:
#include <windows.h>
#include <Shlwapi.h>

// C ランタイム ヘッダー ファイル
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#include <ShellAPI.h>
#include <process.h>

#include <string>
using namespace std;
typedef std::basic_string<TCHAR> tstring;


// TODO: プログラムに必要な追加ヘッダーをここで参照してください。

#include "AnalyzeXML.h"
#include "Miscellaneous.h"
#include "ErrorCodeList.h"
