/*!	@file
	@brief �L�[���[�h�⊮

	@author Norio Nakatani
*/
/*
	Copyright (C) 1998-2001, Norio Nakatani
	Copyright (C) 2000, jepro, genta
	Copyright (C) 2001, asa-o
	Copyright (C) 2002, YAZAKI
	Copyright (C) 2003, Moca, KEITA
	Copyright (C) 2004, genta, Moca, novice
	Copyright (C) 2007, ryoji

	This source code is designed for sakura editor.
	Please contact the copyright holders to use this code for other purpose.
*/
#include "StdAfx.h"
#include "CHokanMgr.h"
#include "env/CShareData.h"
#include "view/CEditView.h"
#include "plugin/CJackManager.h"
#include "plugin/CComplementIfObj.h"
#include "util/input.h"
#include "util/os.h"
#include "util/other_util.h"
#include "sakura_rc.h"

WNDPROC			gm_wpHokanListProc;


LRESULT APIENTRY HokanList_SubclassProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	// Modified by KEITA for WIN64 2003.9.6
	CDialog* pCDialog = ( CDialog* )::GetWindowLongPtr( ::GetParent( hwnd ), DWLP_USER );
	CHokanMgr* pCHokanMgr = (CHokanMgr*)::GetWindowLongPtr( ::GetParent( hwnd ), DWLP_USER );

	switch( uMsg ){
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		{
			// �A�N�e�B�u����h�~���邽�߂Ɏ��O�Ń��X�g�I�����������{����
			LRESULT lResult = ::SendMessageAny( hwnd, LB_ITEMFROMPOINT, 0, lParam );
			if( HIWORD(lResult) == 0 ){	// �N���C�A���g�G���A��
				if( uMsg == WM_LBUTTONDOWN ){
					List_SetCurSel( hwnd, LOWORD(lResult) );
					pCHokanMgr->OnLbnSelChange( hwnd, IDC_LIST_WORDS );
				}
				else if( uMsg == WM_LBUTTONDBLCLK ){
					pCHokanMgr->DoHokan(0);
				}
			}
		}
		return 0;	// �{���̃E�B���h�E�v���V�[�W���͌Ă΂Ȃ��i�A�N�e�B�u�����Ȃ��j
	}
	return CallWindowProc( gm_wpHokanListProc, hwnd, uMsg, wParam, lParam);
}



CHokanMgr::CHokanMgr()
{
	m_cmemCurWord.SetString(L"");

	m_nCurKouhoIdx = -1;
	m_bTimerFlag = TRUE;
}

CHokanMgr::~CHokanMgr()
{
}

/* ���[�h���X�_�C�A���O�̕\�� */
HWND CHokanMgr::DoModeless( HINSTANCE hInstance , HWND hwndParent, LPARAM lParam )
{
	HWND hwndWork;
	hwndWork = CDialog::DoModeless( hInstance, hwndParent, IDD_HOKAN, lParam, SW_HIDE );
	::SetFocus( ((CEditView*)m_lParam)->GetHwnd() );	//�G�f�B�^�Ƀt�H�[�J�X��߂�
	OnSize( 0, 0 );
	/* ���X�g���t�b�N */
	// Modified by KEITA for WIN64 2003.9.6
	::gm_wpHokanListProc = (WNDPROC) ::SetWindowLongPtr( ::GetDlgItem( GetHwnd(), IDC_LIST_WORDS ), GWLP_WNDPROC, (LONG_PTR)HokanList_SubclassProc  );

	return hwndWork;
}

/* ���[�h���X���F�ΏۂƂȂ�r���[�̕ύX */
void CHokanMgr::ChangeView( LPARAM pcEditView )
{
	m_lParam = pcEditView;
	return;
}

void CHokanMgr::Hide( void )
{

	::ShowWindow( GetHwnd(), SW_HIDE );
	m_nCurKouhoIdx = -1;
	/* ���̓t�H�[�J�X���󂯎�����Ƃ��̏��� */
	CEditView* pcEditView = reinterpret_cast<CEditView*>(m_lParam);
	pcEditView->OnSetFocus();
	return;

}

