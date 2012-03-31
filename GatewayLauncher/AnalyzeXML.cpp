#include "StdAfx.h"
#include "GatewayLauncher.h"
#include "AnalyzeXML.h"
#include "Miscellaneous.h"
#include "XMLParser.h"
#include <vector>

extern TCHAR szTitle[MAX_LOADSTRING];

static IXMLDOMElement* g_pRootElement = NULL;
static BOOL g_bInitialized = FALSE;
static map<PCTSTR, PCTSTR>* g_pGlobalConfig		= NULL;
typedef pair<PCTSTR, map<PCTSTR, PCTSTR>*> config_pair;
static vector<config_pair>* g_pConfigPairContainer;

static void CleanupRootElement(void);

static const PCTSTR TAG_GLOBAL		= _T("global");
static const PCTSTR TAG_SOFTS		= _T("softs");
static const PCTSTR TAG_NAME		= _T("name");

inline void SafeReleaseMap(map<PCTSTR, PCTSTR>* pMap)
{
	if (pMap != NULL) {
		CleanupStoredValues(pMap);
		pMap = NULL;
	}
}

AnalyzeXML::AnalyzeXML(void)
{
	g_pConfigPairContainer = new vector<config_pair>;
}

AnalyzeXML::~AnalyzeXML(void)
{
	CleanupRootElement();
	if (g_pConfigPairContainer) {
		g_pConfigPairContainer->clear();
		delete g_pConfigPairContainer;
		g_pConfigPairContainer = NULL;
	}
}

/**
 * @brief
 * XMLParser.dllで取得したXMLのマップオブジェクトを解放します。
 * 
 * XMLParser.dllで取得したXMLのマップオブジェクトを解放します。
 * 
 * @remarks
 * マップの取得に成功した場合には必ず本関数で解放してください。
 * 
 * @see
 * ReadGlobalTag() | ReadSoftsTag()
 */
void CleanupRootElement(void)
{
	if (g_bInitialized) {
		if (g_pConfigPairContainer) {
			int size = g_pConfigPairContainer->size();
			for (int i = 0; i < size; ++i) {
				delete (*g_pConfigPairContainer)[i].first;
				map<PCTSTR, PCTSTR>* pMap = (*g_pConfigPairContainer)[i].second;
				SafeReleaseMap(pMap);
				pMap = NULL;
			}
		}
		SafeReleaseMap(g_pGlobalConfig);
		g_pGlobalConfig		= NULL;

		UnInitialize(g_pRootElement);
		g_pRootElement = NULL;
		g_bInitialized = FALSE;
	}
}

/**
 * @brief
 * XMLをロードしオブジェクトにします。
 * 
 * @param szXMLUri
 * ロードするXMLファイル。
 * 
 * @returns
 * XMLファイルのロードに成功した場合にはTRUE、失敗した場合にはFALSEが返ります。
 * 
 * XMLParser.dllを利用し、XMLをロードしオブジェクトにします。
 */
BOOL AnalyzeXML::LoadXML(PCTSTR szXMLUri)
{
	CleanupRootElement();
	if (!PathFileExists(szXMLUri)) {
		{
			TCHAR szError[BUFFER_SIZE];
			_stprintf_s(szError, _countof(szError), _T("指定したパスに設定ファイルが存在しません[%s]。<AnalyzeXML::LoadXML>"), szXMLUri);
			LogDebugMessage(Log_Error, szError);
		}
		return FALSE;
	}
	g_bInitialized = Initialize(&g_pRootElement, szXMLUri);
	return g_bInitialized;
}

/**
 * @brief
 * globalタグの内容を問い合わせます。
 * 
 * @param szKey
 * global/keys/keyタグのキー値。
 * 
 * @returns
 * マップ中にキーで指定した値があれば値を、なければNULLを返します。
 * 
 * globalタグの内容を格納したマップにキーを問い合わせ、値を取得します。
 * 
 * @see
 * ReadGlobalTag()
 */
PCTSTR AnalyzeXML::GetGlobalValue(PCTSTR szKey)
{
	if (!ReadGlobalTag()) {
		ReportError(_T("[ERROR] globalタグの読み込みに失敗しています。"));
		return NULL;
	}

	return GetMapItem(g_pGlobalConfig, szKey);
}

