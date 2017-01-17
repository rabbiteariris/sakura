/*!	@file
@brief CViewCommander�N���X�̃R�}���h(�ҏW�n ���x�ȑ���(���P��/�s����))�֐��Q

	2012/12/17	CViewCommander.cpp,CViewCommander_New.cpp���番��
*/
/*
	Copyright (C) 1998-2001, Norio Nakatani
	Copyright (C) 2000-2001, jepro, genta, �݂�
	Copyright (C) 2001, MIK, Stonee, Misaka, asa-o, novice, hor, YAZAKI
	Copyright (C) 2002, hor, YAZAKI, novice, genta, aroka, Azumaiya, minfu, MIK, oak, ���Ȃӂ�, Moca, ai
	Copyright (C) 2003, MIK, genta, �����, zenryaku, Moca, ryoji, naoh, KEITA, ���イ��
	Copyright (C) 2004, isearch, Moca, gis_dur, genta, crayonzen, fotomo, MIK, novice, �݂��΂�, Kazika
	Copyright (C) 2005, genta, novice, �����, MIK, Moca, D.S.Koba, aroka, ryoji, maru
	Copyright (C) 2006, genta, aroka, ryoji, �����, fon, yukihane, Moca
	Copyright (C) 2007, ryoji, maru, Uchi
	Copyright (C) 2008, ryoji, nasukoji
	Copyright (C) 2009, ryoji, nasukoji
	Copyright (C) 2010, ryoji
	Copyright (C) 2011, ryoji
	Copyright (C) 2012, Moca, ryoji

	This source code is designed for sakura editor.
	Please contact the copyright holders to use this code for other purpose.
*/

#include "StdAfx.h"
#include "CViewCommander.h"
#include "CViewCommander_inline.h"

#include "uiparts/CWaitCursor.h"
#include "mem/CMemoryIterator.h"	// @@@ 2002.09.28 YAZAKI
#include "_os/COsVersionInfo.h"


using namespace std; // 2002/2/3 aroka to here

#ifndef FID_RECONVERT_VERSION  // 2002.04.10 minfu 
#define FID_RECONVERT_VERSION 0x10000000
#endif
#ifndef SCS_CAP_SETRECONVERTSTRING
#define SCS_CAP_SETRECONVERTSTRING 0x00000004
#define SCS_QUERYRECONVERTSTRING 0x00020000
#define SCS_SETRECONVERTSTRING 0x00010000
#endif


/* �C���f���g ver1 */
void CViewCommander::Command_INDENT( wchar_t wcChar, EIndentType eIndent )
{
	using namespace WCODE;

#if 1	// ���������c���ΑI�𕝃[�����ő�ɂ���i�]���݊������j�B�����Ă� Command_INDENT() ver0 ���K�؂ɓ��삷��悤�ɕύX���ꂽ�̂ŁA�폜���Ă����ɕs�s���ɂ͂Ȃ�Ȃ��B
	// From Here 2001.12.03 hor
	/* SPACEorTAB�C�����f���g�ŋ�`�I�������[���̎��͑I��͈͂��ő�ɂ��� */
	//	Aug. 14, 2005 genta �܂�Ԃ�����LayoutMgr����擾����悤��
	if( INDENT_NONE != eIndent && m_pCommanderView->GetSelectionInfo().IsBoxSelecting() && GetSelect().GetFrom().x==GetSelect().GetTo().x ){
		GetSelect().SetToX( GetDocument()->m_cLayoutMgr.GetMaxLineKetas() );
		m_pCommanderView->RedrawAll();
		return;
	}
	// To Here 2001.12.03 hor
#endif
	Command_INDENT( &wcChar, CLogicInt(1), eIndent );
	return;
}