/*!	������
	pcmemHokanWord == NULL�̂Ƃ��A�⊮��₪�ЂƂ�������A�⊮�E�B���h�E��\�����Ȃ��ŏI�����܂��B
	Search()�Ăяo�����Ŋm�菈����i�߂Ă��������B

	@date 2002.2.17 YAZAKI CShareData�̃C���X�^���X�́ACProcess�ɂЂƂ���̂݁B
*/
int CHokanMgr::Search(
	POINT*			ppoWin,
	int				nWinHeight,
	int				nColumnWidth,
	const wchar_t*	pszCurWord,
	const TCHAR*	pszHokanFile,
	bool			bHokanLoHiCase,	// ���͕⊮�@�\�F�p�啶���������𓯈ꎋ���� 2001/06/19 asa-o
	bool			bHokanByFile,	// �ҏW���f�[�^�������T�� 2003.06.23 Moca
	int				nHokanType,
	bool			bHokanByKeyword,
	CNativeW*		pcmemHokanWord	// 2001/06/19 asa-o
)
{
	CEditView* pcEditView = reinterpret_cast<CEditView*>(m_lParam);

	/* ���L�f�[�^�\���̂̃A�h���X��Ԃ� */
	m_pShareData = &GetDllShareData();

	/*
	||  �⊮�L�[���[�h�̌���
	||
	||  �E���������������ׂĕԂ�(���s�ŋ�؂��ĕԂ�)
	||  �E�w�肳�ꂽ���̍ő吔�𒴂���Ə����𒆒f����
	||  �E������������Ԃ�
	||
	*/
	m_vKouho.clear();
	CDicMgr::HokanSearch(
		pszCurWord,
		bHokanLoHiCase,								// ��������ɕύX	2001/06/19 asa-o
		m_vKouho,
		0, //Max��␔
		pszHokanFile
	);

	// 2003.05.16 Moca �ǉ� �ҏW���f�[�^���������T��
	if( bHokanByFile ){
		pcEditView->HokanSearchByFile(
			pszCurWord,
			bHokanLoHiCase,
			m_vKouho,
			1024 // �ҏW���f�[�^����Ȃ̂Ő��𐧌����Ă���
		);
	}
	// 2012.10.13 Moca �����L�[���[�h�������T��
	if( bHokanByKeyword ){
		HokanSearchByKeyword(
			pszCurWord,
			bHokanLoHiCase,
			m_vKouho
		);
	}

	{
		int nOption = (
			  (bHokanLoHiCase ? 0x01 : 0)
			  | (bHokanByFile ? 0x02 : 0)
			);
		
		CPlug::Array plugs;
		CPlug::Array plugType;
		CJackManager::getInstance()->GetUsablePlug( PP_COMPLEMENTGLOBAL, 0, &plugs );
		if( nHokanType != 0 ){
			CJackManager::getInstance()->GetUsablePlug( PP_COMPLEMENT, nHokanType, &plugType );
			if( 0 < plugType.size() ){
				plugs.push_back( plugType[0] );
			}
		}

		for( CPlug::Array::iterator it = plugs.begin(); it != plugs.end(); ++it ){
			//�C���^�t�F�[�X�I�u�W�F�N�g����
			CWSHIfObj::List params;
			std::wstring curWord = pszCurWord;
			CComplementIfObj* objComp = new CComplementIfObj( curWord , this, nOption );
			objComp->AddRef();
			params.push_back( objComp );
			//�v���O�C���Ăяo��
			(*it)->Invoke( pcEditView, params );

			objComp->Release();
		}
	}

	if( 0 == m_vKouho.size() ){
		m_nCurKouhoIdx = -1;
		return 0;
	}

//	2001/06/19 asa-o ��₪�P�̏ꍇ�⊮�E�B���h�E�͕\�����Ȃ�(�����⊮�̏ꍇ�͏���)
	if( 1 == m_vKouho.size() ){
		if(pcmemHokanWord != NULL){
			m_nCurKouhoIdx = -1;
			pcmemHokanWord->SetString( m_vKouho[0].c_str() );
			return 1;
		}
	}



//	m_hFont = hFont;
	m_poWin.x = ppoWin->x;
	m_poWin.y = ppoWin->y;
	m_nWinHeight = nWinHeight;
	m_nColumnWidth = nColumnWidth;
//	m_cmemCurWord.SetData( pszCurWord, lstrlen( pszCurWord ) );
	m_cmemCurWord.SetString( pszCurWord );


	m_nCurKouhoIdx = 0;
//	SetCurKouhoStr();



//	::ShowWindow( GetHwnd(), SW_SHOWNA );







	HWND hwndList;
	hwndList = ::GetDlgItem( GetHwnd(), IDC_LIST_WORDS );
	List_ResetContent( hwndList );
	{
		size_t kouhoNum = m_vKouho.size();
		for( size_t i = 0; i < kouhoNum; ++i ){
			::List_AddString( hwndList, m_vKouho[i].c_str() );
		}
	}
	List_SetCurSel( hwndList, 0 );


//@@	::EnableWindow( ::GetParent( ::GetParent( m_hwndParent ) ), FALSE );


	int nX;
	int nY;
	int nCX;
	int nCY;
	RECT	rcDesktop;
	//	May 01, 2004 genta �}���`���j�^�Ή�
	::GetMonitorWorkRect( GetHwnd(), &rcDesktop );

	nX = m_poWin.x - m_nColumnWidth;
	nY = m_poWin.y + m_nWinHeight + 4;
	nCX = m_nWidth;
	nCY = m_nHeight;

	/* ���ɓ���Ȃ� */
	if( nY + nCY < rcDesktop.bottom ){
		/* �������Ȃ� */
	}else
	/* ��ɓ���Ȃ� */
	if( rcDesktop.top < m_poWin.y - m_nHeight - 4 ){
		/* ��ɏo�� */
		nY = m_poWin.y - m_nHeight - 4;
	}else
	/* ��ɏo�������ɏo����(�L���ق��ɏo��) */
	if(	rcDesktop.bottom - nY > m_poWin.y ){
		/* ���ɏo�� */
//		m_nHeight = rcDesktop.bottom - nY;
		nCY = rcDesktop.bottom - nY;
	}else{
		/* ��ɏo�� */
		nY = rcDesktop.top;
		nCY = m_poWin.y - 4 - rcDesktop.top;
	}

//	2001/06/19 Start by asa-o: �\���ʒu�␳

	// �E�ɓ���
	if(nX + nCX < rcDesktop.right ){
		// ���̂܂�
	}else
	// ���ɓ���
	if(rcDesktop.left < nX - nCX + 8){
		// ���ɕ\��
		nX -= nCX - 8;
	}else{
		// �T�C�Y�𒲐����ĉE�ɕ\��
		nCX = t_max((int)(rcDesktop.right - nX) , 100);	// �Œ�T�C�Y��100���炢��
	}

//	2001/06/19 End

//	2001/06/18 Start by asa-o: �␳��̈ʒu�E�T�C�Y��ۑ�
	m_poWin.x = nX;
	m_poWin.y = nY;
	m_nHeight = nCY;
	m_nWidth = nCX;
//	2001/06/18 End

	/* �͂ݏo���Ȃ珬�������� */
//	if( rcDesktop.bottom < nY + nCY ){
//		/* ���ɂ͂ݏo�� */
//		if( m_poWin.y - 4 - nCY < 0 ){
//			/* ��ɂ͂ݏo�� */
//			/* �������������� */
//			nCY = rcDesktop.bottom - nY - 4;
//		}else{
//
//		}
//
//	}
	::MoveWindow( GetHwnd(), nX, nY, nCX, nCY, TRUE );
	::ShowWindow( GetHwnd(), SW_SHOWNA );


//	2001/06/18 asa-o:
	ShowTip();	// �⊮�E�B���h�E�őI�𒆂̒P��ɃL�[���[�h�w���v��\��

//	2003.06.25 Moca ���̃��\�b�h�Ŏg���Ă��Ȃ��̂ŁA�Ƃ肠�����폜���Ă���
	int kouhoNum = m_vKouho.size();
	m_vKouho.clear();
	return kouhoNum;
}

