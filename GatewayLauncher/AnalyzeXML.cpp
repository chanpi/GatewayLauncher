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
 * XMLParser.dll�Ŏ擾����XML�̃}�b�v�I�u�W�F�N�g��������܂��B
 * 
 * XMLParser.dll�Ŏ擾����XML�̃}�b�v�I�u�W�F�N�g��������܂��B
 * 
 * @remarks
 * �}�b�v�̎擾�ɐ��������ꍇ�ɂ͕K���{�֐��ŉ�����Ă��������B
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
 * XML�����[�h���I�u�W�F�N�g�ɂ��܂��B
 * 
 * @param szXMLUri
 * ���[�h����XML�t�@�C���B
 * 
 * @returns
 * XML�t�@�C���̃��[�h�ɐ��������ꍇ�ɂ�TRUE�A���s�����ꍇ�ɂ�FALSE���Ԃ�܂��B
 * 
 * XMLParser.dll�𗘗p���AXML�����[�h���I�u�W�F�N�g�ɂ��܂��B
 */
BOOL AnalyzeXML::LoadXML(PCTSTR szXMLUri)
{
	CleanupRootElement();
	if (!PathFileExists(szXMLUri)) {
		{
			TCHAR szError[BUFFER_SIZE];
			_stprintf_s(szError, _countof(szError), _T("�w�肵���p�X�ɐݒ�t�@�C�������݂��܂���[%s]�B<AnalyzeXML::LoadXML>"), szXMLUri);
			LogDebugMessage(Log_Error, szError);
		}
		return FALSE;
	}
	g_bInitialized = Initialize(&g_pRootElement, szXMLUri);
	return g_bInitialized;
}

/**
 * @brief
 * global�^�O�̓��e��₢���킹�܂��B
 * 
 * @param szKey
 * global/keys/key�^�O�̃L�[�l�B
 * 
 * @returns
 * �}�b�v���ɃL�[�Ŏw�肵���l������Βl���A�Ȃ����NULL��Ԃ��܂��B
 * 
 * global�^�O�̓��e���i�[�����}�b�v�ɃL�[��₢���킹�A�l���擾���܂��B
 * 
 * @see
 * ReadGlobalTag()
 */
PCTSTR AnalyzeXML::GetGlobalValue(PCTSTR szKey)
{
	if (!ReadGlobalTag()) {
		ReportError(_T("[ERROR] global�^�O�̓ǂݍ��݂Ɏ��s���Ă��܂��B"));
		return NULL;
	}

	return GetMapItem(g_pGlobalConfig, szKey);
}

/**
 * @brief
 * softs�^�O�̓��e��₢���킹�܂��B
 * 
 * @param szKey
 * softs/soft/keys/key�^�O�̃L�[�l�B
 * 
 * @returns
 * �}�b�v���ɃL�[�Ŏw�肵���l������Βl���A�Ȃ����NULL��Ԃ��܂��B
 * 
 * softs�^�O�̓��e���i�[�����}�b�v�ɃL�[��₢���킹�A�l���擾���܂��B
 * 
 * @see
 * ReadGlobalTag() | ReadSoftsTag()
 */
PCTSTR AnalyzeXML::GetSoftValue(PCTSTR szSoftName, PCTSTR szKey)
{
	//if (!this->ReadGlobalTag()) {
	//	ReportError(_T("[ERROR] global�^�O�̓ǂݍ��݂Ɏ��s���Ă��܂��B"));
	//	return NULL;
	//}
	if (!this->ReadSoftsTag()) {
		ReportError(_T("[ERROR] softs�^�O�̓ǂݍ��݂Ɏ��s���Ă��܂��B"));
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
 * I4C3D.xml��global�^�O�̉�͂��s���܂��B
 * 
 * @returns
 * I4C3D.xml��global�^�O�̉�͂ɐ��������ꍇ�ɂ�TRUE�A���s�����ꍇ�ɂ�FALSE��Ԃ��܂��B
 * 
 * XMLParser.dll���g�p����I4C3D.xml��global�^�O�̉�͂��s���A�}�b�v�Ɋi�[���܂��B
 * ��: <key name="log">info</key>
 * key�^�Oname�A�g���r���[�g�̒l���}�b�v�̃L�[�ɁA�o�����[���}�b�v�̃o�����[�Ɋi�[���܂��B
 * 
 * @remarks
 * �}�b�v�̓��e���Q�Ƃ���ɂ�GetGlobalValue()���g�p���܂��B
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
 * I4C3D.xml��softs�^�O�̉�͂��s���܂��B
 * 
 * @returns
 * I4C3D.xml��softs�^�O�̉�͂ɐ��������ꍇ�ɂ�TRUE�A���s�����ꍇ�ɂ�FALSE��Ԃ��܂��B
 * 
 * XMLParser.dll���g�p����I4C3D.xml��softs�^�O�̉�͂��s���A�}�b�v�Ɋi�[���܂��B
 * 
 * @remarks
 * �}�b�v�̓��e���Q�Ƃ���ɂ�GetSoftValue()���g�p���܂��B
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
			LogDebugMessage(Log_Error, _T("softs�^�O��DOM�擾�Ɏ��s���Ă��܂��B<AnalyzeXML::ReadSoftsTag>"));
			return FALSE;
		}

		LONG childCount = GetChildList(pSofts, &pSoftsList);
		for (int i = 0; i < childCount; i++) {
			IXMLDOMNode* pTempNode = NULL;
			if (GetListItem(pSoftsList, i, &pTempNode)) {
				TCHAR szSoftwareName[BUFFER_SIZE] = {0};
				if (GetAttribute(pTempNode, TAG_NAME, szSoftwareName, _countof(szSoftwareName))) {

					// global�^�O�̃^�[�Q�b�g���Ɣ�r
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