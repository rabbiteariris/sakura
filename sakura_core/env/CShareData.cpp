/*!	@file
	@brief �v���Z�X�ԋ��L�f�[�^�ւ̃A�N�Z�X

	@author Norio Nakatani
	@date 1998/05/26  �V�K�쐬
*/
/*
	Copyright (C) 1998-2001, Norio Nakatani
	Copyright (C) 2000, jepro, genta, MIK
	Copyright (C) 2001, jepro, genta, asa-o, MIK, YAZAKI, hor
	Copyright (C) 2002, genta, ai, Moca, MIK, YAZAKI, hor, KK, aroka
	Copyright (C) 2003, Moca, aroka, MIK, genta, wmlhq, sui
	Copyright (C) 2004, Moca, novice, genta, isearch, MIK
	Copyright (C) 2005, Moca, MIK, genta, ryoji, ���, aroka
	Copyright (C) 2006, aroka, ryoji, genta
	Copyright (C) 2007, ryoji, genta, maru
	Copyright (C) 2008, ryoji, Uchi, nasukoji
	Copyright (C) 2009, nasukoji, ryoji
	Copyright (C) 2011, nasukoji
	Copyright (C) 2012, Moca, ryoji

	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

#include "StdAfx.h"
#include "env/CShareData.h"
#include "env/DLLSHAREDATA.h"
#include "env/CShareData_IO.h"
#include "env/CSakuraEnvironment.h"
#include "doc/CDocListener.h" // SLoadInfo
#include "_main/CControlTray.h"
#include "_main/CCommandLine.h"
#include "_main/CMutex.h"
#include "charset/CCodePage.h"
#include "debug/CRunningTimer.h"
#include "recent/CMRUFile.h"
#include "recent/CMRUFolder.h"
#include "util/module.h"
#include "util/string_ex2.h"
#include "util/window.h"
#include "util/os.h"
#include "CDataProfile.h"
#include "sakura_rc.h"

struct ARRHEAD {
	int		nLength;
	int		nItemNum;
};

const unsigned int uShareDataVersion = N_SHAREDATA_VERSION;

//	CShareData_new2.cpp�Ɠ���
//@@@ 2002.01.03 YAZAKI m_tbMyButton�Ȃǂ�CShareData����CMenuDrawer�ֈړ�
CShareData::CShareData()
{
	m_hFileMap   = NULL;
	m_pShareData = NULL;
	m_pvTypeSettings = NULL;
}

/*!
	���L�������̈悪����ꍇ�̓v���Z�X�̃A�h���X��Ԃ���
	���łɃ}�b�v����Ă���t�@�C�� �r���[���A���}�b�v����B
*/
CShareData::~CShareData()
{
	if( m_pShareData ){
		/* �v���Z�X�̃A�h���X��Ԃ��� ���łɃ}�b�v����Ă���t�@�C�� �r���[���A���}�b�v���܂� */
		SetDllShareData( NULL );
		::UnmapViewOfFile( m_pShareData );
		m_pShareData = NULL;
	}
	if( m_hFileMap ){
		CloseHandle( m_hFileMap );
	}
	if( m_pvTypeSettings ){
		for( int i = 0; i < (int)m_pvTypeSettings->size(); i++ ){
			delete (*m_pvTypeSettings)[i];
			(*m_pvTypeSettings)[i] = NULL;
		}
		delete m_pvTypeSettings;
		m_pvTypeSettings = NULL;
	}
}


static CMutex g_cMutexShareWork( FALSE, GSTR_MUTEX_SAKURA_SHAREWORK );
 
CMutex& CShareData::GetMutexShareWork(){
	return g_cMutexShareWork;
}