void CHokanMgr::HokanSearchByKeyword(
	const wchar_t*	pszCurWord,
	bool 			bHokanLoHiCase,
	vector_ex<std::wstring>& 	vKouho
){
	const CEditView* pcEditView = reinterpret_cast<const CEditView*>(m_lParam);
	const STypeConfig& type = pcEditView->GetDocument()->m_cDocType.GetDocumentAttribute();
	CKeyWordSetMgr& keywordMgr = m_pShareData->m_Common.m_sSpecialKeyword.m_CKeyWordSetMgr;
	const int nKeyLen = wcslen(pszCurWord);
	for( int n = 0; n < MAX_KEYWORDSET_PER_TYPE; n++ ){
		int kwdset = type.m_nKeyWordSetIdx[n];
		if( kwdset == -1 ){
			continue;
		}
		const int keyCount = keywordMgr.GetKeyWordNum(kwdset);
		for(int i = 0; i < keyCount; i++){
			const wchar_t* word = keywordMgr.GetKeyWord(kwdset,i);
			int nRet;
			if( bHokanLoHiCase ){
				nRet = auto_memicmp(pszCurWord, word, nKeyLen );
			}else{
				nRet = auto_memcmp(pszCurWord, word, nKeyLen );
			}
			if( nRet != 0 ){
				continue;
			}
			std::wstring strWord = std::wstring(word);
			AddKouhoUnique(vKouho, strWord);
		}
	}
}