/**
 * @brief
 * softsタグの内容を問い合わせます。
 * 
 * @param szKey
 * softs/soft/keys/keyタグのキー値。
 * 
 * @returns
 * マップ中にキーで指定した値があれば値を、なければNULLを返します。
 * 
 * softsタグの内容を格納したマップにキーを問い合わせ、値を取得します。
 * 
 * @see
 * ReadGlobalTag() | ReadSoftsTag()
 */
PCTSTR AnalyzeXML::GetSoftValue(PCTSTR szSoftName, PCTSTR szKey)
{
	//if (!this->ReadGlobalTag()) {
	//	ReportError(_T("[ERROR] globalタグの読み込みに失敗しています。"));
	//	return NULL;
	//}
	if (!this->ReadSoftsTag()) {
		ReportError(_T("[ERROR] softsタグの読み込みに失敗しています。"));
		return NULL;
	}

	if (g_pConfigPairContainer) {
		int size = g_pConfigPairContainer->size();
		for (int i = 0; i < size; ++i) {
			if (_tcsicmp((*g_pConfigPairContainer)[i].first, szSoftName) == 0) {
				return GetMapItem((*g_pConfigPairContainer)[i].second, szKey);
			}
		}
	}
	return NULL;
}

/**
 * @brief
 * I4C3D.xmlのglobalタグの解析を行います。
 * 
 * @returns
 * I4C3D.xmlのglobalタグの解析に成功した場合にはTRUE、失敗した場合にはFALSEを返します。
 * 
 * XMLParser.dllを使用してI4C3D.xmlのglobalタグの解析を行い、マップに格納します。
 * 例: <key name="log">info</key>
 * keyタグnameアトリビュートの値をマップのキーに、バリューをマップのバリューに格納します。
 * 
 * @remarks
 * マップの内容を参照するにはGetGlobalValue()を使用します。
 * 
 * @see
 * GetGlobalValue()
 */
BOOL AnalyzeXML::ReadGlobalTag(void)
{
	if (g_pGlobalConfig == NULL) {
		IXMLDOMNode* pGlobal = NULL;
		if (GetDOMTree(g_pRootElement, TAG_GLOBAL, &pGlobal)) {
			g_pGlobalConfig = StoreValues(pGlobal, TAG_NAME);
			FreeDOMTree(pGlobal);

		} else {
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * @brief
 * I4C3D.xmlのsoftsタグの解析を行います。
 * 
 * @returns
 * I4C3D.xmlのsoftsタグの解析に成功した場合にはTRUE、失敗した場合にはFALSEを返します。
 * 
 * XMLParser.dllを使用してI4C3D.xmlのsoftsタグの解析を行い、マップに格納します。
 * 
 * @remarks
 * マップの内容を参照するにはGetSoftValue()を使用します。
 * 
 * @see
 * GetSoftValue()
 */
BOOL AnalyzeXML::ReadSoftsTag(void)
{
	if (g_pConfigPairContainer->size() == 0) {

		IXMLDOMNode* pSofts = NULL;
		IXMLDOMNodeList* pSoftsList = NULL;
		if (!GetDOMTree(g_pRootElement, TAG_SOFTS, &pSofts)) {
			LogDebugMessage(Log_Error, _T("softsタグのDOM取得に失敗しています。<AnalyzeXML::ReadSoftsTag>"));
			return FALSE;
		}

		LONG childCount = GetChildList(pSofts, &pSoftsList);
		for (int i = 0; i < childCount; i++) {
			IXMLDOMNode* pTempNode = NULL;
			if (GetListItem(pSoftsList, i, &pTempNode)) {
				TCHAR szSoftwareName[BUFFER_SIZE] = {0};
				if (GetAttribute(pTempNode, TAG_NAME, szSoftwareName, _countof(szSoftwareName))) {

					// globalタグのターゲット名と比較
					TCHAR* pSoftwareName = new TCHAR[_tcslen(szSoftwareName)+1];
					_tcscpy_s(pSoftwareName, _tcslen(szSoftwareName)+1, szSoftwareName);
					g_pConfigPairContainer->push_back(config_pair(pSoftwareName, StoreValues(pTempNode, TAG_NAME)));
					OutputDebugString(szSoftwareName);
					OutputDebugString(_T(" push_back\n"));
				}
				FreeListItem(pTempNode);
			}
		}

		FreeChildList(pSoftsList);
		FreeDOMTree(pSofts);
	}
	return TRUE;
}