//! CShareData�N���X�̏���������
/*!
	CShareData�N���X�𗘗p����O�ɕK���Ăяo�����ƁB

	@retval true ����������
	@retval false ���������s

	@note ���ɑ��݂��鋤�L�������̃o�[�W���������̃G�f�B�^���g�����̂�
	�قȂ�ꍇ�͒v���I�G���[��h�����߂�false��Ԃ��܂��BCProcess::Initialize()
	��Init()�Ɏ��s����ƃ��b�Z�[�W���o���ăG�f�B�^�̋N���𒆎~���܂��B
*/
bool CShareData::InitShareData()
{
	MY_RUNNINGTIMER(cRunningTimer,"CShareData::InitShareData" );

	m_hwndTraceOutSource = NULL;	// 2006.06.26 ryoji

	/* �t�@�C���}�b�s���O�I�u�W�F�N�g */
	{
		std::tstring strProfileName = to_tchar(CCommandLine::getInstance()->GetProfileName());
		std::tstring strShareDataName = GSTR_SHAREDATA;
		strShareDataName += strProfileName;
		m_hFileMap = ::CreateFileMapping(
			INVALID_HANDLE_VALUE,	//	Sep. 6, 2003 wmlhq
			NULL,
			PAGE_READWRITE | SEC_COMMIT,
			0,
			sizeof( DLLSHAREDATA ),
			strShareDataName.c_str()
		);
	}
	if( NULL == m_hFileMap ){
		::MessageBox(
			NULL,
			_T("CreateFileMapping()�Ɏ��s���܂���"),
			_T("�\�����ʃG���["),
			MB_OK | MB_APPLMODAL | MB_ICONSTOP
		);
		return false;
	}

	if( GetLastError() != ERROR_ALREADY_EXISTS ){
		/* �I�u�W�F�N�g�����݂��Ă��Ȃ������ꍇ */
		/* �t�@�C���̃r���[�� �Ăяo�����v���Z�X�̃A�h���X��ԂɃ}�b�v���܂� */
		m_pShareData = (DLLSHAREDATA*)::MapViewOfFile(
			m_hFileMap,
			FILE_MAP_ALL_ACCESS,
			0,
			0,
			0
		);
		CreateTypeSettings();
		SetDllShareData( m_pShareData );

		// 2007.05.19 ryoji ���s�t�@�C���t�H���_->�ݒ�t�@�C���t�H���_�ɕύX
		TCHAR	szIniFolder[_MAX_PATH];
		m_pShareData->m_sFileNameManagement.m_IniFolder.m_bInit = false;
		GetInidir( szIniFolder );
		AddLastChar( szIniFolder, _MAX_PATH, _T('\\') );

		m_pShareData->m_vStructureVersion = uShareDataVersion;
		m_pShareData->m_nSize = sizeof(*m_pShareData);

		// 2004.05.13 Moca ���\�[�X���琻�i�o�[�W�����̎擾
		GetAppVersionInfo( NULL, VS_VERSION_INFO,
			&m_pShareData->m_sVersion.m_dwProductVersionMS, &m_pShareData->m_sVersion.m_dwProductVersionLS );

		m_pShareData->m_sFlags.m_bEditWndChanging = FALSE;	// �ҏW�E�B���h�E�ؑ֒�	// 2007.04.03 ryoji
		m_pShareData->m_sFlags.m_bRecordingKeyMacro = FALSE;		/* �L�[�{�[�h�}�N���̋L�^�� */
		m_pShareData->m_sFlags.m_hwndRecordingKeyMacro = NULL;	/* �L�[�{�[�h�}�N�����L�^���̃E�B���h�E */

		m_pShareData->m_sNodes.m_nSequences = 0;					/* �E�B���h�E�A�� */
		m_pShareData->m_sNodes.m_nNonameSequences = 0;
		m_pShareData->m_sNodes.m_nGroupSequences = 0;			/* �^�u�O���[�v�A�� */	// 2007.06.20 ryoji
		m_pShareData->m_sNodes.m_nEditArrNum = 0;

		m_pShareData->m_sHandles.m_hwndTray = NULL;
		m_pShareData->m_sHandles.m_hAccel = NULL;
		m_pShareData->m_sHandles.m_hwndDebug = NULL;

		for( int i = 0; i < _countof(m_pShareData->m_dwCustColors); i++ ){
			m_pShareData->m_dwCustColors[i] = RGB( 255, 255, 255 );
		}

//@@@ 2001.12.26 YAZAKI MRU���X�g�́ACMRU�Ɉ˗�����
		CMRUFile cMRU;
		cMRU.ClearAll();
//@@@ 2001.12.26 YAZAKI OPENFOLDER���X�g�́ACMRUFolder�ɂ��ׂĈ˗�����
		CMRUFolder cMRUFolder;
		cMRUFolder.ClearAll();

//	From Here Sept. 19, 2000 JEPRO �R�����g�A�E�g�ɂȂ��Ă������߂̃u���b�N�𕜊������̉����R�����g�A�E�g
//	MS �S�V�b�N�W���X�^�C��10pt�ɐݒ�
//		/* LOGFONT�̏����� */
		LOGFONT lf;
		memset_raw( &lf, 0, sizeof( lf ) );
		lf.lfHeight			= DpiPointsToPixels(-10);	// 2009.10.01 ryoji ��DPI�Ή��i�|�C���g������Z�o�j
		lf.lfWidth				= 0;
		lf.lfEscapement		= 0;
		lf.lfOrientation		= 0;
		lf.lfWeight			= 400;
		lf.lfItalic			= 0x0;
		lf.lfUnderline			= 0x0;
		lf.lfStrikeOut			= 0x0;
		lf.lfCharSet			= 0x80;
		lf.lfOutPrecision		= 0x3;
		lf.lfClipPrecision		= 0x2;
		lf.lfQuality			= 0x1;
		lf.lfPitchAndFamily	= 0x31;
		_tcscpy( lf.lfFaceName, _T("�l�r �S�V�b�N") );

		// LoadShareData�Ńt�H���g���ς��\��������̂ŁA�����ł͕s�v // 2013.04.08 aroka
		//InitCharWidthCacheCommon();								// 2008/5/17 Uchi

		// �L�[���[�h�w���v�̃t�H���g ai 02/05/21 Add S
		LOGFONT lfIconTitle;	// �G�N�X�v���[���̃t�@�C�����\���Ɏg�p�����t�H���g
		::SystemParametersInfo(
			SPI_GETICONTITLELOGFONT,				// system parameter to query or set
			sizeof(LOGFONT),						// depends on action to be taken
			(PVOID)&lfIconTitle,					// depends on action to be taken
			0										// user profile update flag
		);
		// ai 02/05/21 Add E

		INT		nIconPointSize = lfIconTitle.lfHeight >=0 ? lfIconTitle.lfHeight : DpiPixelsToPoints( -lfIconTitle.lfHeight, 10 );	// �t�H���g�T�C�Y�i1/10�|�C���g�P�ʁj
//	To Here Sept. 19,2000

		// [�S��]�^�u
		{
			CommonSetting_General& sGeneral = m_pShareData->m_Common.m_sGeneral;

			sGeneral.m_nMRUArrNum_MAX = 15;	/* �t�@�C���̗���MAX */	//Oct. 14, 2000 JEPRO �������₵��(10��15)
			sGeneral.m_nOPENFOLDERArrNum_MAX = 15;	/* �t�H���_�̗���MAX */	//Oct. 14, 2000 JEPRO �������₵��(10��15)

			sGeneral.m_nCaretType = 0;					/* �J�[�\���̃^�C�v 0=win 1=dos */
			sGeneral.m_bIsINSMode = true;				/* �}���^�㏑�����[�h */
			sGeneral.m_bIsFreeCursorMode = false;		/* �t���[�J�[�\�����[�h�� */	//Oct. 29, 2000 JEPRO �u�Ȃ��v�ɕύX

			sGeneral.m_bStopsBothEndsWhenSearchWord = FALSE;	/* �P��P�ʂňړ�����Ƃ��ɁA�P��̗��[�Ŏ~�܂邩 */
			sGeneral.m_bStopsBothEndsWhenSearchParagraph = FALSE;	/* �P��P�ʂňړ�����Ƃ��ɁA�P��̗��[�Ŏ~�܂邩 */

			sGeneral.m_bCloseAllConfirm = FALSE;		/* [���ׂĕ���]�ő��ɕҏW�p�̃E�B���h�E������Ίm�F���� */	// 2006.12.25 ryoji
			sGeneral.m_bExitConfirm = FALSE;			/* �I�����̊m�F������ */
			sGeneral.m_nRepeatedScrollLineNum = CLayoutInt(3);	/* �L�[���s�[�g���̃X�N���[���s�� */
			sGeneral.m_nRepeatedScroll_Smooth = FALSE;	/* �L�[���s�[�g���̃X�N���[�������炩�ɂ��邩 */
			sGeneral.m_nPageScrollByWheel = 0;			/* �L�[/�}�E�X�{�^�� + �z�C�[���X�N���[���Ńy�[�W�X�N���[������ */	// 2009.01.17 nasukoji
			sGeneral.m_nHorizontalScrollByWheel = 0;	/* �L�[/�}�E�X�{�^�� + �z�C�[���X�N���[���ŉ��X�N���[������ */		// 2009.01.17 nasukoji

			sGeneral.m_bUseTaskTray = TRUE;				/* �^�X�N�g���C�̃A�C�R�����g�� */
#ifdef _DEBUG
			sGeneral.m_bStayTaskTray = FALSE;				/* �^�X�N�g���C�̃A�C�R�����풓 */
#else
			sGeneral.m_bStayTaskTray = TRUE;				/* �^�X�N�g���C�̃A�C�R�����풓 */
#endif
			sGeneral.m_wTrayMenuHotKeyCode = L'Z';		/* �^�X�N�g���C���N���b�N���j���[ �L�[ */
			sGeneral.m_wTrayMenuHotKeyMods = HOTKEYF_ALT | HOTKEYF_CONTROL;	/* �^�X�N�g���C���N���b�N���j���[ �L�[ */

			sGeneral.m_bDispExitingDialog = FALSE;		/* �I���_�C�A���O��\������ */

			sGeneral.m_bNoCaretMoveByActivation = FALSE;	/* �}�E�X�N���b�N�ɂăA�N�e�B�x�[�g���ꂽ���̓J�[�\���ʒu���ړ����Ȃ� 2007.10.02 nasukoji (add by genta) */
		}

		// [�E�B���h�E]�^�u
		{
			CommonSetting_Window& sWindow = m_pShareData->m_Common.m_sWindow;

			sWindow.m_bDispTOOLBAR = TRUE;			/* ����E�B���h�E���J�����Ƃ��c�[���o�[��\������ */
			sWindow.m_bDispSTATUSBAR = TRUE;			/* ����E�B���h�E���J�����Ƃ��X�e�[�^�X�o�[��\������ */
			sWindow.m_bDispFUNCKEYWND = FALSE;		/* ����E�B���h�E���J�����Ƃ��t�@���N�V�����L�[��\������ */
			sWindow.m_bDispMiniMap = false;			// �~�j�}�b�v��\������
			sWindow.m_nFUNCKEYWND_Place = 1;			/* �t�@���N�V�����L�[�\���ʒu�^0:�� 1:�� */
			sWindow.m_nFUNCKEYWND_GroupNum = 4;			// 2002/11/04 Moca �t�@���N�V�����L�[�̃O���[�v�{�^����
			sWindow.m_nMiniMapFontSize = -1;
			sWindow.m_nMiniMapQuality = NONANTIALIASED_QUALITY;
			sWindow.m_nMiniMapWidth = 150;

			sWindow.m_bSplitterWndHScroll = TRUE;	// 2001/06/20 asa-o �����E�B���h�E�̐����X�N���[���̓������Ƃ�
			sWindow.m_bSplitterWndVScroll = TRUE;	// 2001/06/20 asa-o �����E�B���h�E�̐����X�N���[���̓������Ƃ�

			// 2001/06/14 asa-o �⊮�ƃL�[���[�h�w���v�̓^�C�v�ʂɈړ������̂ō폜
			//	2004.05.13 Moca �E�B���h�E�T�C�Y�Œ�w��ǉ��ɔ����w����@�ύX
			sWindow.m_eSaveWindowSize = WINSIZEMODE_SAVE;	// �E�B���h�E�T�C�Y�p��
			sWindow.m_nWinSizeType = SIZE_RESTORED;
			sWindow.m_nWinSizeCX = CW_USEDEFAULT;
			sWindow.m_nWinSizeCY = 0;
		
			sWindow.m_bScrollBarHorz = TRUE;				/* �����X�N���[���o�[���g�� */
			//	2004.05.13 Moca �E�B���h�E�ʒu
			sWindow.m_eSaveWindowPos = WINSIZEMODE_DEF;		// �E�B���h�E�ʒu�Œ�E�p��
			sWindow.m_nWinPosX = CW_USEDEFAULT;
			sWindow.m_nWinPosY = 0;

			sWindow.m_nRulerHeight = 13;					/* ���[���[�̍��� */
			sWindow.m_nRulerBottomSpace = 0;				/* ���[���[�ƃe�L�X�g�̌��� */
			sWindow.m_nRulerType = 0;					/* ���[���[�̃^�C�v */
			sWindow.m_nLineNumRightSpace = 0;			/* �s�ԍ��̉E�̌��� */
			sWindow.m_nVertLineOffset = -1;			// 2005.11.10 Moca �w�茅�c��
			sWindow.m_bUseCompatibleBMP = TRUE;		// 2007.09.09 Moca ��ʃL���b�V�����g��	// 2009.06.09 ryoji FALSE->TRUE

			sWindow.m_bMenuIcon = TRUE;		/* ���j���[�ɃA�C�R����\������ */

			//	Apr. 05, 2003 genta �E�B���h�E�L���v�V�����̏����l
			//	Aug. 16, 2003 genta $N(�t�@�C�����ȗ��\��)���f�t�H���g�ɕύX
			_tcscpy( sWindow.m_szWindowCaptionActive, 
				_T("${w?$h$:�A�E�g�v�b�g$:${I?$f$n$:$N$n$}$}${U?(�X�V)$} -")
				_T(" $A $V ${R?(�r���[���[�h)$:(�㏑���֎~)$}${M?  �y�L�[�}�N���̋L�^���z$} $<profile>") );
			_tcscpy( sWindow.m_szWindowCaptionInactive, 
				_T("${w?$h$:�A�E�g�v�b�g$:$f$n$}${U?(�X�V)$} -")
				_T(" $A $V ${R?(�r���[���[�h)$:(�㏑���֎~)$}${M?  �y�L�[�}�N���̋L�^���z$} $<profile>") );
		}

		// [�^�u�o�[]�^�u
		{
			CommonSetting_TabBar& sTabBar = m_pShareData->m_Common.m_sTabBar;

			sTabBar.m_bDispTabWnd = FALSE;			//�^�u�E�C���h�E�\��	//@@@ 2003.05.31 MIK
			sTabBar.m_bDispTabWndMultiWin = FALSE;	//�^�u�E�C���h�E�\��	//@@@ 2003.05.31 MIK
			wcscpy(	//@@@ 2003.06.13 MIK
				sTabBar.m_szTabWndCaption,
				L"${w?�yGrep�z$h$:�y�A�E�g�v�b�g�z$:$f$n$}${U?(�X�V)$}${R?(�r���[���[�h)$:(�㏑���֎~)$}${M?�y�L�[�}�N���̋L�^���z$}"
			);
			sTabBar.m_bSameTabWidth = FALSE;			//�^�u�𓙕��ɂ���			//@@@ 2006.01.28 ryoji
			sTabBar.m_bDispTabIcon = FALSE;			//�^�u�ɃA�C�R����\������	//@@@ 2006.01.28 ryoji
			sTabBar.m_bDispTabClose = DISPTABCLOSE_NO;	//�^�u�ɕ���{�^����\������	//@@@ 2012.04.14 syat
			sTabBar.m_bSortTabList = TRUE;			//�^�u�ꗗ���\�[�g����		//@@@ 2006.05.10 ryoji
			sTabBar.m_bTab_RetainEmptyWin = TRUE;	// �Ō�̃t�@�C��������ꂽ�Ƃ�(����)���c��	// 2007.02.11 genta
			sTabBar.m_bTab_CloseOneWin = FALSE;	// �^�u���[�h�ł��E�B���h�E�̕���{�^���Ō��݂̃t�@�C���̂ݕ���	// 2007.02.11 genta
			sTabBar.m_bTab_ListFull = FALSE;			//�^�u�ꗗ���t���p�X�\������	//@@@ 2007.02.28 ryoji
			sTabBar.m_bChgWndByWheel = FALSE;		//�}�E�X�z�C�[���ŃE�B���h�E�ؑ�	//@@@ 2006.03.26 ryoji
			sTabBar.m_bNewWindow = FALSE;			// �O������N������Ƃ��͐V�����E�C���h�E�ŊJ��
			sTabBar.m_bTabMultiLine = false;		// �^�u���i
			sTabBar.m_eTabPosition = TabPosition_Top;		//�^�u�ʒu

			sTabBar.m_lf = lfIconTitle;
			sTabBar.m_nPointSize = nIconPointSize;
			sTabBar.m_nTabMaxWidth = 200;
			sTabBar.m_nTabMinWidth = 60;
			sTabBar.m_nTabMinWidthOnMulti = 100;
		}

		// [�ҏW]�^�u
		{
			CommonSetting_Edit& sEdit = m_pShareData->m_Common.m_sEdit;

			sEdit.m_bAddCRLFWhenCopy = false;			/* �܂�Ԃ��s�ɉ��s��t���ăR�s�[ */

			sEdit.m_bUseOLE_DragDrop = TRUE;			/* OLE�ɂ��h���b�O & �h���b�v���g�� */
			sEdit.m_bUseOLE_DropSource = TRUE;			/* OLE�ɂ��h���b�O���ɂ��邩 */
			sEdit.m_bSelectClickedURL = TRUE;			/* URL���N���b�N���ꂽ��I�����邩 */
			sEdit.m_bCopyAndDisablSelection = FALSE;	/* �R�s�[������I������ */
			sEdit.m_bEnableNoSelectCopy = TRUE;		/* �I���Ȃ��ŃR�s�[���\�ɂ��� */	// 2007.11.18 ryoji
			sEdit.m_bEnableLineModePaste = true;		/* ���C�����[�h�\��t�����\�ɂ��� */	// 2007.10.08 ryoji
			sEdit.m_bConvertEOLPaste = false;			/* ���s�R�[�h��ϊ����ē\��t���� */	// 2009.02.28 salarm
			sEdit.m_bEnableExtEol = false;
			sEdit.m_bBoxSelectLock = true;

			sEdit.m_bNotOverWriteCRLF = TRUE;			/* ���s�͏㏑�����Ȃ� */
			sEdit.m_bOverWriteFixMode = false;			// �������ɍ��킹�ăX�y�[�X���l�߂�

			sEdit.m_bOverWriteBoxDelete = false;
			sEdit.m_eOpenDialogDir = OPENDIALOGDIR_CUR;
			auto_strcpy(sEdit.m_OpenDialogSelDir, _T("%Personal%\\"));
			sEdit.m_bAutoColumnPaste = TRUE;			/* ��`�R�s�[�̃e�L�X�g�͏�ɋ�`�\��t�� */
		}

		// [�t�@�C��]�^�u
		{
			CommonSetting_File& sFile = m_pShareData->m_Common.m_sFile;

			//�t�@�C���̔r������
			sFile.m_nFileShareMode = SHAREMODE_DENY_WRITE;	// �t�@�C���̔r�����䃂�[�h
			sFile.m_bCheckFileTimeStamp = true;			// �X�V�̊Ď�
			sFile.m_nAutoloadDelay = 0;					// �����Ǎ����x��
			sFile.m_bUneditableIfUnwritable = true;		// �㏑���֎~���o���͕ҏW�֎~�ɂ���

			//�t�@�C���̕ۑ�
			sFile.m_bEnableUnmodifiedOverwrite = false;	// ���ύX�ł��㏑�����邩

			// �u���O��t���ĕۑ��v�Ńt�@�C���̎�ނ�[���[�U�w��]�̂Ƃ��̃t�@�C���ꗗ�\��	//�t�@�C���ۑ��_�C�A���O�̃t�B���^�ݒ�	// 2006.11.16 ryoji
			sFile.m_bNoFilterSaveNew = true;		// �V�K����ۑ����͑S�t�@�C���\��
			sFile.m_bNoFilterSaveFile = true;		// �V�K�ȊO����ۑ����͑S�t�@�C���\��

			//�t�@�C���I�[�v��
			sFile.m_bDropFileAndClose = false;		// �t�@�C�����h���b�v�����Ƃ��͕��ĊJ��
			sFile.m_nDropFileNumMax = 8;			// ��x�Ƀh���b�v�\�ȃt�@�C����
			sFile.m_bRestoreCurPosition = true;	// �J�[�\���ʒu����	//	Oct. 27, 2000 genta
			sFile.m_bRestoreBookmarks = true;		// �u�b�N�}�[�N����	//2002.01.16 hor
			sFile.m_bAutoMIMEdecode = false;		// �t�@�C���ǂݍ��ݎ���MIME�̃f�R�[�h���s����	//Jul. 13, 2001 JEPRO
			sFile.m_bQueryIfCodeChange = true;		// �O��ƈقȂ镶���R�[�h�̎��ɖ₢���킹���s����	Oct. 03, 2004 genta
			sFile.m_bAlertIfFileNotExist = false;	// �J�����Ƃ����t�@�C�������݂��Ȃ��Ƃ��x������	Oct. 09, 2004 genta
			sFile.m_bAlertIfLargeFile = false;		// �J�����Ƃ����t�@�C�����傫���ꍇ�Ɍx������
			sFile.m_nAlertFileSize = 10;			// �x�����n�߂�t�@�C���T�C�Y�iMB�P�ʁj
		}

		// [�o�b�N�A�b�v]�^�u
		{
			CommonSetting_Backup& sBackup = m_pShareData->m_Common.m_sBackup;

			sBackup.m_bBackUp = false;										/* �o�b�N�A�b�v�̍쐬 */
			sBackup.m_bBackUpDialog = true;									/* �o�b�N�A�b�v�̍쐬�O�Ɋm�F */
			sBackup.m_bBackUpFolder = false;								/* �w��t�H���_�Ƀo�b�N�A�b�v���쐬���� */
			sBackup.m_szBackUpFolder[0] = L'\0';							/* �o�b�N�A�b�v���쐬����t�H���_ */
			sBackup.m_nBackUpType = 2;										/* �o�b�N�A�b�v�t�@�C�����̃^�C�v 1=(.bak) 2=*_���t.* */
			sBackup.m_nBackUpType_Opt1 = BKUP_YEAR | BKUP_MONTH | BKUP_DAY;	/* �o�b�N�A�b�v�t�@�C�����F���t */
			sBackup.m_nBackUpType_Opt2 = ('b' << 16 ) + 10;					/* �o�b�N�A�b�v�t�@�C�����F�A�Ԃ̐��Ɛ擪���� */
			sBackup.m_nBackUpType_Opt3 = 5;									/* �o�b�N�A�b�v�t�@�C�����FOption3 */
			sBackup.m_nBackUpType_Opt4 = 0;									/* �o�b�N�A�b�v�t�@�C�����FOption4 */
			sBackup.m_nBackUpType_Opt5 = 0;									/* �o�b�N�A�b�v�t�@�C�����FOption5 */
			sBackup.m_nBackUpType_Opt6 = 0;									/* �o�b�N�A�b�v�t�@�C�����FOption6 */
			sBackup.m_bBackUpDustBox = false;								/* �o�b�N�A�b�v�t�@�C�������ݔ��ɕ��荞�� */	//@@@ 2001.12.11 add MIK
			sBackup.m_bBackUpPathAdvanced = false;							/* 20051107 aroka �o�b�N�A�b�v��t�H���_���ڍאݒ肷�� */
			sBackup.m_szBackUpPathAdvanced[0] = _T('\0');					/* 20051107 aroka �o�b�N�A�b�v���쐬����t�H���_�̏ڍאݒ� */
		}

		// [����]�^�u
		{
			CommonSetting_Format& sFormat = m_pShareData->m_Common.m_sFormat;

			/* ���o���L�� */
			wcscpy( sFormat.m_szMidashiKigou, L"�P�Q�R�S�T�U�V�W�X�O�i(�m[�u�w�y�������������������������E��������@�A�B�C�D�E�F�G�H�I�J�K�L�M�N�O�P�Q�R�S�T�U�V�W�X�Y�Z�[�\�]���O�l�ܘZ������\���Q��" );
			/* ���p�� */
			wcscpy( sFormat.m_szInyouKigou, L"> " );		/* ���p�� */

			/*
				�����w��q�̈Ӗ���Windows SDK��GetDateFormat(), GetTimeFormat()���Q�Ƃ̂���
			*/

			sFormat.m_nDateFormatType = 0;	//���t�����̃^�C�v
			_tcscpy( sFormat.m_szDateFormat, _T("yyyy\'�N\'M\'��\'d\'��(\'dddd\')\'") );	//���t����
			sFormat.m_nTimeFormatType = 0;	//���������̃^�C�v
			_tcscpy( sFormat.m_szTimeFormat, _T("tthh\'��\'mm\'��\'ss\'�b\'")  );			//��������
		}

		// [����]�^�u
		{
			CommonSetting_Search& sSearch = m_pShareData->m_Common.m_sSearch;

			sSearch.m_sSearchOption.Reset();			// �����I�v�V����
			sSearch.m_bConsecutiveAll = 0;			// �u���ׂĒu���v�͒u���̌J�Ԃ�	// 2007.01.16 ryoji
			sSearch.m_bSelectedArea = FALSE;			// �I��͈͓��u��
			sSearch.m_bNOTIFYNOTFOUND = TRUE;		/* �����^�u��  ������Ȃ��Ƃ����b�Z�[�W��\�� */

			sSearch.m_bGrepSubFolder = TRUE;			/* Grep: �T�u�t�H���_������ */
			sSearch.m_nGrepOutputLineType = 1;			// Grep: �s���o��/�Y������/�ۃ}�b�`�s ���o��
			sSearch.m_nGrepOutputStyle = 1;			/* Grep: �o�͌`�� */
			sSearch.m_bGrepOutputFileOnly = false;
			sSearch.m_bGrepOutputBaseFolder = false;
			sSearch.m_bGrepSeparateFolder = false;
			sSearch.m_bGrepBackup = true;

			sSearch.m_bGrepDefaultFolder=FALSE;		/* Grep: �t�H���_�̏����l���J�����g�t�H���_�ɂ��� */
			sSearch.m_nGrepCharSet = CODE_AUTODETECT;	/* Grep: �����R�[�h�Z�b�g */
			sSearch.m_bGrepRealTimeView = FALSE;		/* 2003.06.28 Moca Grep���ʂ̃��A���^�C���\�� */
			sSearch.m_bCaretTextForSearch = TRUE;		/* 2006.08.23 ryoji �J�[�\���ʒu�̕�������f�t�H���g�̌���������ɂ��� */
			sSearch.m_bInheritKeyOtherView = true;
			sSearch.m_szRegexpLib[0] = _T('\0');		/* 2007.08.12 genta ���K�\��DLL */
			sSearch.m_bGTJW_RETURN = TRUE;				/* �G���^�[�L�[�Ń^�O�W�����v */
			sSearch.m_bGTJW_LDBLCLK = TRUE;			/* �_�u���N���b�N�Ń^�O�W�����v */

			sSearch.m_bGrepExitConfirm = FALSE;			/* Grep���[�h�ŕۑ��m�F���邩 */

			sSearch.m_bAutoCloseDlgFind = TRUE;			/* �����_�C�A���O�������I�ɕ��� */
			sSearch.m_bSearchAll		 = FALSE;			/* �����^�u���^�u�b�N�}�[�N  �擪�i�����j����Č��� 2002.01.26 hor */
			sSearch.m_bAutoCloseDlgReplace = TRUE;		/* �u�� �_�C�A���O�������I�ɕ��� */

			sSearch.m_nTagJumpMode = 1;				//�^�O�W�����v���[�h
			sSearch.m_nTagJumpModeKeyword = 3;			//�^�O�W�����v���[�h
		}

		// [�L�[���蓖��]�^�u
		{
			//	Jan. 30, 2005 genta �֐��Ƃ��ēƗ�
			//	2007.11.04 genta �߂�l�`�F�b�N�Dfalse�Ȃ�N�����f�D
			if( ! InitKeyAssign( m_pShareData )){
				return false;
			}
		}

		// [�J�X�^�����j���[]�^�u
		{
			CommonSetting_CustomMenu& sCustomMenu = m_pShareData->m_Common.m_sCustomMenu;

			for( int i = 0; i < MAX_CUSTOM_MENU; ++i ){
				sCustomMenu.m_szCustMenuNameArr[i][0] = '\0';
				sCustomMenu.m_nCustMenuItemNumArr[i] = 0;
				for( int j = 0; j < MAX_CUSTOM_MENU_ITEMS; ++j ){
					sCustomMenu.m_nCustMenuItemFuncArr[i][j] = F_0;
					sCustomMenu.m_nCustMenuItemKeyArr [i][j] = '\0';
				}
				sCustomMenu.m_bCustMenuPopupArr[i] = true;
			}
			sCustomMenu.m_szCustMenuNameArr[CUSTMENU_INDEX_FOR_TABWND][0] = '\0';	//@@@ 2003.06.13 MIK

			InitPopupMenu( m_pShareData );
		}

		// [�c�[���o�[]�^�u
		{
			//	Jan. 30, 2005 genta �֐��Ƃ��ēƗ�
			InitToolButtons( m_pShareData );
		}

		// [�����L�[���[�h]�^�u
		{
			InitKeyword( m_pShareData, false );
		}

		// [�x��]�^�u
		{
			CommonSetting_Helper& sHelper = m_pShareData->m_Common.m_sHelper;

			sHelper.m_lf = lfIconTitle;
			sHelper.m_nPointSize = nIconPointSize;	// �t�H���g�T�C�Y�i1/10�|�C���g�P�ʁj ���Â��o�[�W��������̈ڍs���l�����Ė����l�ŏ�����	// 2009.10.01 ryoji

			sHelper.m_szExtHelp[0] = L'\0';			// �O���w���v�P
			sHelper.m_szExtHtmlHelp[0] = L'\0';		// �O��HTML�w���v
		
			sHelper.m_szMigemoDll[0] = L'\0';			/* migemo dll */
			sHelper.m_szMigemoDict[0] = L'\0';		/* migemo dict */

			sHelper.m_bHtmlHelpIsSingle = true;		/* HtmlHelp�r���[�A�͂ЂƂ� */

			sHelper.m_bHokanKey_RETURN	= TRUE;			/* VK_RETURN �⊮����L�[���L��/���� */
			sHelper.m_bHokanKey_TAB		= FALSE;		/* VK_TAB   �⊮����L�[���L��/���� */
			sHelper.m_bHokanKey_RIGHT	= TRUE;			/* VK_RIGHT �⊮����L�[���L��/���� */
			sHelper.m_bHokanKey_SPACE	= FALSE;		/* VK_SPACE �⊮����L�[���L��/���� */
		}

		// [�A�E�g���C��]�^�u
		{
			CommonSetting_OutLine& sOutline = m_pShareData->m_Common.m_sOutline;

			sOutline.m_nOutlineDockSet = 0;					/* �A�E�g���C����͂̃h�b�L���O�ʒu�p�����@ */
			sOutline.m_bOutlineDockSync = TRUE;				/* �A�E�g���C����͂̃h�b�L���O�ʒu�𓯊����� */
			sOutline.m_bOutlineDockDisp = FALSE;				/* �A�E�g���C����͕\���̗L�� */
			sOutline.m_eOutlineDockSide = DOCKSIDE_FLOAT;		/* �A�E�g���C����̓h�b�L���O�z�u */
			sOutline.m_cxOutlineDockLeft		=	0;	// �A�E�g���C���̍��h�b�L���O��
			sOutline.m_cyOutlineDockTop		=	0;	// �A�E�g���C���̏�h�b�L���O��
			sOutline.m_cxOutlineDockRight		=	0;	// �A�E�g���C���̉E�h�b�L���O��
			sOutline.m_cyOutlineDockBottom		=	0;	// �A�E�g���C���̉��h�b�L���O��
			sOutline.m_nDockOutline = OUTLINE_TEXT;
			sOutline.m_bAutoCloseDlgFuncList = FALSE;		/* �A�E�g���C�� �_�C�A���O�������I�ɕ��� */	//Nov. 18, 2000 JEPRO TRUE��FALSE �ɕύX
			sOutline.m_bMarkUpBlankLineEnable	=	FALSE;	//�A�E�g���C���_�C�A���O�Ńu�b�N�}�[�N�̋�s�𖳎�			2002.02.08 aroka,hor
			sOutline.m_bFunclistSetFocusOnJump	=	FALSE;	//�A�E�g���C���_�C�A���O�ŃW�����v������t�H�[�J�X���ڂ�	2002.02.08 hor

			InitFileTree( &sOutline.m_sFileTree );
			sOutline.m_sFileTreeDefIniName = _T("_sakurafiletree.ini");
		}

		// [�t�@�C�����e��r]�^�u
		{
			CommonSetting_Compare& sCompare = m_pShareData->m_Common.m_sCompare;

			sCompare.m_bCompareAndTileHorz = TRUE;		/* ������r��A���E�ɕ��ׂĕ\�� */
		}

		// [�r���[]�^�u
		{
			CommonSetting_View& sView = m_pShareData->m_Common.m_sView;

			sView.m_lf = lf;
			sView.m_nPointSize = 0;	// �t�H���g�T�C�Y�i1/10�|�C���g�P�ʁj ���Â��o�[�W��������̈ڍs���l�����Ė����l�ŏ�����	// 2009.10.01 ryoji

			sView.m_bFontIs_FIXED_PITCH = TRUE;				/* ���݂̃t�H���g�͌Œ蕝�t�H���g�ł��� */
		}

		// [�}�N��]�^�u
		{
			CommonSetting_Macro& sMacro = m_pShareData->m_Common.m_sMacro;

			sMacro.m_szKeyMacroFileName[0] = _T('\0');	/* �L�[���[�h�}�N���̃t�@�C���� */ //@@@ 2002.1.24 YAZAKI

			//	From Here Sep. 14, 2001 genta
			//	Macro�o�^�̏�����
			MacroRec *mptr = sMacro.m_MacroTable;
			for( int i = 0; i < MAX_CUSTMACRO; ++i, ++mptr ){
				mptr->m_szName[0] = L'\0';
				mptr->m_szFile[0] = L'\0';
				mptr->m_bReloadWhenExecute = false;
			}
			//	To Here Sep. 14, 2001 genta

			_tcscpy( sMacro.m_szMACROFOLDER, szIniFolder );	/* �}�N���p�t�H���_ */

			sMacro.m_nMacroOnOpened = -1;	/* �I�[�v���㎩�����s�}�N���ԍ� */	//@@@ 2006.09.01 ryoji
			sMacro.m_nMacroOnTypeChanged = -1;	/* �^�C�v�ύX�㎩�����s�}�N���ԍ� */	//@@@ 2006.09.01 ryoji
			sMacro.m_nMacroOnSave = -1;	/* �ۑ��O�������s�}�N���ԍ� */	//@@@ 2006.09.01 ryoji
			sMacro.m_nMacroCancelTimer = 10;	// �}�N����~�_�C�A���O�\���҂�����(�b)	// 2011.08.04 syat
		}

		// [�t�@�C�����\��]�^�u
		{
			CommonSetting_FileName& sFileName = m_pShareData->m_Common.m_sFileName;

			sFileName.m_bTransformShortPath = true;
			sFileName.m_nTransformShortMaxWidth = 100; // 100'x'��

			for( int i = 0; i < MAX_TRANSFORM_FILENAME; ++i ){
				sFileName.m_szTransformFileNameFrom[i][0] = _T('\0');
				sFileName.m_szTransformFileNameTo[i][0] = _T('\0');
			}
			_tcscpy( sFileName.m_szTransformFileNameFrom[0], _T("%DeskTop%\\") );
			_tcscpy( sFileName.m_szTransformFileNameTo[0],   _T("�f�X�N�g�b�v\\") );
			_tcscpy( sFileName.m_szTransformFileNameFrom[1], _T("%Personal%\\") );
			_tcscpy( sFileName.m_szTransformFileNameTo[1],   _T("�}�C�h�L�������g\\") );
			_tcscpy( sFileName.m_szTransformFileNameFrom[2], _T("%Cache%\\Content.IE5\\") );
			_tcscpy( sFileName.m_szTransformFileNameTo[2],   _T("IE�L���b�V��\\") );
			_tcscpy( sFileName.m_szTransformFileNameFrom[3], _T("%TEMP%\\") );
			_tcscpy( sFileName.m_szTransformFileNameTo[3],   _T("TEMP\\") );
			_tcscpy( sFileName.m_szTransformFileNameFrom[4], _T("%Common DeskTop%\\") );
			_tcscpy( sFileName.m_szTransformFileNameTo[4],   _T("���L�f�X�N�g�b�v\\") );
			_tcscpy( sFileName.m_szTransformFileNameFrom[5], _T("%Common Documents%\\") );
			_tcscpy( sFileName.m_szTransformFileNameTo[5],   _T("���L�h�L�������g\\") );
			_tcscpy( sFileName.m_szTransformFileNameFrom[6], _T("%AppData%\\") );	// 2007.05.19 ryoji �ǉ�
			_tcscpy( sFileName.m_szTransformFileNameTo[6],   _T("�A�v���f�[�^\\") );	// 2007.05.19 ryoji �ǉ�
			sFileName.m_nTransformFileNameArrNum = 7;
		}

		// [���̑�]�^�u
		{
			CommonSetting_Others& sOthers = m_pShareData->m_Common.m_sOthers;

			::SetRect( &sOthers.m_rcOpenDialog, 0, 0, 0, 0 );	/* �u�J���v�_�C�A���O�̃T�C�Y�ƈʒu */
			::SetRect( &sOthers.m_rcCompareDialog, 0, 0, 0, 0 );
			::SetRect( &sOthers.m_rcDiffDialog, 0, 0, 0, 0 );
			::SetRect( &sOthers.m_rcFavoriteDialog, 0, 0, 0, 0 );
			::SetRect( &sOthers.m_rcTagJumpDialog, 0, 0, 0, 0 );
		}

		// [�X�e�[�^�X�o�[]�^�u
		{
			CommonSetting_Statusbar& sStatusbar = m_pShareData->m_Common.m_sStatusbar;

			// �\�������R�[�h�̎w��		2008/6/21	Uchi
			sStatusbar.m_bDispUniInSjis		= FALSE;	// SJIS�ŕ����R�[�h�l��Unicode�ŕ\������
			sStatusbar.m_bDispUniInJis			= FALSE;	// JIS�ŕ����R�[�h�l��Unicode�ŕ\������
			sStatusbar.m_bDispUniInEuc			= FALSE;	// EUC�ŕ����R�[�h�l��Unicode�ŕ\������
			sStatusbar.m_bDispUtf8Codepoint	= TRUE;		// UTF-8���R�[�h�|�C���g�ŕ\������
			sStatusbar.m_bDispSPCodepoint		= TRUE;		// �T���Q�[�g�y�A���R�[�h�|�C���g�ŕ\������
			sStatusbar.m_bDispSelCountByByte	= FALSE;	// �I�𕶎����𕶎��P�ʂł͂Ȃ��o�C�g�P�ʂŕ\������
		}

		// [�v���O�C��]�^�u
		{
			CommonSetting_Plugin& sPlugin = m_pShareData->m_Common.m_sPlugin;

			sPlugin.m_bEnablePlugin			= FALSE;	// �v���O�C�����g�p����
			for( int nPlugin=0; nPlugin < MAX_PLUGIN; nPlugin++ ){
				sPlugin.m_PluginTable[nPlugin].m_szName[0]	= L'\0';	// �v���O�C����
				sPlugin.m_PluginTable[nPlugin].m_szId[0]	= L'\0';	// �v���O�C��ID
				sPlugin.m_PluginTable[nPlugin].m_state = PLS_NONE;		// �v���O�C�����
			}
		}

		// [���C�����j���[]�^�u
		{
			CDataProfile	cProfile;
			std::vector<std::wstring> data;
			cProfile.SetReadingMode();
			cProfile.ReadProfileRes( MAKEINTRESOURCE(IDR_MENU1), MAKEINTRESOURCE(ID_RC_TYPE_INI), &data );

			CShareData_IO::IO_MainMenu( cProfile, &data, m_pShareData->m_Common.m_sMainMenu, false );
		}

		{
			InitTypeConfigs( m_pShareData, *m_pvTypeSettings );
		}

		{
			/* m_PrintSettingArr[0]��ݒ肵�āA�c���1�`7�ɃR�s�[����B
				�K�v�ɂȂ�܂Œx�点�邽�߂ɁACPrint�ɁACShareData�𑀍삷�錠����^����B
				YAZAKI.
			*/
			{
				/*
					2006.08.16 Moca �������P�ʂ� PRINTSETTING�ɕύX�BCShareData�ɂ͈ˑ����Ȃ��B
				*/
				TCHAR szSettingName[64];
				int i = 0;
				auto_sprintf( szSettingName, _T("����ݒ� %d"), i + 1 );
				CPrint::SettingInitialize( m_pShareData->m_PrintSettingArr[0], szSettingName );	//	���������߁B
			}
			for( int i = 1; i < MAX_PRINTSETTINGARR; ++i ){
				m_pShareData->m_PrintSettingArr[i] = m_pShareData->m_PrintSettingArr[0];
				auto_sprintf( m_pShareData->m_PrintSettingArr[i].m_szPrintSettingName, _T("����ݒ� %d"), i + 1 );	/* ����ݒ�̖��O */
			}
		}

		{
			m_pShareData->m_sSearchKeywords.m_aSearchKeys.clear();
			m_pShareData->m_sSearchKeywords.m_aReplaceKeys.clear();
			m_pShareData->m_sSearchKeywords.m_aGrepFiles.clear();
			m_pShareData->m_sSearchKeywords.m_aGrepFiles.push_back(_T("*.*"));
			m_pShareData->m_sSearchKeywords.m_aGrepFolders.clear();
#if REI_MOD_GREP
			m_pShareData->m_sSearchKeywords.m_bGrepFolders99 = true;
			m_pShareData->m_sSearchKeywords.m_bGrepFolders2 = false;
			m_pShareData->m_sSearchKeywords.m_bGrepFolders3 = false;
			m_pShareData->m_sSearchKeywords.m_bGrepFolders4 = false;
			m_pShareData->m_sSearchKeywords.m_szGrepFolders2 = _T("");
			m_pShareData->m_sSearchKeywords.m_szGrepFolders3 = _T("");
			m_pShareData->m_sSearchKeywords.m_szGrepFolders4 = _T("");
#endif // rei_

			// 2004/06/21 novice �^�O�W�����v�@�\�ǉ�
			m_pShareData->m_sTagJump.m_TagJumpNum = 0;
			// 2004.06.22 Moca �^�O�W�����v�̐擪
			m_pShareData->m_sTagJump.m_TagJumpTop = 0;
			//From Here 2005.04.03 MIK �L�[���[�h�w��^�O�W�����v��History�ۊ�
			m_pShareData->m_sTagJump.m_aTagJumpKeywords.clear();
			m_pShareData->m_sTagJump.m_bTagJumpICase = FALSE;
			m_pShareData->m_sTagJump.m_bTagJumpAnyWhere = FALSE;
			//To Here 2005.04.03 MIK 

			m_pShareData->m_sHistory.m_aExceptMRU.clear();

			_tcscpy( m_pShareData->m_sHistory.m_szIMPORTFOLDER, szIniFolder );	/* �ݒ�C���|�[�g�p�t�H���_ */

			m_pShareData->m_sHistory.m_aCommands.clear();
			m_pShareData->m_sHistory.m_aCurDirs.clear();

			m_pShareData->m_nExecFlgOpt = 1;	/* �O���R�}���h���s�́u�W���o�͂𓾂�v */	// 2006.12.03 maru �I�v�V�����̊g���̂���

			m_pShareData->m_nDiffFlgOpt = 0;	/* DIFF�����\�� */	//@@@ 2002.05.27 MIK

			m_pShareData->m_szTagsCmdLine[0] = _T('\0');	/* CTAGS */	//@@@ 2003.05.12 MIK
			m_pShareData->m_nTagsOpt = 0;	/* CTAGS */	//@@@ 2003.05.12 MIK

			m_pShareData->m_bLineNumIsCRLF_ForJump = true;	/* �w��s�փW�����v�́u���s�P�ʂ̍s�ԍ��v���u�܂�Ԃ��P�ʂ̍s�ԍ��v�� */
		}
	}else{
		/* �I�u�W�F�N�g�����łɑ��݂���ꍇ */
		/* �t�@�C���̃r���[�� �Ăяo�����v���Z�X�̃A�h���X��ԂɃ}�b�v���܂� */
		m_pShareData = (DLLSHAREDATA*)::MapViewOfFile(
			m_hFileMap,
			FILE_MAP_ALL_ACCESS,
			0,
			0,
			0
		);
		SetDllShareData( m_pShareData );

		SelectCharWidthCache( CWM_FONT_EDIT, CWM_CACHE_SHARE );
		InitCharWidthCache(m_pShareData->m_Common.m_sView.m_lf);	// 2008/5/15 Uchi

		//	From Here Oct. 27, 2000 genta
		//	2014.01.08 Moca �T�C�Y�`�F�b�N�ǉ�
		if( m_pShareData->m_vStructureVersion != uShareDataVersion ||
			m_pShareData->m_nSize != sizeof(*m_pShareData) ){
			//	���̋��L�f�[�^�̈�͎g���Ȃ��D
			//	�n���h�����������
			SetDllShareData( NULL );
			::UnmapViewOfFile( m_pShareData );
			m_pShareData = NULL;
			return false;
		}
		//	To Here Oct. 27, 2000 genta

	}
	return true;
}