/*!
	�W���ȊO�̃��b�Z�[�W��ߑ�����
*/
INT_PTR CHokanMgr::DispatchEvent( HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam )
{
	// �O�̂��� IME �֘A�̃��b�Z�[�W������悤�Ȃ�r���[�ɏ���������
	// �����̊��ˑ��i�풓�\�t�g�H�j�ɂ����̂�������Ȃ����A
	// �t�H�[�J�X�������Ă� IME �֘A���b�Z�[�W���������ɗ���P�[�X���������̂ŁA���̑΍�
	if(wMsg >= WM_IME_STARTCOMPOSITION && wMsg <= WM_IME_KEYLAST || wMsg >= WM_IME_SETCONTEXT && wMsg <= WM_IME_KEYUP){
		CEditView* pcEditView = (CEditView*)m_lParam;
		pcEditView->DispatchEvent( pcEditView->GetHwnd(), wMsg, wParam, lParam );
		return TRUE;
	}

	INT_PTR result;
	result = CDialog::DispatchEvent( hWnd, wMsg, wParam, lParam );
	switch( wMsg ){
	case WM_MOUSEACTIVATE:
		// �A�N�e�B�u�ɂ��Ȃ��ł���
		::SetWindowLongPtr( GetHwnd(), DWLP_MSGRESULT, MA_NOACTIVATE );
		return TRUE;
	case WM_LBUTTONDOWN:
		return TRUE;	// �N���C�A���g�̈�̓��X�g�{�b�N�X�Ŗ��܂��Ă���̂ł����ւ͗��Ȃ��͂������ǔO�̂���
	case WM_NCLBUTTONDOWN:
		// �����ł��A�N�e�B�u���h�~�̑΍􂪕K�v
		// ::SetCapture() ���Ď��O�ŃT�C�Y�ύX����
		{
			POINT ptStart;
			POINT pt;
			RECT rcStart;
			RECT rc;
			::GetCursorPos( &ptStart );
			::GetWindowRect( GetHwnd(), &rcStart );
			::SetCapture( GetHwnd() );
			while( ::GetCapture() == GetHwnd() )
			{
				MSG msg;
				if (!::GetMessage(&msg, NULL, 0, 0)){
					::PostQuitMessage( (int)msg.wParam );
					break;
				}

				switch (msg.message){
				case WM_MOUSEMOVE:
					rc = rcStart;
					::GetCursorPos( &pt );
					{
						switch( wParam ){
						case HTTOP:
							rc.top += pt.y - ptStart.y;
							break;
						case HTBOTTOM:
							rc.bottom += pt.y - ptStart.y;
							break;
						case HTLEFT:
							rc.left += pt.x - ptStart.x;
							break;
						case HTRIGHT:
							rc.right += pt.x - ptStart.x;
							break;
						case HTTOPLEFT:
							rc.top += pt.y - ptStart.y;
							rc.left += pt.x - ptStart.x;
							break;
						case HTTOPRIGHT:
							rc.top += pt.y - ptStart.y;
							rc.right += pt.x - ptStart.x;
							break;
						case HTBOTTOMLEFT:
							rc.bottom += pt.y - ptStart.y;
							rc.left += pt.x - ptStart.x;
							break;
						case HTBOTTOMRIGHT:
							rc.bottom += pt.y - ptStart.y;
							rc.right += pt.x - ptStart.x;
							break;
						}
						::MoveWindow( GetHwnd(), rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE );
					}
					break;
				case WM_LBUTTONUP:
				case WM_RBUTTONDOWN:
					::ReleaseCapture();
					break;
				case WM_KEYDOWN:
					if( msg.wParam == VK_ESCAPE ){
						// �L�����Z��
						::ReleaseCapture();
					}
					break;
				default:
					::DispatchMessage( &msg );
					break;
				}
			}
		}
		return TRUE;
	case WM_GETMINMAXINFO:
		// �ŏ��T�C�Y�𐧌�����
		MINMAXINFO *pmmi;
		pmmi = (MINMAXINFO*)lParam;
		pmmi->ptMinTrackSize.x = ::GetSystemMetrics(SM_CXVSCROLL) * 4;
		pmmi->ptMinTrackSize.y = ::GetSystemMetrics(SM_CYHSCROLL) * 4;
		break;
	}
	return result;
}

