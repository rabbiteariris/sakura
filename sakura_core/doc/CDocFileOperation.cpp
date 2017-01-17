/*
	Copyright (C) 2008, kobake

	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

		1. The origin of this software must not be misrepresented;
		   you must not claim that you wrote the original software.
		   If you use this software in a product, an acknowledgment
		   in the product documentation would be appreciated but is
		   not required.

		2. Altered source versions must be plainly marked as such,
		   and must not be misrepresented as being the original software.

		3. This notice may not be removed or altered from any source
		   distribution.
*/
#include "StdAfx.h"
#include "CDocFileOperation.h"
#include "CDocVisitor.h"
#include "CEditDoc.h"

#include "recent/CMRUFile.h"
#include "recent/CMRUFolder.h"
#include "dlg/CDlgOpenFile.h"
#include "_main/CAppMode.h" 
#include "_main/CControlTray.h"
#include "CEditApp.h"
#include "window/CEditWnd.h"
#include "uiparts/CWaitCursor.h"
#include "util/window.h"
#include "env/DLLSHAREDATA.h"
#include "env/CSakuraEnvironment.h"
#include "plugin/CPlugin.h"
#include "plugin/CJackManager.h"


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                          ���b�N                             //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

bool CDocFileOperation::_ToDoLock() const
{
	// �t�@�C�����J���Ă��Ȃ�
	if( !m_pcDocRef->m_cDocFile.GetFilePathClass().IsValidPath() )return false;

	// �r���[���[�h
	if( CAppMode::getInstance()->IsViewMode() )return false;

	// �r���ݒ�
	if( GetDllShareData().m_Common.m_sFile.m_nFileShareMode == SHAREMODE_NOT_EXCLUSIVE )return false;

	return true;
}

void CDocFileOperation::DoFileLock(bool bMsg)
{
	if(this->_ToDoLock()){
		m_pcDocRef->m_cDocFile.FileLock(GetDllShareData().m_Common.m_sFile.m_nFileShareMode, bMsg);
	}
}