/* �C���f���g ver0 */
/*
	�I�����ꂽ�e�s�͈̔͂̒��O�ɁA�^����ꂽ������( pData )��}������B
	@param eIndent �C���f���g�̎��
*/
void CViewCommander::Command_INDENT( const wchar_t* const pData, const CLogicInt nDataLen, EIndentType eIndent )
{
	if( nDataLen <= 0 ) return;

	CLayoutRange sSelectOld;		//�͈͑I��
	CLayoutPoint ptInserted;		//�}����̑}���ʒu
	const struct IsIndentCharSpaceTab{
		IsIndentCharSpaceTab(){}
		bool operator()( const wchar_t ch ) const
		{ return ch == WCODE::SPACE || ch == WCODE::TAB; }
	} IsIndentChar;
	struct SSoftTabData {
		SSoftTabData( CLayoutInt nTab ) : m_szTab(NULL), m_nTab((Int)nTab) {}
		~SSoftTabData() { delete []m_szTab; }
		operator const wchar_t* ()
		{
			if( !m_szTab ){
				m_szTab = new wchar_t[m_nTab];
				wmemset( m_szTab, WCODE::SPACE, m_nTab );
			}
			return m_szTab;
		}
		int Len( CLayoutInt nCol ) { return m_nTab - ((Int)nCol % m_nTab); }
		wchar_t* m_szTab;
		int m_nTab;
	} stabData( GetDocument()->m_cLayoutMgr.GetTabSpace() );

	const bool bSoftTab = ( eIndent == INDENT_TAB && m_pCommanderView->m_pTypeData->m_bInsSpace );
	GetDocument()->m_cDocEditor.SetModified(true,true);	//	Jan. 22, 2002 genta

	if( !m_pCommanderView->GetSelectionInfo().IsTextSelected() ){			/* �e�L�X�g���I������Ă��邩 */
		if( INDENT_NONE != eIndent && !bSoftTab ){
			// ����`�I���ł͂Ȃ��̂� Command_WCHAR ����Ăі߂������悤�Ȃ��Ƃ͂Ȃ�
			Command_WCHAR( pData[0] );	// 1��������
		}
		else{
			// ����`�I���ł͂Ȃ��̂ł����֗���͎̂��ۂɂ̓\�t�g�^�u�̂Ƃ�����
			if( bSoftTab && !m_pCommanderView->IsInsMode() ){
				DelCharForOverwrite(pData, nDataLen);
			}
			m_pCommanderView->InsertData_CEditView(
				GetCaret().GetCaretLayoutPos(),
				!bSoftTab? pData: stabData,
				!bSoftTab? nDataLen: stabData.Len(GetCaret().GetCaretLayoutPos().GetX2()),
				&ptInserted,
				true
			);
			GetCaret().MoveCursor( ptInserted, true );
			GetCaret().m_nCaretPosX_Prev = GetCaret().GetCaretLayoutPos().GetX2();
		}
		return;
	}
	const bool bDrawSwitchOld = m_pCommanderView->SetDrawSwitch(false);	// 2002.01.25 hor
	/* ��`�͈͑I�𒆂� */
	if( m_pCommanderView->GetSelectionInfo().IsBoxSelecting() ){
// 2012.10.31 Moca �㏑�����[�h�̂Ƃ��̑I��͈͍폜����߂�
// 2014.06.02 Moca ���d�l��I�ׂ�悤��
		if( GetDllShareData().m_Common.m_sEdit.m_bOverWriteBoxDelete ){
			// From Here 2001.12.03 hor
			/* �㏑���[�h�̂Ƃ��͑I��͈͍폜 */
			if( ! m_pCommanderView->IsInsMode() /* Oct. 2, 2005 genta */){
				sSelectOld = GetSelect();
				m_pCommanderView->DeleteData( false );
				GetSelect() = sSelectOld;
				m_pCommanderView->GetSelectionInfo().SetBoxSelect(true);
			}
			// To Here 2001.12.03 hor
		}

		/* 2�_��Ίp�Ƃ����`�����߂� */
		CLayoutRange rcSel;
		TwoPointToRange(
			&rcSel,
			GetSelect().GetFrom(),	// �͈͑I���J�n
			GetSelect().GetTo()		// �͈͑I���I��
		);
		/* ���݂̑I��͈͂��I����Ԃɖ߂� */
		m_pCommanderView->GetSelectionInfo().DisableSelectArea( false/*true 2002.01.25 hor*/ );

		/*
			�����𒼑O�ɑ}�����ꂽ�������A����ɂ�茳�̈ʒu����ǂꂾ�����ɂ��ꂽ���B
			����ɏ]����`�I��͈͂����ɂ��炷�B
		*/
		CLayoutInt minOffset( -1 );
		/*
			���S�p�����̍����̌������ɂ���
			(1) eIndent == INDENT_TAB �̂Ƃ�
				�I��͈͂��^�u���E�ɂ���Ƃ��Ƀ^�u����͂���ƁA�S�p�����̑O�����I��͈͂���
				�͂ݏo���Ă���s�Ƃ����łȂ��s�Ń^�u�̕����A1����ݒ肳�ꂽ�ő�܂łƑ傫���قȂ�A
				�ŏ��ɑI������Ă���������I��͈͓��ɂƂǂ߂Ă������Ƃ��ł��Ȃ��Ȃ�B
				�ŏ��͋�`�I��͈͓��ɂ��ꂢ�Ɏ��܂��Ă���s�ɂ̓^�u��}�������A������Ƃ����͂�
				�o���Ă���s�ɂ����^�u��}�����邱�ƂƂ��A����ł͂ǂ̍s�ɂ��^�u���}������Ȃ�
				�Ƃ킩�����Ƃ��͂�蒼���ă^�u��}������B
			(2) eIndent == INDENT_SPACE �̂Ƃ��i���]���݊��I�ȓ���j
				��1�őI�����Ă���ꍇ�̂ݑS�p�����̍���������������B
				�ŏ��͋�`�I��͈͓��ɂ��ꂢ�Ɏ��܂��Ă���s�ɂ̓X�y�[�X��}�������A������Ƃ����͂�
				�o���Ă���s�ɂ����X�y�[�X��}�����邱�ƂƂ��A����ł͂ǂ̍s�ɂ��X�y�[�X���}������Ȃ�
				�Ƃ킩�����Ƃ��͂�蒼���ăX�y�[�X��}������B
		*/
		bool alignFullWidthChar = eIndent == INDENT_TAB && 0 == rcSel.GetFrom().x % this->GetDocument()->m_cLayoutMgr.GetTabSpace();
#if 1	// ���������c���ΑI��1��SPACE�C���f���g�őS�p�����𑵂���@�\(2)���ǉ������B
		alignFullWidthChar = alignFullWidthChar || (eIndent == INDENT_SPACE && 1 == rcSel.GetTo().x - rcSel.GetFrom().x);
#endif
		CWaitCursor cWaitCursor( m_pCommanderView->GetHwnd(), 1000 < rcSel.GetTo().y - rcSel.GetFrom().y );
		HWND hwndProgress = NULL;
		int nProgressPos = 0;
		if( cWaitCursor.IsEnable() ){
			hwndProgress = m_pCommanderView->StartProgress();
		}
		for( bool insertionWasDone = false; ; alignFullWidthChar = false ) {
			minOffset = CLayoutInt( -1 );
			for( CLayoutInt nLineNum = rcSel.GetFrom().y; nLineNum <= rcSel.GetTo().y; ++nLineNum ){
				const CLayout* pcLayout = GetDocument()->m_cLayoutMgr.SearchLineByLayoutY( nLineNum );
				//	Nov. 6, 2002 genta NULL�`�F�b�N�ǉ�
				//	���ꂪ�Ȃ���EOF�s���܂ދ�`�I�𒆂̕�������͂ŗ�����
				CLogicInt nIdxFrom, nIdxTo;
				CLayoutInt xLayoutFrom, xLayoutTo;
				bool reachEndOfLayout = false;
				if( pcLayout ) {
					/* �w�肳�ꂽ���ɑΉ�����s�̃f�[�^���̈ʒu�𒲂ׂ� */
					const struct {
						CLayoutInt keta;
						CLogicInt* outLogicX;
						CLayoutInt* outLayoutX;
					} sortedKetas[] = {
						{ rcSel.GetFrom().x, &nIdxFrom, &xLayoutFrom },
						{ rcSel.GetTo().x, &nIdxTo, &xLayoutTo },
						{ CLayoutInt(-1), 0, 0 }
					};
					CMemoryIterator it( pcLayout, this->GetDocument()->m_cLayoutMgr.GetTabSpace(), this->GetDocument()->m_cLayoutMgr.m_tsvInfo );
					for( int i = 0; 0 <= sortedKetas[i].keta; ++i ) {
						for( ; ! it.end(); it.addDelta() ) {
							if( sortedKetas[i].keta == it.getColumn() ) {
								break;
							}
							it.scanNext();
							if( sortedKetas[i].keta < it.getColumn() + it.getColumnDelta() ) {
								break;
							}
						}
						*sortedKetas[i].outLogicX = it.getIndex();
						*sortedKetas[i].outLayoutX = it.getColumn();
					}
					reachEndOfLayout = it.end();
				}else{
					nIdxFrom = nIdxTo = CLogicInt(0);
					xLayoutFrom = xLayoutTo = CLayoutInt(0);
					reachEndOfLayout = true;
				}
				const bool emptyLine = ! pcLayout || 0 == pcLayout->GetLengthWithoutEOL();
				const bool selectionIsOutOfLine = reachEndOfLayout && (
					(pcLayout && pcLayout->GetLayoutEol() != EOL_NONE) ? xLayoutFrom == xLayoutTo : xLayoutTo < rcSel.GetFrom().x
				);

				// ���͕����̑}���ʒu
				const CLayoutPoint ptInsert( selectionIsOutOfLine ? rcSel.GetFrom().x : xLayoutFrom, nLineNum );

				/* TAB��X�y�[�X�C���f���g�̎� */
				if( INDENT_NONE != eIndent ) {
					if( emptyLine || selectionIsOutOfLine ) {
						continue; // �C���f���g�������C���f���g�Ώۂ����݂��Ȃ�����(���s�����̌����s)�ɑ}�����Ȃ��B
					}
					/*
						���͂��C���f���g�p�̕����̂Ƃ��A��������œ��͕�����}�����Ȃ����Ƃ�
						�C���f���g�𑵂��邱�Ƃ��ł���B
						http://sakura-editor.sourceforge.net/cgi-bin/cyclamen/cyclamen.cgi?log=dev&v=4103
					*/
					if( nIdxFrom == nIdxTo // ��`�I��͈͂̉E�[�܂łɔ͈͂̍��[�ɂ��镶���̖������܂܂�Ă��炸�A
						&& ! selectionIsOutOfLine && pcLayout && IsIndentChar( pcLayout->GetPtr()[nIdxFrom] ) // ���́A�����̊܂܂�Ă��Ȃ��������C���f���g�����ł���A
						&& rcSel.GetFrom().x < rcSel.GetTo().x // ��0��`�I���ł͂Ȃ�(<<�݊����ƃC���f���g�����}���̎g������̂��߂ɏ��O����)�Ƃ��B
					) {
						continue;
					}
					/*
						�S�p�����̍����̌�����
					*/
					if( alignFullWidthChar
						&& (ptInsert.x == rcSel.GetFrom().x || (pcLayout && IsIndentChar( pcLayout->GetPtr()[nIdxFrom] )))
					) {	// �����̍������͈͂ɂ҂�������܂��Ă���
						minOffset = CLayoutInt(0);
						continue;
					}
				}

				/* ���݈ʒu�Ƀf�[�^��}�� */
				m_pCommanderView->InsertData_CEditView(
					ptInsert,
					!bSoftTab? pData: stabData,
					!bSoftTab? nDataLen: stabData.Len(ptInsert.x),
					&ptInserted,
					false
				);
				insertionWasDone = true;
				minOffset = t_min(
					0 <= minOffset ? minOffset : this->GetDocument()->m_cLayoutMgr.GetMaxLineKetas(),
					ptInsert.x <= ptInserted.x ? ptInserted.x - ptInsert.x : t_max( CLayoutInt(0), this->GetDocument()->m_cLayoutMgr.GetMaxLineKetas() - ptInsert.x)
				);

				GetCaret().MoveCursor( ptInserted, false );
				GetCaret().m_nCaretPosX_Prev = GetCaret().GetCaretLayoutPos().GetX2();

				if( hwndProgress ){
					int newPos = ::MulDiv((Int)nLineNum, 100, (Int)rcSel.GetTo().y);
					if( newPos != nProgressPos ){
						nProgressPos = newPos;
						Progress_SetPos( hwndProgress, newPos + 1 );
						Progress_SetPos( hwndProgress, newPos );
					}
				}
			}
			if( insertionWasDone || !alignFullWidthChar ) {
				break; // ���[�v�̕K�v�͂Ȃ��B(1.�����̑}�����s��ꂽ����B2.�����ł͂Ȃ��������̑}�����T���������ł͂Ȃ�����)
			}
		}

		if( hwndProgress ){
			::ShowWindow( hwndProgress, SW_HIDE );
		}

		// �}�����ꂽ�����̕������I��͈͂����ɂ��炵�ArcSel�ɃZ�b�g����B
		if( 0 < minOffset ) {
			rcSel.GetFromPointer()->x = t_min( rcSel.GetFrom().x + minOffset, this->GetDocument()->m_cLayoutMgr.GetMaxLineKetas() );
			rcSel.GetToPointer()->x = t_min( rcSel.GetTo().x + minOffset, this->GetDocument()->m_cLayoutMgr.GetMaxLineKetas() );
		}

		/* �J�[�\�����ړ� */
		GetCaret().MoveCursor( rcSel.GetFrom(), true );
		GetCaret().m_nCaretPosX_Prev = GetCaret().GetCaretLayoutPos().GetX2();

		if( !m_pCommanderView->m_bDoing_UndoRedo ){	/* �A���h�D�E���h�D�̎��s���� */
			/* ����̒ǉ� */
			GetOpeBlk()->AppendOpe(
				new CMoveCaretOpe(
					GetCaret().GetCaretLogicPos()	// ����O��̃L�����b�g�ʒu
				)
			);
		}
		GetSelect().SetFrom(rcSel.GetFrom());	//�͈͑I���J�n�ʒu
		GetSelect().SetTo(rcSel.GetTo());		//�͈͑I���I���ʒu
		m_pCommanderView->GetSelectionInfo().SetBoxSelect(true);
	}
	else if( GetSelect().IsLineOne() ){	// �ʏ�I��(1�s��)
		if( INDENT_NONE != eIndent && !bSoftTab ){
			// ����`�I���ł͂Ȃ��̂� Command_WCHAR ����Ăі߂������悤�Ȃ��Ƃ͂Ȃ�
			Command_WCHAR( pData[0] );	// 1��������
		}
		else{
			// ����`�I���ł͂Ȃ��̂ł����֗���͎̂��ۂɂ̓\�t�g�^�u�̂Ƃ�����
			m_pCommanderView->DeleteData( false );
			m_pCommanderView->InsertData_CEditView(
				GetCaret().GetCaretLayoutPos(),
				!bSoftTab? pData: stabData,
				!bSoftTab? nDataLen: stabData.Len(GetCaret().GetCaretLayoutPos().GetX2()),
				&ptInserted,
				false
			);
			GetCaret().MoveCursor( ptInserted, true );
			GetCaret().m_nCaretPosX_Prev = GetCaret().GetCaretLayoutPos().GetX2();
		}
	}
	else{	// �ʏ�I��(�����s)
		sSelectOld.SetFrom(CLayoutPoint(CLayoutInt(0),GetSelect().GetFrom().y));
		sSelectOld.SetTo  (CLayoutPoint(CLayoutInt(0),GetSelect().GetTo().y  ));
		if( GetSelect().GetTo().x > 0 ){
			sSelectOld.GetToPointer()->y++;
		}

		// ���݂̑I��͈͂��I����Ԃɖ߂�
		m_pCommanderView->GetSelectionInfo().DisableSelectArea( false );

		CWaitCursor cWaitCursor( m_pCommanderView->GetHwnd(), 1000 < sSelectOld.GetTo().GetY2() - sSelectOld.GetFrom().GetY2() );
		HWND hwndProgress = NULL;
		int nProgressPos = 0;
		if( cWaitCursor.IsEnable() ){
			hwndProgress = m_pCommanderView->StartProgress();
		}

		for( CLayoutInt i = sSelectOld.GetFrom().GetY2(); i < sSelectOld.GetTo().GetY2(); i++ ){
			CLayoutInt nLineCountPrev = GetDocument()->m_cLayoutMgr.GetLineCount();
			const CLayout* pcLayout = GetDocument()->m_cLayoutMgr.SearchLineByLayoutY( i );
			if( NULL == pcLayout ||						//	�e�L�X�g������EOL�̍s�͖���
				pcLayout->GetLogicOffset() > 0 ||				//	�܂�Ԃ��s�͖���
				pcLayout->GetLengthWithoutEOL() == 0 ){	//	���s�݂̂̍s�͖�������B
				continue;
			}

			/* �J�[�\�����ړ� */
			GetCaret().MoveCursor( CLayoutPoint(CLayoutInt(0), i), false );
			GetCaret().m_nCaretPosX_Prev = GetCaret().GetCaretLayoutPos().GetX2();

			/* ���݈ʒu�Ƀf�[�^��}�� */
			m_pCommanderView->InsertData_CEditView(
				CLayoutPoint(CLayoutInt(0),i),
				!bSoftTab? pData: stabData,
				!bSoftTab? nDataLen: stabData.Len(CLayoutInt(0)),
				&ptInserted,
				false
			);
			/* �J�[�\�����ړ� */
			GetCaret().MoveCursor( ptInserted, false );
			GetCaret().m_nCaretPosX_Prev = GetCaret().GetCaretLayoutPos().GetX2();

			if ( nLineCountPrev != GetDocument()->m_cLayoutMgr.GetLineCount() ){
				//	�s�����ω�����!!
				sSelectOld.GetToPointer()->y += GetDocument()->m_cLayoutMgr.GetLineCount() - nLineCountPrev;
			}
			if( hwndProgress ){
				int newPos = ::MulDiv((Int)i, 100, (Int)sSelectOld.GetTo().GetY());
				if( newPos != nProgressPos ){
					nProgressPos = newPos;
					Progress_SetPos( hwndProgress, newPos + 1 );
					Progress_SetPos( hwndProgress, newPos );
				}
			}
		}

		if( hwndProgress ){
			::ShowWindow( hwndProgress, SW_HIDE );
		}

		GetSelect() = sSelectOld;

		// From Here 2001.12.03 hor
		GetCaret().MoveCursor( GetSelect().GetTo(), true );
		GetCaret().m_nCaretPosX_Prev = GetCaret().GetCaretLayoutPos().GetX2();
		if( !m_pCommanderView->m_bDoing_UndoRedo ){	/* �A���h�D�E���h�D�̎��s���� */
			GetOpeBlk()->AppendOpe(
				new CMoveCaretOpe(
					GetCaret().GetCaretLogicPos()	// ����O��̃L�����b�g�ʒu
				)
			);
		}
		// To Here 2001.12.03 hor
	}
	/* �ĕ`�� */
	m_pCommanderView->SetDrawSwitch(bDrawSwitchOld);	// 2002.01.25 hor
	m_pCommanderView->RedrawAll();			// 2002.01.25 hor	// 2009.07.25 ryoji Redraw()->RedrawAll()
	return;
}



