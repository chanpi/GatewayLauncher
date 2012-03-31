#pragma once
#include <map>

class AnalyzeXML
{
public:
	AnalyzeXML(void);
	~AnalyzeXML(void);

	// �������R���X�g���N�^���Ă΂Ȃ��ꍇ�́A�ʓr�Ăяo�����Ƃ��ł��܂��B
	BOOL LoadXML(PCTSTR szXMLUri);

	PCTSTR GetGlobalValue(PCTSTR szKey);
	PCTSTR GetSoftValue(PCTSTR szSoftName, PCTSTR szKey);

private:
	BOOL ReadGlobalTag(void);
	BOOL ReadSoftsTag(void);
};