BOOL CHokanMgr::OnInitDialog( HWND hwndDlg, WPARAM wParam, LPARAM lParam )
{
	_SetHwnd( hwndDlg );
	/* ���N���X�����o */
//-	CreateSizeBox();
	return CDialog::OnInitDialog( hwndDlg, wParam, lParam );

}

BOOL CHokanMgr::OnDestroy( void )
{
	/* ���N���X�����o */
	CreateSizeBox();
	return CDialog::OnDestroy();


}


BOOL CHokanMgr::OnSize( WPARAM wParam, LPARAM lParam )
{
	/* ���N���X�����o */
	CDialog::OnSize( wParam, lParam );

	int	Controls[] = {
		IDC_LIST_WORDS
	};
	int		nControls = _countof( Controls );
	int		nWidth;
	int		nHeight;
	int		i;
	RECT	rc;
	HWND	hwndCtrl;
	POINT	po;
	RECT	rcDlg;


	::GetClientRect( GetHwnd(), &rcDlg );
	nWidth = rcDlg.right - rcDlg.left;  // width of client area
	nHeight = rcDlg.bottom - rcDlg.top; // height of client area

//	2001/06/18 Start by asa-o: �T�C�Y�ύX��̈ʒu��ۑ�
	m_poWin.x = rcDlg.left - 4;
	m_poWin.y = rcDlg.top - 3;
	::ClientToScreen(GetHwnd(),&m_poWin);
//	2001/06/18 End

	for ( i = 0; i < nControls; ++i ){
		hwndCtrl = ::GetDlgItem( GetHwnd(), Controls[i] );
		::GetWindowRect( hwndCtrl, &rc );
		po.x = rc.left;
		po.y = rc.top;
		::ScreenToClient( GetHwnd(), &po );
		rc.left = po.x;
		rc.top  = po.y;
		po.x = rc.right;
		po.y = rc.bottom;
		::ScreenToClient( GetHwnd(), &po );
		rc.right = po.x;
		rc.bottom  = po.y;
		if( Controls[i] == IDC_LIST_WORDS ){
			::SetWindowPos(
				hwndCtrl,
				NULL,
				rc.left,
				rc.top,
				nWidth - rc.left * 2,
				nHeight - rc.top * 2/* - 20*/,
				SWP_NOOWNERZORDER | SWP_NOZORDER
			);
		}
	}

//	2001/06/18 asa-o:
	ShowTip();	// �⊮�E�B���h�E�őI�𒆂̒P��ɃL�[���[�h�w���v��\��

	return TRUE;

}


BOOL CHokanMgr::OnLbnSelChange( HWND hwndCtl, int wID )
{
//	2001/06/18 asa-o:
	ShowTip();	// �⊮�E�B���h�E�őI�𒆂̒P��ɃL�[���[�h�w���v��\��
	return TRUE;
}