/* �t�C���f���g */
void CViewCommander::Command_UNINDENT( wchar_t wcChar )
{
	//	Aug. 9, 2003 genta
	//	�I������Ă��Ȃ��ꍇ�ɋt�C���f���g�����ꍇ��
	//	���Ӄ��b�Z�[�W���o��
	if( !m_pCommanderView->GetSelectionInfo().IsTextSelected() ){	/* �e�L�X�g���I������Ă��邩 */
		EIndentType eIndent;
		switch( wcChar ){
		case WCODE::TAB:
			eIndent = INDENT_TAB;	// ��[SPACE�̑}��]�I�v�V������ ON �Ȃ�\�t�g�^�u�ɂ���iWiki BugReport/66�j
			break;
		case WCODE::SPACE:
			eIndent = INDENT_SPACE;
			break;
		default:
			eIndent = INDENT_NONE;
		}
		Command_INDENT( wcChar, eIndent );
		m_pCommanderView->SendStatusMessage(LS(STR_ERR_UNINDENT1));
		return;
	}

	/* ��`�͈͑I�𒆂� */
	if( m_pCommanderView->GetSelectionInfo().IsBoxSelecting() ){
		ErrorBeep();
//**********************************************
//	 ���^�t�C���f���g�ɂ��ẮA�ۗ��Ƃ��� (1998.10.22)
//**********************************************
	}
	else{
		GetDocument()->m_cDocEditor.SetModified(true,true);	//	Jan. 22, 2002 genta

		CLayoutRange sSelectOld;	//�͈͑I��
		sSelectOld.SetFrom(CLayoutPoint(CLayoutInt(0),GetSelect().GetFrom().y));
		sSelectOld.SetTo  (CLayoutPoint(CLayoutInt(0),GetSelect().GetTo().y  ));
		if( GetSelect().GetTo().x > 0 ){
			sSelectOld.GetToPointer()->y++;
		}

		/* ���݂̑I��͈͂��I����Ԃɖ߂� */
		m_pCommanderView->GetSelectionInfo().DisableSelectArea( false );

		CWaitCursor cWaitCursor( m_pCommanderView->GetHwnd(), 1000 < sSelectOld.GetTo().GetY() - sSelectOld.GetFrom().GetY() );
		HWND hwndProgress = NULL;
		int nProgressPos = 0;
		if( cWaitCursor.IsEnable() ){
			hwndProgress = m_pCommanderView->StartProgress();
		}

		CLogicInt		nDelLen;
		for( CLayoutInt i = sSelectOld.GetFrom().GetY2(); i < sSelectOld.GetTo().GetY2(); i++ ){
			CLayoutInt nLineCountPrev = GetDocument()->m_cLayoutMgr.GetLineCount();

			const CLayout*	pcLayout;
			CLogicInt		nLineLen;
			const wchar_t*	pLine = GetDocument()->m_cLayoutMgr.GetLineStr( i, &nLineLen, &pcLayout );
			if( NULL == pcLayout || pcLayout->GetLogicOffset() > 0 ){ //�܂�Ԃ��ȍ~�̍s�̓C���f���g�������s��Ȃ�
				continue;
			}

			if( WCODE::TAB == wcChar ){
				if( pLine[0] == wcChar ){
					nDelLen = CLogicInt(1);
				}
				else{
					//����锼�p�X�y�[�X�� (1�`�^�u����) -> nDelLen
					CLogicInt i;
					CLogicInt nTabSpaces = CLogicInt((Int)GetDocument()->m_cLayoutMgr.GetTabSpace());
					for( i = CLogicInt(0); i < nLineLen; i++ ){
						if( WCODE::SPACE != pLine[i] ){
							break;
						}
						//	Sep. 23, 2002 genta LayoutMgr�̒l���g��
						if( i >= nTabSpaces ){
							break;
						}
					}
					if( 0 == i ){
						continue;
					}
					nDelLen = i;
				}
			}
			else{
				if( pLine[0] != wcChar ){
					continue;
				}
				nDelLen = CLogicInt(1);
			}

			/* �J�[�\�����ړ� */
			GetCaret().MoveCursor( CLayoutPoint(CLayoutInt(0), i), false );
			GetCaret().m_nCaretPosX_Prev = GetCaret().GetCaretLayoutPos().GetX2();

			/* �w��ʒu�̎w�蒷�f�[�^�폜 */
			m_pCommanderView->DeleteData2(
				CLayoutPoint(CLayoutInt(0),i),
				nDelLen,	// 2001.12.03 hor
				NULL
			);
			if ( nLineCountPrev != GetDocument()->m_cLayoutMgr.GetLineCount() ){
				//	�s�����ω�����!!
				sSelectOld.GetToPointer()->y += GetDocument()->m_cLayoutMgr.GetLineCount() - nLineCountPrev;
			}
			if( hwndProgress ){
				int newPos = ::MulDiv((Int)i, 100, (Int)sSelectOld.GetTo().GetY());
				if( newPos != nProgressPos ){
					nProgressPos = newPos;
					Progress_SetPos( hwndProgress, newPos + 1 );
					Progress_SetPos( hwndProgress, newPos );
				}
			}
		}
		if( hwndProgress ){
			::ShowWindow( hwndProgress, SW_HIDE );
		}
		GetSelect() = sSelectOld;	//�͈͑I��

		// From Here 2001.12.03 hor
		GetCaret().MoveCursor( GetSelect().GetTo(), true );
		GetCaret().m_nCaretPosX_Prev = GetCaret().GetCaretLayoutPos().GetX2();
		if( !m_pCommanderView->m_bDoing_UndoRedo ){	/* �A���h�D�E���h�D�̎��s���� */
			GetOpeBlk()->AppendOpe(
				new CMoveCaretOpe(
					GetCaret().GetCaretLogicPos()	// ����O��̃L�����b�g�ʒu
				)
			);
		}
		// To Here 2001.12.03 hor
	}

	/* �ĕ`�� */
	m_pCommanderView->RedrawAll();	// 2002.01.25 hor	// 2009.07.25 ryoji Redraw()->RedrawAll()
}