static void ConvertLangString( wchar_t* pBuf, size_t chBufSize, std::wstring& org, std::wstring& to )
{
	CNativeW mem;
	mem.SetString(pBuf);
	mem.Replace(org.c_str(), to.c_str());
	auto_strncpy(pBuf, mem.GetStringPtr(), chBufSize);
	pBuf[chBufSize - 1] = L'\0';
}

static void ConvertLangString( char* pBuf, size_t chBufSize, std::wstring& org, std::wstring& to )
{
	CNativeA mem;
	mem.SetString(pBuf);
	mem.Replace_j(to_achar(org.c_str()), to_achar(to.c_str()));
	auto_strncpy(pBuf, mem.GetStringPtr(), chBufSize);
	pBuf[chBufSize - 1] = '\0';
}

static void ConvertLangValueImpl( wchar_t* pBuf, size_t chBufSize, int nStrId, std::vector<std::wstring>& values, int& index, bool setValues, bool bUpdate )
{
	if( setValues ){
		if( bUpdate ){
			values.push_back( std::wstring(LSW(nStrId)) );
		}
		return;
	}
	std::wstring to = LSW(nStrId);
	ConvertLangString( pBuf, chBufSize, values[index], to );
	index++;
}

static void ConvertLangValueImpl( char* pBuf, size_t chBufSize, int nStrId, std::vector<std::wstring>& values, int& index, bool setValues, bool bUpdate )
{
	if( setValues ){
		if( bUpdate ){
			values.push_back( std::wstring(LSW(nStrId)) );
		}
		return;
	}
	std::wstring to = LSW(nStrId);
	ConvertLangString( pBuf, chBufSize, values[index], to );
	index++;
}


