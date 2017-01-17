/*!	@file
	@brief �L�����b�g�̊Ǘ�

	@author	kobake
*/
/*
	Copyright (C) 2008, kobake, ryoji, Uchi
	Copyright (C) 2009, ryoji, nasukoji
	Copyright (C) 2010, ryoji, Moca
	Copyright (C) 2011, Moca, syat
	Copyright (C) 2012, ryoji, Moca
	Copyright (C) 2013, Moca, Uchi

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
#include <algorithm>
#include "view/CCaret.h"
#include "view/CEditView.h"
#include "view/CTextArea.h"
#include "view/CTextMetrics.h"
#include "view/CViewFont.h"
#include "view/CRuler.h"
#include "doc/CEditDoc.h"
#include "doc/layout/CLayout.h"
#include "mem/CMemoryIterator.h"
#include "charset/charcode.h"
#include "charset/CCodePage.h"
#include "charset/CCodeFactory.h"
#include "charset/CCodeBase.h"
#include "window/CEditWnd.h"

using namespace std;

#define SCROLLMARGIN_LEFT 4
#define SCROLLMARGIN_RIGHT 4
#define SCROLLMARGIN_NOMOVE 4

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                         �O���ˑ�                            //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //


inline int CCaret::GetHankakuDx() const
{
	return m_pEditView->GetTextMetrics().GetHankakuDx();
}

inline int CCaret::GetHankakuHeight() const
{
	return m_pEditView->GetTextMetrics().GetHankakuHeight();
}

inline int CCaret::GetHankakuDy() const
{
	return m_pEditView->GetTextMetrics().GetHankakuDy();
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                      CCaretUnderLine                        //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

/* �J�[�\���s�A���_�[���C����ON */
void CCaretUnderLine::CaretUnderLineON( bool bDraw, bool bPaintDraw )
{
	if( m_nLockCounter ) return;	//	���b�N����Ă����牽���ł��Ȃ��B
	m_pcEditView->CaretUnderLineON( bDraw, bPaintDraw, m_nUnderLineLockCounter != 0 );
}

/* �J�[�\���s�A���_�[���C����OFF */
void CCaretUnderLine::CaretUnderLineOFF( bool bDraw, bool bDrawPaint, bool bResetFlag )
{
	if( m_nLockCounter ) return;	//	���b�N����Ă����牽���ł��Ȃ��B
	m_pcEditView->CaretUnderLineOFF( bDraw, bDrawPaint, bResetFlag, m_nUnderLineLockCounter != 0 );
}




// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//               �R���X�g���N�^�E�f�X�g���N�^                  //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

CCaret::CCaret(CEditView* pEditView, const CEditDoc* pEditDoc)
: m_pEditView(pEditView)
, m_pEditDoc(pEditDoc)
, m_ptCaretPos_Layout(0,0)
, m_ptCaretPos_Logic(0,0)			// �J�[�\���ʒu (���s�P�ʍs�擪����̃o�C�g��(0�J�n), ���s�P�ʍs�̍s�ԍ�(0�J�n))
, m_sizeCaret(0,0)				// �L�����b�g�̃T�C�Y
, m_cUnderLine(pEditView)
{
	m_nCaretPosX_Prev = CLayoutInt(0);		/* �r���[���[����̃J�[�\�������O�̈ʒu(�O�I���W��) */

	m_crCaret = -1;				/* �L�����b�g�̐F */			// 2006.12.16 ryoji
	m_hbmpCaret = NULL;			/* �L�����b�g�p�r�b�g�}�b�v */	// 2006.11.28 ryoji
	m_bClearStatus = true;
	ClearCaretPosInfoCache();
}