//	from CViewCommander_New.cpp
/*! TRIM Step1
	��I�����̓J�����g�s��I������ m_pCommanderView->ConvSelectedArea �� ConvMemory ��
	@author hor
	@date 2001.12.03 hor �V�K�쐬
*/
void CViewCommander::Command_TRIM(
	BOOL bLeft	//!<  [in] FALSE: �ETRIM / ����ȊO: ��TRIM
)
{
	bool bBeDisableSelectArea = false;
	CViewSelect& cViewSelect = m_pCommanderView->GetSelectionInfo();

	if(!cViewSelect.IsTextSelected()){	//	��I�����͍s�I���ɕύX
		cViewSelect.m_sSelect.SetFrom(
			CLayoutPoint(
				CLayoutInt(0),
				GetCaret().GetCaretLayoutPos().GetY()
			)
		);
		cViewSelect.m_sSelect.SetTo  (
			CLayoutPoint(
				GetDocument()->m_cLayoutMgr.GetMaxLineKetas(),
				GetCaret().GetCaretLayoutPos().GetY()
			)
		);
		bBeDisableSelectArea = true;
	}

	if(bLeft){
		m_pCommanderView->ConvSelectedArea( F_LTRIM );
	}
	else{
		m_pCommanderView->ConvSelectedArea( F_RTRIM );
	}

	if(bBeDisableSelectArea)
		cViewSelect.DisableSelectArea( true );
}