/* �⊮���s */
BOOL CHokanMgr::DoHokan( int nVKey )
{
	DEBUG_TRACE( _T("CHokanMgr::DoHokan( nVKey==%xh )\n"), nVKey );

	/* �⊮��⌈��L�[ */
	if( VK_RETURN	== nVKey && !m_pShareData->m_Common.m_sHelper.m_bHokanKey_RETURN )	return FALSE;/* VK_RETURN �⊮����L�[���L��/���� */
	if( VK_TAB		== nVKey && !m_pShareData->m_Common.m_sHelper.m_bHokanKey_TAB ) 		return FALSE;/* VK_TAB    �⊮����L�[���L��/���� */
	if( VK_RIGHT	== nVKey && !m_pShareData->m_Common.m_sHelper.m_bHokanKey_RIGHT )		return FALSE;/* VK_RIGHT  �⊮����L�[���L��/���� */

	HWND hwndList;
	int nItem;
	CEditView* pcEditView;
	hwndList = ::GetDlgItem( GetHwnd(), IDC_LIST_WORDS );
	nItem = List_GetCurSel( hwndList );
	if( LB_ERR == nItem ){
		return FALSE;
	}
	int nLabelLen = List_GetTextLen( hwndList, nItem );
	auto_array_ptr<WCHAR> wszLabel( new WCHAR [nLabelLen + 1] );
	List_GetText( hwndList, nItem, &wszLabel[0] );

 	/* �e�L�X�g��\��t�� */
	pcEditView = reinterpret_cast<CEditView*>(m_lParam);
	//	Apr. 28, 2000 genta
	pcEditView->GetCommander().HandleCommand( F_WordDeleteToStart, false, 0, 0, 0, 0 );
	pcEditView->GetCommander().HandleCommand( F_INSTEXT_W, true, (LPARAM)&wszLabel[0], wcslen(&wszLabel[0]), TRUE, 0 );

	// Until here
//	pcEditView->GetCommander().HandleCommand( F_INSTEXT_W, true, (LPARAM)(wszLabel + m_cmemCurWord.GetLength()), TRUE, 0, 0 );
	Hide();

	return TRUE;
}

/*
�߂�l�� -2 �̏ꍇ�́A�A�v���P�[�V�����͍��ڂ̑I�����������A
���X�g �{�b�N�X�ł���ȏ�̓��삪�K�v�łȂ����Ƃ������܂��B

�߂�l�� -1 �̏ꍇ�́A���X�g �{�b�N�X���L�[�X�g���[�N�ɉ�����
�f�t�H���g�̓�������s���邱�Ƃ������܂��B

 �߂�l�� 0 �ȏ�̏ꍇ�́A���̒l�̓��X�g �{�b�N�X�̍��ڂ� 0 ��
��Ƃ����C���f�b�N�X���Ӗ����A���X�g �{�b�N�X�����̍��ڂł�
�L�[�X�g���[�N�ɉ����ăf�t�H���g�̓�������s���邱�Ƃ������܂��B

*/
//	int CHokanMgr::OnVKeyToItem( WPARAM wParam, LPARAM lParam )
//	{
//		return KeyProc( wParam, lParam );
//	}

/*
�߂�l�� -2 �̏ꍇ�́A�A�v���P�[�V�����͍��ڂ̑I�����������A
���X�g �{�b�N�X�ł���ȏ�̓��삪�K�v�łȂ����Ƃ������܂��B

�߂�l�� -1 �̏ꍇ�́A���X�g �{�b�N�X���L�[�X�g���[�N�ɉ�����
�f�t�H���g�̓�������s���邱�Ƃ������܂��B

 �߂�l�� 0 �ȏ�̏ꍇ�́A���̒l�̓��X�g �{�b�N�X�̍��ڂ� 0 ��
��Ƃ����C���f�b�N�X���Ӗ����A���X�g �{�b�N�X�����̍��ڂł�
�L�[�X�g���[�N�ɉ����ăf�t�H���g�̓�������s���邱�Ƃ������܂��B

*/
//	int CHokanMgr::OnCharToItem( WPARAM wParam, LPARAM lParam )
//	{
//		WORD vkey;
//		WORD nCaretPos;
//		LPARAM hwndLB;
//		vkey = LOWORD(wParam);		// virtual-key code
//		nCaretPos = HIWORD(wParam);	// caret position
//		hwndLB = lParam;			// handle to list box
//	//	switch( vkey ){
//	//	}
//
//		MYTRACE( _T("CHokanMgr::OnCharToItem vkey=%xh\n"), vkey );
//		return -1;
//	}

