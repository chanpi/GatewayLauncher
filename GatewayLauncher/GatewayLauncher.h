#pragma once

#include "resource.h"
#include <vector>
#include "stdafx.h"

#define MAX_LOADSTRING		100
#define BUFFER_SIZE			256
#define MAX_TARGET_COUNT	20

class AnalyzeXML;

#define MY_NOTIFYICON		(WM_APP+1)
#define MY_LAUNCHFAILED		(WM_APP+2)

typedef struct GatewayLauncherContext {
	AnalyzeXML* pAnalyzer;
	vector<pair<tstring, tstring>> targetList;
	HANDLE hThread[MAX_TARGET_COUNT];
	int nThreadCount;
	int nDiedThreadCount;
} GatewayLauncherContext;