//	from CViewCommander_New.cpp
/*!	�����s�̃\�[�g�Ɏg���\����*/
struct SORTDATA {
	const CNativeW* pCmemLine;
	CStringRef sKey;
};

inline int CNativeW_comp(const CNativeW& lhs, const CNativeW& rhs )
{
	// ��r���ɂ͏I�[NUL���܂߂Ȃ��Ƃ����Ȃ�
	return wmemcmp(lhs.GetStringPtr(), rhs.GetStringPtr(),
			t_min(lhs.GetStringLength() + 1, rhs.GetStringLength() + 1));
}

/*!	�����s�̃\�[�g�Ɏg���֐�(����) */
bool SortByLineAsc (SORTDATA* pst1, SORTDATA* pst2) {return CNativeW_comp(*pst1->pCmemLine, *pst2->pCmemLine) < 0;}

/*!	�����s�̃\�[�g�Ɏg���֐�(�~��) */
bool SortByLineDesc(SORTDATA* pst1, SORTDATA* pst2) {return CNativeW_comp(*pst1->pCmemLine, *pst2->pCmemLine) > 0;}

inline int CStringRef_comp(const CStringRef& c1, const CStringRef& c2)
{
	int ret = wmemcmp(c1.GetPtr(), c2.GetPtr(), t_min(c1.GetLength(), c2.GetLength()));
	if( ret == 0 ){
		return c1.GetLength() - c2.GetLength();
	}
	return ret;
}

/*!	�����s�̃\�[�g�Ɏg���֐�(����) */
bool SortByKeyAsc(SORTDATA* pst1, SORTDATA* pst2)  {return CStringRef_comp(pst1->sKey, pst2->sKey) < 0 ;}

/*!	�����s�̃\�[�g�Ɏg���֐�(�~��) */
bool SortByKeyDesc(SORTDATA* pst1, SORTDATA* pst2) {return CStringRef_comp(pst1->sKey, pst2->sKey) > 0 ;}