#define ConvertLangValue(buf, id)  ConvertLangValueImpl(buf, _countof(buf), id, values, index, bSetValues, true);
#define ConvertLangValue2(buf, id) ConvertLangValueImpl(buf, _countof(buf), id, values, index, bSetValues, false);



/*!
	���ۉ��Ή��̂��߂̕������ύX����

	1. 1��ڌĂяo���AsetValues��true�ɂ��āAvalues�ɋ��ݒ�̌��ꕶ�����ǂݍ���
	2. SelectLang�Ăяo��
	3. 2��ڌĂяo���Avalues���g���ĐV�ݒ�̌���ɏ�������
*/
void CShareData::ConvertLangValues(std::vector<std::wstring>& values, bool bSetValues)
{
	DLLSHAREDATA&	shareData = *m_pShareData;
	int i;
	int index = 0;
	int indexBackup;
	CommonSetting& common = shareData.m_Common;
	ConvertLangValue( common.m_sTabBar.m_szTabWndCaption, STR_TAB_CAPTION_OUTPUT );
	ConvertLangValue( common.m_sTabBar.m_szTabWndCaption, STR_TAB_CAPTION_GREP );
	indexBackup = index;
	ConvertLangValue( common.m_sTabBar.m_szTabWndCaption, STR_CAPTION_ACTIVE_OUTPUT );
	ConvertLangValue( common.m_sTabBar.m_szTabWndCaption, STR_CAPTION_ACTIVE_UPDATE );
	ConvertLangValue( common.m_sTabBar.m_szTabWndCaption, STR_CAPTION_ACTIVE_VIEW );
	ConvertLangValue( common.m_sTabBar.m_szTabWndCaption, STR_CAPTION_ACTIVE_OVERWRITE );
	ConvertLangValue( common.m_sTabBar.m_szTabWndCaption, STR_CAPTION_ACTIVE_KEYMACRO );
	index = indexBackup;
	ConvertLangValue2( common.m_sWindow.m_szWindowCaptionActive, STR_CAPTION_ACTIVE_OUTPUT );
	ConvertLangValue2( common.m_sWindow.m_szWindowCaptionActive, STR_CAPTION_ACTIVE_UPDATE );
	ConvertLangValue2( common.m_sWindow.m_szWindowCaptionActive, STR_CAPTION_ACTIVE_VIEW );
	ConvertLangValue2( common.m_sWindow.m_szWindowCaptionActive, STR_CAPTION_ACTIVE_OVERWRITE );
	ConvertLangValue2( common.m_sWindow.m_szWindowCaptionActive, STR_CAPTION_ACTIVE_KEYMACRO );
	index = indexBackup;
	ConvertLangValue2( common.m_sWindow.m_szWindowCaptionInactive, STR_CAPTION_ACTIVE_OUTPUT );
	ConvertLangValue2( common.m_sWindow.m_szWindowCaptionInactive, STR_CAPTION_ACTIVE_UPDATE );
	ConvertLangValue2( common.m_sWindow.m_szWindowCaptionInactive, STR_CAPTION_ACTIVE_VIEW );
	ConvertLangValue2( common.m_sWindow.m_szWindowCaptionInactive, STR_CAPTION_ACTIVE_OVERWRITE );
	ConvertLangValue2( common.m_sWindow.m_szWindowCaptionInactive, STR_CAPTION_ACTIVE_KEYMACRO );
	ConvertLangValue( common.m_sFormat.m_szDateFormat, STR_DATA_FORMAT );
	ConvertLangValue( common.m_sFormat.m_szTimeFormat, STR_TIME_FORMAT );
	indexBackup = index;
	for( i = 0; i < common.m_sFileName.m_nTransformFileNameArrNum; i++ ){
		index = indexBackup;
		ConvertLangValue( common.m_sFileName.m_szTransformFileNameTo[i], STR_TRANSNAME_COMDESKTOP );
		ConvertLangValue( common.m_sFileName.m_szTransformFileNameTo[i], STR_TRANSNAME_COMDOC );
		ConvertLangValue( common.m_sFileName.m_szTransformFileNameTo[i], STR_TRANSNAME_DESKTOP );
		ConvertLangValue( common.m_sFileName.m_szTransformFileNameTo[i], STR_TRANSNAME_MYDOC );
		ConvertLangValue( common.m_sFileName.m_szTransformFileNameTo[i], STR_TRANSNAME_IE );
		ConvertLangValue( common.m_sFileName.m_szTransformFileNameTo[i], STR_TRANSNAME_TEMP );
		ConvertLangValue( common.m_sFileName.m_szTransformFileNameTo[i], STR_TRANSNAME_APPDATA );
		if( bSetValues ){
			break;
		}
	}
	indexBackup = index;
	for( i = 0; i < MAX_PRINTSETTINGARR; i++ ){
		index = indexBackup;
		ConvertLangValue( shareData.m_PrintSettingArr[i].m_szPrintSettingName, STR_PRINT_SET_NAME );
		if( bSetValues ){
			break;
		}
	}
	assert( m_pvTypeSettings != NULL );
	indexBackup = index;
	ConvertLangValue( shareData.m_TypeBasis.m_szTypeName, STR_TYPE_NAME_BASIS );
	for( i = 0; i < (int)GetTypeSettings().size(); i++ ){
		index = indexBackup;
		STypeConfig& type = *(GetTypeSettings()[i]);
		ConvertLangValue2( type.m_szTypeName, STR_TYPE_NAME_BASIS );
		ConvertLangValue( type.m_szTypeName, STR_TYPE_NAME_RICHTEXT );
		ConvertLangValue( type.m_szTypeName, STR_TYPE_NAME_TEXT );
		ConvertLangValue( type.m_szTypeName, STR_TYPE_NAME_DOS );
		ConvertLangValue( type.m_szTypeName, STR_TYPE_NAME_ASM );
		ConvertLangValue( type.m_szTypeName, STR_TYPE_NAME_INI );
		index = indexBackup;
		ConvertLangValue2( shareData.m_TypeMini[i].m_szTypeName, STR_TYPE_NAME_BASIS );
		ConvertLangValue2( shareData.m_TypeMini[i].m_szTypeName, STR_TYPE_NAME_RICHTEXT );
		ConvertLangValue2( shareData.m_TypeMini[i].m_szTypeName, STR_TYPE_NAME_TEXT );
		ConvertLangValue2( shareData.m_TypeMini[i].m_szTypeName, STR_TYPE_NAME_DOS );
		ConvertLangValue2( shareData.m_TypeMini[i].m_szTypeName, STR_TYPE_NAME_ASM );
		ConvertLangValue2( shareData.m_TypeMini[i].m_szTypeName, STR_TYPE_NAME_INI );
		if( bSetValues ){
			break;
		}
	}
}