CCaret::~CCaret()
{
	// �L�����b�g�p�r�b�g�}�b�v	// 2006.11.28 ryoji
	if( m_hbmpCaret != NULL )
		DeleteObject( m_hbmpCaret );
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                     �C���^�[�t�F�[�X                        //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

/*!	@brief �s���w��ɂ��J�[�\���ړ�

	�K�v�ɉ����ďc/���X�N���[��������D
	�����X�N���[���������ꍇ�͂��̍s����Ԃ��i���^���j�D
	
	@return �c�X�N���[���s��(��:��X�N���[��/��:���X�N���[��)

	@note �s���Ȉʒu���w�肳�ꂽ�ꍇ�ɂ͓K�؂ȍ��W�l��
		�ړ����邽�߁C�����ŗ^�������W�ƈړ���̍��W��
		�K��������v���Ȃ��D
	
	@note bScroll��false�̏ꍇ�ɂ̓J�[�\���ʒu�݈̂ړ�����D
		true�̏ꍇ�ɂ̓X�N���[���ʒu�����킹�ĕύX�����

	@note �����s�̍��E�ړ��̓A���_�[���C������x�����K�v�������̂�
		bUnderlineDoNotOFF���w�肷��ƍ������ł���.
		���l�ɓ������̏㉺�ړ���bVertLineDoNotOFF���w�肷���
		�J�[�\���ʒu�c���̏������Ȃ��č������ł���.

	@date 2001.10.20 deleted by novice AdjustScrollBar()���ĂԈʒu��ύX
	@date 2004.04.02 Moca �s�����L���ȍ��W�ɏC������̂������ɏ�������
	@date 2004.09.11 genta bDraw�X�C�b�`�͓���Ɩ��̂���v���Ă��Ȃ��̂�
		�ĕ`��X�C�b�`����ʈʒu�����X�C�b�`�Ɩ��̕ύX
	@date 2009.08.28 nasukoji	�e�L�X�g�܂�Ԃ��́u�܂�Ԃ��Ȃ��v�Ή�
	@date 2010.11.27 syat �A���_�[���C���A�c�����������Ȃ��t���O��ǉ�
*/
CLayoutInt CCaret::MoveCursor(
	CLayoutPoint	ptWk_CaretPos,		//!< [in] �ړ��惌�C�A�E�g�ʒu
	bool			bScroll,			//!< [in] true: ��ʈʒu�����L��  false: ��ʈʒu��������
	int				nCaretMarginRate,	//!< [in] �c�X�N���[���J�n�ʒu�����߂�l
	bool			bUnderLineDoNotOFF,	//!< [in] �A���_�[���C�����������Ȃ�
	bool			bVertLineDoNotOFF	//!< [in] �J�[�\���ʒu�c�����������Ȃ�
)
{
	// �X�N���[������
	CLayoutInt	nScrollRowNum = CLayoutInt(0);
	CLayoutInt	nScrollColNum = CLayoutInt(0);
	int		nCaretMarginY;
	CLayoutInt		nScrollMarginRight;
	CLayoutInt		nScrollMarginLeft;

	if( 0 >= m_pEditView->GetTextArea().m_nViewColNum ){
		return CLayoutInt(0);
	}

#if REI_OUTPUT_DEBUG_STRING
	::OutputDebugStringW(L"MoveCursor start.\n");
#endif // rei_

	if( m_pEditView->GetSelectionInfo().IsMouseSelecting() ){	// �͈͑I��
		nCaretMarginY = 0;
	}
	else{
		//	2001/10/20 novice
		nCaretMarginY = (Int)m_pEditView->GetTextArea().m_nViewRowNum / nCaretMarginRate;
		if( 1 > nCaretMarginY ){
			nCaretMarginY = 1;
		}
	}
	// 2004.04.02 Moca �s�����L���ȍ��W�ɏC������̂������ɏ�������
	GetAdjustCursorPos( &ptWk_CaretPos );
	m_pEditDoc->m_cLayoutMgr.LayoutToLogic(
		ptWk_CaretPos,
		&m_ptCaretPos_Logic	//�J�[�\���ʒu�B���W�b�N�P�ʁB
	);
	/* �L�����b�g�ړ� */
	SetCaretLayoutPos(ptWk_CaretPos);


	// �J�[�\���s�A���_�[���C����OFF
	bool bDrawPaint = ptWk_CaretPos.GetY2() != m_pEditView->m_nOldUnderLineYBg;
	m_cUnderLine.SetUnderLineDoNotOFF( bUnderLineDoNotOFF );
	m_cUnderLine.SetVertLineDoNotOFF( bVertLineDoNotOFF );
	m_cUnderLine.CaretUnderLineOFF( bScroll, bDrawPaint );	//	YAZAKI
	m_cUnderLine.SetUnderLineDoNotOFF( false );
	m_cUnderLine.SetVertLineDoNotOFF( false );
	
	// �����X�N���[���ʁi�������j�̎Z�o
	nScrollColNum = CLayoutInt(0);
#if REI_MOD_HORIZONTAL_SCR
  {
    static int margin_size = RegGetDword(L"HScrollMargin", 1);
    nScrollMarginRight = CLayoutInt(margin_size);
    nScrollMarginLeft = CLayoutInt(margin_size);
  }
#else
	nScrollMarginRight = CLayoutInt(SCROLLMARGIN_RIGHT);
	nScrollMarginLeft = CLayoutInt(SCROLLMARGIN_LEFT);
#endif

	// 2010.08.24 Moca ���������ꍇ�̃}�[�W���̒���
	{
		// �J�[�\�����^�񒆂ɂ���Ƃ��ɍ��E�ɂԂ�Ȃ��悤��
		int nNoMove = SCROLLMARGIN_NOMOVE;
		CLayoutInt a = ((m_pEditView->GetTextArea().m_nViewColNum) - nNoMove) / 2;
		CLayoutInt nMin = (2 <= a ? a : CLayoutInt(0)); // 1���ƑS�p�ړ��Ɏx�Ⴊ����̂�2�ȏ�
		nScrollMarginRight = t_min(nScrollMarginRight, nMin);
		nScrollMarginLeft  = t_min(nScrollMarginLeft,  nMin);
	}
	
	//	Aug. 14, 2005 genta �܂�Ԃ�����LayoutMgr����擾����悤��
	if( m_pEditDoc->m_cLayoutMgr.GetMaxLineKetas() > m_pEditView->GetTextArea().m_nViewColNum &&
		ptWk_CaretPos.GetX() > m_pEditView->GetTextArea().GetViewLeftCol() + m_pEditView->GetTextArea().m_nViewColNum - nScrollMarginRight ){
		nScrollColNum =
			( m_pEditView->GetTextArea().GetViewLeftCol() + m_pEditView->GetTextArea().m_nViewColNum - nScrollMarginRight ) - ptWk_CaretPos.GetX2();
#if REI_MOD_HORIZONTAL_SCR
		if (nScrollColNum != 0) {
		  static int scr_size = RegGetDword(L"HScrollSize", REI_MOD_HORIZONTAL_SCR);
      if (scr_size > 1) {
  			if (nScrollColNum < 0) {
  				nScrollColNum = -(-nScrollColNum + scr_size - 1) / scr_size * scr_size;
  			} else {
  				nScrollColNum = (nScrollColNum + scr_size - 1) / scr_size * scr_size;
  			}
  		}
		}
#endif // rei_
	}
	else if( 0 < m_pEditView->GetTextArea().GetViewLeftCol() &&
		ptWk_CaretPos.GetX() < m_pEditView->GetTextArea().GetViewLeftCol() + nScrollMarginLeft
	){
		nScrollColNum = m_pEditView->GetTextArea().GetViewLeftCol() + nScrollMarginLeft - ptWk_CaretPos.GetX2();
#if REI_MOD_HORIZONTAL_SCR
		if (nScrollColNum != 0) {
		  static int scr_size = RegGetDword(L"HScrollSize", REI_MOD_HORIZONTAL_SCR);
      if (scr_size > 1) {
  			if (nScrollColNum < 0) {
  				nScrollColNum = -(-nScrollColNum + scr_size - 1) / scr_size * scr_size;
  			} else {
  				nScrollColNum = (nScrollColNum + scr_size - 1) / scr_size * scr_size;
  			}
  	  }
		}
#endif // rei_
		if( 0 > m_pEditView->GetTextArea().GetViewLeftCol() - nScrollColNum ){
			nScrollColNum = m_pEditView->GetTextArea().GetViewLeftCol();
		}

	}

	// 2013.12.30 bScroll��OFF�̂Ƃ��͉��X�N���[�����Ȃ�
	if( bScroll ){
		m_pEditView->GetTextArea().SetViewLeftCol(m_pEditView->GetTextArea().GetViewLeftCol() - nScrollColNum);
	}else{
		nScrollColNum = 0;
	}

	//	From Here 2007.07.28 ���イ�� : �\���s����3�s�ȉ��̏ꍇ�̓�����P
	/* �����X�N���[���ʁi�s���j�̎Z�o */
										// ��ʂ��R�s�ȉ�
	if( m_pEditView->GetTextArea().m_nViewRowNum <= 3 ){
							// �ړ���́A��ʂ̃X�N���[�����C�����ォ�H�iup �L�[�j
		if( ptWk_CaretPos.y - m_pEditView->GetTextArea().GetViewTopLine() < nCaretMarginY ){
			if( ptWk_CaretPos.y < nCaretMarginY ){	//�P�s�ڂɈړ�
				nScrollRowNum = m_pEditView->GetTextArea().GetViewTopLine();
			}
			else if( m_pEditView->GetTextArea().m_nViewRowNum <= 1 ){	// ��ʂ��P�s
				nScrollRowNum = m_pEditView->GetTextArea().GetViewTopLine() - ptWk_CaretPos.y;
			}
#if !(0)	// COMMENT�ɂ���ƁA�㉺�̋󂫂����炵�Ȃ��ׁA�c�ړ���good�����A���ړ��̏ꍇ�㉺�ɂԂ��
			else if( m_pEditView->GetTextArea().m_nViewRowNum <= 2 ){	// ��ʂ��Q�s
				nScrollRowNum = m_pEditView->GetTextArea().GetViewTopLine() - ptWk_CaretPos.y;
			}
#endif
			else
			{						// ��ʂ��R�s
				nScrollRowNum = m_pEditView->GetTextArea().GetViewTopLine() - ptWk_CaretPos.y + 1;
			}
		}else
							// �ړ���́A��ʂ̍ő�s���|�Q��艺���H�idown �L�[�j
		if( ptWk_CaretPos.y - m_pEditView->GetTextArea().GetViewTopLine() >= (m_pEditView->GetTextArea().m_nViewRowNum - nCaretMarginY - 2) ){
			CLayoutInt ii = m_pEditDoc->m_cLayoutMgr.GetLineCount();
			if( ii - ptWk_CaretPos.y < nCaretMarginY + 1 &&
				ii - m_pEditView->GetTextArea().GetViewTopLine() < m_pEditView->GetTextArea().m_nViewRowNum ) {
			}
			else if( m_pEditView->GetTextArea().m_nViewRowNum <= 2 ){	// ��ʂ��Q�s�A�P�s
				nScrollRowNum = m_pEditView->GetTextArea().GetViewTopLine() - ptWk_CaretPos.y;
			}else{						// ��ʂ��R�s
				nScrollRowNum = m_pEditView->GetTextArea().GetViewTopLine() - ptWk_CaretPos.y + 1;
			}
		}
	}
	// �ړ���́A��ʂ̃X�N���[�����C�����ォ�H�iup �L�[�j
	else if( ptWk_CaretPos.y - m_pEditView->GetTextArea().GetViewTopLine() < nCaretMarginY ){
		if( ptWk_CaretPos.y < nCaretMarginY ){	//�P�s�ڂɈړ�
			nScrollRowNum = m_pEditView->GetTextArea().GetViewTopLine();
		}else{
			nScrollRowNum = -(ptWk_CaretPos.y - m_pEditView->GetTextArea().GetViewTopLine()) + nCaretMarginY;
		}
	}
	// �ړ���́A��ʂ̍ő�s���|�Q��艺���H�idown �L�[�j
	else if( ptWk_CaretPos.y - m_pEditView->GetTextArea().GetViewTopLine() >= m_pEditView->GetTextArea().m_nViewRowNum - nCaretMarginY - 2 ){
		CLayoutInt ii = m_pEditDoc->m_cLayoutMgr.GetLineCount();
		if( ii - ptWk_CaretPos.y < nCaretMarginY + 1 &&
			ii - m_pEditView->GetTextArea().GetViewTopLine() < m_pEditView->GetTextArea().m_nViewRowNum ) {
		}
		else{
			nScrollRowNum =
				-(ptWk_CaretPos.y - m_pEditView->GetTextArea().GetViewTopLine()) + (m_pEditView->GetTextArea().m_nViewRowNum - nCaretMarginY - 2);
		}
	}
#if REI_FIX_CURSOR_MOVE_FLICKER
	// �����E�����̃X�N���[�����Ȃ��Ƃ��͉�ʓ��̃J�[�\���ړ��Ƃ��ĕ`����s��
	bool isFinalDraw = false;
	bool oldDraw = m_pEditView->GetDrawSwitch();
	if (oldDraw) {
		if (nScrollColNum == 0 /* && nScrollRowNum == 0 */) {
			//m_pEditView->SetDrawSwitch(true);
		} else {
			isFinalDraw = true;
			m_pEditView->SetDrawSwitch(false);
		}
	}
#endif // rei_
	//	To Here 2007.07.28 ���イ��
	if( bScroll ){
		/* �X�N���[�� */
		if( t_abs( nScrollColNum ) >= m_pEditView->GetTextArea().m_nViewColNum ||
			t_abs( nScrollRowNum ) >= m_pEditView->GetTextArea().m_nViewRowNum ){
			m_pEditView->GetTextArea().OffsetViewTopLine(-nScrollRowNum);
			if( m_pEditView->GetDrawSwitch() ){
				m_pEditView->InvalidateRect( NULL );
				if( m_pEditView->m_pcEditWnd->GetMiniMap().GetHwnd() ){
					m_pEditView->MiniMapRedraw(true);
				}
			}
		}
		else if( nScrollRowNum != 0 || nScrollColNum != 0 ){
			RECT	rcClip;
			RECT	rcClip2;
			RECT	rcScroll;

			m_pEditView->GetTextArea().GenerateTextAreaRect(&rcScroll);
			if( nScrollRowNum > 0 ){
				rcScroll.bottom = m_pEditView->GetTextArea().GetAreaBottom() - (Int)nScrollRowNum * m_pEditView->GetTextMetrics().GetHankakuDy();
				m_pEditView->GetTextArea().OffsetViewTopLine(-nScrollRowNum);
				m_pEditView->GetTextArea().GenerateTopRect(&rcClip,nScrollRowNum);
			}
			else if( nScrollRowNum < 0 ){
				rcScroll.top = m_pEditView->GetTextArea().GetAreaTop() - (Int)nScrollRowNum * m_pEditView->GetTextMetrics().GetHankakuDy();
				m_pEditView->GetTextArea().OffsetViewTopLine(-nScrollRowNum);
				m_pEditView->GetTextArea().GenerateBottomRect(&rcClip,-nScrollRowNum);
			}

			if( nScrollColNum > 0 ){
				rcScroll.left = m_pEditView->GetTextArea().GetAreaLeft();
				rcScroll.right = m_pEditView->GetTextArea().GetAreaRight() - (Int)nScrollColNum * GetHankakuDx();
				m_pEditView->GetTextArea().GenerateLeftRect(&rcClip2, nScrollColNum);
			}
			else if( nScrollColNum < 0 ){
				rcScroll.left = m_pEditView->GetTextArea().GetAreaLeft() - (Int)nScrollColNum * GetHankakuDx();
				m_pEditView->GetTextArea().GenerateRightRect(&rcClip2, -nScrollColNum);
			}

			if( m_pEditView->GetDrawSwitch() ){
				m_pEditView->ScrollDraw(nScrollRowNum, nScrollColNum, rcScroll, rcClip, rcClip2);
				if( m_pEditView->m_pcEditWnd->GetMiniMap().GetHwnd() ){
					m_pEditView->MiniMapRedraw(false);
				}
			}
		}

		/* �X�N���[���o�[�̏�Ԃ��X�V���� */
		m_pEditView->AdjustScrollBars(); // 2001/10/20 novice
	}

#if REI_FIX_CURSOR_MOVE_FLICKER
	m_pEditView->SetDrawSwitch(oldDraw);
#endif // rei_

	// ���X�N���[��������������A���[���[�S�̂��ĕ`�� 2002.02.25 Add By KK
	if (nScrollColNum != 0 ){
		//����DispRuler�Ăяo�����ɍĕ`��B�ibDraw=false�̃P�[�X���l�������B�j
		m_pEditView->GetRuler().SetRedrawFlag();
	}

	/* �J�[�\���s�A���_�[���C����ON */
	//CaretUnderLineON( bDraw ); //2002.02.27 Del By KK �A���_�[���C���̂������ጸ
	if( bScroll ){
		/* �L�����b�g�̕\���E�X�V */
		ShowEditCaret();

		/* ���[���̍ĕ`�� */
		HDC		hdc = m_pEditView->GetDC();
		m_pEditView->GetRuler().DispRuler( hdc );
		m_pEditView->ReleaseDC( hdc );

		/* �A���_�[���C���̍ĕ`�� */
		m_cUnderLine.CaretUnderLineON(true, bDrawPaint);

		/* �L�����b�g�̍s���ʒu��\������ */
		ShowCaretPosInfo();

		//	Sep. 11, 2004 genta �����X�N���[���̊֐���
		//	bScroll == FALSE�̎��ɂ̓X�N���[�����Ȃ��̂ŁC���s���Ȃ�
		m_pEditView->SyncScrollV( -nScrollRowNum );	//	�������t�Ȃ̂ŕ������]���K�v
		m_pEditView->SyncScrollH( -nScrollColNum );	//	�������t�Ȃ̂ŕ������]���K�v

	}

// 02/09/18 �Ί��ʂ̋����\�� ai Start	03/02/18 ai mod S
	m_pEditView->DrawBracketPair( false );
	m_pEditView->SetBracketPairPos( true );
	m_pEditView->DrawBracketPair( true );
// 02/09/18 �Ί��ʂ̋����\�� ai End		03/02/18 ai mod E

#if REI_OUTPUT_DEBUG_STRING
	::OutputDebugStringW(L"MoveCursor finish.\n");
#endif // rei_

#if REI_FIX_CURSOR_MOVE_FLICKER
	if (isFinalDraw) {
		m_pEditView->Call_OnPaint(PAINT_LINENUMBER | PAINT_BODY, false);
	}
#endif // rei_

	return nScrollRowNum;

}


CLayoutInt CCaret::MoveCursorFastMode(
	const CLogicPoint&		ptWk_CaretPosLogic	//!< [in] �ړ��惍�W�b�N�ʒu
)
{
	// fastMode
	SetCaretLogicPos(ptWk_CaretPosLogic);
	return CLayoutInt(0);
}

/* �}�E�X���ɂ����W�w��ɂ��J�[�\���ړ�
|| �K�v�ɉ����ďc/���X�N���[��������
|| �����X�N���[���������ꍇ�͂��̍s����Ԃ�(���^��)
*/
//2007.09.11 kobake �֐����ύX: MoveCursorToPoint��MoveCursorToClientPoint
CLayoutInt CCaret::MoveCursorToClientPoint( const POINT& ptClientPos, bool test, CLayoutPoint* pCaretPosNew )
{
	CLayoutInt		nScrollRowNum;
	CLayoutPoint	ptLayoutPos;
	m_pEditView->GetTextArea().ClientToLayout(ptClientPos, &ptLayoutPos);

	int	dx = (ptClientPos.x - m_pEditView->GetTextArea().GetAreaLeft()) % ( m_pEditView->GetTextMetrics().GetHankakuDx() );

	nScrollRowNum = MoveCursorProperly( ptLayoutPos, true, test, pCaretPosNew, 1000, dx );
	if( !test ){
		m_nCaretPosX_Prev = GetCaretLayoutPos().GetX2();
	}
	return nScrollRowNum;
}
//_CARETMARGINRATE_CARETMARGINRATE_CARETMARGINRATE



/*! �������J�[�\���ʒu���Z�o����(EOF�ȍ~�̂�)
	@param pptPosXY [in,out] �J�[�\���̃��C�A�E�g���W
	@retval	TRUE ���W���C������
	@retval	FALSE ���W�͏C������Ȃ�����
	@note	EOF�̒��O�����s�łȂ��ꍇ�́A���̍s�Ɍ���EOF�ȍ~�ɂ��ړ��\
			EOF�����̍s�́A�擪�ʒu�̂ݐ������B
	@date 2004.04.02 Moca �֐���
*/
BOOL CCaret::GetAdjustCursorPos(
	CLayoutPoint* pptPosXY
)
{
	// 2004.03.28 Moca EOF�݂̂̃��C�A�E�g�s�́A0���ڂ̂ݗL��.EOF��艺�̍s�̂���ꍇ�́AEOF�ʒu�ɂ���
	CLayoutInt nLayoutLineCount = m_pEditDoc->m_cLayoutMgr.GetLineCount();

	CLayoutPoint ptPosXY2 = *pptPosXY;
	BOOL ret = FALSE;
	if( ptPosXY2.y >= nLayoutLineCount ){
		if( 0 < nLayoutLineCount ){
			ptPosXY2.y = nLayoutLineCount - 1;
			const CLayout* pcLayout = m_pEditDoc->m_cLayoutMgr.SearchLineByLayoutY( ptPosXY2.GetY2() );
			if( pcLayout->GetLayoutEol() == EOL_NONE ){
				ptPosXY2.x = m_pEditView->LineIndexToColumn( pcLayout, (CLogicInt)pcLayout->GetLengthWithEOL() );
				// [EOF]�̂ݐ܂�Ԃ��̂͂�߂�	// 2009.02.17 ryoji
				// ��������Ȃ� ptPosXY2.x �ɐ܂�Ԃ��s�C���f���g��K�p����̂��悢

				// EOF�����܂�Ԃ���Ă��邩
				//	Aug. 14, 2005 genta �܂�Ԃ�����LayoutMgr����擾����悤��
				//if( ptPosXY2.x >= m_pEditDoc->m_cLayoutMgr.GetMaxLineKetas() ){
				//	ptPosXY2.y++;
				//	ptPosXY2.x = CLayoutInt(0);
				//}
			}
			else{
				// EOF�����̍s
				ptPosXY2.y++;
				ptPosXY2.x = CLayoutInt(0);
			}
		}else{
			// ��̃t�@�C��
			ptPosXY2.Set(CLayoutInt(0), CLayoutInt(0));
		}
		if( *pptPosXY != ptPosXY2 ){
			*pptPosXY = ptPosXY2;
			ret = TRUE;
		}
	}
	return ret;
}

/* �L�����b�g�̕\���E�X�V */
void CCaret::ShowEditCaret()
{
	if( m_pEditView->m_bMiniMap ){
		return;
	}
	//�K�v�ȃC���^�[�t�F�[�X
	const CLayoutMgr* pLayoutMgr=&m_pEditDoc->m_cLayoutMgr;
	CommonSetting* pCommon=&GetDllShareData().m_Common;
	const STypeConfig* pTypes=&m_pEditDoc->m_cDocType.GetDocumentAttribute();


	using namespace WCODE;

	int				nIdxFrom;

/*
	�t�H�[�J�X�������Ƃ��ɓ����I�ɃL�����b�g�쐬����ƈÖٓI�ɃL�����b�g�j���i���j����Ă�
	�L�����b�g������im_nCaretWidth != 0�j�Ƃ������ƂɂȂ��Ă��܂��A�t�H�[�J�X���擾���Ă�
	�L�����b�g���o�Ă��Ȃ��Ȃ�ꍇ������
	�t�H�[�J�X�������Ƃ��̓L�����b�g���쐬�^�\�����Ȃ��悤�ɂ���

	���L�����b�g�̓X���b�h�ɂЂƂ����Ȃ̂ŗႦ�΃G�f�B�b�g�{�b�N�X���t�H�[�J�X�擾�����
	�@�ʌ`��̃L�����b�g�ɈÖٓI�ɍ����ւ����邵�t�H�[�J�X�������ΈÖٓI�ɔj�������

	2007.12.11 ryoji
	�h���b�O�A���h�h���b�v�ҏW���̓L�����b�g���K�v�ňÖٔj���̗v���������̂ŗ�O�I�ɕ\������
*/
	if( ::GetFocus() != m_pEditView->GetHwnd() && !m_pEditView->m_bDragMode ){
		m_sizeCaret.cx = 0;
		return;
	}
#if 1 // rei_ form 2.2.0.1
	// 2014.07.02 GetDrawSwitch������
	if( !m_pEditView->GetDrawSwitch() ){
		return;
	}
#endif // rei_

	// CalcCaretDrawPos�̂��߂�Caret�T�C�Y�����ݒ�
	int				nCaretWidth = 0;
	int				nCaretHeight = 0;
	if( 0 == pCommon->m_sGeneral.GetCaretType() ){
		nCaretHeight = GetHankakuHeight();
#if 0//REI_LINE_CENTERING // Caret�̍���
		nCaretHeight += m_pEditView->m_pTypeData->m_nLineSpace;
#endif // rei_
		if( m_pEditView->IsInsMode() ){
			nCaretWidth = 2;
		}else{
			nCaretWidth = GetHankakuDx();
		}
	}else if( 1 == pCommon->m_sGeneral.GetCaretType() ){
		if( m_pEditView->IsInsMode() ){
			nCaretHeight = GetHankakuHeight() / 2;
		}
		else{
			nCaretHeight = GetHankakuHeight();
		}
		nCaretWidth = GetHankakuDx();
	}
	CMySize caretSizeOld = GetCaretSize();
	SetCaretSize(nCaretWidth,nCaretHeight);
	POINT ptDrawPos=CalcCaretDrawPos(GetCaretLayoutPos());
	SetCaretSize(caretSizeOld.cx, caretSizeOld.cy); // ��Ŕ�r����̂Ŗ߂�
	bool bShowCaret = false;
	if ( m_pEditView->GetTextArea().GetAreaLeft() <= ptDrawPos.x && m_pEditView->GetTextArea().GetAreaTop() <= ptDrawPos.y
		&& ptDrawPos.x < m_pEditView->GetTextArea().GetAreaRight() && ptDrawPos.y < m_pEditView->GetTextArea().GetAreaBottom() ){
		// �L�����b�g�̕\��
		bShowCaret = true;
	}
	/* �L�����b�g�̕��A���������� */
	// �J�[�\���̃^�C�v = win
	if( 0 == pCommon->m_sGeneral.GetCaretType() ){
		nCaretHeight = GetHankakuHeight();					/* �L�����b�g�̍��� */
#if 0//REI_LINE_CENTERING // Caret�̍���
		nCaretHeight += m_pEditView->m_pTypeData->m_nLineSpace;
#endif // rei_
		if( m_pEditView->IsInsMode() /* Oct. 2, 2005 genta */ ){
#if REI_MOD_CARET
      static int caret_type = RegGetDword(L"CaretType", REI_MOD_CARET);
      if (caret_type == 11) {
  			nCaretWidth = 1;
  			
  			const wchar_t*	pLine;
  			CLogicInt		nLineLen;
  			const CLayout*	pcLayout;
  			pLine = pLayoutMgr->GetLineStr( GetCaretLayoutPos().GetY2(), &nLineLen, &pcLayout );

  			if (pLine) {
  				/* �w�肳�ꂽ���ɑΉ�����s�̃f�[�^���̈ʒu�𒲂ׂ� */
  				//nIdxFrom = m_pEditView->LineColmnToIndex( pcLayout, GetCaretLayoutPos().GetX2() );
  				nIdxFrom = GetCaretLogicPos().GetX2() - pcLayout->GetLogicOffset();
  				if (pLine[nIdxFrom] == TAB ||
  				    pLine[nIdxFrom] == CR || pLine[nIdxFrom] == LF ||
  				    nIdxFrom >= nLineLen) {
  					nCaretWidth = 1;//GetHankakuDx();
  				} else {
  					CLayoutInt nKeta = CNativeW::GetKetaOfChar( pLine, nLineLen, nIdxFrom );
  					if (0 < nKeta) {
  						nCaretWidth = 1 * (Int)nKeta;//GetHankakuDx() * (Int)nKeta;
  					}
  				}
  			}
  		} else if (caret_type == 12) {
  			if (m_pEditView->IsImeON()) {
  				nCaretWidth = 2;
  			} else {
  				nCaretWidth = 1;
  			}
  		} else if (1 <= caret_type && caret_type <= 10) {
  		  nCaretWidth = caret_type;
  		} else {
#endif // rei_
			nCaretWidth = 2; //2px
			// 2011.12.22 �V�X�e���̐ݒ�ɏ]��(����2px�ȏ�)
			DWORD dwWidth;
			if( ::SystemParametersInfo(SPI_GETCARETWIDTH, 0, &dwWidth, 0) && 2 < dwWidth){
				nCaretWidth = t_min((int)dwWidth, GetHankakuDx());
			}
#if REI_MOD_CARET
      }
#endif // rei_
		}
		else{
			nCaretWidth = GetHankakuDx();

			const wchar_t*	pLine = NULL;
			CLogicInt		nLineLen = CLogicInt(0);
			const CLayout*	pcLayout = NULL;
			if( bShowCaret ){
				// ��ʊO�̂Ƃ���GetLineStr���Ă΂Ȃ�
				pLine = pLayoutMgr->GetLineStr( GetCaretLayoutPos().GetY2(), &nLineLen, &pcLayout );
			}

			if( NULL != pLine ){
				/* �w�肳�ꂽ���ɑΉ�����s�̃f�[�^���̈ʒu�𒲂ׂ� */
				nIdxFrom = GetCaretLogicPos().GetX() - pcLayout->GetLogicOffset();
				if( nIdxFrom >= nLineLen ||
					WCODE::IsLineDelimiter(pLine[nIdxFrom], GetDllShareData().m_Common.m_sEdit.m_bEnableExtEol) ||
					pLine[nIdxFrom] == TAB ){
					nCaretWidth = GetHankakuDx();
				}
				else{
					CLayoutInt nKeta = CNativeW::GetKetaOfChar( pLine, nLineLen, nIdxFrom );
					if( 0 < nKeta ){
						nCaretWidth = GetHankakuDx() * (Int)nKeta;
					}
				}
			}
		}
	}
	// �J�[�\���̃^�C�v = dos
	else if( 1 == pCommon->m_sGeneral.GetCaretType() ){
		if( m_pEditView->IsInsMode() /* Oct. 2, 2005 genta */ ){
			nCaretHeight = GetHankakuHeight() / 2;			/* �L�����b�g�̍��� */
		}
		else{
			nCaretHeight = GetHankakuHeight();				/* �L�����b�g�̍��� */
		}
		nCaretWidth = GetHankakuDx();

		const wchar_t*	pLine = NULL;
		CLogicInt		nLineLen = CLogicInt(0);
		const CLayout*	pcLayout = NULL;
		if( bShowCaret ){
			pLine= pLayoutMgr->GetLineStr( GetCaretLayoutPos().GetY2(), &nLineLen, &pcLayout );
		}

		if( NULL != pLine ){
			/* �w�肳�ꂽ���ɑΉ�����s�̃f�[�^���̈ʒu�𒲂ׂ� */
			nIdxFrom = m_pEditView->LineColumnToIndex( pcLayout, GetCaretLayoutPos().GetX2() );
			if( nIdxFrom >= nLineLen ||
				WCODE::IsLineDelimiter(pLine[nIdxFrom], GetDllShareData().m_Common.m_sEdit.m_bEnableExtEol) ||
				pLine[nIdxFrom] == TAB ){
				nCaretWidth = GetHankakuDx();
			}else{
				CLayoutInt nKeta = CNativeW::GetKetaOfChar( pLine, nLineLen, nIdxFrom );
				if( 0 < nKeta ){
					nCaretWidth = GetHankakuDx() * (Int)nKeta;
				}
			}
		}
	}

	//	�L�����b�g�F�̎擾
	const ColorInfo* ColorInfoArr = pTypes->m_ColorInfoArr;
	int nCaretColor = ( ColorInfoArr[COLORIDX_CARET_IME].m_bDisp && m_pEditView->IsImeON() )? COLORIDX_CARET_IME: COLORIDX_CARET;
	COLORREF crCaret = ColorInfoArr[nCaretColor].m_sColorAttr.m_cTEXT;
	COLORREF crBack = ColorInfoArr[COLORIDX_TEXT].m_sColorAttr.m_cBACK;

	if( !ExistCaretFocus() ){
		/* �L�����b�g���Ȃ������ꍇ */
		/* �L�����b�g�̍쐬 */
		CreateEditCaret( crCaret, crBack, nCaretWidth, nCaretHeight );	// 2006.12.07 ryoji
		m_bCaretShowFlag = false; // 2002/07/22 novice
	}
	else{
		if( GetCaretSize() != CMySize(nCaretWidth,nCaretHeight) || m_crCaret != crCaret || m_pEditView->m_crBack2 != crBack ){
			/* �L�����b�g�͂��邪�A�傫����F���ς�����ꍇ */
			/* ���݂̃L�����b�g���폜 */
			::DestroyCaret();

			/* �L�����b�g�̍쐬 */
			CreateEditCaret( crCaret, crBack, nCaretWidth, nCaretHeight );	// 2006.12.07 ryoji
			m_bCaretShowFlag = false; // 2002/07/22 novice
		}
		else{
			/* �L�����b�g�͂��邵�A�傫�����ς���Ă��Ȃ��ꍇ */
			/* �L�����b�g���B�� */
			HideCaret_( m_pEditView->GetHwnd() ); // 2002/07/22 novice
		}
	}

	// �L�����b�g�T�C�Y
	SetCaretSize(nCaretWidth,nCaretHeight);

	/* �L�����b�g�̈ʒu�𒲐� */
	//2007.08.26 kobake �L�����b�gX���W�̌v�Z��UNICODE�d�l�ɂ����B
	::SetCaretPos( ptDrawPos.x, ptDrawPos.y );
	if ( bShowCaret ){
		/* �L�����b�g�̕\�� */
		ShowCaret_( m_pEditView->GetHwnd() ); // 2002/07/22 novice
	}

	m_crCaret = crCaret;	//	2006.12.07 ryoji
	m_pEditView->m_crBack2 = crBack;		//	2006.12.07 ryoji
	m_pEditView->SetIMECompFormPos();
	
#if REI_MOD_LINE_NR
	// �s�ԍ��̍ĕ`��
	m_pEditView->Call_OnPaint( PAINT_LINENUMBER, false );
#endif // rei_
}




/*! �L�����b�g�̍s���ʒu����уX�e�[�^�X�o�[�̏�ԕ\���̍X�V

	@note �X�e�[�^�X�o�[�̏�Ԃ̕��ѕ��̕ύX�̓��b�Z�[�W����M����
		CEditWnd::DispatchEvent()��WM_NOTIFY�ɂ��e�������邱�Ƃɒ���
	
	@note �X�e�[�^�X�o�[�̏o�͓��e�̕ύX��CEditWnd::OnSize()��
		�J�������v�Z�ɉe�������邱�Ƃɒ���
*/
//2007.10.17 kobake �d������R�[�h�𐮗�
void CCaret::ShowCaretPosInfo()
{
	//�K�v�ȃC���^�[�t�F�[�X
	const CLayoutMgr* pLayoutMgr=&m_pEditDoc->m_cLayoutMgr;
	const STypeConfig* pTypes=&m_pEditDoc->m_cDocType.GetDocumentAttribute();


	if( !m_pEditView->GetDrawSwitch() ){
		return;
	}

	// �X�e�[�^�X�o�[�n���h�����擾
	HWND hwndStatusBar = m_pEditDoc->m_pcEditWnd->m_cStatusBar.GetStatusHwnd();


	// �J�[�\���ʒu�̕�������擾
	const CLayout*	pcLayout;
	CLogicInt		nLineLen;
	const wchar_t*	pLine = pLayoutMgr->GetLineStr( GetCaretLayoutPos().GetY2(), &nLineLen, &pcLayout );


	// -- -- -- -- �����R�[�h��� -> pszCodeName -- -- -- -- //
	const TCHAR* pszCodeName;
	CNativeT cmemCodeName;
	if (hwndStatusBar) {
		TCHAR szCodeName[100];
		CCodePage::GetNameNormal(szCodeName, m_pEditDoc->GetDocumentEncoding());
		cmemCodeName.AppendString(szCodeName);
		if (m_pEditDoc->GetDocumentBomExist()) {
			cmemCodeName.AppendString( LS(STR_CARET_WITHBOM) );
		}
	}
	else {
		TCHAR szCodeName[100];
		CCodePage::GetNameShort(szCodeName, m_pEditDoc->GetDocumentEncoding());
		cmemCodeName.AppendString(szCodeName);
		if (m_pEditDoc->GetDocumentBomExist()) {
			cmemCodeName.AppendString( _T("#") );		// BOM�t(���j���[�o�[�Ȃ̂ŏ�����)	// 2013/4/17 Uchi
		}
	}
	pszCodeName = cmemCodeName.GetStringPtr();


	// -- -- -- -- ���s���[�h -> szEolMode -- -- -- -- //
	//	May 12, 2000 genta
	//	���s�R�[�h�̕\����ǉ�
	CEol cNlType = m_pEditDoc->m_cDocEditor.GetNewLineCode();
	const TCHAR* szEolMode = cNlType.GetName();


	// -- -- -- -- �L�����b�g�ʒu -> ptCaret -- -- -- -- //
	//
	CMyPoint ptCaret;
	//�s�ԍ������W�b�N�P�ʂŕ\��
	if(pTypes->m_bLineNumIsCRLF){
		ptCaret.x = 0;
		ptCaret.y = (Int)GetCaretLogicPos().y;
		if(pcLayout){
			// 2014.01.10 ���s�̂Ȃ��傫���s������ƒx���̂ŃL���b�V������
			CLayoutInt offset;
			if( m_nLineLogicNoCache == pcLayout->GetLogicLineNo()
				&& m_nLineNoCache == GetCaretLayoutPos().GetY2()
				&& m_nLineLogicModCache == CModifyVisitor().GetLineModifiedSeq( pcLayout->GetDocLineRef() ) ){
				offset = m_nOffsetCache;
			}else if( m_nLineLogicNoCache == pcLayout->GetLogicLineNo()
				&& m_nLineNoCache < GetCaretLayoutPos().GetY2()
				&& m_nLineLogicModCache == CModifyVisitor().GetLineModifiedSeq( pcLayout->GetDocLineRef() ) ){
				// ���ړ�
				offset = pcLayout->CalcLayoutOffset(*pLayoutMgr, m_nLogicOffsetCache, m_nOffsetCache);
				m_nOffsetCache = offset;
				m_nLogicOffsetCache = pcLayout->GetLogicOffset();
				m_nLineNoCache = GetCaretLayoutPos().GetY2();
			}else if(m_nLineLogicNoCache == pcLayout->GetLogicLineNo()
				&& m_nLineNo50Cache <= GetCaretLayoutPos().GetY2()
				&& GetCaretLayoutPos().GetY2() <= m_nLineNo50Cache + 50
				&& m_nLineLogicModCache == CModifyVisitor().GetLineModifiedSeq( pcLayout->GetDocLineRef() ) ){
				// ��ړ�
				offset = pcLayout->CalcLayoutOffset(*pLayoutMgr, m_nLogicOffset50Cache, m_nOffset50Cache);
				m_nOffsetCache = offset;
				m_nLogicOffsetCache = pcLayout->GetLogicOffset();
				m_nLineNoCache = GetCaretLayoutPos().GetY2();
			}else{
				// 2013.05.11 �܂�Ԃ��Ȃ��Ƃ��Čv�Z����
				const CLayout* pcLayout50 = pcLayout;
				CLayoutInt nLineNum = GetCaretLayoutPos().GetY2();
				for(;;){
					if( pcLayout50->GetLogicOffset() == 0 ){
						break;
					}
					if( nLineNum + 50 == GetCaretLayoutPos().GetY2() ){
						break;
					}
					pcLayout50 = pcLayout50->GetPrevLayout();
					nLineNum--;
				}
				m_nOffset50Cache = pcLayout50->CalcLayoutOffset(*pLayoutMgr);
				m_nLogicOffset50Cache = pcLayout50->GetLogicOffset();
				m_nLineNo50Cache = nLineNum;
				
				offset = pcLayout->CalcLayoutOffset(*pLayoutMgr, m_nLogicOffset50Cache, m_nOffset50Cache);
				m_nOffsetCache = offset;
				m_nLogicOffsetCache = pcLayout->GetLogicOffset();
				m_nLineLogicNoCache = pcLayout->GetLogicLineNo();
				m_nLineNoCache = GetCaretLayoutPos().GetY2();
				m_nLineLogicModCache = CModifyVisitor().GetLineModifiedSeq( pcLayout->GetDocLineRef() );
			}
			CLayout cLayout(
				pcLayout->GetDocLineRef(),
				pcLayout->GetLogicPos(),
				pcLayout->GetLengthWithEOL(),
				pcLayout->GetColorTypePrev(),
				offset,
				NULL
			);
			ptCaret.x = (Int)m_pEditView->LineIndexToColumn(&cLayout, GetCaretLogicPos().x - pcLayout->GetLogicPos().x);
		}
	}
	//�s�ԍ������C�A�E�g�P�ʂŕ\��
	else {
		ptCaret.x = (Int)GetCaretLayoutPos().GetX();
		ptCaret.y = (Int)GetCaretLayoutPos().GetY();
	}
	//�\���l��1����n�܂�悤�ɕ␳
	ptCaret.x++;
	ptCaret.y++;


	// -- -- -- -- �L�����b�g�ʒu�̕������ -> szCaretChar -- -- -- -- //
	//
	TCHAR szCaretChar[32]=_T("");
	if( pLine ){
		// �w�肳�ꂽ���ɑΉ�����s�̃f�[�^���̈ʒu�𒲂ׂ�
		CLogicInt nIdx = GetCaretLogicPos().GetX2() - pcLayout->GetLogicOffset();
		if( nIdx < nLineLen ){
			if( nIdx < nLineLen - (pcLayout->GetLayoutEol().GetLen()?1:0) ){
				//auto_sprintf( szCaretChar, _T("%04x"), );
				//�C�ӂ̕����R�[�h����Unicode�֕ϊ�����		2008/6/9 Uchi
				CCodeBase* pCode = CCodeFactory::CreateCodeBase(m_pEditDoc->GetDocumentEncoding(), false);
				CommonSetting_Statusbar* psStatusbar = &GetDllShareData().m_Common.m_sStatusbar;
				EConvertResult ret = pCode->UnicodeToHex(&pLine[nIdx], nLineLen - nIdx, szCaretChar, psStatusbar);
				delete pCode;
				if (ret != RESULT_COMPLETE) {
					// ���܂��R�[�h�����Ȃ�����(Unicode�ŕ\��)
					pCode = CCodeFactory::CreateCodeBase(CODE_UNICODE, false);
					/* EConvertResult ret = */ pCode->UnicodeToHex(&pLine[nIdx], nLineLen - nIdx, szCaretChar, psStatusbar);
					delete pCode;
				}
			}
			else{
				_tcscpy_s(szCaretChar, _countof(szCaretChar), pcLayout->GetLayoutEol().GetName());
			}
		}
	}


	// -- -- -- --  �X�e�[�^�X���������o�� -- -- -- -- //
	//
	// �E�B���h�E�E��ɏ����o��
	if( !hwndStatusBar ){
		TCHAR	szText[64];
		TCHAR	szFormat[64];
		TCHAR	szLeft[64];
		TCHAR	szRight[64];
		int		nLen;
		{	// ���b�Z�[�W�̍���������i�u�s:��v���������\���j
			nLen = _tcslen(pszCodeName) + _tcslen(szEolMode) + _tcslen(szCaretChar);
			// ����� %s(%s)%6s%s%s ���ɂȂ�B%6ts�\�L�͎g���Ȃ��̂Œ���
			auto_sprintf(
				szFormat,
				_T("%%s(%%s)%%%ds%%s%%s"),	// �u�L�����b�g�ʒu�̕������v���E�l�Ŕz�u�i����Ȃ��Ƃ��͍��l�ɂȂ��ĉE�ɐL�т�j
				(nLen < 15)? 15 - nLen: 1
			);
			auto_sprintf(
				szLeft,
				szFormat,
				pszCodeName,
				szEolMode,
				szCaretChar[0]? _T("["): _T(" "),	// ������񖳂��Ȃ犇�ʂ��ȗ��iEOF��t���[�J�[�\���ʒu�j
				szCaretChar,
				szCaretChar[0]? _T("]"): _T(" ")	// ������񖳂��Ȃ犇�ʂ��ȗ��iEOF��t���[�J�[�\���ʒu�j
			);
		}
		szRight[0] = _T('\0');
		nLen = MENUBAR_MESSAGE_MAX_LEN - _tcslen(szLeft);	// �E���Ɏc���Ă��镶����
		if( nLen > 0 ){	// ���b�Z�[�W�̉E��������i�u�s:��v�\���j
			TCHAR szRowCol[32];
			auto_sprintf(
				szRowCol,
				_T("%d:%-4d"),	// �u��v�͍ŏ������w�肵�č��񂹁i����Ȃ��Ƃ��͉E�ɐL�т�j
				ptCaret.y,
				ptCaret.x
			);
			auto_sprintf(
				szFormat,
				_T("%%%ds"),	// �u�s:��v���E�l�Ŕz�u�i����Ȃ��Ƃ��͍��l�ɂȂ��ĉE�ɐL�т�j
				nLen
			);
			auto_sprintf(
				szRight,
				szFormat,
				szRowCol
			);
		}
		auto_sprintf(
			szText,
			_T("%s%s"),
			szLeft,
			szRight
		);
		m_pEditDoc->m_pcEditWnd->PrintMenubarMessage( szText );
	}
	// �X�e�[�^�X�o�[�ɏ�Ԃ������o��
	else{
#if REI_MOD_STATUSBAR
		int columnCnt = 0;
		
		TCHAR	szText_1[64];
		int max = std::max(m_pEditView->m_pcEditDoc->m_cLayoutMgr.GetLineCount(), 1);
		int cur = std::max(m_pEditView->GetCaret().GetCaretLogicPos().y, 0);
		auto_sprintf( szText_1, LS( STR_STATUS_ROW_COL ), ptCaret.y, ptCaret.x );

		TCHAR	szText_6[16];
		if( m_pEditView->IsInsMode() /* Oct. 2, 2005 genta */ ){
			_tcscpy( szText_6, LS( STR_INS_MODE_INS ) );	// "�}��"
		}else{
			_tcscpy( szText_6, LS( STR_INS_MODE_OVR ) );	// "�㏑"
		}
//		if( m_bClearStatus ){
//			::StatusBar_SetText( hwndStatusBar, columnCnt | SBT_NOBORDERS, _T("") );
//		}
//		columnCnt++;
		if (m_pEditView->GetSelectionInfo().IsTextSelected()) {
			columnCnt++;
		} else {
			::StatusBar_SetText( hwndStatusBar, columnCnt++ | 0,             szText_1 );
		}
		// ID�����킹�邽�߂Ƀ^�u�T�C�Y���s���̈ʒu�ɕ\��
		TCHAR	szText_TabSize[16];
		auto_sprintf( szText_TabSize, _T("Tab Size: %d"), m_pEditView->m_pcEditDoc->m_cLayoutMgr.GetTabSpace() );
//-		::StatusBar_SetText( hwndStatusBar, columnCnt++ | 0,             szText_TabSize );
		::StatusBar_SetText( hwndStatusBar, columnCnt++ | 0,             _T("") );
		//	May 12, 2000 genta
		//	���s�R�[�h�̕\����ǉ��D���̔ԍ���1�����炷
		//	From Here
		::StatusBar_SetText( hwndStatusBar, columnCnt++ | 0,             szEolMode );
		//	To Here
		::StatusBar_SetText( hwndStatusBar, columnCnt++ | 0,             szCaretChar );
		::StatusBar_SetText( hwndStatusBar, columnCnt++ | 0,             pszCodeName );
		::StatusBar_SetText( hwndStatusBar, columnCnt++ | SBT_OWNERDRAW, _T("") );
		::StatusBar_SetText( hwndStatusBar, columnCnt++ | 0,             szText_6 );
		::StatusBar_SetText( hwndStatusBar, columnCnt++ | 0,             szText_TabSize );
		::StatusBar_SetText( hwndStatusBar, columnCnt++ | 0,             m_pEditView->m_pTypeData->m_szTypeName );
#else
		TCHAR	szText_1[64];
		auto_sprintf( szText_1, LS( STR_STATUS_ROW_COL ), ptCaret.y, ptCaret.x );	//Oct. 30, 2000 JEPRO �疜�s���v���

		TCHAR	szText_6[16];
		if( m_pEditView->IsInsMode() /* Oct. 2, 2005 genta */ ){
			_tcscpy( szText_6, LS( STR_INS_MODE_INS ) );	// "�}��"
		}else{
			_tcscpy( szText_6, LS( STR_INS_MODE_OVR ) );	// "�㏑"
		}
		if( m_bClearStatus ){
			::StatusBar_SetText( hwndStatusBar, 0 | SBT_NOBORDERS, _T("") );
		}
		::StatusBar_SetText( hwndStatusBar, 1 | 0,             szText_1 );
		//	May 12, 2000 genta
		//	���s�R�[�h�̕\����ǉ��D���̔ԍ���1�����炷
		//	From Here
		::StatusBar_SetText( hwndStatusBar, 2 | 0,             szEolMode );
		//	To Here
		::StatusBar_SetText( hwndStatusBar, 3 | 0,             szCaretChar );
		::StatusBar_SetText( hwndStatusBar, 4 | 0,             pszCodeName );
		::StatusBar_SetText( hwndStatusBar, 5 | SBT_OWNERDRAW, _T("") );
		::StatusBar_SetText( hwndStatusBar, 6 | 0,             szText_6 );
#endif // rei_
	}

}

void CCaret::ClearCaretPosInfoCache()
{
	m_nOffsetCache = CLayoutInt(-1);
	m_nLineNoCache = CLayoutInt(-1);
	m_nLogicOffsetCache = CLogicInt(-1);
	m_nLineLogicNoCache = CLogicInt(-1);
	m_nLineNo50Cache = CLayoutInt(-1);
	m_nOffset50Cache = CLayoutInt(-1);
	m_nLogicOffset50Cache = CLogicInt(-1);
	m_nLineLogicModCache = -1;
}

/* �J�[�\���㉺�ړ����� */
CLayoutInt CCaret::Cursor_UPDOWN( CLayoutInt nMoveLines, bool bSelect )
{
	//�K�v�ȃC���^�[�t�F�[�X
	const CLayoutMgr* const pLayoutMgr = &m_pEditDoc->m_cLayoutMgr;
	const CommonSetting* const pCommon = &GetDllShareData().m_Common;

	const CLayoutPoint ptCaret = GetCaretLayoutPos();

	bool	bVertLineDoNotOFF = true;	// �J�[�\���ʒu�c�����������Ȃ�
	if( bSelect ){
		bVertLineDoNotOFF = false;		//�I����ԂȂ�J�[�\���ʒu�c���������s��
	}

	// ���݂̃L�����b�gY���W + nMoveLines�����������C�A�E�g�s�͈͓̔��Ɏ��܂�悤�� nMoveLines�𒲐�����B
	if( nMoveLines > 0 ) { // ���ړ��B
		const bool existsEOFOnlyLine = pLayoutMgr->GetBottomLayout() && pLayoutMgr->GetBottomLayout()->GetLayoutEol() != EOL_NONE
			|| pLayoutMgr->GetLineCount() == 0;
		const CLayoutInt maxLayoutLine = pLayoutMgr->GetLineCount() + (existsEOFOnlyLine ? 1 : 0 ) - 1;
		// �ړ��悪 EOF�݂̂̍s���܂߂����C�A�E�g�s�������ɂȂ�悤�Ɉړ��ʂ��K������B
		nMoveLines = t_min( nMoveLines,  maxLayoutLine - ptCaret.y );
		if( ptCaret.y + nMoveLines == maxLayoutLine && existsEOFOnlyLine // �ړ��悪 EOF�݂̂̍s
			&& m_pEditView->GetSelectionInfo().IsBoxSelecting() && 0 != ptCaret.x // ����`�I�𒆂Ȃ�A
		) {
			// EOF�݂̂̍s�ɂ͈ړ����Ȃ��B���ړ��ŃL�����b�g�� X���W�𓮂��������Ȃ��̂ŁB
			nMoveLines = t_max( CLayoutInt(0), nMoveLines - 1 ); // ���������ړ����Ȃ��悤�� 0�ȏ�����B
		}
	} else { // ��ړ��B
		// �ړ��悪 0�s�ڂ�菬�����Ȃ�Ȃ��悤�Ɉړ��ʂ��K���B
		nMoveLines = t_max( nMoveLines, - GetCaretLayoutPos().GetY() );
	}

	if( bSelect && ! m_pEditView->GetSelectionInfo().IsTextSelected() ) {
		/* ���݂̃J�[�\���ʒu����I�����J�n���� */
		m_pEditView->GetSelectionInfo().BeginSelectArea();
	}
	if( ! bSelect ){
		if( m_pEditView->GetSelectionInfo().IsTextSelected() ) {
			/* ���݂̑I��͈͂��I����Ԃɖ߂� */
			m_pEditView->GetSelectionInfo().DisableSelectArea(true);
		}else if( m_pEditView->GetSelectionInfo().IsBoxSelecting() ){
			m_pEditView->GetSelectionInfo().SetBoxSelect(false);
		}
	}

	// (���ꂩ�狁�߂�)�L�����b�g�̈ړ���B
	CLayoutPoint ptTo( CLayoutInt(0), ptCaret.y + nMoveLines );

	/* �ړ���̍s�̃f�[�^���擾 */
	const CLayout* const pLayout = pLayoutMgr->SearchLineByLayoutY( ptTo.y );
	const CLogicInt nLineLen = pLayout ? pLayout->GetLengthWithEOL() : CLogicInt(0);
	int i = 0; ///< ���H
	if( pLayout ) {
		CMemoryIterator it( pLayout, pLayoutMgr->GetTabSpace(), pLayoutMgr->m_tsvInfo );
		while( ! it.end() ){
			it.scanNext();
			if ( it.getIndex() + it.getIndexDelta() > pLayout->GetLengthWithoutEOL() ){
				i = nLineLen;
				break;
			}
			if( it.getColumn() + it.getColumnDelta() > m_nCaretPosX_Prev ){
				i = it.getIndex();
				break;
			}
			it.addDelta();
		}
		ptTo.x += it.getColumn();
		if( it.end() ) {
			i = it.getIndex();
		}
	}
	if( i >= nLineLen ) {
		/* �t���[�J�[�\�����[�h�Ƌ�`�I�𒆂́A�L�����b�g�̈ʒu�����s�� EOF�̑O�ɐ������Ȃ� */
		if( pCommon->m_sGeneral.m_bIsFreeCursorMode
			|| m_pEditView->GetSelectionInfo().IsBoxSelecting()
		) {
			ptTo.x = m_nCaretPosX_Prev;
		}
	}
	if( ptTo.x != GetCaretLayoutPos().GetX() ){
		bVertLineDoNotOFF = false;
	}
	GetAdjustCursorPos( &ptTo );
	if( bSelect ) {
		/* ���݂̃J�[�\���ʒu�ɂ���đI��͈͂�ύX */
		m_pEditView->GetSelectionInfo().ChangeSelectAreaByCurrentCursor( ptTo );
	}
	const CLayoutInt nScrollLines = MoveCursor(	ptTo,
								m_pEditView->GetDrawSwitch() /* TRUE */,
								_CARETMARGINRATE,
								false,
								bVertLineDoNotOFF );
	return nScrollLines;
}


/*!	�L�����b�g�̍쐬

	@param nCaretColor [in]	�L�����b�g�̐F��� (0:�ʏ�, 1:IME ON)
	@param nWidth [in]		�L�����b�g��
	@param nHeight [in]		�L�����b�g��

	@date 2006.12.07 ryoji �V�K�쐬
*/
void CCaret::CreateEditCaret( COLORREF crCaret, COLORREF crBack, int nWidth, int nHeight )
{
	//
	// �L�����b�g�p�̃r�b�g�}�b�v���쐬����
	//
	// Note: �E�B���h�E�݊��̃����� DC ��� PatBlt ��p���ăL�����b�g�F�Ɣw�i�F�� XOR ����
	//       ���邱�ƂŁC�ړI�̃r�b�g�}�b�v�𓾂�D
	//       �� 256 �F���ł� RGB �l��P���ɒ��ډ��Z���Ă��L�����b�g�F���o�����߂̐�����
	//          �r�b�g�}�b�v�F�͓����Ȃ��D
	//       �Q�l: [HOWTO] �L�����b�g�̐F�𐧌䂷����@
	//             http://support.microsoft.com/kb/84054/ja
	//

	HBITMAP hbmpCaret;	// �L�����b�g�p�̃r�b�g�}�b�v

	HDC hdc = m_pEditView->GetDC();

	hbmpCaret = ::CreateCompatibleBitmap( hdc, nWidth, nHeight );
	HDC hdcMem = ::CreateCompatibleDC( hdc );
	HBITMAP hbmpOld = (HBITMAP)::SelectObject( hdcMem, hbmpCaret );
	HBRUSH hbrCaret = ::CreateSolidBrush( crCaret );
	HBRUSH hbrBack = ::CreateSolidBrush( crBack );
	HBRUSH hbrOld = (HBRUSH)::SelectObject( hdcMem, hbrCaret );
	::PatBlt( hdcMem, 0, 0, nWidth, nHeight, PATCOPY );
	::SelectObject( hdcMem, hbrBack );
	::PatBlt( hdcMem, 0, 0, nWidth, nHeight, PATINVERT );
	::SelectObject( hdcMem, hbrOld );
	::SelectObject( hdcMem, hbmpOld );
	::DeleteObject( hbrCaret );
	::DeleteObject( hbrBack );
	::DeleteDC( hdcMem );

	m_pEditView->ReleaseDC( hdc );

	// �ȑO�̃r�b�g�}�b�v��j������
	if( m_hbmpCaret != NULL )
		::DeleteObject( m_hbmpCaret );
	m_hbmpCaret = hbmpCaret;

	// �L�����b�g���쐬����
	m_pEditView->CreateCaret( hbmpCaret, nWidth, nHeight );
	return;
}


// 2002/07/22 novice
/*!
	�L�����b�g�̕\��
*/
void CCaret::ShowCaret_( HWND hwnd )
{
	if ( m_bCaretShowFlag == false ){
		::ShowCaret( hwnd );
		m_bCaretShowFlag = true;
	}
}


/*!
	�L�����b�g�̔�\��
*/
void CCaret::HideCaret_( HWND hwnd )
{
	if ( m_bCaretShowFlag == true ){
		::HideCaret( hwnd );
		m_bCaretShowFlag = false;
	}
}

//! �����̏�Ԃ𑼂�CCaret�ɃR�s�[
void CCaret::CopyCaretStatus(CCaret* pCaret) const
{
	pCaret->SetCaretLayoutPos(GetCaretLayoutPos());
	pCaret->SetCaretLogicPos(GetCaretLogicPos());
	pCaret->m_nCaretPosX_Prev = m_nCaretPosX_Prev;	/* �r���[���[����̃J�[�\�����ʒu�i�O�I���W���j*/

	//�� �L�����b�g�̃T�C�Y�̓R�s�[���Ȃ��B2002/05/12 YAZAKI
}


POINT CCaret::CalcCaretDrawPos(const CLayoutPoint& ptCaretPos) const
{
	int nPosX = m_pEditView->GetTextArea().GetAreaLeft()
		+ (Int)(ptCaretPos.x - m_pEditView->GetTextArea().GetViewLeftCol()) * GetHankakuDx();

	CLayoutYInt nY = ptCaretPos.y - m_pEditView->GetTextArea().GetViewTopLine();
	int nPosY;
	if( nY < 0 ){
		nPosY = -1;
	}else if( m_pEditView->GetTextArea().m_nViewRowNum < nY ){
		nPosY = m_pEditView->GetTextArea().GetAreaBottom() + 1;
	}else{
		nPosY = m_pEditView->GetTextArea().GetAreaTop()
			+ (Int)(nY) * m_pEditView->GetTextMetrics().GetHankakuDy()
			+ m_pEditView->GetTextMetrics().GetHankakuHeight() - GetCaretSize().cy; //����
	}
#if REI_LINE_CENTERING
	//nPosY += m_pEditView->m_pTypeData->m_nLineSpace;  // �s�Ԋu���܂ލ����ɂ���ꍇ
	nPosY += m_pEditView->m_pTypeData->m_nLineSpace/2;  // �����̍����ɂ���ꍇ
#endif // rei_

	return CMyPoint(nPosX,nPosY);
}




/*!
	�s���w��ɂ��J�[�\���ړ��i���W�����t���j

	@return �c�X�N���[���s��(��:��X�N���[��/��:���X�N���[��)

	@note �}�E�X���ɂ��ړ��ŕs�K�؂Ȉʒu�ɍs���Ȃ��悤���W�������ăJ�[�\���ړ�����

	@date 2007.08.23 ryoji �֐����iMoveCursorToPoint()���珈���𔲂��o���j
	@date 2007.09.26 ryoji ���p�����ł������ō��E�ɃJ�[�\����U�蕪����
	@date 2007.10.23 kobake ���������̌����C�� ([in,out]��[in])
	@date 2009.02.17 ryoji ���C�A�E�g�s���Ȍ�̃J�����ʒu�w��Ȃ疖�������̑O�ł͂Ȃ����������̌�Ɉړ�����
*/
CLayoutInt CCaret::MoveCursorProperly(
	CLayoutPoint	ptNewXY,			//!< [in] �J�[�\���̃��C�A�E�g���WX
	bool			bScroll,			//!< [in] true: ��ʈʒu�����L��/ false: ��ʈʒu�����L�薳��
	bool			test,				//!< [in] true: �J�[�\���ړ��͂��Ȃ�
	CLayoutPoint*	ptNewXYNew,			//!< [out] �V�������C�A�E�g���W
	int				nCaretMarginRate,	//!< [in] �c�X�N���[���J�n�ʒu�����߂�l
	int				dx					//!< [in] ptNewXY.x�ƃ}�E�X�J�[�\���ʒu�Ƃ̌덷(�J�����������̃h�b�g��)
)
{
	CLogicInt		nLineLen;
	const CLayout*	pcLayout;

	if( 0 > ptNewXY.y ){
		ptNewXY.y = CLayoutInt(0);
	}
	
	// 2011.12.26 EOF�ȉ��̍s�������ꍇ�ŋ�`�̂Ƃ��́A�ŏI���C�A�E�g�s�ֈړ�����
	if( ptNewXY.y >= m_pEditDoc->m_cLayoutMgr.GetLineCount()
	 && (m_pEditView->GetSelectionInfo().IsMouseSelecting() && m_pEditView->GetSelectionInfo().IsBoxSelecting()) ){
		const CLayout* layoutEnd = m_pEditDoc->m_cLayoutMgr.GetBottomLayout();
		bool bEofOnly = (layoutEnd && layoutEnd->GetLayoutEol() != EOL_NONE) || NULL == layoutEnd;
	 	// 2012.01.09 �҂�����[EOF]�ʒu�ɂ���ꍇ�͈ʒu���ێ�(1��̍s�ɂ��Ȃ�)
	 	if( bEofOnly && ptNewXY.y == m_pEditDoc->m_cLayoutMgr.GetLineCount() && ptNewXY.x == 0 ){
	 	}else{
			ptNewXY.y = t_max(CLayoutInt(0), m_pEditDoc->m_cLayoutMgr.GetLineCount() - 1);
		}
	}
	/* �J�[�\�����e�L�X�g�ŉ��[�s�ɂ��邩 */
	if( ptNewXY.y >= m_pEditDoc->m_cLayoutMgr.GetLineCount() ){
		// 2004.04.03 Moca EOF�����̍��W�����́AMoveCursor���ł���Ă��炤�̂ŁA�폜
	}
	/* �J�[�\�����e�L�X�g�ŏ�[�s�ɂ��邩 */
	else if( ptNewXY.y < 0 ){
		ptNewXY.Set(CLayoutInt(0), CLayoutInt(0));
	}
	else{
		/* �ړ���̍s�̃f�[�^���擾 */
		m_pEditDoc->m_cLayoutMgr.GetLineStr( ptNewXY.GetY2(), &nLineLen, &pcLayout );

		int nColWidth = m_pEditView->GetTextMetrics().GetHankakuDx();
		CLayoutInt nPosX = CLayoutInt(0);
		int i = 0;
		CMemoryIterator it( pcLayout, m_pEditDoc->m_cLayoutMgr.GetTabSpace(), m_pEditDoc->m_cLayoutMgr.m_tsvInfo );
		while( !it.end() ){
			it.scanNext();
			if ( it.getIndex() + it.getIndexDelta() > CLogicInt(pcLayout->GetLengthWithoutEOL()) ){
				i = nLineLen;
				break;
			}
			if( it.getColumn() + it.getColumnDelta() > ptNewXY.GetX2() ){
				if (ptNewXY.GetX2() >= (pcLayout ? pcLayout->GetIndent() : CLayoutInt(0)) && ((ptNewXY.GetX2() - it.getColumn()) * nColWidth + dx) * 2 >= it.getColumnDelta() * nColWidth){
				//if (ptNewXY.GetX2() >= (pcLayout ? pcLayout->GetIndent() : CLayoutInt(0)) && (it.getColumnDelta() > CLayoutInt(1)) && ((it.getColumn() + it.getColumnDelta() - ptNewXY.GetX2()) <= it.getColumnDelta() / 2)){
					nPosX += it.getColumnDelta();
				}
				i = it.getIndex();
				break;
			}
			it.addDelta();
		}
		nPosX += it.getColumn();
		if ( it.end() ){
			i = it.getIndex();
			//nPosX -= it.getColumnDelta();	// 2009.02.17 ryoji �R�����g�A�E�g�i���������̌�Ɉړ�����j
		}

		if( i >= nLineLen ){
			// 2011.12.26 �t���[�J�[�\��/��`�Ńf�[�^�t��EOF�̉E���ֈړ��ł���悤��
			/* �t���[�J�[�\�����[�h�� */
			if( GetDllShareData().m_Common.m_sGeneral.m_bIsFreeCursorMode
			  || ( m_pEditView->GetSelectionInfo().IsMouseSelecting() && m_pEditView->GetSelectionInfo().IsBoxSelecting() )	/* �}�E�X�͈͑I�� && ��`�͈͑I�� */
			  || ( m_pEditView->m_bDragMode && m_pEditView->m_bDragBoxData ) /* OLE DropTarget && ��`�f�[�^ */
			){
				// �܂�Ԃ����ƃ��C�A�E�g�s�����i�Ԃ牺�����܂ށj�̂ǂ��炩�傫���ق��܂ŃJ�[�\���ړ��\
				//	Aug. 14, 2005 genta �܂�Ԃ�����LayoutMgr����擾����悤��
				CLayoutInt nMaxX = t_max(nPosX, m_pEditDoc->m_cLayoutMgr.GetMaxLineKetas());
				nPosX = ptNewXY.GetX2();
				if( nPosX < CLayoutInt(0) ){
					nPosX = CLayoutInt(0);
				}
				else if( nPosX > nMaxX ){
					nPosX = nMaxX;
				}
			}
		}
		ptNewXY.SetX( nPosX );
	}
	
	if( ptNewXYNew ){
		*ptNewXYNew = ptNewXY;
		GetAdjustCursorPos( ptNewXYNew );
	}
	if( test ){
		return CLayoutInt(0);
	}
	return MoveCursor( ptNewXY, bScroll, nCaretMarginRate );
}