int CHokanMgr::KeyProc( WPARAM wParam, LPARAM lParam )
{
	WORD vkey;
	vkey = LOWORD(wParam);		// virtual-key code
//	MYTRACE( _T("CHokanMgr::OnVKeyToItem vkey=%xh\n"), vkey );
	switch( vkey ){
	case VK_HOME:
	case VK_END:
	case VK_UP:
	case VK_DOWN:
	case VK_PRIOR:
	case VK_NEXT:
		/* ���X�g�{�b�N�X�̃f�t�H���g�̓���������� */
		::CallWindowProc( (WNDPROC)gm_wpHokanListProc, ::GetDlgItem( GetHwnd(), IDC_LIST_WORDS ), WM_KEYDOWN, wParam, lParam );
		return -1;
	case VK_RETURN:
	case VK_TAB:
	case VK_RIGHT:
#if 0
	case VK_SPACE:
#endif
		/* �⊮���s */
		if( DoHokan( vkey ) ){
			return -1;
		}else{
			return -2;
		}
	case VK_ESCAPE:
	case VK_LEFT:
		return -2;
	}
	return -2;
}

//	2001/06/18 Start by asa-o: �⊮�E�B���h�E�őI�𒆂̒P��ɃL�[���[�h�w���v��\��
void CHokanMgr::ShowTip()
{
	INT			nItem,
				nTopItem,
				nItemHeight;
	POINT		point;
	CEditView*	pcEditView;
	HWND		hwndCtrl;
	RECT		rcHokanWin;

	hwndCtrl = ::GetDlgItem( GetHwnd(), IDC_LIST_WORDS );

	nItem = List_GetCurSel( hwndCtrl );
	if( LB_ERR == nItem )	return ;

	int nLabelLen = List_GetTextLen( hwndCtrl, nItem );
	auto_array_ptr<WCHAR> szLabel( new WCHAR [nLabelLen + 1] );
	List_GetText( hwndCtrl, nItem, &szLabel[0] );	// �I�𒆂̒P����擾

	pcEditView = reinterpret_cast<CEditView*>(m_lParam);

	// ���łɎ���Tip���\������Ă�����
	if( pcEditView->m_dwTipTimer == 0 )
	{
		// ����Tip������
		pcEditView -> m_cTipWnd.Hide();
		pcEditView -> m_dwTipTimer = ::GetTickCount();
	}

	// �\������ʒu������
	nTopItem = List_GetTopIndex( hwndCtrl );
	nItemHeight = List_GetItemHeight( hwndCtrl, 0 );
	point.x = m_poWin.x + m_nWidth;
	point.y = m_poWin.y + 4 + (nItem - nTopItem) * nItemHeight;
	// 2001/06/19 asa-o �I�𒆂̒P�ꂪ�⊮�E�B���h�E�ɕ\������Ă���Ȃ玫��Tip��\��
	if( point.y > m_poWin.y && point.y < m_poWin.y + m_nHeight )
	{
		::SetRect( &rcHokanWin , m_poWin.x, m_poWin.y, m_poWin.x + m_nWidth, m_poWin.y + m_nHeight );
		if( !pcEditView -> ShowKeywordHelp( point, &szLabel[0], &rcHokanWin ) )
			pcEditView -> m_dwTipTimer = ::GetTickCount();	// �\������ׂ��L�[���[�h�w���v������
	}
}
//	2001/06/18 End

bool CHokanMgr::AddKouhoUnique(vector_ex<std::wstring>& kouhoList, const std::wstring& strWord)
{
	return kouhoList.push_back_unique(strWord);
}

//@@@ 2002.01.18 add start
const DWORD p_helpids[] = {
	0, 0
};

LPVOID CHokanMgr::GetHelpIdTable(void)
{
	return (LPVOID)p_helpids;
}
//@@@ 2002.01.18 add end