/*!
	@brief	�w��t�@�C�����J����Ă��邩���ׂ�
	
	�w��̃t�@�C�����J����Ă���ꍇ�͊J���Ă���E�B���h�E�̃n���h����Ԃ�

	@retval	TRUE ���łɊJ���Ă���
	@retval	FALSE �J���Ă��Ȃ�����
*/
BOOL CShareData::IsPathOpened( const TCHAR* pszPath, HWND* phwndOwner )
{
	EditInfo*	pfi;
	*phwndOwner = NULL;

	//	2007.10.01 genta ���΃p�X���΃p�X�ɕϊ�
	//	�ϊ����Ȃ���IsPathOpened�Ő��������ʂ�����ꂸ�C
	//	����t�@�C���𕡐��J�����Ƃ�����D
	TCHAR	szBuf[_MAX_PATH];
	if( GetLongFileName( pszPath, szBuf )){
		pszPath = szBuf;
	}

	// ���݂̕ҏW�E�B���h�E�̐��𒲂ׂ�
	if( 0 == CAppNodeGroupHandle(0).GetEditorWindowsNum() ){
		return FALSE;
	}
	
	for( int i = 0; i < m_pShareData->m_sNodes.m_nEditArrNum; ++i ){
		if( IsSakuraMainWindow( m_pShareData->m_sNodes.m_pEditArr[i].m_hWnd ) ){
			// �g���C����G�f�B�^�ւ̕ҏW�t�@�C�����v���ʒm
			::SendMessageAny( m_pShareData->m_sNodes.m_pEditArr[i].m_hWnd, MYWM_GETFILEINFO, 1, 0 );
			pfi = (EditInfo*)&m_pShareData->m_sWorkBuffer.m_EditInfo_MYWM_GETFILEINFO;

			// ����p�X�̃t�@�C�������ɊJ����Ă��邩
			if( 0 == _tcsicmp( pfi->m_szPath, pszPath ) ){
#if REI_MULTIPLE_OPEN_FILES
				if (::GetAsyncKeyState(VK_SHIFT)) {
					return FALSE;
				}
#endif // rei_
				*phwndOwner = m_pShareData->m_sNodes.m_pEditArr[i].m_hWnd;
				return TRUE;
			}
		}
	}
	return FALSE;
}