/*!	@brief �����s�̃\�[�g

	��I�����͉������s���Ȃ��D��`�I�����́A���͈̔͂��L�[�ɂ��ĕ����s���\�[�g�D
	
	@note �Ƃ肠�������s�R�[�h���܂ރf�[�^���\�[�g���Ă���̂ŁA
	�t�@�C���̍ŏI�s�̓\�[�g�ΏۊO�ɂ��Ă��܂�
	@author hor
	@date 2001.12.03 hor �V�K�쐬
	@date 2001.12.21 hor �I��͈͂̒������W�b�N�����
	@date 2010.07.27 �s�\�[�g�ŃR�s�[�����炷/NUL��������r�ΏƂ�
	@date 2013.06.19 Moca ��`�I�����ŏI�s�ɉ��s���Ȃ��ꍇ�͕t��+�\�[�g��̍ŏI�s�̉��s���폜
*/
void CViewCommander::Command_SORT(BOOL bAsc)	//bAsc:TRUE=����,FALSE=�~��
{
	CLayoutRange sRangeA;
	CLogicRange sSelectOld;

	int			nColumnFrom, nColumnTo;
	CLayoutInt	nCF(0), nCT(0);
	CLayoutInt	nCaretPosYOLD;
	bool		bBeginBoxSelectOld;
	const wchar_t*	pLine;
	CLogicInt		nLineLen;
	int			j;
	std::vector<SORTDATA*> sta;

	if( !m_pCommanderView->GetSelectionInfo().IsTextSelected() ){			/* �e�L�X�g���I������Ă��邩 */
		return;
	}

	if( m_pCommanderView->GetSelectionInfo().IsBoxSelecting() ){
		sRangeA=m_pCommanderView->GetSelectionInfo().m_sSelect;
		if( m_pCommanderView->GetSelectionInfo().m_sSelect.GetFrom().x==m_pCommanderView->GetSelectionInfo().m_sSelect.GetTo().x ){
			//	Aug. 14, 2005 genta �܂�Ԃ�����LayoutMgr����擾����悤��
			m_pCommanderView->GetSelectionInfo().m_sSelect.SetToX( GetDocument()->m_cLayoutMgr.GetMaxLineKetas() );
		}
		if(m_pCommanderView->GetSelectionInfo().m_sSelect.GetFrom().x<m_pCommanderView->GetSelectionInfo().m_sSelect.GetTo().x){
			nCF=m_pCommanderView->GetSelectionInfo().m_sSelect.GetFrom().GetX2();
			nCT=m_pCommanderView->GetSelectionInfo().m_sSelect.GetTo().GetX2();
		}else{
			nCF=m_pCommanderView->GetSelectionInfo().m_sSelect.GetTo().GetX2();
			nCT=m_pCommanderView->GetSelectionInfo().m_sSelect.GetFrom().GetX2();
		}
	}
	bBeginBoxSelectOld=m_pCommanderView->GetSelectionInfo().IsBoxSelecting();
	nCaretPosYOLD=GetCaret().GetCaretLayoutPos().GetY();
	GetDocument()->m_cLayoutMgr.LayoutToLogic(
		m_pCommanderView->GetSelectionInfo().m_sSelect,
		&sSelectOld
	);

	if( bBeginBoxSelectOld ){
		sSelectOld.GetToPointer()->y++;
	}
	else{
		// �J�[�\���ʒu���s������Ȃ� �� �I��͈͂̏I�[�ɉ��s�R�[�h������ꍇ��
		// ���̍s���I��͈͂ɉ�����
		if ( sSelectOld.GetTo().x > 0 ) {
			// 2006.03.31 Moca nSelectLineToOld�́A�����s�Ȃ̂�Layout�n����DocLine�n�ɏC��
			const CDocLine* pcDocLine = GetDocument()->m_cDocLineMgr.GetLine( sSelectOld.GetTo().GetY2() );
			if( NULL != pcDocLine && EOL_NONE != pcDocLine->GetEol() ){
				sSelectOld.GetToPointer()->y++;
			}
		}
	}
	sSelectOld.SetFromX(CLogicInt(0));
	sSelectOld.SetToX(CLogicInt(0));

	//�s�I������ĂȂ�
	if(sSelectOld.IsLineOne()){
		return;
	}

	sta.reserve(sSelectOld.GetTo().GetY2() - sSelectOld.GetFrom().GetY2() );
	for( CLogicInt i = sSelectOld.GetFrom().GetY2(); i < sSelectOld.GetTo().y; i++ ){
		const CDocLine* pcDocLine = GetDocument()->m_cDocLineMgr.GetLine( i );
		const CNativeW& cmemLine = pcDocLine->_GetDocLineDataWithEOL();
		pLine = cmemLine.GetStringPtr(&nLineLen);
		CLogicInt nLineLenWithoutEOL = pcDocLine->GetLengthWithoutEOL();
		if( NULL == pLine ) continue;
		SORTDATA* pst = new SORTDATA;
		if( bBeginBoxSelectOld ){
			nColumnFrom = m_pCommanderView->LineColumnToIndex( pcDocLine, nCF );
			nColumnTo   = m_pCommanderView->LineColumnToIndex( pcDocLine, nCT );
			if(nColumnTo<nLineLenWithoutEOL){	// BOX�I��͈͂̉E�[���s���Ɏ��܂��Ă���ꍇ
				// 2006.03.31 genta std::string::assign���g���Ĉꎞ�ϐ��폜
				pst->sKey = CStringRef( &pLine[nColumnFrom], nColumnTo-nColumnFrom );
			}else if(nColumnFrom<nLineLenWithoutEOL){	// BOX�I��͈͂̉E�[���s�����E�ɂ͂ݏo���Ă���ꍇ
				pst->sKey = CStringRef( &pLine[nColumnFrom], nLineLenWithoutEOL-nColumnFrom );
			}else{
				// �I��͈͂̍��[���͂ݏo���Ă���==�f�[�^�Ȃ�
				pst->sKey = CStringRef( L"", 0 );
			}
		}
		pst->pCmemLine = &cmemLine;
		sta.push_back(pst);
	}
	const wchar_t* pStrLast = NULL; // �Ō�̍s�ɉ��s���Ȃ���΂��̃|�C���^
	if( 0 < sta.size() ){
		pStrLast = sta[sta.size()-1]->pCmemLine->GetStringPtr();
		int nlen = sta[sta.size()-1]->pCmemLine->GetStringLength();
		if( 0 < nlen ){
			if( WCODE::IsLineDelimiter(pStrLast[nlen-1], GetDllShareData().m_Common.m_sEdit.m_bEnableExtEol) ){
				pStrLast = NULL;
			}
		}
	}
	if( bBeginBoxSelectOld ){
		if(bAsc){
			std::stable_sort(sta.begin(), sta.end(), SortByKeyAsc);
		}else{
			std::stable_sort(sta.begin(), sta.end(), SortByKeyDesc);
		}
	}else{
		if(bAsc){
			std::stable_sort(sta.begin(), sta.end(), SortByLineAsc);
		}else{
			std::stable_sort(sta.begin(), sta.end(), SortByLineDesc);
		}
	}
	COpeLineData repData;
	j=(int)sta.size();
	repData.resize(sta.size());
	int opeSeq = GetDocument()->m_cDocEditor.m_cOpeBuf.GetNextSeq();
	for (int i=0; i<j; i++){
		repData[i].nSeq = opeSeq;
		repData[i].cmemLine.SetString( sta[i]->pCmemLine->GetStringPtr(), sta[i]->pCmemLine->GetStringLength() );
		if( pStrLast == sta[i]->pCmemLine->GetStringPtr() ){
			// ���ŏI�s�ɉ��s���Ȃ��̂ł���
			CEol cWork = GetDocument()->m_cDocEditor.GetNewLineCode();
			repData[i].cmemLine.AppendString( cWork.GetValue2(), cWork.GetLen() );
		}
	}
	if( pStrLast ){
		// �ŏI�s�̉��s���폜
		CLineData& lastData = repData[repData.size()-1];
		int nLen = lastData.cmemLine.GetStringLength();
		bool bExtEol = GetDllShareData().m_Common.m_sEdit.m_bEnableExtEol;
		while( 0 <nLen && WCODE::IsLineDelimiter(lastData.cmemLine[nLen-1], bExtEol) ){
			nLen--;
		}
		lastData.cmemLine._SetStringLength(nLen);
	}
	{
		// 2016.03.04 Moca sta���f�[�^�̍폜�Y��C��
		int nSize = (int)sta.size();
		for(int k = 0; k < nSize; k++){
			delete sta[k];
		}
	}

	CLayoutRange sSelectOld_Layout;
	GetDocument()->m_cLayoutMgr.LogicToLayout(sSelectOld, &sSelectOld_Layout);
	m_pCommanderView->ReplaceData_CEditView3(
		sSelectOld_Layout,
		NULL,
		&repData,
		false,
		m_pCommanderView->m_bDoing_UndoRedo?NULL:GetOpeBlk(),
		opeSeq,
		NULL
	);

	//	�I���G���A�̕���
	if(bBeginBoxSelectOld){
		m_pCommanderView->GetSelectionInfo().SetBoxSelect(bBeginBoxSelectOld);
		m_pCommanderView->GetSelectionInfo().m_sSelect=sRangeA;
	}else{
		m_pCommanderView->GetSelectionInfo().m_sSelect=sSelectOld_Layout;
	}
	if(nCaretPosYOLD==m_pCommanderView->GetSelectionInfo().m_sSelect.GetFrom().y || m_pCommanderView->GetSelectionInfo().IsBoxSelecting() ) {
		GetCaret().MoveCursor( m_pCommanderView->GetSelectionInfo().m_sSelect.GetFrom(), true );
	}else{
		GetCaret().MoveCursor( m_pCommanderView->GetSelectionInfo().m_sSelect.GetTo(), true );
	}
	GetCaret().m_nCaretPosX_Prev = GetCaret().GetCaretLayoutPos().GetX();
	if( !m_pCommanderView->m_bDoing_UndoRedo ){	/* �A���h�D�E���h�D�̎��s���� */
		GetOpeBlk()->AppendOpe(
			new CMoveCaretOpe(
				GetCaret().GetCaretLogicPos()	// ����O��̃L�����b�g�ʒu
			)
		);
	}
	m_pCommanderView->RedrawAll();
}