void CDocFileOperation::DoFileUnlock()
{
	m_pcDocRef->m_cDocFile.FileUnlock();
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                         ���[�hUI                            //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

/* �u�t�@�C�����J���v�_�C�A���O */
//	Mar. 30, 2003 genta	�t�@�C�������莞�̏����f�B���N�g�����J�����g�t�H���_��
bool CDocFileOperation::OpenFileDialog(
	HWND				hwndParent,		//!< [in]
	const TCHAR*		pszOpenFolder,	//!< [in]     NULL�ȊO���w�肷��Ə����t�H���_���w��ł���
	SLoadInfo*			pLoadInfo,		//!< [in,out] ���[�h���
	std::vector<std::tstring>&	files
)
{
	/* �A�N�e�B�u�ɂ��� */
	ActivateFrameWindow( hwndParent );

	// �t�@�C���I�[�v���_�C�A���O��\��
	CDlgOpenFile cDlgOpenFile;
	cDlgOpenFile.Create(
		G_AppInstance(),
		hwndParent,
		_T("*.*"),
		pszOpenFolder ? pszOpenFolder : CSakuraEnvironment::GetDlgInitialDir().c_str(),	// �����t�H���_
		CMRUFile().GetPathList(),															// MRU���X�g�̃t�@�C���̃��X�g
		CMRUFolder().GetPathList()														// OPENFOLDER���X�g�̃t�@�C���̃��X�g
	);
	return cDlgOpenFile.DoModalOpenDlg( pLoadInfo, &files );
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                       ���[�h�t���[                          //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

bool CDocFileOperation::DoLoadFlow(SLoadInfo* pLoadInfo)
{
	ELoadResult eLoadResult = LOADED_FAILURE;

	try{
		//���[�h�O�`�F�b�N
		if(CALLBACK_INTERRUPT==m_pcDocRef->NotifyCheckLoad(pLoadInfo))throw CFlowInterruption();

		//���[�h����
		m_pcDocRef->NotifyBeforeLoad(pLoadInfo);			//�O����
		eLoadResult = m_pcDocRef->NotifyLoad(*pLoadInfo);	//�{����
		m_pcDocRef->NotifyAfterLoad(*pLoadInfo);			//�㏈��
	}
	catch(CFlowInterruption){
		eLoadResult = LOADED_INTERRUPT;
	}
	catch(...){
		//�\�����ʗ�O�����������ꍇ�� NotifyFinalLoad �͕K���ĂԁI
		m_pcDocRef->NotifyFinalLoad(LOADED_FAILURE);
		throw;
	}

	//�ŏI����
	m_pcDocRef->NotifyFinalLoad(eLoadResult);

	return eLoadResult==LOADED_OK;
}

//! �t�@�C�����J��
bool CDocFileOperation::FileLoad(
	SLoadInfo*	pLoadInfo		//!< [in,out]
)
{
	bool bRet = DoLoadFlow(pLoadInfo);
	// 2006.09.01 ryoji �I�[�v���㎩�����s�}�N�������s����
	if( bRet ){
		m_pcDocRef->RunAutoMacro( GetDllShareData().m_Common.m_sMacro.m_nMacroOnOpened );

		//�v���O�C���FDocumentOpen�C�x���g���s
		CPlug::Array plugs;
		CWSHIfObj::List params;
		CJackManager::getInstance()->GetUsablePlug( PP_DOCUMENT_OPEN, 0, &plugs );
		for( CPlug::ArrayIter it = plugs.begin(); it != plugs.end(); it++ ){
			(*it)->Invoke(&m_pcDocRef->m_pcEditWnd->GetActiveView(), params);
		}
	}
	return bRet;
}

//! �t�@�C�����J���i�������s�}�N�������s���Ȃ��j
//	2009.08.11 ryoji FileLoad�ւ̃p�����[�^�ǉ��ɂ��Ă�������ANSI�łƐ������Ƃ�₷���̂œ��ʂ͕ʊ֐��ɂ��Ă���
bool CDocFileOperation::FileLoadWithoutAutoMacro(
	SLoadInfo*	pLoadInfo		//!< [in,out]
)
{
	return DoLoadFlow(pLoadInfo);
}

//! ����t�@�C���̍ăI�[�v��
void CDocFileOperation::ReloadCurrentFile(
	ECodeType	nCharCode		//!< [in] �����R�[�h���
)
{
	//�v���O�C���FDocumentClose�C�x���g���s
	CPlug::Array plugs;
	CWSHIfObj::List params;
	CJackManager::getInstance()->GetUsablePlug( PP_DOCUMENT_CLOSE, 0, &plugs );
	for( CPlug::ArrayIter it = plugs.begin(); it != plugs.end(); it++ ){
		(*it)->Invoke(&m_pcDocRef->m_pcEditWnd->GetActiveView(), params);
	}

	if( !fexist(m_pcDocRef->m_cDocFile.GetFilePath()) ){
		/* �t�@�C�������݂��Ȃ� */
		//	Jul. 26, 2003 ryoji BOM��W���ݒ��	// IsBomDefOn�g�p 2013/5/17	Uchi
		m_pcDocRef->m_cDocFile.SetCodeSet( nCharCode,  CCodeTypeName( nCharCode ).IsBomDefOn() );
		// �J�[�\���ʒu�\�����X�V����	// 2008.07.22 ryoji
		m_pcDocRef->m_pcEditWnd->GetActiveView().GetCaret().ShowCaretPosInfo();
		return;
	}

	//�J�[�\���ʒu�ۑ�
	CLayoutInt		nViewTopLine = m_pcDocRef->m_pcEditWnd->GetActiveView().GetTextArea().GetViewTopLine();	/* �\����̈�ԏ�̍s(0�J�n) */
	CLayoutInt		nViewLeftCol = m_pcDocRef->m_pcEditWnd->GetActiveView().GetTextArea().GetViewLeftCol();	/* �\����̈�ԍ��̌�(0�J�n) */
	CLayoutPoint	ptCaretPosXY = m_pcDocRef->m_pcEditWnd->GetActiveView().GetCaret().GetCaretLayoutPos();

	//���[�h
	SLoadInfo sLoadInfo;
	sLoadInfo.cFilePath=m_pcDocRef->m_cDocFile.GetFilePath();
	sLoadInfo.eCharCode=nCharCode;
	sLoadInfo.bViewMode=CAppMode::getInstance()->IsViewMode(); // 2014.06.13 IsEditable->IsViewMode�ɖ߂��B������ bForceNoMsg��ǉ�
	sLoadInfo.bWritableNoMsg = !m_pcDocRef->IsEditable(); // ���łɕҏW�ł��Ȃ���ԂȂ�t�@�C�����b�N�̃��b�Z�[�W��\�����Ȃ�
	sLoadInfo.bRequestReload=true;
	bool bRet = this->DoLoadFlow(&sLoadInfo);

	// �J�[�\���ʒu���� (�������ł̓I�v�V�����̃J�[�\���ʒu�����i�����s�P�ʁj���w�肳��Ă��Ȃ��ꍇ�ł���������)
	// 2007.08.23 ryoji �\���̈敜��
	if( ptCaretPosXY.GetY2() < m_pcDocRef->m_cLayoutMgr.GetLineCount() ){
		m_pcDocRef->m_pcEditWnd->GetActiveView().GetTextArea().SetViewTopLine(nViewTopLine);
		m_pcDocRef->m_pcEditWnd->GetActiveView().GetTextArea().SetViewLeftCol(nViewLeftCol);
	}
	m_pcDocRef->m_pcEditWnd->GetActiveView().GetCaret().MoveCursorProperly( ptCaretPosXY, true );	// 2007.08.23 ryoji MoveCursor()->MoveCursorProperly()
	m_pcDocRef->m_pcEditWnd->GetActiveView().GetCaret().m_nCaretPosX_Prev = m_pcDocRef->m_pcEditWnd->GetActiveView().GetCaret().GetCaretLayoutPos().GetX2();

	// 2006.09.01 ryoji �I�[�v���㎩�����s�}�N�������s����
	if( bRet ){
		m_pcDocRef->RunAutoMacro( GetDllShareData().m_Common.m_sMacro.m_nMacroOnOpened );

		//�v���O�C���FDocumentOpen�C�x���g���s
		CPlug::Array plugs;
		CWSHIfObj::List params;
		CJackManager::getInstance()->GetUsablePlug( PP_DOCUMENT_OPEN, 0, &plugs );
		for( CPlug::ArrayIter it = plugs.begin(); it != plugs.end(); it++ ){
			(*it)->Invoke(&m_pcDocRef->m_pcEditWnd->GetActiveView(), params);
		}
	}
}




// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                         �Z�[�uUI                            //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

/*! �u�t�@�C������t���ĕۑ��v�_�C�A���O
	@date 2001.02.09 genta	���s�R�[�h�����������ǉ�
	@date 2003.03.30 genta	�t�@�C�������莞�̏����f�B���N�g�����J�����g�t�H���_��
	@date 2003.07.20 ryoji	BOM�̗L�������������ǉ�
	@date 2006.11.10 ryoji	���[�U�[�w��̊g���q���󋵈ˑ��ŕω�������
*/
bool CDocFileOperation::SaveFileDialog(
	SSaveInfo*	pSaveInfo	//!< [out]
)
{
	//�g���q�w��
	// �ꎞ�K�p��g���q�Ȃ��̏ꍇ�̊g���q���^�C�v�ʐݒ肩�玝���Ă���
	// 2008/6/14 �傫������ Uchi
	TCHAR	szDefaultWildCard[_MAX_PATH + 10];	// ���[�U�[�w��g���q
	{
		LPCTSTR	szExt;

		const STypeConfig& type = m_pcDocRef->m_cDocType.GetDocumentAttribute();
		//�t�@�C���p�X�������ꍇ�� *.txt �Ƃ���
		if(!this->m_pcDocRef->m_cDocFile.GetFilePathClass().IsValidPath()){
			szExt = _T("");
		}
		else {
			szExt = this->m_pcDocRef->m_cDocFile.GetFilePathClass().GetExt();
		}
		if (type.m_nIdx == 0) {
			// ��{
			if (szExt[0] == _T('\0')) { 
				// �t�@�C���p�X�������܂��͊g���q�Ȃ�
				_tcscpy(szDefaultWildCard, _T("*.txt"));
			}
			else {
				// �g���q����
				_tcscpy(szDefaultWildCard, _T("*"));
				_tcscat(szDefaultWildCard, szExt);
			}
		}
		else {
			szDefaultWildCard[0] = _T('\0'); 
			CDocTypeManager::ConvertTypesExtToDlgExt(type.m_szTypeExts, szExt, szDefaultWildCard);
		}

		if(!this->m_pcDocRef->m_cDocFile.GetFilePathClass().IsValidPath()){
			//�u�V�K����ۑ����͑S�t�@�C���\���v�I�v�V����	// 2008/6/15 �o�O�t�B�b�N�X Uchi
			if( GetDllShareData().m_Common.m_sFile.m_bNoFilterSaveNew )
				_tcscat(szDefaultWildCard, _T(";*.*"));	// �S�t�@�C���\��
		}
		else {
			//�u�V�K�ȊO����ۑ����͑S�t�@�C���\���v�I�v�V����
			if( GetDllShareData().m_Common.m_sFile.m_bNoFilterSaveFile )
				_tcscat(szDefaultWildCard, _T(";*.*"));	// �S�t�@�C���\��
		}
	}
	// ����ɁA����ԍ���t����
	if( pSaveInfo->cFilePath[0] == _T('\0') ){
		const EditNode* node = CAppNodeManager::getInstance()->GetEditNode( m_pcDocRef->m_pcEditWnd->GetHwnd() );
		if( 0 < node->m_nId ){
			TCHAR szText[16];
			auto_sprintf(szText, _T("%d"), node->m_nId);
			auto_strcpy(pSaveInfo->cFilePath, LS(STR_NO_TITLE2));	// ����
			auto_strcat(pSaveInfo->cFilePath, szText);
		}
	}

	// �_�C�A���O��\��
	CDlgOpenFile cDlgOpenFile;
	cDlgOpenFile.Create(
		G_AppInstance(),
		CEditWnd::getInstance()->GetHwnd(),
		szDefaultWildCard,
		CSakuraEnvironment::GetDlgInitialDir().c_str(),	// �����t�H���_
		CMRUFile().GetPathList(),		//	�ŋ߂̃t�@�C��
		CMRUFolder().GetPathList()	//	�ŋ߂̃t�H���_
	);
	return cDlgOpenFile.DoModalSaveDlg( pSaveInfo, pSaveInfo->eCharCode == CODE_CODEMAX );
}

//!�u�t�@�C������t���ĕۑ��v�_�C�A���O
bool CDocFileOperation::SaveFileDialog(LPTSTR szPath)
{
	SSaveInfo sSaveInfo;
	sSaveInfo.cFilePath = szPath;
	sSaveInfo.eCharCode = CODE_CODEMAX; //###�g���b�L�[
	bool bRet = SaveFileDialog(&sSaveInfo);
	_tcscpy_s(szPath, _MAX_PATH, sSaveInfo.cFilePath);
	return bRet;
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                       �Z�[�u�t���[                          //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

bool CDocFileOperation::DoSaveFlow(SSaveInfo* pSaveInfo)
{
	ESaveResult eSaveResult = SAVED_FAILURE;

	try{
		//�I�v�V�����F���ύX�ł��㏑�����邩
		// 2009.04.12 ryoji CSaveAgent::OnCheckSave()����ړ�
		// ### ���ύX�Ȃ�㏑�����Ȃ��Ŕ����鏈���͂ǂ� CDocListener �� OnCheckSave() �����O��
		// ### �i�ۑ����邩�ǂ����₢���킹���肷������O�Ɂj���؂����ƂȂ̂ŁA
		// ### �X�}�[�g����Ȃ��H��������Ȃ����ǁA�Ƃ肠���������ɔz�u���Ă���
		if( !GetDllShareData().m_Common.m_sFile.m_bEnableUnmodifiedOverwrite ){
			// �㏑���̏ꍇ
			if(pSaveInfo->bOverwriteMode){
				// ���ύX�̏ꍇ�͌x�������o���A�I��
				if (!m_pcDocRef->m_cDocEditor.IsModified() &&
					pSaveInfo->cEol==EOL_NONE &&	//�����s�R�[�h�w��ۑ������N�G�X�g���ꂽ�ꍇ�́A�u�ύX�����������́v�Ƃ݂Ȃ�
					!pSaveInfo->bChgCodeSet) {		// �����R�[�h�Z�b�g�̕ύX���L�����ꍇ�́A�u�ύX�����������́v�Ƃ݂Ȃ�
					CEditApp::getInstance()->m_cSoundSet.NeedlessToSaveBeep();
					throw CFlowInterruption();
				}
			}
		}

		//�Z�[�u�O�`�F�b�N
		if(CALLBACK_INTERRUPT==m_pcDocRef->NotifyCheckSave(pSaveInfo))throw CFlowInterruption();

		//�Z�[�u�O���܂�����
		if(CALLBACK_INTERRUPT==m_pcDocRef->NotifyPreBeforeSave(pSaveInfo))throw CFlowInterruption();

		// 2006.09.01 ryoji �ۑ��O�������s�}�N�������s����
		m_pcDocRef->RunAutoMacro( GetDllShareData().m_Common.m_sMacro.m_nMacroOnSave, pSaveInfo->cFilePath );

		//�v���O�C���FDocumentBeforeSave�C�x���g���s
		CPlug::Array plugs;
		CWSHIfObj::List params;
		CJackManager::getInstance()->GetUsablePlug( PP_DOCUMENT_BEFORE_SAVE, 0, &plugs );
		for( CPlug::ArrayIter it = plugs.begin(); it != plugs.end(); it++ ){
			(*it)->Invoke(&m_pcDocRef->m_pcEditWnd->GetActiveView(), params);
		}

		if(!pSaveInfo->bOverwriteMode){	//�㏑���łȂ���ΑO�����̃N���[�Y�C�x���g���Ă�
			//�v���O�C���FDocumentClose�C�x���g���s
			plugs.clear();
			CJackManager::getInstance()->GetUsablePlug( PP_DOCUMENT_CLOSE, 0, &plugs );
			for( CPlug::ArrayIter it = plugs.begin(); it != plugs.end(); it++ ){
				(*it)->Invoke(&m_pcDocRef->m_pcEditWnd->GetActiveView(), params);
			}
		}

		//�Z�[�u����
		m_pcDocRef->NotifyBeforeSave(*pSaveInfo);	//�O����
		m_pcDocRef->NotifySave(*pSaveInfo);			//�{����
		m_pcDocRef->NotifyAfterSave(*pSaveInfo);	//�㏈��

		//�v���O�C���FDocumentAfterSave�C�x���g���s
		plugs.clear();
		CJackManager::getInstance()->GetUsablePlug( PP_DOCUMENT_AFTER_SAVE, 0, &plugs );
		for( CPlug::ArrayIter it = plugs.begin(); it != plugs.end(); it++ ){
			(*it)->Invoke(&m_pcDocRef->m_pcEditWnd->GetActiveView(), params);
		}

		//����
		eSaveResult = SAVED_OK; //###��
	}
	catch(CFlowInterruption){
		eSaveResult = SAVED_INTERRUPT;
	}
	catch(...){
		//�\�����ʗ�O�����������ꍇ�� NotifyFinalSave �͕K���ĂԁI
		m_pcDocRef->NotifyFinalSave(SAVED_FAILURE);
		throw;
	}

	//�ŏI����
	m_pcDocRef->NotifyFinalSave(eSaveResult);

	return eSaveResult==SAVED_OK;
}


/*! �㏑���ۑ�

	@return �ۑ����s��ꂽor�ۑ��s�v�̂��߉����s��Ȃ������Ƃ���true��Ԃ�

	@date 2004.06.05 genta  �r���[���[�h�̃`�F�b�N��CEditDoc����㏑���ۑ������Ɉړ�
	@date 2006.12.30 ryoji  CEditView::Command_FILESAVE()���珈���{�̂�؂�o��
	@date 2008.03.20 kobake �߂�l�̎d�l���`
*/
bool CDocFileOperation::FileSave()
{
	//�t�@�C�������w�肳��Ă��Ȃ��ꍇ�́u���O��t���ĕۑ��v�̃t���[�֑J��
	if( !m_pcDocRef->m_cDocFile.GetFilePathClass().IsValidPath() ){
		return FileSaveAs();
	}

	//�Z�[�u���
	SSaveInfo sSaveInfo;
	m_pcDocRef->GetSaveInfo(&sSaveInfo);
	sSaveInfo.cEol = EOL_NONE; //���s�R�[�h���ϊ�
	sSaveInfo.bOverwriteMode = true; //�㏑���v��

	//�㏑������
	return m_pcDocRef->m_cDocFileOperation.DoSaveFlow(&sSaveInfo);
}



/*! ���O��t���ĕۑ��t���[

	@date 2006.12.30 ryoji CEditView::Command_FILESAVEAS_DIALOG()���珈���{�̂�؂�o��
*/
bool CDocFileOperation::FileSaveAs( const WCHAR* filename,ECodeType eCodeType, EEolType eEolType, bool bDialog )
{
	//�Z�[�u���
	SSaveInfo sSaveInfo;
	m_pcDocRef->GetSaveInfo(&sSaveInfo);
	sSaveInfo.cEol = EOL_NONE; // �����l�͕ϊ����Ȃ�
	if( filename ){
		// �_�C�A���O�Ȃ��ۑ��A�܂��̓}�N���̈�������
		sSaveInfo.cFilePath = to_tchar(filename);
		if( EOL_NONE <= eEolType && eEolType < EOL_CODEMAX ){
			sSaveInfo.cEol = eEolType;
		}
		if( IsValidCodeType(eCodeType) && eCodeType != sSaveInfo.eCharCode ){
			sSaveInfo.eCharCode = eCodeType;
			sSaveInfo.bBomExist = CCodeTypeName(eCodeType).IsBomDefOn();
		}
	}
	if( bDialog ){
		if(!filename && CAppMode::getInstance()->IsViewMode()){
			sSaveInfo.cFilePath = _T(""); //���ǂݍ��ݐ�p���[�h�̂Ƃ��̓t�@�C�������w�肵�Ȃ�
		}

		//�_�C�A���O�\��
		if(!SaveFileDialog(&sSaveInfo))return false;
	}

	//�Z�[�u����
	if( DoSaveFlow(&sSaveInfo) ){
		// �I�[�v���㎩�����s�}�N�������s����iANSI�łł͂����ōă��[�h���s���������s�}�N�������s�����j
		// ��Ď��� Patches#1550557 �ɁA�u���O��t���ĕۑ��v�ŃI�[�v���㎩�����s�}�N�������s����邱�Ƃ̐���ɂ��ċc�_�̌o�܂���
		//   ���h�t�@�C�����ɉ����ĕ\����ω�������}�N���Ƃ���z�肷��ƁA����͂���ł����悤�Ɏv���܂��B�h
		m_pcDocRef->RunAutoMacro( GetDllShareData().m_Common.m_sMacro.m_nMacroOnOpened );

		//�v���O�C���FDocumentOpen�C�x���g���s
		CPlug::Array plugs;
		CWSHIfObj::List params;
		CJackManager::getInstance()->GetUsablePlug( PP_DOCUMENT_OPEN, 0, &plugs );
		for( CPlug::ArrayIter it = plugs.begin(); it != plugs.end(); it++ ){
			(*it)->Invoke(&m_pcDocRef->m_pcEditWnd->GetActiveView(), params);
		}

		return true;
	}

	return false;
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                         �N���[�Y                            //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //


/*
	����(����)�B
	���[�U�L�����Z�����쓙�ɂ��N���[�Y����Ȃ������ꍇ�� false ��Ԃ��B

	@date 2006.12.30 ryoji CEditView::Command_FILESAVEAS()���珈���{�̂�؂�o��
*/
bool CDocFileOperation::FileClose()
{
	/* �t�@�C�������Ƃ���MRU�o�^ & �ۑ��m�F & �ۑ����s */
	if( !m_pcDocRef->OnFileClose(false) ){
		return false;
	}

	//�v���O�C���FDocumentClose�C�x���g���s
	CPlug::Array plugs;
	CWSHIfObj::List params;
	CJackManager::getInstance()->GetUsablePlug( PP_DOCUMENT_CLOSE, 0, &plugs );
	for( CPlug::ArrayIter it = plugs.begin(); it != plugs.end(); it++ ){
		(*it)->Invoke(&m_pcDocRef->m_pcEditWnd->GetActiveView(), params);
	}

	/* �����f�[�^�̃N���A */
	m_pcDocRef->InitDoc();

	/* �S�r���[�̏����� */
	m_pcDocRef->InitAllView();

	m_pcDocRef->SetCurDirNotitle();

	// ����ԍ��擾
	CAppNodeManager::getInstance()->GetNoNameNumber( m_pcDocRef->m_pcEditWnd->GetHwnd() );

	/* �e�E�B���h�E�̃^�C�g�����X�V */
	m_pcDocRef->m_pcEditWnd->UpdateCaption();

	// 2006.09.01 ryoji �I�[�v���㎩�����s�}�N�������s����
	m_pcDocRef->RunAutoMacro( GetDllShareData().m_Common.m_sMacro.m_nMacroOnOpened );

	return true;
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                          ���̑�                             //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

/* ���ĊJ��
	@date 2006.12.30 ryoji CEditView::Command_FILESAVEAS()���珈���{�̂�؂�o��
*/
void CDocFileOperation::FileCloseOpen( const SLoadInfo& _sLoadInfo )
{
	/* �t�@�C�������Ƃ���MRU�o�^ & �ۑ��m�F & �ۑ����s */
	if( !m_pcDocRef->OnFileClose(false) ){
		return;
	}

	//�v���O�C���FDocumentClose�C�x���g���s
	CPlug::Array plugs;
	CWSHIfObj::List params;
	CJackManager::getInstance()->GetUsablePlug( PP_DOCUMENT_CLOSE, 0, &plugs );
	for( CPlug::ArrayIter it = plugs.begin(); it != plugs.end(); it++ ){
		(*it)->Invoke(&m_pcDocRef->m_pcEditWnd->GetActiveView(), params);
	}

	//�t�@�C�����w�肪�����ꍇ�̓_�C�A���O�œ��͂�����
	SLoadInfo sLoadInfo = _sLoadInfo;
	if( sLoadInfo.cFilePath.Length()==0 ){
		std::vector<std::tstring> files;
		if( !OpenFileDialog( CEditWnd::getInstance()->GetHwnd(), NULL, &sLoadInfo, files ) ){
			return;
		}
		sLoadInfo.cFilePath = files[0].c_str();
		// ���̃t�@�C���͐V�K�E�B���h�E
		size_t nSize = files.size();
		for( size_t i = 1; i < nSize; i++ ){
			SLoadInfo sFilesLoadInfo = sLoadInfo;
			sFilesLoadInfo.cFilePath = files[i].c_str();
			CControlTray::OpenNewEditor(
				G_AppInstance(),
				CEditWnd::getInstance()->GetHwnd(),
				sFilesLoadInfo,
				NULL,
				true
			);
		}
	}

	/* �����f�[�^�̃N���A */
	m_pcDocRef->InitDoc();

	/* �S�r���[�̏����� */
	m_pcDocRef->InitAllView();

	//�J��
	FileLoadWithoutAutoMacro(&sLoadInfo);

	if( !m_pcDocRef->m_cDocFile.GetFilePathClass().IsValidPath() ){
		m_pcDocRef->SetCurDirNotitle();
		CAppNodeManager::getInstance()->GetNoNameNumber( m_pcDocRef->m_pcEditWnd->GetHwnd() );
	}

	/* �e�E�B���h�E�̃^�C�g�����X�V */
	m_pcDocRef->m_pcEditWnd->UpdateCaption();

	// �I�[�v���㎩�����s�}�N�������s����
	// �����[�h���ĂȂ��Ă�(����)�ɂ͕ύX�ς�
	m_pcDocRef->RunAutoMacro( GetDllShareData().m_Common.m_sMacro.m_nMacroOnOpened );

	//�v���O�C���FDocumentOpen�C�x���g���s
	plugs.clear();
	CJackManager::getInstance()->GetUsablePlug( PP_DOCUMENT_OPEN, 0, &plugs );
	for( CPlug::ArrayIter it = plugs.begin(); it != plugs.end(); it++ ){
		(*it)->Invoke(&m_pcDocRef->m_pcEditWnd->GetActiveView(), params);
	}
}