/*!
	@brief	�w��t�@�C�����J����Ă��邩���ׂA���d�I�[�v�����̕����R�[�h�Փ˂��m�F

	�������łɊJ���Ă���΃A�N�e�B�u�ɂ��āA�E�B���h�E�̃n���h����Ԃ��B
	����ɁA�����R�[�h���قȂ�Ƃ��̃��[�j���O����������B
	���������ɎU��΂������d�I�[�v���������W��������̂��ړI�B

	@retval	�J����Ă���ꍇ�͊J���Ă���E�B���h�E�̃n���h��

	@note	CEditDoc::FileLoad�ɐ旧���Ď��s����邱�Ƃ����邪�A
			CEditDoc::FileLoad��������s�����K�v�����邱�Ƃɒ��ӁB
			(�t�H���_�w��̏ꍇ��CEditDoc::FileLoad�����ڎ��s�����ꍇ�����邽��)

	@retval	TRUE ���łɊJ���Ă���
	@retval	FALSE �J���Ă��Ȃ�����

	@date 2007.03.12 maru �V�K�쐬
*/
BOOL CShareData::ActiveAlreadyOpenedWindow( const TCHAR* pszPath, HWND* phwndOwner, ECodeType nCharCode )
{
	if( IsPathOpened( pszPath, phwndOwner ) ){
		
		//�����R�[�h�̈�v�m�F
		EditInfo*		pfi;
		::SendMessageAny( *phwndOwner, MYWM_GETFILEINFO, 0, 0 );
		pfi = (EditInfo*)&m_pShareData->m_sWorkBuffer.m_EditInfo_MYWM_GETFILEINFO;
		if(nCharCode != CODE_AUTODETECT){
			TCHAR szCpNameCur[100];
			CCodePage::GetNameLong(szCpNameCur, pfi->m_nCharCode);
			TCHAR szCpNameNew[100];
			CCodePage::GetNameLong(szCpNameNew, pfi->m_nCharCode);
			if(szCpNameCur[0] && szCpNameNew[0]){
				if(nCharCode != pfi->m_nCharCode){
					TopWarningMessage( *phwndOwner,
						LS(STR_ERR_CSHAREDATA20),
						pszPath,
						szCpNameCur,
						szCpNameNew
					);
				}
			}
			else{
				TopWarningMessage( *phwndOwner,
					LS(STR_ERR_CSHAREDATA21),
					pszPath,
					pfi->m_nCharCode,
					0==szCpNameCur[0]?LS(STR_ERR_CSHAREDATA22):szCpNameCur,
					nCharCode,
					0==szCpNameNew[0]?LS(STR_ERR_CSHAREDATA22):szCpNameNew
				);
			}
		}

		// �J���Ă���E�B���h�E���A�N�e�B�u�ɂ���
		ActivateFrameWindow( *phwndOwner );

		// MRU���X�g�ւ̓o�^
		CMRUFile().Add( pfi );
		return TRUE;
	}
	else {
		return FALSE;
	}

}













/*!
	�A�E�g�v�b�g�E�C���h�E�ɏo��(�����t)

	�A�E�g�v�b�g�E�C���h�E��������΃I�[�v������
	@param lpFmt [in] �����w�蕶����(wchar_t��)
	@date 2010.02.22 Moca auto_vsprintf���� tchar_vsnprintf_s �ɕύX.��������Ƃ��͐؂�l�߂���
*/
void CShareData::TraceOut( LPCTSTR lpFmt, ... )
{
	if( false == OpenDebugWindow( m_hwndTraceOutSource, false ) ){
		return;
	}
	
	LockGuard<CMutex> guard( CShareData::GetMutexShareWork() );
	va_list argList;
	va_start( argList, lpFmt );
	int ret = tchar_vsnprintf_s( m_pShareData->m_sWorkBuffer.GetWorkBuffer<WCHAR>(), 
		m_pShareData->m_sWorkBuffer.GetWorkBufferCount<WCHAR>(),
		to_wchar(lpFmt), argList );
	va_end( argList );
	if( -1 == ret ){
		// �؂�l�߂�ꂽ
		ret = auto_strlen( m_pShareData->m_sWorkBuffer.GetWorkBuffer<WCHAR>() );
	}else if( ret < 0 ){
		// �ی�R�[�h:�󂯑���wParam��size_t�ŕ����Ȃ��̂���
		ret = 0;
	}
	DWORD_PTR dwMsgResult;
	::SendMessageTimeout( m_pShareData->m_sHandles.m_hwndDebug, MYWM_ADDSTRINGLEN_W, ret, 0,
		SMTO_NORMAL, 10000, &dwMsgResult );
}

/*!
	�A�E�g�v�b�g�E�C���h�E�ɏo��(������w��)

	�����ꍇ�͕������đ���
	�A�E�g�v�b�g�E�C���h�E��������΃I�[�v������
	@param  pStr  �o�͂��镶����
	@param  len   pStr�̕�����(�I�[NUL���܂܂Ȃ�) -1�Ŏ����v�Z
	@date 2010.05.11 Moca �V��
*/
void CShareData::TraceOutString( const wchar_t* pStr, int len )
{
	if( false == OpenDebugWindow( m_hwndTraceOutSource, false ) ){
		return;
	}
	if( -1 == len ){
		len = wcslen(pStr);
	}
	// m_sWorkBuffer���肬��ł����Ȃ�����ǁA�O�̂���\0�I�[�ɂ��邽�߂ɗ]�T���Ƃ�
	// -1 ��� 8,4�o�C�g���E�̂ق����R�s�[�������͂��Ȃ̂ŁA-4�ɂ���
	const int buffLen = (int)m_pShareData->m_sWorkBuffer.GetWorkBufferCount<WCHAR>() - 4;
	wchar_t*  pOutBuffer = m_pShareData->m_sWorkBuffer.GetWorkBuffer<WCHAR>();
	int outPos = 0;
	if(0 == len){
		// 0�̂Ƃ��͉����ǉ����Ȃ����A�J�[�\���ړ�����������
		LockGuard<CMutex> guard( CShareData::GetMutexShareWork() );
		pOutBuffer[0] = L'\0';
		::SendMessage( m_pShareData->m_sHandles.m_hwndDebug, MYWM_ADDSTRINGLEN_W, 0, 0 );
	}else{
		while(outPos < len){
			int outLen = buffLen;
			if(len - outPos < buffLen){
				// �c��S��
				outLen = len - outPos;
			}
			// ���܂肪1�����ȏ゠��
			if( outPos + outLen < len ){
				// CRLF(\r\n)��UTF-16����������Ȃ��悤��
				if( (pStr[outPos + outLen - 1] == WCODE::CR && pStr[outPos + outLen] == WCODE::LF)
					|| (IsUtf16SurrogHi( pStr[outPos + outLen - 1] ) && IsUtf16SurrogLow( pStr[outPos + outLen] )) ){
					--outLen;
				}
			}
			LockGuard<CMutex> guard( CShareData::GetMutexShareWork() );
			wmemcpy( pOutBuffer, pStr + outPos, outLen );
			pOutBuffer[outLen] = L'\0';
			DWORD_PTR	dwMsgResult;
			if( 0 == ::SendMessageTimeout( m_pShareData->m_sHandles.m_hwndDebug, MYWM_ADDSTRINGLEN_W, outLen, 0,
				SMTO_NORMAL, 10000, &dwMsgResult ) ){
				// �G���[���^�C���A�E�g
				break;
			}
			outPos += outLen;
		}
	}
}

/*
	�f�o�b�O�E�B���h�E��\��
	@param hwnd �V�K�E�B���h�E�̂Ƃ��̃f�o�b�O�E�B���h�E�̏����O���[�v
	@param bAllwaysActive �\���ς݃E�B���h�E�ł��A�N�e�B�u�ɂ���
	@return true:�\���ł����B�܂��͂��łɕ\������Ă���B
	@date 2010.05.11 Moca TraceOut���番��
*/
bool CShareData::OpenDebugWindow( HWND hwnd, bool bAllwaysActive )
{
	bool ret = true;
	if( NULL == m_pShareData->m_sHandles.m_hwndDebug
	|| !IsSakuraMainWindow( m_pShareData->m_sHandles.m_hwndDebug )
	){
		// 2007.06.26 ryoji
		// �A�E�g�v�b�g�E�B���h�E���쐬���Ɠ����O���[�v�ɍ쐬���邽�߂� m_hwndTraceOutSource ���g���Ă��܂�
		// �im_hwndTraceOutSource �� CEditWnd::Create() �ŗ\�ߐݒ�j
		// ������ƕs���D�����ǁATraceOut() �̈����ɂ��������N�������w�肷��̂��D�D�D
		// 2010.05.11 Moca m_hwndTraceOutSource�͈ˑR�Ƃ��Ďg���Ă��܂��������ɂ��܂���
		SLoadInfo sLoadInfo;
		sLoadInfo.cFilePath = _T("");
		// CODE_SJIS->CODE_UNICODE	2008/6/8 Uchi
		// CODE_UNICODE->CODE_NONE	2010.05.11 Moca �f�t�H���g�����R�[�h�Őݒ�ł���悤�ɖ��w��ɕύX
		sLoadInfo.eCharCode = CODE_NONE;
		sLoadInfo.bViewMode = false;
		ret = CControlTray::OpenNewEditor( NULL, hwnd, sLoadInfo, _T("-DEBUGMODE"), true );
		//	2001/06/23 N.Nakatani �����o��܂ŃE�G�C�g��������悤�ɏC��
		//�A�E�g�v�b�g�E�C���h�E���o����܂�5�b���炢�҂B
		//	Jun. 25, 2001 genta OpenNewEditor�̓����@�\�𗘗p����悤�ɕύX
		bAllwaysActive = true; // �V����������Ƃ���active
	}
	/* �J���Ă���E�B���h�E���A�N�e�B�u�ɂ��� */
	if(ret && bAllwaysActive){
		ActivateFrameWindow( m_pShareData->m_sHandles.m_hwndDebug );
	}
	return ret;
}