//	from CViewCommander_New.cpp
/*! @brief �����s�̃}�[�W

	�A�����镨���s�œ��e������̕���1�s�ɂ܂Ƃ߂܂��D
	
	��`�I�����͂Ȃɂ����s���܂���D
	
	@note ���s�R�[�h���܂ރf�[�^���r���Ă���̂ŁA
	�t�@�C���̍ŏI�s�̓\�[�g�ΏۊO�ɂ��Ă��܂�
	
	@author hor
	@date 2001.12.03 hor �V�K�쐬
	@date 2001.12.21 hor �I��͈͂̒������W�b�N�����
*/
void CViewCommander::Command_MERGE(void)
{
	CLayoutInt		nCaretPosYOLD;
	const wchar_t*	pLinew;
	CLogicInt		nLineLen;
	int			j;
	CLayoutInt		nMergeLayoutLines;

	if( !m_pCommanderView->GetSelectionInfo().IsTextSelected() ){			/* �e�L�X�g���I������Ă��邩 */
		return;
	}
	if( m_pCommanderView->GetSelectionInfo().IsBoxSelecting() ){
		return;
	}

	nCaretPosYOLD=GetCaret().GetCaretLayoutPos().GetY();
	CLogicRange sSelectOld; //�͈͑I��
	GetDocument()->m_cLayoutMgr.LayoutToLogic(
		m_pCommanderView->GetSelectionInfo().m_sSelect,
		&sSelectOld
	);

	// 2001.12.21 hor
	// �J�[�\���ʒu���s������Ȃ� �� �I��͈͂̏I�[�ɉ��s�R�[�h������ꍇ��
	// ���̍s���I��͈͂ɉ�����
	if ( sSelectOld.GetTo().x > 0 ) {
#if 0
		const CLayout* pcLayout=GetDocument()->m_cLayoutMgr.SearchLineByLayoutY(m_pCommanderView->GetSelectionInfo().m_sSelect.GetTo().GetY2()); //2007.10.09 kobake �P�ʍ��݃o�O�C��
		if( NULL != pcLayout && EOL_NONE != pcLayout->GetLayoutEol() ){
			sSelectOld.GetToPointer()->y++;
			//sSelectOld.GetTo().y++;
		}
#else
		// 2010.08.22 Moca �\�[�g�Ǝd�l�����킹��
		const CDocLine* pcDocLine = GetDocument()->m_cDocLineMgr.GetLine( sSelectOld.GetTo().GetY2() );
		if( NULL != pcDocLine && EOL_NONE != pcDocLine->GetEol() ){
			sSelectOld.GetToPointer()->y++;
		}
#endif
	}

	sSelectOld.SetFromX(CLogicInt(0));
	sSelectOld.SetToX(CLogicInt(0));

	//�s�I������ĂȂ�
	if(sSelectOld.IsLineOne()){
		return;
	}

	j=GetDocument()->m_cDocLineMgr.GetLineCount();
	nMergeLayoutLines = GetDocument()->m_cLayoutMgr.GetLineCount();

	CLayoutRange sSelectOld_Layout;
	GetDocument()->m_cLayoutMgr.LogicToLayout(sSelectOld, &sSelectOld_Layout);

	// 2010.08.22 NUL�Ή��C��
	std::vector<CStringRef> lineArr;
	pLinew=NULL;
	int nLineLenw = 0;
	bool bMerge = false;
	lineArr.reserve(sSelectOld.GetTo().y - sSelectOld.GetFrom().GetY2());
	for( CLogicInt i = sSelectOld.GetFrom().GetY2(); i < sSelectOld.GetTo().y; i++ ){
		const wchar_t*	pLine = GetDocument()->m_cDocLineMgr.GetLine(i)->GetDocLineStrWithEOL(&nLineLen);
		if( NULL == pLine ) continue;
		if( NULL == pLinew || nLineLen != nLineLenw || wmemcmp(pLine, pLinew, nLineLen) ){
			lineArr.push_back( CStringRef(pLine, nLineLen) );
		}else{
			bMerge = true;
		}
		pLinew=pLine;
		nLineLenw=nLineLen;
	}
	if( bMerge ){
		COpeLineData repData;
		int nSize = (int)lineArr.size();
		repData.resize(nSize);
		int opeSeq = GetDocument()->m_cDocEditor.m_cOpeBuf.GetNextSeq();
		for( int idx = 0; idx < nSize; idx++ ){
			repData[idx].nSeq = opeSeq;
			repData[idx].cmemLine.SetString( lineArr[idx].GetPtr(), lineArr[idx].GetLength() );
		}
		m_pCommanderView->ReplaceData_CEditView3(
			sSelectOld_Layout,
			NULL,
			&repData,
			false,
			m_pCommanderView->m_bDoing_UndoRedo?NULL:GetOpeBlk(),
			opeSeq,
			NULL
		);
	}else{
		// 2010.08.23 ���ύX�Ȃ�ύX���Ȃ�
	}

	j-=GetDocument()->m_cDocLineMgr.GetLineCount();
	nMergeLayoutLines -= GetDocument()->m_cLayoutMgr.GetLineCount();

	//	�I���G���A�̕���
	m_pCommanderView->GetSelectionInfo().m_sSelect=sSelectOld_Layout;
	// 2010.08.22 ���W���݃o�O
	m_pCommanderView->GetSelectionInfo().m_sSelect.GetToPointer()->y -= nMergeLayoutLines;

	if(nCaretPosYOLD==m_pCommanderView->GetSelectionInfo().m_sSelect.GetFrom().y){
		GetCaret().MoveCursor( m_pCommanderView->GetSelectionInfo().m_sSelect.GetFrom(), true );
	}else{
		GetCaret().MoveCursor( m_pCommanderView->GetSelectionInfo().m_sSelect.GetTo(), true );
	}
	GetCaret().m_nCaretPosX_Prev = GetCaret().GetCaretLayoutPos().GetX();
	if( !m_pCommanderView->m_bDoing_UndoRedo ){	/* �A���h�D�E���h�D�̎��s���� */
		GetOpeBlk()->AppendOpe(
			new CMoveCaretOpe(
				GetCaret().GetCaretLogicPos()	// ����O��̃L�����b�g�ʒu
			)
		);
	}
	m_pCommanderView->RedrawAll();

	if(j){
		TopOkMessage( m_pCommanderView->GetHwnd(), LS(STR_ERR_DLGEDITVWCMDNW7), j);
	}else{
		InfoMessage( m_pCommanderView->GetHwnd(), LS(STR_ERR_DLGEDITVWCMDNW8) );
	}
}