/* ini�t�@�C���̕ۑ��悪���[�U�ʐݒ�t�H���_���ǂ��� */	// 2007.05.25 ryoji
BOOL CShareData::IsPrivateSettings( void ){
	return m_pShareData->m_sFileNameManagement.m_IniFolder.m_bWritePrivate;
}



/*
	CShareData::CheckMRUandOPENFOLDERList
	MRU��OPENFOLDER���X�g�̑��݃`�F�b�N�Ȃ�
	���݂��Ȃ��t�@�C����t�H���_��MRU��OPENFOLDER���X�g����폜����

	@note ���݂͎g���Ă��Ȃ��悤���B
	@par History
	2001.12.26 �폜�����B�iYAZAKI�j
	
*/
/*!	idx�Ŏw�肵���}�N���t�@�C�����i�t���p�X�j���擾����D

	@param pszPath [in]	�p�X���̏o�͐�D�����݂̂�m�肽���Ƃ���NULL������D
	@param idx [in]		�}�N���ԍ�
	@param nBufLen [in]	pszPath�Ŏw�肳�ꂽ�o�b�t�@�̃o�b�t�@�T�C�Y

	@retval >0 : �p�X���̒����D
	@retval  0 : �G���[�C���̃}�N���͎g���Ȃ��C�t�@�C�������w�肳��Ă��Ȃ��D
	@retval <0 : �o�b�t�@�s���D�K�v�ȃo�b�t�@�T�C�Y�� -(�߂�l)+1

	@author YAZAKI
	@date 2003.06.08 Moca ���[�J���ϐ��ւ̃|�C���^��Ԃ��Ȃ��悤�Ɏd�l�ύX
	@date 2003.06.14 genta �����񒷁C�|�C���^�̃`�F�b�N��ǉ�
	@date 2003.06.24 Moca idx��-1�̂Ƃ��A�L�[�}�N���̃t���p�X��Ԃ�.
	
	@note idx�͐��m�Ȃ��̂łȂ���΂Ȃ�Ȃ��B(�����Ő������`�F�b�N���s���Ă��Ȃ�)
*/
int CShareData::GetMacroFilename( int idx, TCHAR *pszPath, int nBufLen )
{
	if( -1 != idx && !m_pShareData->m_Common.m_sMacro.m_MacroTable[idx].IsEnabled() )
		return 0;
	const TCHAR *ptr;
	const TCHAR *pszFile;

	if( -1 == idx ){
		pszFile = _T("RecKey.mac");
	}else{
		pszFile = m_pShareData->m_Common.m_sMacro.m_MacroTable[idx].m_szFile;
	}
	if( pszFile[0] == _T('\0') ){	//	�t�@�C����������
		if( pszPath != NULL ){
			pszPath[0] = _T('\0');
		}
		return 0;
	}
	ptr = pszFile;
	int nLen = _tcslen( ptr ); // Jul. 21, 2003 genta wcslen�Ώۂ�����Ă������߃}�N�����s���ł��Ȃ�

	if( !_IS_REL_PATH( pszFile )	// ��΃p�X
		|| m_pShareData->m_Common.m_sMacro.m_szMACROFOLDER[0] == _T('\0') ){	//	�t�H���_�w��Ȃ�
		if( pszPath == NULL || nBufLen <= nLen ){
			return -nLen;
		}
		_tcscpy( pszPath, pszFile );
		return nLen;
	}
	else {	//	�t�H���_�w�肠��
		//	���΃p�X����΃p�X
		int nFolderSep = AddLastChar( m_pShareData->m_Common.m_sMacro.m_szMACROFOLDER, _countof2(m_pShareData->m_Common.m_sMacro.m_szMACROFOLDER), _T('\\') );
		int nAllLen;
		TCHAR *pszDir;
		TCHAR szDir[_MAX_PATH + _countof2( m_pShareData->m_Common.m_sMacro.m_szMACROFOLDER )];

		 // 2003.06.24 Moca �t�H���_�����΃p�X�Ȃ���s�t�@�C������̃p�X
		// 2007.05.19 ryoji ���΃p�X�͐ݒ�t�@�C������̃p�X��D��
		if( _IS_REL_PATH( m_pShareData->m_Common.m_sMacro.m_szMACROFOLDER ) ){
			GetInidirOrExedir( szDir, m_pShareData->m_Common.m_sMacro.m_szMACROFOLDER );
			pszDir = szDir;
		}else{
			pszDir = m_pShareData->m_Common.m_sMacro.m_szMACROFOLDER;
		}

		int nDirLen = _tcslen( pszDir );
		nAllLen = nDirLen + nLen + ( -1 == nFolderSep ? 1 : 0 );
		if( pszPath == NULL || nBufLen <= nAllLen ){
			return -nAllLen;
		}

		_tcscpy( pszPath, pszDir );
		TCHAR *ptr = pszPath + nDirLen;
		if( -1 == nFolderSep ){
			*ptr++ = _T('\\');
		}
		_tcscpy( ptr, pszFile );
		return nAllLen;
	}

}

/*!	idx�Ŏw�肵���}�N����m_bReloadWhenExecute���擾����B
	idx�͐��m�Ȃ��̂łȂ���΂Ȃ�Ȃ��B
	YAZAKI
*/
bool CShareData::BeReloadWhenExecuteMacro( int idx )
{
	if( !m_pShareData->m_Common.m_sMacro.m_MacroTable[idx].IsEnabled() )
		return false;

	return m_pShareData->m_Common.m_sMacro.m_MacroTable[idx].m_bReloadWhenExecute;
}




/*!	@brief ���L������������/�c�[���o�[

	�c�[���o�[�֘A�̏���������

	@author genta
	@date 2005.01.30 genta CShareData::Init()���番���D
		����ݒ肵�Ȃ��ň�C�Ƀf�[�^�]������悤�ɁD
*/
void CShareData::InitToolButtons(DLLSHAREDATA* pShareData)
{
		/* �c�[���o�[�{�^���\���� */
//Sept. 16, 2000 JEPRO
//	CShareData_new2.cpp�łł��邾���n���ƂɏW�܂�悤�ɃA�C�R���̏��Ԃ�啝�ɓ���ւ����̂ɔ����ȉ��̏����ݒ�l��ύX
	// 2010.06.26 Moca ���e�� CMenuDrawer::FindToolbarNoFromCommandId �̖߂�l�ł�
	static const int DEFAULT_TOOL_BUTTONS[] = {
		1,	//�V�K�쐬
		25,		//�t�@�C�����J��(DropDown)
		3,		//�㏑���ۑ�		//Sept. 16, 2000 JEPRO 3��11�ɕύX	//Oct. 25, 2000 11��3
		4,		//���O��t���ĕۑ�	//Sept. 19, 2000 JEPRO �ǉ�
		0,

		33,	//���ɖ߂�(Undo)	//Sept. 16, 2000 JEPRO 7��19�ɕύX	//Oct. 25, 2000 19��33
		34,	//��蒼��(Redo)	//Sept. 16, 2000 JEPRO 8��20�ɕύX	//Oct. 25, 2000 20��34
		0,

		87,	//�ړ�����: �O��	//Dec. 24, 2000 JEPRO �ǉ�
		88,	//�ړ�����: ����	//Dec. 24, 2000 JEPRO �ǉ�
		0,

		225,	//����		//Sept. 16, 2000 JEPRO 9��22�ɕύX	//Oct. 25, 2000 22��225
		226,	//��������	//Sept. 16, 2000 JEPRO 16��23�ɕύX	//Oct. 25, 2000 23��226
		227,	//�O������	//Sept. 16, 2000 JEPRO 17��24�ɕύX	//Oct. 25, 2000 24��227
		228,	//�u��		// Oct. 7, 2000 JEPRO �ǉ�
		229,	//�����}�[�N�̃N���A	//Sept. 16, 2000 JEPRO 41��25�ɕύX(Oct. 7, 2000 25��26)	//Oct. 25, 2000 25��229
		230,	//Grep		//Sept. 16, 2000 JEPRO 14��31�ɕύX	//Oct. 25, 2000 31��230
		232,	//�A�E�g���C�����	//Dec. 24, 2000 JEPRO �ǉ�
		0,

		264,	//�^�C�v�ʐݒ�ꗗ	//Sept. 16, 2000 JEPRO �ǉ�
		265,	//�^�C�v�ʐݒ�		//Sept. 16, 2000 JEPRO 18��36�ɕύX	//Oct. 25, 2000 36��265
		266,	//���ʐݒ�			//Sept. 16, 2000 JEPRO 10��37�ɕύX �������u�ݒ�v���p�e�B�V�[�g�v����ύX	//Oct. 25, 2000 37��266
		0,		//Oct. 8, 2000 jepro ���s�̂��߂ɒǉ�
		346,	//�R�}���h�ꗗ	//Oct. 8, 2000 JEPRO �ǉ�
	};

	//	�c�[���o�[�A�C�R�����̍ő�l�𒴂��Ȃ����߂̂��܂��Ȃ�
	//	�ő�l�𒴂��Ē�`���悤�Ƃ���Ƃ����ŃR���p�C���G���[�ɂȂ�܂��D
	char dummy[ _countof(DEFAULT_TOOL_BUTTONS) < MAX_TOOLBAR_BUTTON_ITEMS ? 1:0 ];
	dummy[0]=0;

	memcpy_raw(
		pShareData->m_Common.m_sToolBar.m_nToolBarButtonIdxArr,
		DEFAULT_TOOL_BUTTONS,
		sizeof(DEFAULT_TOOL_BUTTONS)
	);

	/* �c�[���o�[�{�^���̐� */
	pShareData->m_Common.m_sToolBar.m_nToolBarButtonNum = _countof(DEFAULT_TOOL_BUTTONS);
	pShareData->m_Common.m_sToolBar.m_bToolBarIsFlat = !IsVisualStyle();			/* �t���b�g�c�[���o�[�ɂ���^���Ȃ� */	// 2006.06.23 ryoji �r�W���A���X�^�C���ł͏����l���m�[�}���ɂ���
	
}


/*!	@brief ���L������������/�|�b�v�A�b�v���j���[

	�|�b�v�A�b�v���j���[�̏���������

	@date 2005.01.30 genta CShareData::Init()���番���D
*/
void CShareData::InitPopupMenu(DLLSHAREDATA* pShareData)
{
	/* �J�X�^�����j���[ �K��l */
	
	CommonSetting_CustomMenu& rMenu = m_pShareData->m_Common.m_sCustomMenu;

	/* �E�N���b�N���j���[ */
	int n = 0;
	rMenu.m_nCustMenuItemFuncArr[0][n] = F_UNDO;
	rMenu.m_nCustMenuItemKeyArr [0][n] = 'U';
	n++;
	rMenu.m_nCustMenuItemFuncArr[0][n] = F_REDO;
	rMenu.m_nCustMenuItemKeyArr [0][n] = 'R';
	n++;
	rMenu.m_nCustMenuItemFuncArr[0][n] = F_0;
	rMenu.m_nCustMenuItemKeyArr [0][n] = '\0';
	n++;
	rMenu.m_nCustMenuItemFuncArr[0][n] = F_CUT;
	rMenu.m_nCustMenuItemKeyArr [0][n] = 'T';
	n++;
	rMenu.m_nCustMenuItemFuncArr[0][n] = F_COPY;
	rMenu.m_nCustMenuItemKeyArr [0][n] = 'C';
	n++;
	rMenu.m_nCustMenuItemFuncArr[0][n] = F_PASTE;
	rMenu.m_nCustMenuItemKeyArr [0][n] = 'P';
	n++;
	rMenu.m_nCustMenuItemFuncArr[0][n] = F_DELETE;
	rMenu.m_nCustMenuItemKeyArr [0][n] = 'D';
	n++;
	rMenu.m_nCustMenuItemFuncArr[0][n] = F_0;
	rMenu.m_nCustMenuItemKeyArr [0][n] = '\0';
	n++;
	rMenu.m_nCustMenuItemFuncArr[0][n] = F_COPY_CRLF;	//Nov. 9, 2000 JEPRO �uCRLF���s�ŃR�s�[�v��ǉ�
	rMenu.m_nCustMenuItemKeyArr [0][n] = 'L';
	n++;
	rMenu.m_nCustMenuItemFuncArr[0][n] = F_COPY_ADDCRLF;
	rMenu.m_nCustMenuItemKeyArr [0][n] = 'H';
	n++;
	rMenu.m_nCustMenuItemFuncArr[0][n] = F_PASTEBOX;	//Nov. 9, 2000 JEPRO �u��`�\��t���v�𕜊�
	rMenu.m_nCustMenuItemKeyArr [0][n] = 'X';
	n++;
	rMenu.m_nCustMenuItemFuncArr[0][n] = F_0;
	rMenu.m_nCustMenuItemKeyArr [0][n] = '\0';
	n++;
	rMenu.m_nCustMenuItemFuncArr[0][n] = F_SELECTALL;
	rMenu.m_nCustMenuItemKeyArr [0][n] = 'A';
	n++;

	rMenu.m_nCustMenuItemFuncArr[0][n] = F_0;		//Oct. 3, 2000 JEPRO �ȉ��Ɂu�^�O�W�����v�v�Ɓu�^�O�W�����v�o�b�N�v��ǉ�
	rMenu.m_nCustMenuItemKeyArr [0][n] = '\0';
	n++;
	rMenu.m_nCustMenuItemFuncArr[0][n] = F_TAGJUMP;
	rMenu.m_nCustMenuItemKeyArr [0][n] = 'G';		//Nov. 9, 2000 JEPRO �u�R�s�[�v�ƃo�b�e�B���O���Ă����A�N�Z�X�L�[��ύX(T��G)
	n++;
	rMenu.m_nCustMenuItemFuncArr[0][n] = F_TAGJUMPBACK;
	rMenu.m_nCustMenuItemKeyArr [0][n] = 'B';
	n++;
	rMenu.m_nCustMenuItemFuncArr[0][n] = F_0;		//Oct. 15, 2000 JEPRO �ȉ��Ɂu�I��͈͓��S�s�R�s�[�v�Ɓu���p���t���R�s�[�v��ǉ�
	rMenu.m_nCustMenuItemKeyArr [0][n] = '\0';
	n++;
	rMenu.m_nCustMenuItemFuncArr[0][n] = F_COPYLINES;
	rMenu.m_nCustMenuItemKeyArr [0][n] = '@';
	n++;
	rMenu.m_nCustMenuItemFuncArr[0][n] = F_COPYLINESASPASSAGE;
	rMenu.m_nCustMenuItemKeyArr [0][n] = '.';
	n++;
	rMenu.m_nCustMenuItemFuncArr[0][n] = F_0;
	rMenu.m_nCustMenuItemKeyArr [0][n] = '\0';
	n++;
	rMenu.m_nCustMenuItemFuncArr[0][n] = F_COPYPATH;
	rMenu.m_nCustMenuItemKeyArr [0][n] = '\\';
	n++;
	rMenu.m_nCustMenuItemFuncArr[0][n] = F_PROPERTY_FILE;
	rMenu.m_nCustMenuItemKeyArr [0][n] = 'F';		//Nov. 9, 2000 JEPRO �u��蒼���v�ƃo�b�e�B���O���Ă����A�N�Z�X�L�[��ύX(R��F)
	n++;
	rMenu.m_nCustMenuItemNumArr[0] = n;

	/* �J�X�^�����j���[�P */
	rMenu.m_nCustMenuItemNumArr[1] = 7;
	rMenu.m_nCustMenuItemFuncArr[1][0] = F_FILEOPEN;
	rMenu.m_nCustMenuItemKeyArr [1][0] = 'O';		//Sept. 14, 2000 JEPRO �ł��邾���W���ݒ�l�ɍ��킹��悤�ɕύX (F��O)
	rMenu.m_nCustMenuItemFuncArr[1][1] = F_FILESAVE;
	rMenu.m_nCustMenuItemKeyArr [1][1] = 'S';
	rMenu.m_nCustMenuItemFuncArr[1][2] = F_NEXTWINDOW;
	rMenu.m_nCustMenuItemKeyArr [1][2] = 'N';		//Sept. 14, 2000 JEPRO �ł��邾���W���ݒ�l�ɍ��킹��悤�ɕύX (O��N)
	rMenu.m_nCustMenuItemFuncArr[1][3] = F_TOLOWER;
	rMenu.m_nCustMenuItemKeyArr [1][3] = 'L';
	rMenu.m_nCustMenuItemFuncArr[1][4] = F_TOUPPER;
	rMenu.m_nCustMenuItemKeyArr [1][4] = 'U';
	rMenu.m_nCustMenuItemFuncArr[1][5] = F_0;
	rMenu.m_nCustMenuItemKeyArr [1][5] = '\0';
	rMenu.m_nCustMenuItemFuncArr[1][6] = F_WINCLOSE;
	rMenu.m_nCustMenuItemKeyArr [1][6] = 'C';

	/* �^�u���j���[ */	//@@@ 2003.06.14 MIK
	n = 0;
	rMenu.m_nCustMenuItemFuncArr[CUSTMENU_INDEX_FOR_TABWND][n] = F_FILESAVE;
	rMenu.m_nCustMenuItemKeyArr [CUSTMENU_INDEX_FOR_TABWND][n] = 'S';
	n++;
	rMenu.m_nCustMenuItemFuncArr[CUSTMENU_INDEX_FOR_TABWND][n] = F_FILESAVEAS_DIALOG;
	rMenu.m_nCustMenuItemKeyArr [CUSTMENU_INDEX_FOR_TABWND][n] = 'A';
	n++;
	rMenu.m_nCustMenuItemFuncArr[CUSTMENU_INDEX_FOR_TABWND][n] = F_FILECLOSE;
	rMenu.m_nCustMenuItemKeyArr [CUSTMENU_INDEX_FOR_TABWND][n] = 'R';	// 2007.06.26 ryoji B -> R
	n++;
	rMenu.m_nCustMenuItemFuncArr[CUSTMENU_INDEX_FOR_TABWND][n] = F_FILECLOSE_OPEN;
	rMenu.m_nCustMenuItemKeyArr [CUSTMENU_INDEX_FOR_TABWND][n] = 'L';
	n++;
	rMenu.m_nCustMenuItemFuncArr[CUSTMENU_INDEX_FOR_TABWND][n] = F_WINCLOSE;
	rMenu.m_nCustMenuItemKeyArr [CUSTMENU_INDEX_FOR_TABWND][n] = 'C';
	n++;
	rMenu.m_nCustMenuItemFuncArr[CUSTMENU_INDEX_FOR_TABWND][n] = F_FILE_REOPEN;
	rMenu.m_nCustMenuItemKeyArr [CUSTMENU_INDEX_FOR_TABWND][n] = 'W';
	n++;
	rMenu.m_nCustMenuItemFuncArr[CUSTMENU_INDEX_FOR_TABWND][n] = F_0;
	rMenu.m_nCustMenuItemKeyArr [CUSTMENU_INDEX_FOR_TABWND][n] = '\0';
	n++;
	rMenu.m_nCustMenuItemFuncArr[CUSTMENU_INDEX_FOR_TABWND][n] = F_GROUPCLOSE;
	rMenu.m_nCustMenuItemKeyArr [CUSTMENU_INDEX_FOR_TABWND][n] = 'G';
	n++;
	rMenu.m_nCustMenuItemFuncArr[CUSTMENU_INDEX_FOR_TABWND][n] = F_TAB_CLOSEOTHER;
	rMenu.m_nCustMenuItemKeyArr [CUSTMENU_INDEX_FOR_TABWND][n] = 'O';
	n++;
	rMenu.m_nCustMenuItemFuncArr[CUSTMENU_INDEX_FOR_TABWND][n] = F_TAB_CLOSELEFT;
	rMenu.m_nCustMenuItemKeyArr [CUSTMENU_INDEX_FOR_TABWND][n] = 'H';
	n++;
	rMenu.m_nCustMenuItemFuncArr[CUSTMENU_INDEX_FOR_TABWND][n] = F_TAB_CLOSERIGHT;
	rMenu.m_nCustMenuItemKeyArr [CUSTMENU_INDEX_FOR_TABWND][n] = 'M';
	n++;
	rMenu.m_nCustMenuItemFuncArr[CUSTMENU_INDEX_FOR_TABWND][n] = F_0;
	rMenu.m_nCustMenuItemKeyArr [CUSTMENU_INDEX_FOR_TABWND][n] = '\0';
	n++;
	rMenu.m_nCustMenuItemFuncArr[CUSTMENU_INDEX_FOR_TABWND][n] = F_TAB_MOVERIGHT;
	rMenu.m_nCustMenuItemKeyArr [CUSTMENU_INDEX_FOR_TABWND][n] = '0';
	n++;
	rMenu.m_nCustMenuItemFuncArr[CUSTMENU_INDEX_FOR_TABWND][n] = F_TAB_MOVELEFT;
	rMenu.m_nCustMenuItemKeyArr [CUSTMENU_INDEX_FOR_TABWND][n] = '1';
	n++;
	rMenu.m_nCustMenuItemFuncArr[CUSTMENU_INDEX_FOR_TABWND][n] = F_TAB_SEPARATE;
	rMenu.m_nCustMenuItemKeyArr [CUSTMENU_INDEX_FOR_TABWND][n] = 'E';
	n++;
	rMenu.m_nCustMenuItemFuncArr[CUSTMENU_INDEX_FOR_TABWND][n] = F_TAB_JOINTNEXT;
	rMenu.m_nCustMenuItemKeyArr [CUSTMENU_INDEX_FOR_TABWND][n] = 'X';
	n++;
	rMenu.m_nCustMenuItemFuncArr[CUSTMENU_INDEX_FOR_TABWND][n] = F_TAB_JOINTPREV;
	rMenu.m_nCustMenuItemKeyArr [CUSTMENU_INDEX_FOR_TABWND][n] = 'V';
	n++;
	rMenu.m_nCustMenuItemNumArr[CUSTMENU_INDEX_FOR_TABWND] = n;
}

/* ����I����ɋ��L���������̕�������X�V���� */
void CShareData::RefreshString()
{

	RefreshKeyAssignString( m_pShareData );
}

void CShareData::CreateTypeSettings()
{
	if( NULL == m_pvTypeSettings ){
		m_pvTypeSettings = new std::vector<STypeConfig*>();
	}
}

std::vector<STypeConfig*>& CShareData::GetTypeSettings()
{
	return *m_pvTypeSettings;
}


void CShareData::InitFileTree( SFileTree* setting )
{
	setting->m_bProject = true;
	for(int i = 0; i < (int)_countof(setting->m_aItems); i++){
		SFileTreeItem& item = setting->m_aItems[i];
		item.m_eFileTreeItemType = EFileTreeItemType_Grep;
		item.m_szTargetPath = _T("");
		item.m_szLabelName = _T("");
		item.m_szTargetPath = _T("");
		item.m_nDepth = 0;
		item.m_szTargetFile = _T("");
		item.m_bIgnoreHidden = true;
		item.m_bIgnoreReadOnly = false;
		item.m_bIgnoreSystem = false;
	}
	setting->m_nItemCount = 1;
	setting->m_aItems[0].m_szTargetPath = _T(".");
	setting->m_aItems[0].m_szTargetFile = _T("*.*");
}