//	from CViewCommander_New.cpp
/* ���j���[����̍ĕϊ��Ή� minfu 2002.04.09

	@date 2002.04.11 YAZAKI COsVersionInfo�̃J�v�Z���������܂��傤�B
	@date 2010.03.17 ATOK�p��SCS_SETRECONVERTSTRING => ATRECONVERTSTRING_SET�ɕύX
		2002.11.20 Stonee����̏��
*/
void CViewCommander::Command_Reconvert(void)
{
	const int ATRECONVERTSTRING_SET = 1;

	//�T�C�Y���擾
	int nSize = m_pCommanderView->SetReconvertStruct(NULL,UNICODE_BOOL);
	if( 0 == nSize )  // �T�C�Y�O�̎��͉������Ȃ�
		return ;

	bool bUseUnicodeATOK = false;
	//�o�[�W�����`�F�b�N
	if( !OsSupportReconvert() ){
		
		// MSIME���ǂ���
		HWND hWnd = ImmGetDefaultIMEWnd(m_pCommanderView->GetHwnd());
		if (SendMessage(hWnd, m_pCommanderView->m_uWM_MSIME_RECONVERTREQUEST, FID_RECONVERT_VERSION, 0)){
			SendMessage(hWnd, m_pCommanderView->m_uWM_MSIME_RECONVERTREQUEST, 0, (LPARAM)m_pCommanderView->GetHwnd());
			return ;
		}

		// ATOK���g���邩�ǂ���
		TCHAR sz[256];
		ImmGetDescription(GetKeyboardLayout(0),sz,_countof(sz)); //�����̎擾
		if ( (_tcsncmp(sz,_T("ATOK"),4) == 0) && (NULL != m_pCommanderView->m_AT_ImmSetReconvertString) ){
			bUseUnicodeATOK = true;
		}else{
			//�Ή�IME�Ȃ�
			return;
		}
	}else{
		//���݂�IME���Ή����Ă��邩�ǂ���
		//IME�̃v���p�e�B
		if ( !(ImmGetProperty(GetKeyboardLayout(0),IGP_SETCOMPSTR) & SCS_CAP_SETRECONVERTSTRING) ){
			//�Ή�IME�Ȃ�
			return ;
		}
	}

	//�T�C�Y�擾������
	if (!UNICODE_BOOL && bUseUnicodeATOK) {
		nSize = m_pCommanderView->SetReconvertStruct(NULL,UNICODE_BOOL || bUseUnicodeATOK);
		if( 0 == nSize )  // �T�C�Y�O�̎��͉������Ȃ�
			return ;
	}

	//IME�̃R���e�L�X�g�擾
	HIMC hIMC = ::ImmGetContext( m_pCommanderView->GetHwnd() );
	
	//�̈�m��
	PRECONVERTSTRING pReconv = (PRECONVERTSTRING)::HeapAlloc(
		GetProcessHeap(),
		HEAP_GENERATE_EXCEPTIONS,
		nSize
	);
	
	//�\���̐ݒ�
	// Size�̓o�b�t�@�m�ۑ����ݒ�
	pReconv->dwSize = nSize;
	pReconv->dwVersion = 0;
	m_pCommanderView->SetReconvertStruct( pReconv, UNICODE_BOOL || bUseUnicodeATOK);
	
	//�ϊ��͈͂̒���
	if(bUseUnicodeATOK){
		(*m_pCommanderView->m_AT_ImmSetReconvertString)(hIMC, ATRECONVERTSTRING_SET, pReconv, pReconv->dwSize);
	}else{
		::ImmSetCompositionString(hIMC, SCS_QUERYRECONVERTSTRING, pReconv, pReconv->dwSize, NULL,0);
	}

	//���������ϊ��͈͂�I������
	m_pCommanderView->SetSelectionFromReonvert(pReconv, UNICODE_BOOL || bUseUnicodeATOK);
	
	//�ĕϊ����s
	if(bUseUnicodeATOK){
		(*m_pCommanderView->m_AT_ImmSetReconvertString)(hIMC, ATRECONVERTSTRING_SET, pReconv, pReconv->dwSize);
	}else{
		::ImmSetCompositionString(hIMC, SCS_SETRECONVERTSTRING, pReconv, pReconv->dwSize, NULL, 0);
	}

	//�̈���
	::HeapFree(GetProcessHeap(),0,(LPVOID)pReconv);
	::ImmReleaseContext( m_pCommanderView->GetHwnd(), hIMC);
}