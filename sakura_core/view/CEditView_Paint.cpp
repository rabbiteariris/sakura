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
#include <vector>
#include <limits.h>
#include "view/CEditView_Paint.h"
#include "view/CEditView.h"
#include "view/CViewFont.h"
#include "view/CRuler.h"
#include "view/colors/CColorStrategy.h"
#include "view/colors/CColor_Found.h"
#include "view/figures/CFigureManager.h"
#include "types/CTypeSupport.h"
#include "doc/CEditDoc.h"
#include "doc/layout/CLayout.h"
#include "window/CEditWnd.h"
#include "parse/CWordParse.h"
#include "util/string_ex2.h"
#ifdef USE_SSE2
#ifdef __MINGW32__
#include <x86intrin.h>
#else
#include <intrin.h>
#endif
#endif

void _DispWrap(CGraphics& gr, DispPos* pDispPos, const CEditView* pcView, CLayoutYInt nLineNum);

/*
	PAINT_LINENUMBER = (1<<0), //!< �s�ԍ�
	PAINT_RULER      = (1<<1), //!< ���[���[
	PAINT_BODY       = (1<<2), //!< �{��
*/

void CEditView_Paint::Call_OnPaint(
	int nPaintFlag,   //!< �`�悷��̈��I������
	bool bUseMemoryDC //!< ������DC���g�p����
)
{
	CEditView* pView = GetEditView();

	//�e�v�f
	CMyRect rcLineNumber(0,pView->GetTextArea().GetAreaTop(),pView->GetTextArea().GetAreaLeft(),pView->GetTextArea().GetAreaBottom());
	CMyRect rcRuler(pView->GetTextArea().GetAreaLeft(),0,pView->GetTextArea().GetAreaRight(),pView->GetTextArea().GetAreaTop());
	CMyRect rcBody(pView->GetTextArea().GetAreaLeft(),pView->GetTextArea().GetAreaTop(),pView->GetTextArea().GetAreaRight(),pView->GetTextArea().GetAreaBottom());

	//�̈���쐬 -> rc
	std::vector<CMyRect> rcs;
	if(nPaintFlag & PAINT_LINENUMBER)rcs.push_back(rcLineNumber);
	if(nPaintFlag & PAINT_RULER)rcs.push_back(rcRuler);
	if(nPaintFlag & PAINT_BODY)rcs.push_back(rcBody);
	if(rcs.size()==0)return;
	CMyRect rc=rcs[0];
	int nSize = (int)rcs.size();
	for(int i=1;i<nSize;i++)
		rc=MergeRect(rc,rcs[i]);

	//�`��
	PAINTSTRUCT	ps;
	ps.rcPaint = rc;
	HDC hdc = pView->GetDC();
	pView->OnPaint( hdc, &ps, bUseMemoryDC );
	pView->ReleaseDC( hdc );
}



/* �t�H�[�J�X�ړ����̍ĕ`��

	@date 2001/06/21 asa-o �u�X�N���[���o�[�̏�Ԃ��X�V����v�u�J�[�\���ړ��v�폜
*/
void CEditView::RedrawAll()
{
	if( NULL == GetHwnd() ){
		return;
	}
	
	if( GetDrawSwitch() ){
		// �E�B���h�E�S�̂��ĕ`��
		PAINTSTRUCT	ps;
		HDC hdc = ::GetDC( GetHwnd() );
		::GetClientRect( GetHwnd(), &ps.rcPaint );
		OnPaint( hdc, &ps, FALSE );
		::ReleaseDC( GetHwnd(), hdc );
	}

	// �L�����b�g�̕\��
	GetCaret().ShowEditCaret();

	// �L�����b�g�̍s���ʒu��\������
	GetCaret().ShowCaretPosInfo();

	// �e�E�B���h�E�̃^�C�g�����X�V
	m_pcEditWnd->UpdateCaption();

	//	Jul. 9, 2005 genta	�I��͈͂̏����X�e�[�^�X�o�[�֕\��
	GetSelectionInfo().PrintSelectionInfoMsg();

	// �X�N���[���o�[�̏�Ԃ��X�V����
	AdjustScrollBars();
}

// 2001/06/21 Start by asa-o �ĕ`��
void CEditView::Redraw()
{
	if( NULL == GetHwnd() ){
		return;
	}
	if( !GetDrawSwitch() ){
		return;
	}

	HDC			hdc;
	PAINTSTRUCT	ps;

	hdc = ::GetDC( GetHwnd() );

	::GetClientRect( GetHwnd(), &ps.rcPaint );

	OnPaint( hdc, &ps, FALSE );

	::ReleaseDC( GetHwnd(), hdc );
}
// 2001/06/21 End

void CEditView::RedrawLines( CLayoutYInt top, CLayoutYInt bottom )
{
	if( NULL == GetHwnd() ){
		return;
	}
	if( !GetDrawSwitch() ){
		return;
	}

	if( bottom < GetTextArea().GetViewTopLine() ){
		return;
	}
	if( GetTextArea().GetBottomLine() <= top ){
		return;
	}
	HDC			hdc;
	PAINTSTRUCT	ps;

	hdc = GetDC();

	ps.rcPaint.left = 0;
	ps.rcPaint.right = GetTextArea().GetAreaRight();
	ps.rcPaint.top = GetTextArea().GenerateYPx(top);
	ps.rcPaint.bottom = GetTextArea().GenerateYPx(bottom);

	OnPaint( hdc, &ps, FALSE );

	ReleaseDC( hdc );
}

void MyFillRect(HDC hdc, RECT& re)
{
	::ExtTextOut(hdc, re.left, re.top, ETO_OPAQUE|ETO_CLIPPED, &re, _T(""), 0, NULL);
}

void CEditView::DrawBackImage(HDC hdc, RECT& rcPaint, HDC hdcBgImg)
{
#if 0
//	�e�X�g�w�i�p�^�[��
	static int testColorIndex = 0;
	testColorIndex = testColorIndex % 7;
	COLORREF cols[7] = {RGB(255,255,255),
		RGB(200,255,255),RGB(255,200,255),RGB(255,255,200),
		RGB(200,200,255),RGB(255,200,200),RGB(200,255,200),
	};
	COLORREF colorOld = ::SetBkColor(hdc, cols[testColorIndex]);
	MyFillRect(hdc, rcPaint);
	::SetBkColor(hdc, colorOld);
	testColorIndex++;
#else
	CTypeSupport cTextType(this,COLORIDX_TEXT);
	COLORREF colorOld = ::SetBkColor(hdc, cTextType.GetBackColor());
	const CTextArea& area = GetTextArea();
	const CEditDoc& doc  = *m_pcEditDoc;
	const STypeConfig& typeConfig = doc.m_cDocType.GetDocumentAttribute();

	CMyRect rcImagePos;
	switch( typeConfig.m_backImgPos ){
	case BGIMAGE_TOP_LEFT:
	case BGIMAGE_BOTTOM_LEFT:
	case BGIMAGE_CENTER_LEFT:
		rcImagePos.left = area.GetAreaLeft();
		break;
	case BGIMAGE_TOP_RIGHT:
	case BGIMAGE_BOTTOM_RIGHT:
	case BGIMAGE_CENTER_RIGHT:
		rcImagePos.left = area.GetAreaRight() - doc.m_nBackImgWidth;
		break;
	case BGIMAGE_TOP_CENTER:
	case BGIMAGE_BOTTOM_CENTER:
	case BGIMAGE_CENTER:
		rcImagePos.left = area.GetAreaLeft() + area.GetAreaWidth()/2 - doc.m_nBackImgWidth/2;
		break;
	default:
		assert_warning(0 != typeConfig.m_backImgPos);
		break;
	}
	switch( typeConfig.m_backImgPos ){
	case BGIMAGE_TOP_LEFT:
	case BGIMAGE_TOP_RIGHT:
	case BGIMAGE_TOP_CENTER:
		rcImagePos.top  = area.GetAreaTop();
		break;
	case BGIMAGE_BOTTOM_LEFT:
	case BGIMAGE_BOTTOM_RIGHT:
	case BGIMAGE_BOTTOM_CENTER:
		rcImagePos.top  = area.GetAreaBottom() - doc.m_nBackImgHeight;
		break;
	case BGIMAGE_CENTER_LEFT:
	case BGIMAGE_CENTER_RIGHT:
	case BGIMAGE_CENTER:
		rcImagePos.top  = area.GetAreaTop() + area.GetAreaHeight()/2 - doc.m_nBackImgHeight/2;
		break;
	default:
		assert_warning(0 != typeConfig.m_backImgPos);
		break;
	}
	rcImagePos.left += typeConfig.m_backImgPosOffset.x;
	rcImagePos.top  += typeConfig.m_backImgPosOffset.y;
	// �X�N���[�����̉�ʂ̒[����悷��Ƃ��̈ʒu������ֈړ�
	if( typeConfig.m_backImgScrollX ){
		int tile = typeConfig.m_backImgRepeatX ? doc.m_nBackImgWidth : INT_MAX;
		Int posX = (area.GetViewLeftCol() % tile) * GetTextMetrics().GetHankakuDx();
		rcImagePos.left -= posX % tile;
	}
	if( typeConfig.m_backImgScrollY ){
		int tile = typeConfig.m_backImgRepeatY ? doc.m_nBackImgHeight : INT_MAX;
		Int posY = (area.GetViewTopLine() % tile) * GetTextMetrics().GetHankakuDy();
		rcImagePos.top -= posY % tile;
	}
	if( typeConfig.m_backImgRepeatX ){
		if( 0 < rcImagePos.left ){
			// rcImagePos.left = rcImagePos.left - (rcImagePos.left / doc.m_nBackImgWidth + 1) * doc.m_nBackImgWidth;
			rcImagePos.left = rcImagePos.left % doc.m_nBackImgWidth - doc.m_nBackImgWidth;
		}
	}
	if( typeConfig.m_backImgRepeatY ){
		if( 0 < rcImagePos.top ){
			// rcImagePos.top = rcImagePos.top - (rcImagePos.top / doc.m_nBackImgHeight + 1) * doc.m_nBackImgHeight;
			rcImagePos.top = rcImagePos.top % doc.m_nBackImgHeight - doc.m_nBackImgHeight;
		}
	}
	rcImagePos.SetSize(doc.m_nBackImgWidth, doc.m_nBackImgHeight);
	
	
	RECT rc = rcPaint;
	// rc.left = t_max((int)rc.left, area.GetAreaLeft());
	rc.top  = t_max((int)rc.top,  area.GetRulerHeight()); // ���[���[�����O
	const int nXEnd = area.GetAreaRight();
	const int nYEnd = area.GetAreaBottom();
	CMyRect rcBltAll;
	rcBltAll.SetLTRB(INT_MAX, INT_MAX, -INT_MAX, -INT_MAX);
	CMyRect rcImagePosOrg = rcImagePos;
	for(; rcImagePos.top <= nYEnd; ){
		for(; rcImagePos.left <= nXEnd; ){
			CMyRect rcBlt;
			if( ::IntersectRect(&rcBlt, &rc, &rcImagePos) ){
				::BitBlt(
					hdc,
					rcBlt.left,
					rcBlt.top,
					rcBlt.right  - rcBlt.left,
					rcBlt.bottom - rcBlt.top,
					hdcBgImg,
					rcBlt.left - rcImagePos.left,
					rcBlt.top - rcImagePos.top,
					SRCCOPY
				);
				rcBltAll.left   = t_min(rcBltAll.left,   rcBlt.left);
				rcBltAll.top    = t_min(rcBltAll.top,    rcBlt.top);
				rcBltAll.right  = t_max(rcBltAll.right,  rcBlt.right);
				rcBltAll.bottom = t_max(rcBltAll.bottom, rcBlt.bottom);
			}
			rcImagePos.left  += doc.m_nBackImgWidth;
			rcImagePos.right += doc.m_nBackImgWidth;
			if( !typeConfig.m_backImgRepeatX ){
				break;
			}
		}
		rcImagePos.left  = rcImagePosOrg.left;
		rcImagePos.right = rcImagePosOrg.right;
		rcImagePos.top    += doc.m_nBackImgHeight;
		rcImagePos.bottom += doc.m_nBackImgHeight;
		if( !typeConfig.m_backImgRepeatY ){
			break;
		}
	}
	if( rcBltAll.left != INT_MAX ){
		// �㉺���E�ȂȂ߂̌��Ԃ𖄂߂�
		CMyRect rcFill;
		LONG& x1 = rc.left;
		LONG& x2 = rcBltAll.left;
		LONG& x3 = rcBltAll.right;
		LONG& x4 = rc.right;
		LONG& y1 = rc.top;
		LONG& y2 = rcBltAll.top;
		LONG& y3 = rcBltAll.bottom;
		LONG& y4 = rc.bottom;
		if( y1 < y2 ){
			rcFill.SetLTRB(x1,y1, x4,y2); MyFillRect(hdc, rcFill);
		}
		if( x1 < x2 ){
			rcFill.SetLTRB(x1,y2, x2,y3); MyFillRect(hdc, rcFill);
		}
		if( x3 < x4 ){
			rcFill.SetLTRB(x3,y2, x4,y3); MyFillRect(hdc, rcFill);
		}
		if( y3 < y4 ){
			rcFill.SetLTRB(x1,y3, x4,y4); MyFillRect(hdc, rcFill);
		}
	}else{
		MyFillRect(hdc, rc);
	}
	::SetBkColor(hdc, colorOld);
#endif
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                          �F�ݒ�                             //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

/*! �w��ʒu��ColorIndex�̎擾
	CEditView::DrawLogicLine�����ɂ�������CEditView::DrawLogicLine��
	�C�����������ꍇ�́A�������C�����K�v�B
*/
CColor3Setting CEditView::GetColorIndex(
	const CLayout*			pcLayout,
	CLayoutYInt				nLineNum,
	int						nIndex,
	SColorStrategyInfo* 	pInfo,			// 2010.03.31 ryoji �ǉ�
	bool					bPrev			// �w��ʒu�̐F�ύX���O�܂�	2010.06.19 ryoji �ǉ�
)
{
	EColorIndexType eRet = COLORIDX_TEXT;

	if(!pcLayout){
		CColor3Setting cColor = { COLORIDX_TEXT, COLORIDX_TEXT, COLORIDX_TEXT };
		return cColor;
	}
	// 2014.12.30 Skip���[�h�̎���COLORIDX_TEXT
	if (CColorStrategyPool::getInstance()->IsSkipBeforeLayout()) {
		CColor3Setting cColor = { COLORIDX_TEXT, COLORIDX_TEXT, COLORIDX_TEXT };
		return cColor;
	}

	const CLayoutColorInfo* colorInfo;
	const CLayout* pcLayoutLineFirst = pcLayout;
	CLayoutYInt nLineNumFirst = nLineNum;
	{
		// 2002/2/10 aroka CMemory�ύX
		pInfo->m_pLineOfLogic = pcLayout->GetDocLineRef()->GetPtr();

		// �_���s�̍ŏ��̃��C�A�E�g�����擾 -> pcLayoutLineFirst
		while( 0 != pcLayoutLineFirst->GetLogicOffset() ){
			pcLayoutLineFirst = pcLayoutLineFirst->GetPrevLayout();
			nLineNumFirst--;

			// �_���s�̐擪�܂Ŗ߂�Ȃ��Ɗm���ɂ͐��m�ȐF�͓����Ȃ�
			// �i���K�\���L�[���[�h�Ƀ}�b�`�������������\�������̈ʒu�̃��C�A�E�g�s�����܂����ł���ꍇ�Ȃǁj
			//if( pcLayout->GetLogicOffset() - pcLayoutLineFirst->GetLogicOffset() > 260 )
			//	break;
		}

		// 2005.11.20 Moca �F���������Ȃ����Ƃ�������ɑΏ�
		eRet = pcLayoutLineFirst->GetColorTypePrev();	/* ���݂̐F���w�� */	// 02/12/18 ai
		colorInfo = pcLayoutLineFirst->GetColorInfo();
		pInfo->m_nPosInLogic = pcLayoutLineFirst->GetLogicOffset();

		//CColorStrategyPool������
		CColorStrategyPool* pool = CColorStrategyPool::getInstance();
		pool->SetCurrentView(this);
		pool->NotifyOnStartScanLogic();


		// 2009.02.07 ryoji ���̊֐��ł� pInfo->CheckChangeColor() �ŐF�𒲂ׂ邾���Ȃ̂ňȉ��̏����͕s�v
		//
		////############�����B�{����Visitor���g���ׂ�
		//class TmpVisitor{
		//public:
		//	static int CalcLayoutIndex(const CLayout* pcLayout)
		//	{
		//		int n = -1;
		//		while(pcLayout){
		//			pcLayout = pcLayout->GetPrevLayout(); //prev or null
		//			n++;
		//		}
		//		return n;
		//	}
		//};
		//pInfo->pDispPos->SetLayoutLineRef(CLayoutInt(TmpVisitor::CalcLayoutIndex(pcLayout)));
		// 2013.12.11 Moca �J�����g�s�̐F�ւ��ŕK�v�ɂȂ�܂���
		pInfo->m_pDispPos->SetLayoutLineRef(nLineNumFirst);
	}

	//������Q��
	const CDocLine* pcDocLine = pcLayout->GetDocLineRef();
	CStringRef cLineStr(pcDocLine->GetPtr(),pcDocLine->GetLengthWithEOL());

	//color strategy
	CColorStrategyPool* pool = CColorStrategyPool::getInstance();
	pInfo->m_pStrategy = pool->GetStrategyByColor(eRet);
	if(pInfo->m_pStrategy){
		pInfo->m_pStrategy->InitStrategyStatus();
		pInfo->m_pStrategy->SetStrategyColorInfo(colorInfo);
	}

	const CLayout* pcLayoutNext = pcLayoutLineFirst->GetNextLayout();
	CLayoutYInt nLineNumScan = nLineNumFirst;
	int nPosTo = pcLayout->GetLogicOffset() + t_min(nIndex, (int)pcLayout->GetLengthWithEOL() - 1);
	while(pInfo->m_nPosInLogic <= nPosTo){
		if( bPrev && pInfo->m_nPosInLogic == nPosTo )
			break;

		//�F�ؑ�
		pInfo->CheckChangeColor(cLineStr);

		//1�����i��
		pInfo->m_nPosInLogic += CNativeW::GetSizeOfChar(
									cLineStr.GetPtr(),
									cLineStr.GetLength(),
									pInfo->m_nPosInLogic
								);
		if( pcLayoutNext && pcLayoutNext->GetLogicOffset() <= pInfo->m_nPosInLogic ){
			nLineNumScan++;
			pInfo->m_pDispPos->SetLayoutLineRef(nLineNumScan);
			pcLayoutNext = pcLayoutNext->GetNextLayout();
		}
	}

	CColor3Setting cColor;
	pInfo->DoChangeColor(&cColor);

	return cColor;
}

/*! ���݂̐F���w��
	@param eColorIndex   �I�����܂ތ��݂̐F
	@param eColorIndex2  �I���ȊO�̌��݂̐F
	@param eColorIndexBg �w�i�F

	@date 2013.05.08 novice �͈͊O�`�F�b�N�폜
*/
void CEditView::SetCurrentColor( CGraphics& gr, EColorIndexType eColorIndex,  EColorIndexType eColorIndex2, EColorIndexType eColorIndexBg)
{
	//�C���f�b�N�X����
	int		nColorIdx = ToColorInfoArrIndex(eColorIndex);
	int		nColorIdx2 = ToColorInfoArrIndex(eColorIndex2);
	int		nColorIdxBg = ToColorInfoArrIndex(eColorIndexBg);

	//���ۂɐF��ݒ�
	const ColorInfo& info  = m_pTypeData->m_ColorInfoArr[nColorIdx];
	const ColorInfo& info2 = m_pTypeData->m_ColorInfoArr[nColorIdx2];
	const ColorInfo& infoBg = m_pTypeData->m_ColorInfoArr[nColorIdxBg];
	COLORREF fgcolor = GetTextColorByColorInfo2(info, info2);
	gr.SetTextForeColor(fgcolor);
	// 2012.11.21 �w�i�F���e�L�X�g�Ƃ��Ȃ��Ȃ�w�i�F�̓J�[�\���s�w�i
	const ColorInfo& info3 = (info2.m_sColorAttr.m_cBACK == m_crBack ? infoBg : info2);
	COLORREF bkcolor = (nColorIdx == nColorIdx2) ? info3.m_sColorAttr.m_cBACK : GetBackColorByColorInfo2(info, info3);
	gr.SetTextBackColor(bkcolor);
	SFONT sFont;
	sFont.m_sFontAttr = (info.m_sColorAttr.m_cTEXT != info.m_sColorAttr.m_cBACK) ? info.m_sFontAttr : info2.m_sFontAttr;
	sFont.m_hFont = GetFontset().ChooseFontHandle( sFont.m_sFontAttr );
	gr.SetMyFont(sFont);
}

inline COLORREF MakeColor2(COLORREF a, COLORREF b, int alpha)
{
#ifdef USE_SSE2
	// (a * alpha + b * (256 - alpha)) / 256 -> ((a - b) * alpha) / 256 + b
	__m128i xmm0, xmm1, xmm2, xmm3;
	COLORREF color;
	xmm0 = _mm_setzero_si128();
	xmm1 = _mm_cvtsi32_si128( a );
	xmm2 = _mm_cvtsi32_si128( b );
	xmm3 = _mm_cvtsi32_si128( alpha );

	xmm1 = _mm_unpacklo_epi8( xmm1, xmm0 ); // a:a:a:a
	xmm2 = _mm_unpacklo_epi8( xmm2, xmm0 ); // b:b:b:b
	xmm3 = _mm_shufflelo_epi16( xmm3, 0 ); // alpha:alpha:alpha:alpha

	xmm1 = _mm_sub_epi16( xmm1, xmm2 ); // (a - b)
	xmm1 = _mm_mullo_epi16( xmm1, xmm3 ); // (a - b) * alpha
	xmm1 = _mm_srli_epi16( xmm1, 8 ); // ((a - b) * alpha) / 256
	xmm1 = _mm_add_epi8( xmm1, xmm2 ); // ((a - b) * alpha) / 256 + b

	xmm1 = _mm_packus_epi16( xmm1, xmm0 );
	color = _mm_cvtsi128_si32( xmm1 );

	return color;
#else
	const int ap = alpha;
	const int bp = 256 - ap;
	BYTE valR = (BYTE)((GetRValue(a) * ap + GetRValue(b) * bp) / 256);
	BYTE valG = (BYTE)((GetGValue(a) * ap + GetGValue(b) * bp) / 256);
	BYTE valB = (BYTE)((GetBValue(a) * ap + GetBValue(b) * bp) / 256);
	return RGB(valR, valG, valB);
#endif
}

COLORREF CEditView::GetTextColorByColorInfo2(const ColorInfo& info, const ColorInfo& info2)
{
#if REI_MOD_SELAREA == 0 && REI_MOD_SP_COLOR == 0
	if( info.m_sColorAttr.m_cTEXT != info.m_sColorAttr.m_cBACK ){
		return info.m_sColorAttr.m_cTEXT;
	}
	// ���]�\��
	if( info.m_sColorAttr.m_cBACK == m_crBack ){
		return  info2.m_sColorAttr.m_cTEXT ^ 0x00FFFFFF;
	}
#endif // rei_
#if REI_MOD_SELAREA
	static int nBlendPer = RegGetDword(L"SelectAreaTextBlendPer", REI_MOD_SELAREA_TEXT_BLEND_PER);
	int alpha = 255*nBlendPer/100;
#else
	int alpha = 255*30/100; // 30%
#endif // rei_
	return MakeColor2(info.m_sColorAttr.m_cTEXT, info2.m_sColorAttr.m_cTEXT, alpha);
}

COLORREF CEditView::GetBackColorByColorInfo2(const ColorInfo& info, const ColorInfo& info2)
{
#if REI_MOD_SELAREA == 0 && REI_MOD_SP_COLOR == 0
	if( info.m_sColorAttr.m_cTEXT != info.m_sColorAttr.m_cBACK ){
		return info.m_sColorAttr.m_cBACK;
	}
	// ���]�\��
	if( info.m_sColorAttr.m_cBACK == m_crBack ){
		return  info2.m_sColorAttr.m_cBACK ^ 0x00FFFFFF;
	}
#endif // rei_
#if REI_MOD_SELAREA
	if (info.m_sColorAttr.m_cTEXT == RGB(255,0,255)) { // �e�L�X�g�J���[���}�[���^�������炻�̂܂�
		return info.m_sColorAttr.m_cBACK;
	}
	static int nBlendPer = RegGetDword(L"SelectAreaBackBlendPer", REI_MOD_SELAREA_BACK_BLEND_PER);
	int alpha = 255*nBlendPer/100;
#else
	int alpha = 255*30/100; // 30%
#endif // rei_
	return MakeColor2(info.m_sColorAttr.m_cBACK, info2.m_sColorAttr.m_cBACK, alpha);
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                           �`��                              //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

void CEditView::OnPaint( HDC _hdc, PAINTSTRUCT *pPs, BOOL bDrawFromComptibleBmp )
{
	if (m_pcEditWnd->m_pPrintPreview) {
		return;
	}
	bool bChangeFont = m_bMiniMap;
	if( bChangeFont ){
		SelectCharWidthCache( CWM_FONT_MINIMAP, CWM_CACHE_LOCAL );
	}
	OnPaint2( _hdc, pPs, bDrawFromComptibleBmp );
	if( bChangeFont ){
		SelectCharWidthCache( CWM_FONT_EDIT, m_pcEditWnd->GetLogfontCacheMode() );
	}
}

/*! �ʏ�̕`�揈�� new 
	@param pPs  pPs.rcPaint �͐������K�v������
	@param bDrawFromComptibleBmp  TRUE ��ʃo�b�t�@����hdc�ɍ�悷��(�R�s�[���邾��)�B
			TRUE�̏ꍇ�ApPs.rcPaint�̈�O�͍�悳��Ȃ����AFALSE�̏ꍇ�͍�悳��鎖������B
			�݊�DC/BMP�������ꍇ�́A���ʂ̍�揈��������B

	@date 2007.09.09 Moca ���X����������Ă�����O�p�����[�^��bUseMemoryDC��bDrawFromComptibleBmp�ɕύX�B
	@date 2009.03.26 ryoji �s�ԍ��̂ݕ`���ʏ�̍s�`��ƕ����i�������j
*/
void CEditView::OnPaint2( HDC _hdc, PAINTSTRUCT *pPs, BOOL bDrawFromComptibleBmp )
{
//	MY_RUNNINGTIMER( cRunningTimer, "CEditView::OnPaint" );
	CGraphics gr(_hdc);

	// 2004.01.28 Moca �f�X�N�g�b�v�ɍ�悵�Ȃ��悤��
	if( NULL == GetHwnd() || NULL == _hdc )return;

	if( !GetDrawSwitch() )return;
	//@@@
#if 0
	::MYTRACE( _T("OnPaint(%d,%d)-(%d,%d) : %d\n"),
		pPs->rcPaint.left,
		pPs->rcPaint.top,
		pPs->rcPaint.right,
		pPs->rcPaint.bottom,
		bDrawFromComptibleBmp
		);
#endif
	
	// From Here 2007.09.09 Moca �݊�BMP�ɂ���ʃo�b�t�@
	// �݊�BMP����̓]���݂̂ɂ����
	if( bDrawFromComptibleBmp
		&& m_hdcCompatDC && m_hbmpCompatBMP ){
		::BitBlt(
			gr,
			pPs->rcPaint.left,
			pPs->rcPaint.top,
			pPs->rcPaint.right - pPs->rcPaint.left,
			pPs->rcPaint.bottom - pPs->rcPaint.top,
			m_hdcCompatDC,
			pPs->rcPaint.left,
			pPs->rcPaint.top,
			SRCCOPY
		);
		if ( m_pcEditWnd->GetActivePane() == m_nMyIndex ){
			/* �A�N�e�B�u�y�C���́A�A���_�[���C���`�� */
			GetCaret().m_cUnderLine.CaretUnderLineON( true, false );
		}
		return;
	}
#if REI_OUTPUT_DEBUG_STRING
	::OutputDebugStringW(L"OnPaint2 start.\n");
#endif // rei_
	if( m_hdcCompatDC && NULL == m_hbmpCompatBMP
		 || m_nCompatBMPWidth < (pPs->rcPaint.right - pPs->rcPaint.left)
		 || m_nCompatBMPHeight < (pPs->rcPaint.bottom - pPs->rcPaint.top) ){
		RECT rect;
		::GetWindowRect( this->GetHwnd(), &rect );
		CreateOrUpdateCompatibleBitmap( rect.right - rect.left, rect.bottom - rect.top );
	}
	// To Here 2007.09.09 Moca

	// �L�����b�g���B��
	bool bCaretShowFlag_Old = GetCaret().GetCaretShowFlag();	// 2008.06.09 ryoji
	GetCaret().HideCaret_( this->GetHwnd() ); // 2002/07/22 novice


	RECT			rc;
	int				nLineHeight = GetTextMetrics().GetHankakuDy();
	int				nCharDx = GetTextMetrics().GetHankakuDx();

	//�T�|�[�g
	CTypeSupport cTextType(this,COLORIDX_TEXT);

//@@@ 2001.11.17 add start MIK
	//�ύX������΃^�C�v�ݒ���s���B
	if( m_pTypeData->m_bUseRegexKeyword || m_cRegexKeyword->m_bUseRegexKeyword ) //OFF�Ȃ̂ɑO��̃f�[�^���c���Ă�
	{
		//�^�C�v�ʐݒ������B�ݒ�ς݂��ǂ����͌Ăѐ�Ń`�F�b�N����B
		m_cRegexKeyword->RegexKeySetTypes(m_pTypeData);
	}
//@@@ 2001.11.17 add end MIK

	bool bTransText = IsBkBitmap();
	// �������c�b�𗘗p�����ĕ`��̏ꍇ�͕`���̂c�b��؂�ւ���
	HDC hdcOld = 0;
	// 2007.09.09 Moca bUseMemoryDC��L�����B
	// bUseMemoryDC = FALSE;
	BOOL bUseMemoryDC = (m_hdcCompatDC != NULL);
	assert_warning(gr != m_hdcCompatDC);
	bool bClipping = false;
	if( bUseMemoryDC ){
		hdcOld = gr;
		gr = m_hdcCompatDC;
	}else{
		if( bTransText || pPs->rcPaint.bottom - pPs->rcPaint.top <= 2 || pPs->rcPaint.right - pPs->rcPaint.left <= 2 ){
			// ���ߏ����̏ꍇ�t�H���g�̗֊s���d�˓h��ɂȂ邽�ߎ����ŃN���b�s���O�̈��ݒ�
			// 2�ȉ��͂��Ԃ�A���_�[���C���E�J�[�\���s�c���̍��
			// MemoryDC�̏ꍇ�͓]������`�N���b�s���O�̑���ɂȂ��Ă���
			gr.SetClipping(pPs->rcPaint);
			bClipping = true;
		}
	}

	/* 03/02/18 �Ί��ʂ̋����\��(����) ai */
	if( !bUseMemoryDC ){
		// MemoryDC���ƃX�N���[�����ɐ�Ɋ��ʂ����\������ĕs���R�Ȃ̂Ō�ł��B
		DrawBracketPair( false );
	}

	CEditView& cActiveView = m_pcEditWnd->GetActiveView();
	m_nPageViewTop = cActiveView.GetTextArea().GetViewTopLine();
	m_nPageViewBottom = cActiveView.GetTextArea().GetBottomLine();

	// �w�i�̕\��
	if( bTransText ){
		HDC hdcBgImg = CreateCompatibleDC(gr);
		HBITMAP hOldBmp = (HBITMAP)::SelectObject(hdcBgImg, m_pcEditDoc->m_hBackImg);
		DrawBackImage(gr, pPs->rcPaint, hdcBgImg);
		SelectObject(hdcBgImg, hOldBmp);
		DeleteObject(hdcBgImg);
	}

	/* ���[���[�ƃe�L�X�g�̊Ԃ̗]�� */
	//@@@ 2002.01.03 YAZAKI �]����0�̂Ƃ��͖��ʂł����B
	if ( GetTextArea().GetTopYohaku() ){
		if( !bTransText ){
			rc.left   = 0;
			rc.top    = GetTextArea().GetRulerHeight();
			rc.right  = GetTextArea().GetAreaRight();
			rc.bottom = GetTextArea().GetAreaTop();
			cTextType.FillBack(gr,rc);
		}
	}

	/* �s�ԍ��̕\�� */
	//	From Here Sep. 7, 2001 genta
	//	Sep. 23, 2002 genta �s�ԍ���\���ł��s�ԍ��F�̑т�����̂Ō��Ԃ𖄂߂�
	if( GetTextArea().GetTopYohaku() ){
		if( bTransText && m_pTypeData->m_ColorInfoArr[COLORIDX_GYOU].m_sColorAttr.m_cBACK == cTextType.GetBackColor() ){
		}else{
			rc.left   = 0;
			rc.top    = GetTextArea().GetRulerHeight();
			rc.right  = GetTextArea().GetLineNumberWidth(); //	Sep. 23 ,2002 genta �]���̓e�L�X�g�F�̂܂܎c��
			rc.bottom = GetTextArea().GetAreaTop();
			gr.SetTextBackColor(m_pTypeData->m_ColorInfoArr[COLORIDX_GYOU].m_sColorAttr.m_cBACK);
			gr.FillMyRectTextBackColor(rc);
		}
	}
	//	To Here Sep. 7, 2001 genta

	::SetBkMode( gr, TRANSPARENT );

	cTextType.SetGraphicsState_WhileThisObj(gr);


	int nTop = pPs->rcPaint.top;

	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//           �`��J�n���C�A�E�g��΍s -> nLayoutLine             //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	CLayoutInt nLayoutLine;
	if( 0 > nTop - GetTextArea().GetAreaTop() ){
		nLayoutLine = GetTextArea().GetViewTopLine(); //�r���[�㕔����`��
	}else{
		nLayoutLine = GetTextArea().GetViewTopLine() + CLayoutInt( ( nTop - GetTextArea().GetAreaTop() ) / nLineHeight ); //�r���[�r������`��
	}

	// �� �����ɂ������`��͈͂� 260 �������[���o�b�N������ GetColorIndex() �ɋz��	// 2009.02.11 ryoji

	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//          �`��I�����C�A�E�g��΍s -> nLayoutLineTo            //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	CLayoutInt nLayoutLineTo = GetTextArea().GetViewTopLine()
		+ CLayoutInt( ( pPs->rcPaint.bottom - GetTextArea().GetAreaTop() + (nLineHeight - 1) ) / nLineHeight ) - 1;	// 2007.02.17 ryoji �v�Z�𐸖���


	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                         �`����W                            //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	DispPos sPos(GetTextMetrics().GetHankakuDx(),GetTextMetrics().GetHankakuDy());
	sPos.InitDrawPos(CMyPoint(
		GetTextArea().GetAreaLeft() - (Int)GetTextArea().GetViewLeftCol() * nCharDx,
		GetTextArea().GetAreaTop() + (Int)( nLayoutLine - GetTextArea().GetViewTopLine() ) * nLineHeight
	));
	sPos.SetLayoutLineRef(nLayoutLine);


	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                      �S���̍s��`��                         //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

	//�K�v�ȍs��`�悷��	// 2009.03.26 ryoji �s�ԍ��̂ݕ`���ʏ�̍s�`��ƕ����i�������j
	if(pPs->rcPaint.right <= GetTextArea().GetAreaLeft()){
		while(sPos.GetLayoutLineRef() <= nLayoutLineTo)
		{
			if(!sPos.GetLayoutRef())
				break;

			//1�s�`��i�s�ԍ��̂݁j
			GetTextDrawer().DispLineNumber(
				gr,
				sPos.GetLayoutLineRef(),
				sPos.GetDrawPos().y
			);
			//�s��i�߂�
			sPos.ForwardDrawLine(1);		//�`��Y���W�{�{
			sPos.ForwardLayoutLineRef(1);	//���C�A�E�g�s�{�{
		}
	}else{
		while(sPos.GetLayoutLineRef() <= nLayoutLineTo)
		{
			//�`��X�ʒu���Z�b�g
			sPos.ResetDrawCol();

			//1�s�`��
			bool bDispResult = DrawLogicLine(
				gr,
				&sPos,
				nLayoutLineTo
			);

			if(bDispResult){
				// EOF�ĕ`��Ή�
				nLayoutLineTo++;
				int nBackImageTop = pPs->rcPaint.bottom;
				pPs->rcPaint.bottom += nLineHeight;
				if(bClipping){
					gr.SetClipping(pPs->rcPaint);
				}
				if(bTransText){
					HDC hdcBgImg = CreateCompatibleDC(gr);
					HBITMAP hOldBmp = (HBITMAP)::SelectObject(hdcBgImg, m_pcEditDoc->m_hBackImg);
					RECT rc = pPs->rcPaint;
					rc.top = nBackImageTop;
					DrawBackImage(gr, rc, hdcBgImg);
					SelectObject(hdcBgImg, hOldBmp);
					DeleteObject(hdcBgImg);
				}
			}
		}
	}

	cTextType.RewindGraphicsState(gr);


	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                       ���[���[�`��                          //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	if ( pPs->rcPaint.top < GetTextArea().GetRulerHeight() ) { // ���[���[���ĕ`��͈͂ɂ���Ƃ��̂ݍĕ`�悷�� 2002.02.25 Add By KK
		GetRuler().SetRedrawFlag(); //2002.02.25 Add By KK ���[���[�S�̂�`��B
		GetRuler().DispRuler( gr );
	}

	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                     ���̑���n���Ȃ�                        //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	/* �������c�b�𗘗p�����ĕ`��̏ꍇ�̓������c�b�ɕ`�悵�����e����ʂփR�s�[���� */
	if( bUseMemoryDC ){
		// 2010.10.11 ��ɕ`���Ɣw�i�Œ�̃X�N���[���Ȃǂł̕\�����s���R�ɂȂ�
		DrawBracketPair( false );

		::BitBlt(
			hdcOld,
			pPs->rcPaint.left,
			pPs->rcPaint.top,
			pPs->rcPaint.right - pPs->rcPaint.left,
			pPs->rcPaint.bottom - pPs->rcPaint.top,
			gr,
			pPs->rcPaint.left,
			pPs->rcPaint.top,
			SRCCOPY
		);
	}

	// From Here 2007.09.09 Moca �݊�BMP�ɂ���ʃo�b�t�@
	//     �A���_�[���C���`���������DC����̃R�s�[�O���������Ɉړ�
	if ( m_pcEditWnd->GetActivePane() == m_nMyIndex ){
		/* �A�N�e�B�u�y�C���́A�A���_�[���C���`�� */
		GetCaret().m_cUnderLine.CaretUnderLineON( true, false );
	}
	// To Here 2007.09.09 Moca

	/* 03/02/18 �Ί��ʂ̋����\��(�`��) ai */
	DrawBracketPair( true );

	/* �L�����b�g�����݈ʒu�ɕ\�����܂� */
	if( bCaretShowFlag_Old )	// 2008.06.09 ryoji
		GetCaret().ShowCaret_( this->GetHwnd() ); // 2002/07/22 novice
	
#if REI_OUTPUT_DEBUG_STRING
	::OutputDebugStringW(L"OnPaint2 finish.\n");
#endif // rei_
	return;
}

/*!
	�s�̃e�L�X�g�^�I����Ԃ̕`��
	1���1���W�b�N�s������悷��B

	@return EOF����悵����true

	@date 2001.02.17 MIK
	@date 2001.12.21 YAZAKI ���s�L���̕`��������ύX
	@date 2007.08.31 kobake ���� bDispBkBitmap ���폜
*/
bool CEditView::DrawLogicLine(
	HDC				_hdc,			//!< [in]     ���Ώ�
	DispPos*		_pDispPos,		//!< [in,out] �`�悷��ӏ��A�`�挳�\�[�X
	CLayoutInt		nLineTo			//!< [in]     ���I�����郌�C�A�E�g�s�ԍ�
)
{
//	MY_RUNNINGTIMER( cRunningTimer, "CEditView::DrawLogicLine" );
	bool bDispEOF = false;
	SColorStrategyInfo _sInfo;
	SColorStrategyInfo* pInfo = &_sInfo;
	pInfo->m_gr.Init(_hdc);
	pInfo->m_pDispPos = _pDispPos;
	pInfo->m_pcView = this;

	//CColorStrategyPool������
	CColorStrategyPool* pool = CColorStrategyPool::getInstance();
	pool->SetCurrentView(this);
	pool->NotifyOnStartScanLogic();
	bool bSkipBeforeLayout = pool->IsSkipBeforeLayout();

	//DispPos��ۑ����Ă���
	pInfo->m_sDispPosBegin = *pInfo->m_pDispPos;

	//�������镶���ʒu
	pInfo->m_nPosInLogic = CLogicInt(0); //���J�n

	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//          �_���s�f�[�^�̎擾 -> pLine, pLineLen              //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	// �O�s�̍ŏI�ݒ�F
	{
		const CLayout* pcLayout = pInfo->m_pDispPos->GetLayoutRef();
		if( bSkipBeforeLayout ){
			EColorIndexType eRet = COLORIDX_TEXT;
			const CLayoutColorInfo* colorInfo = NULL;
			if( pcLayout ){
				eRet = pcLayout->GetColorTypePrev(); // COLORIDX_TEXT�̂͂�
				colorInfo = pcLayout->GetColorInfo();
			}
			pInfo->m_pStrategy = pool->GetStrategyByColor(eRet);
			if( pInfo->m_pStrategy ){
				pInfo->m_pStrategy->InitStrategyStatus();
				pInfo->m_pStrategy->SetStrategyColorInfo(colorInfo);
			}
		}else{
			CColor3Setting cColor = GetColorIndex(pcLayout, pInfo->m_pDispPos->GetLayoutLineRef(), 0, pInfo, true);
			SetCurrentColor(pInfo->m_gr, cColor.eColorIndex, cColor.eColorIndex2, cColor.eColorIndexBg);
		}
	}

	//�J�n���W�b�N�ʒu���Z�o
	{
		const CLayout* pcLayout = pInfo->m_pDispPos->GetLayoutRef();
		pInfo->m_nPosInLogic = pcLayout?pcLayout->GetLogicOffset():CLogicInt(0);
	}

	for (;;) {
		//�Ώۍs���`��͈͊O��������I��
		if( GetTextArea().GetBottomLine() < pInfo->m_pDispPos->GetLayoutLineRef() ){
			pInfo->m_pDispPos->SetLayoutLineRef(nLineTo + CLayoutInt(1));
			break;
		}
		if( nLineTo < pInfo->m_pDispPos->GetLayoutLineRef() ){
			break;
		}

		//���C�A�E�g�s��1�s�`��
		bDispEOF = DrawLayoutLine(pInfo);

		//�s��i�߂�
		CLogicInt nOldLogicLineNo = pInfo->m_pDispPos->GetLayoutRef()->GetLogicLineNo();
		pInfo->m_pDispPos->ForwardDrawLine(1);		//�`��Y���W�{�{
		pInfo->m_pDispPos->ForwardLayoutLineRef(1);	//���C�A�E�g�s�{�{

		// ���W�b�N�s��`�悵�I������甲����
		if(pInfo->m_pDispPos->GetLayoutRef()->GetLogicLineNo()!=nOldLogicLineNo){
			break;
		}

		// nLineTo�𒴂����甲����
		if(pInfo->m_pDispPos->GetLayoutLineRef() >= nLineTo + CLayoutInt(1)){
			break;
		}
	}

	return bDispEOF;
}

/*!
	���C�A�E�g�s��1�s�`��
*/
//���s�L����`�悵���ꍇ��true��Ԃ��H
bool CEditView::DrawLayoutLine(SColorStrategyInfo* pInfo)
{
	bool bDispEOF = false;
	CTypeSupport cTextType(this,COLORIDX_TEXT);

	const CLayout* pcLayout = pInfo->m_pDispPos->GetLayoutRef(); //m_pcEditDoc->m_cLayoutMgr.SearchLineByLayoutY( pInfo->pDispPos->GetLayoutLineRef() );

	// ���C�A�E�g���
	if( pcLayout ){
		pInfo->m_pLineOfLogic = pcLayout->GetDocLineRef()->GetPtr();
	}
	else{
		pInfo->m_pLineOfLogic = NULL;
	}

	//������Q��
	const CDocLine* pcDocLine = pInfo->GetDocLine();
	CStringRef cLineStr = pcDocLine->GetStringRefWithEOL();

	// �`��͈͊O�̏ꍇ�͐F�ؑւ����Ŕ�����
	if(pInfo->m_pDispPos->GetDrawPos().y < GetTextArea().GetAreaTop()){
		if(pcLayout){
			bool bChange = false;
			int nPosTo = pcLayout->GetLogicOffset() + pcLayout->GetLengthWithEOL();
			CColor3Setting cColor;
			while(pInfo->m_nPosInLogic < nPosTo){
				//�F�ؑ�
				bChange |= pInfo->CheckChangeColor(cLineStr);

				//1�����i��
				pInfo->m_nPosInLogic += CNativeW::GetSizeOfChar(
											cLineStr.GetPtr(),
											cLineStr.GetLength(),
											pInfo->m_nPosInLogic
										);
			}
			if( bChange ){
				pInfo->DoChangeColor(&cColor);
				SetCurrentColor(pInfo->m_gr, cColor.eColorIndex, cColor.eColorIndex2, cColor.eColorIndexBg);
			}
		}
		return false;
	}

	// �R���t�B�O
	int nLineHeight = GetTextMetrics().GetHankakuDy();  //�s�̏c���H
	CTypeSupport	cCaretLineBg(this, COLORIDX_CARETLINEBG);
	CTypeSupport	cEvenLineBg(this, COLORIDX_EVENLINEBG);
	CTypeSupport	cPageViewBg(this, COLORIDX_PAGEVIEW);
#if REI_MOD_COMMENT
  static DWORD comment_type_flag = RegGetDword(L"CommentType", 0x01);
  bool comment_color_whole_line = !!(comment_type_flag & 0x01);
	int comment_mode = 0;
	CTypeSupport	cComment(this, COLORIDX_COMMENT);
#endif  // rei_
	CEditView& cActiveView = m_pcEditWnd->GetActiveView();
	CTypeSupport&	cBackType = (cCaretLineBg.IsDisp() &&
		GetCaret().GetCaretLayoutPos().GetY() == pInfo->m_pDispPos->GetLayoutLineRef() && !m_bMiniMap
			? cCaretLineBg
			: cEvenLineBg.IsDisp() && pInfo->m_pDispPos->GetLayoutLineRef() % 2 == 1 && !m_bMiniMap
				? cEvenLineBg
				: (cPageViewBg.IsDisp() && m_bMiniMap
					&& cActiveView.GetTextArea().GetViewTopLine() <= pInfo->m_pDispPos->GetLayoutLineRef()
					&& pInfo->m_pDispPos->GetLayoutLineRef() < cActiveView.GetTextArea().GetBottomLine())
						? cPageViewBg
						: cTextType);
	bool bTransText = IsBkBitmap();
	if( bTransText ){
		bTransText = cBackType.GetBackColor() == cTextType.GetBackColor();
	}

#if 0//REI_MOD_SP_COLOR
	// �s�w�i�`��
	{
		RECT rcClip;
		bool rcClipRet = GetTextArea().GenerateClipRectLine(&rcClip,*pInfo->m_pDispPos);
		if(rcClipRet){
			if( !bTransText ){
				cBackType.FillBack(pInfo->m_gr,rcClip);
			}
		}
	}
#endif

	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                        �s�ԍ��`��                           //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	GetTextDrawer().DispLineNumber(
		pInfo->m_gr,
		pInfo->m_pDispPos->GetLayoutLineRef(),
		pInfo->m_pDispPos->GetDrawPos().y
	);


	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                       �{���`��J�n                          //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	pInfo->m_pDispPos->ResetDrawCol();


	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                 �s��(�C���f���g)�w�i�`��                    //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	if(pcLayout && pcLayout->GetIndent()!=0)
	{
		RECT rcClip;
		if(!bTransText && GetTextArea().GenerateClipRect(&rcClip,*pInfo->m_pDispPos,(Int)pcLayout->GetIndent())){
			cBackType.FillBack(pInfo->m_gr,rcClip);
		}
		//�`��ʒu�i�߂�
		pInfo->m_pDispPos->ForwardDrawCol((Int)pcLayout->GetIndent());
	}


	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//                         �{���`��                            //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	bool bSkipRight = false; // ������`�悵�Ȃ��Ă����ꍇ�̓X�L�b�v����
	if(pcLayout){
		const CLayout* pcLayoutNext = pcLayout->GetNextLayout();
		if( NULL == pcLayoutNext ){
			bSkipRight = true;
		}else if( pcLayoutNext->GetLogicOffset() == 0 ){
			bSkipRight = true; // ���̍s�͕ʂ̃��W�b�N�s�Ȃ̂ŃX�L�b�v�\
		}
		if( !bSkipRight ){
			bSkipRight = CColorStrategyPool::getInstance()->IsSkipBeforeLayout();
		}
	}
	//�s�I�[�܂��͐܂�Ԃ��ɒB����܂Ń��[�v
	if(pcLayout){
		int nPosBgn = pInfo->m_nPosInLogic; // Logic
		CLayoutInt nDrawX = pInfo->m_pDispPos->GetDrawCol(); // Layout
		const CLayoutInt nDrawXLeft = GetTextArea().GetViewLeftCol() - GetTextMetrics().GetLayoutXDefault(CLayoutXInt(2));
#ifdef _UNICODE
		const int nDrawBlockLen = 1000; // ExtTextOut�̒��������ɂ�����Ȃ��K���Ȓl
#else
		const int nDrawBlockLen = 500;
#endif
		bool bDrawViewLeft = false;
		int nPosTo = pcLayout->GetLogicOffset() + pcLayout->GetLengthWithEOL();
		CFigureManager* pcFigureManager = CFigureManager::getInstance();
		while(pInfo->m_nPosInLogic < nPosTo){
			//1�������擾
			CFigure& cFigure = pcFigureManager->GetFigure(&cLineStr.GetPtr()[pInfo->GetPosInLogic()],
				cLineStr.GetLength() - pInfo->GetPosInLogic());

			if( !cFigure.IsFigureText() || nDrawBlockLen < pInfo->GetPosInLogic() - nPosBgn || (!bDrawViewLeft && nDrawXLeft <= nDrawX) ){
				if( (!bDrawViewLeft && nDrawXLeft <= nDrawX) ){
					// ������������ʂ̓r���Ő؂�Ȃ��悤�ɁA��ʍ��[��菭���O�Ɉ�x�o�͂���
					bDrawViewLeft = true;
				}
				if( 0 < pInfo->GetPosInLogic() - nPosBgn ){
					pcFigureManager->GetTextFigure().DrawImp(pInfo, nPosBgn, pInfo->GetPosInLogic() - nPosBgn);
					nPosBgn = pInfo->GetPosInLogic();
				}
			}

			//�F�ؑ�
			if( pInfo->CheckChangeColor(cLineStr) ){
				if( 0 < pInfo->GetPosInLogic() - nPosBgn ){
					pcFigureManager->GetTextFigure().DrawImp(pInfo, nPosBgn, pInfo->GetPosInLogic() - nPosBgn);
					nPosBgn = pInfo->GetPosInLogic();
				}
				CColor3Setting cColor;
				pInfo->DoChangeColor(&cColor);
				SetCurrentColor(pInfo->m_gr, cColor.eColorIndex, cColor.eColorIndex2, cColor.eColorIndexBg);
				
#if REI_MOD_COMMENT
        if (comment_color_whole_line) {
          if (pInfo->m_pStrategy && pInfo->m_pStrategy->GetStrategyColor() == COLORIDX_COMMENT) {
            comment_mode = 1;
          } else if (pInfo->m_pStrategy && pInfo->m_pStrategy->GetStrategyColor() == COLORIDX_BLOCK1) {
            comment_mode = 2;
          } else if (pInfo->m_pStrategy && pInfo->m_pStrategy->GetStrategyColor() == COLORIDX_BLOCK2) {
            comment_mode = 2;
          } else {
            if (comment_mode != 1) {
              comment_mode = 0;
            }
          }
        }
#endif  // rei_
			}

#if REI_MOD_COMMENT
      if (comment_color_whole_line) {
        if (pInfo->m_pStrategy && pInfo->m_pStrategy->GetStrategyColor() == COLORIDX_COMMENT) {
          comment_mode = 1;
        } else if (pInfo->m_pStrategy && pInfo->m_pStrategy->GetStrategyColor() == COLORIDX_BLOCK1) {
          comment_mode = 2;
        } else if (pInfo->m_pStrategy && pInfo->m_pStrategy->GetStrategyColor() == COLORIDX_BLOCK2) {
          comment_mode = 2;
        }

        if (comment_mode) {
          if (pInfo->m_cIndex.eColorIndex != COLORIDX_SELECT) {
            pInfo->m_cIndex.eColorIndex = COLORIDX_COMMENT;
          }
        }
      }
#endif  // rei_

			//1�����`��
			if( !cFigure.IsFigureText() ){
				cFigure.DrawImp(pInfo);
				nPosBgn = pInfo->GetPosInLogic();
				nDrawX = pInfo->m_pDispPos->GetDrawCol();
			}else{
				nDrawX += pcFigureManager->GetTextFigure().GetDrawSize(pInfo);
				pcFigureManager->GetTextFigure().FowardChars(pInfo);
			}
			if( bSkipRight && GetTextArea().GetRightCol() < nDrawX ){
				if( 0 < pInfo->GetPosInLogic() - nPosBgn ){
					pcFigureManager->GetTextFigure().DrawImp(pInfo, nPosBgn, pInfo->GetPosInLogic() - nPosBgn);
					nPosBgn = pInfo->GetPosInLogic();
				}
				pInfo->m_nPosInLogic = nPosTo;
				break;
			}
		}
		if( 0 < pInfo->GetPosInLogic() - nPosBgn ){
			pcFigureManager->GetTextFigure().DrawImp(pInfo, nPosBgn, pInfo->GetPosInLogic() - nPosBgn);
		}
	}

	// �K�v�Ȃ�EOF�`��
	void _DispEOF( CGraphics& gr, DispPos* pDispPos, const CEditView* pcView);
	if(pcLayout && pcLayout->GetNextLayout()==NULL && pcLayout->GetLayoutEol().GetLen()==0){
		// �L�����s��EOF
		_DispEOF(pInfo->m_gr,pInfo->m_pDispPos,this);
		bDispEOF = true;
	}
	else if(!pcLayout && pInfo->m_pDispPos->GetLayoutLineRef()==m_pcEditDoc->m_cLayoutMgr.GetLineCount()){
		// ��s��EOF
		const CLayout* pBottom = m_pcEditDoc->m_cLayoutMgr.GetBottomLayout();
		if(pBottom==NULL || (pBottom && pBottom->GetLayoutEol().GetLen())){
			_DispEOF(pInfo->m_gr,pInfo->m_pDispPos,this);
			bDispEOF = true;
		}
	}

	// �K�v�Ȃ�܂�Ԃ��L���`��
	if(pcLayout && pcLayout->GetLayoutEol().GetLen()==0 && pcLayout->GetNextLayout()!=NULL){
		_DispWrap(pInfo->m_gr,pInfo->m_pDispPos,this,pInfo->m_pDispPos->GetLayoutLineRef());
	}

	// �s���w�i�`��
	RECT rcClip;
	bool rcClipRet = GetTextArea().GenerateClipRectRight(&rcClip,*pInfo->m_pDispPos);
	if(rcClipRet){
		if( !bTransText ){
#if REI_MOD_COMMENT
		  if (comment_mode) {
		    cComment.FillBack(pInfo->m_gr,rcClip);
		  } else
#endif  // rei_
			cBackType.FillBack(pInfo->m_gr,rcClip);
		}
		CTypeSupport cSelectType(this, COLORIDX_SELECT);
		if( GetSelectionInfo().IsTextSelected() && cSelectType.IsDisp() ){
			// �I��͈͂̎w��F�F�K�v�Ȃ�e�L�X�g�̂Ȃ������̋�`�I�������
			CLayoutRange selectArea = GetSelectionInfo().GetSelectAreaLine(pInfo->m_pDispPos->GetLayoutLineRef(), pcLayout);
			// 2010.10.04 �X�N���[�����̑����Y��
			int nSelectFromPx = GetTextMetrics().GetHankakuDx() * (Int)(selectArea.GetFrom().x - GetTextArea().GetViewLeftCol());
			int nSelectToPx   = GetTextMetrics().GetHankakuDx() * (Int)(selectArea.GetTo().x - GetTextArea().GetViewLeftCol());
			if( nSelectFromPx < nSelectToPx && selectArea.GetTo().x != INT_MAX ){
				RECT rcSelect; // Pixel
				rcSelect.top    = pInfo->m_pDispPos->GetDrawPos().y;
				rcSelect.bottom = pInfo->m_pDispPos->GetDrawPos().y + GetTextMetrics().GetHankakuDy();
				rcSelect.left   = GetTextArea().GetAreaLeft() + nSelectFromPx;
				rcSelect.right  = GetTextArea().GetAreaLeft() + nSelectToPx;
				RECT rcDraw;
				if( ::IntersectRect(&rcDraw, &rcClip, &rcSelect) ){
					COLORREF color = GetBackColorByColorInfo2(cSelectType.GetColorInfo(), cBackType.GetColorInfo());
					if( color != cBackType.GetBackColor() ){
						pInfo->m_gr.FillSolidMyRect(rcDraw, color);
					}
				}
			}
		}
	}

	// �m�[�g���`��
	if( !m_bMiniMap ){
		GetTextDrawer().DispNoteLine(
			pInfo->m_gr,
			pInfo->m_pDispPos->GetDrawPos().y,
			pInfo->m_pDispPos->GetDrawPos().y + nLineHeight,
			GetTextArea().GetAreaLeft(),
			GetTextArea().GetAreaRight()
		);
	}

	// �w�茅�c���`��
	GetTextDrawer().DispVerticalLines(
		pInfo->m_gr,
		pInfo->m_pDispPos->GetDrawPos().y,
		pInfo->m_pDispPos->GetDrawPos().y + nLineHeight,
		CLayoutInt(0),
		CLayoutInt(-1)
	);

#if REI_MOD_WRAP_LINE
  {
    static bool no_wrap_line = !!RegGetDword(L"NoWrapLine", true);
    
    if (!no_wrap_line) {
      // �܂�Ԃ����c���`��
    	if( !m_bMiniMap ){
    		GetTextDrawer().DispWrapLine(
    			pInfo->m_gr,
    			pInfo->m_pDispPos->GetDrawPos().y,
    			pInfo->m_pDispPos->GetDrawPos().y + nLineHeight
    		);
    	}
    }
  }
#else
	// �܂�Ԃ����c���`��
	if( !m_bMiniMap ){
		GetTextDrawer().DispWrapLine(
			pInfo->m_gr,
			pInfo->m_pDispPos->GetDrawPos().y,
			pInfo->m_pDispPos->GetDrawPos().y + nLineHeight
		);
	}
#endif // rei_

	// ���]�`��
	if( pcLayout && GetSelectionInfo().IsTextSelected() ){
		DispTextSelected(
			pInfo->m_gr,
			pInfo->m_pDispPos->GetLayoutLineRef(),
			CMyPoint(pInfo->m_sDispPosBegin.GetDrawPos().x, pInfo->m_pDispPos->GetDrawPos().y),
			pcLayout->CalcLayoutWidth(CEditDoc::GetInstance(0)->m_cLayoutMgr) + CLayoutInt(pcLayout->GetLayoutEol().GetLen()?1:0)
		);
	}

	return bDispEOF;
}








/* �e�L�X�g���]

	@param hdc      
	@param nLineNum 
	@param x        
	@param y        
	@param nX       

	@note
	CCEditView::DrawLogicLine() �ł̍��(WM_PAINT)���ɁA1���C�A�E�g�s���܂Ƃ߂Ĕ��]�������邽�߂̊֐��B
	�͈͑I���̐����X�V�́ACEditView::DrawSelectArea() ���I���E���]�������s���B
	
*/
void CEditView::DispTextSelected(
	HDC				hdc,		//!< ���Ώۃr�b�g�}�b�v���܂ރf�o�C�X
	CLayoutInt		nLineNum,	//!< ���]�����Ώۃ��C�A�E�g�s�ԍ�(0�J�n)
	const CMyPoint&	ptXY,		//!< (���΃��C�A�E�g0���ڂ̍��[���W, �Ώۍs�̏�[���W)
	CLayoutInt		nX_Layout	//!< �Ώۍs�̏I�����ʒu�B�@[ABC\n]�Ȃ���s�̌���4
)
{
	CLayoutInt	nSelectFrom;
	CLayoutInt	nSelectTo;
	RECT		rcClip;
	int			nLineHeight = GetTextMetrics().GetHankakuDy();
	int			nCharWidth = GetTextMetrics().GetHankakuDx();
	HRGN		hrgnDraw;
	const CLayout* pcLayout = m_pcEditDoc->m_cLayoutMgr.SearchLineByLayoutY( nLineNum );
	CLayoutRange& sSelect = GetSelectionInfo().m_sSelect;

	/* �I��͈͓��̍s���� */
//	if( IsTextSelected() ){
		if( nLineNum >= sSelect.GetFrom().y && nLineNum <= sSelect.GetTo().y ){
			CLayoutRange selectArea = GetSelectionInfo().GetSelectAreaLine(nLineNum, pcLayout);
			nSelectFrom = selectArea.GetFrom().x;
			nSelectTo   = selectArea.GetTo().x;
			if( nSelectFrom == INT_MAX ){
				nSelectFrom = nX_Layout;
			}
			if( nSelectTo == INT_MAX ){
				nSelectTo = nX_Layout;
			}

			// 2006.03.28 Moca �\����O�Ȃ牽�����Ȃ�
			if( GetTextArea().GetRightCol() < nSelectFrom ){
				return;
			}
			if( nSelectTo < GetTextArea().GetViewLeftCol() ){	// nSelectTo == GetTextArea().GetViewLeftCol()�̃P�[�X�͌�łO�����}�b�`�łȂ����Ƃ��m�F���Ă��甲����
				return;
			}

			if( nSelectFrom < GetTextArea().GetViewLeftCol() ){
				nSelectFrom = GetTextArea().GetViewLeftCol();
			}
			rcClip.left   = ptXY.x + (Int)nSelectFrom * nCharWidth;
			rcClip.right  = ptXY.x + (Int)nSelectTo   * nCharWidth;
			rcClip.top    = ptXY.y;
			rcClip.bottom = ptXY.y + nLineHeight;

			bool bOMatch = false;

			// 2005/04/02 ����� �O�����}�b�`���Ɣ��]�����O�ƂȂ蔽�]����Ȃ��̂ŁA1/3�������������]������
			// 2005/06/26 zenryaku �I�������ŃL�����b�g�̎c�[���c������C��
			// 2005/09/29 ryoji �X�N���[�����ɃL�����b�g�̂悤�ȃS�~���\�����������C��
			if (GetSelectionInfo().IsTextSelected() && rcClip.right == rcClip.left &&
				sSelect.IsLineOne() &&
				sSelect.GetFrom().x >= GetTextArea().GetViewLeftCol())
			{
				HWND hWnd = ::GetForegroundWindow();
				if( hWnd && (hWnd == m_pcEditWnd->m_cDlgFind.GetHwnd() || hWnd == m_pcEditWnd->m_cDlgReplace.GetHwnd()) ){
					rcClip.right = rcClip.left + (nCharWidth/3 == 0 ? 1 : nCharWidth/3);
					bOMatch = true;
				}
			}
			if( rcClip.right == rcClip.left ){
				return;	//�O�����}�b�`�ɂ�锽�]���g���Ȃ�
			}

			// 2006.03.28 Moca �E�B���h�E�����傫���Ɛ��������]���Ȃ������C��
			if( rcClip.right > GetTextArea().GetAreaRight() ){
				rcClip.right = GetTextArea().GetAreaRight();
			}
			
			// �I��F�\���Ȃ甽�]���Ȃ�
			if( !bOMatch && CTypeSupport(this, COLORIDX_SELECT).IsDisp() ){
				return;
			}
			
			HBRUSH hBrush    = ::CreateSolidBrush( SELECTEDAREA_RGB );

			int    nROP_Old  = ::SetROP2( hdc, SELECTEDAREA_ROP2 );
			HBRUSH hBrushOld = (HBRUSH)::SelectObject( hdc, hBrush );
			hrgnDraw = ::CreateRectRgn( rcClip.left, rcClip.top, rcClip.right, rcClip.bottom );
			::PaintRgn( hdc, hrgnDraw );
			::DeleteObject( hrgnDraw );

			SetROP2( hdc, nROP_Old );
			SelectObject( hdc, hBrushOld );
			DeleteObject( hBrush );
		}
//	}
	return;
}







// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                       ��ʃo�b�t�@                          //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //


/*!
	��ʂ̌݊��r�b�g�}�b�v���쐬�܂��͍X�V����B
		�K�v�̖����Ƃ��͉������Ȃ��B
	
	@param cx �E�B���h�E�̍���
	@param cy �E�B���h�E�̕�
	@return true: �r�b�g�}�b�v�𗘗p�\ / false: �r�b�g�}�b�v�̍쐬�E�X�V�Ɏ��s

	@date 2007.09.09 Moca CEditView::OnSize���番���B
		�P���ɐ������邾�����������̂��A�d�l�ύX�ɏ]�����e�R�s�[��ǉ��B
		�T�C�Y�������Ƃ��͉������Ȃ��悤�ɕύX

	@par �݊�BMP�ɂ̓L�����b�g�E�J�[�\���ʒu���c���E�Ί��ʈȊO�̏���S�ď������ށB
		�I��͈͕ύX���̔��]�����́A��ʂƌ݊�BMP�̗�����ʁX�ɕύX����B
		�J�[�\���ʒu���c���ύX���ɂ́A�݊�BMP�����ʂɌ��̏��𕜋A�����Ă���B

*/
bool CEditView::CreateOrUpdateCompatibleBitmap( int cx, int cy )
{
	if( NULL == m_hdcCompatDC ){
		return false;
	}
	// �T�C�Y��64�̔{���Ő���
	int nBmpWidthNew  = ((cx + 63) & (0x7fffffff - 63));
	int nBmpHeightNew = ((cy + 63) & (0x7fffffff - 63));
	if( nBmpWidthNew != m_nCompatBMPWidth || nBmpHeightNew != m_nCompatBMPHeight ){
#if 0
	MYTRACE( _T("CEditView::CreateOrUpdateCompatibleBitmap( %d, %d ): resized\n"), cx, cy );
#endif
		HDC	hdc = ::GetDC( GetHwnd() );
		HBITMAP hBitmapNew = NULL;
		if( m_hbmpCompatBMP ){
			// BMP�̍X�V
			HDC hdcTemp = ::CreateCompatibleDC( hdc );
			hBitmapNew = ::CreateCompatibleBitmap( hdc, nBmpWidthNew, nBmpHeightNew );
			if( hBitmapNew ){
				HBITMAP hBitmapOld = (HBITMAP)::SelectObject( hdcTemp, hBitmapNew );
				// �O�̉�ʓ��e���R�s�[����
				::BitBlt( hdcTemp, 0, 0,
					t_min( nBmpWidthNew,m_nCompatBMPWidth ),
					t_min( nBmpHeightNew, m_nCompatBMPHeight ),
					m_hdcCompatDC, 0, 0, SRCCOPY );
				::SelectObject( hdcTemp, hBitmapOld );
				::SelectObject( m_hdcCompatDC, m_hbmpCompatBMPOld );
				::DeleteObject( m_hbmpCompatBMP );
			}
			::DeleteDC( hdcTemp );
		}else{
			// BMP�̐V�K�쐬
			hBitmapNew = ::CreateCompatibleBitmap( hdc, nBmpWidthNew, nBmpHeightNew );
		}
		if( hBitmapNew ){
			m_hbmpCompatBMP = hBitmapNew;
			m_nCompatBMPWidth = nBmpWidthNew;
			m_nCompatBMPHeight = nBmpHeightNew;
			m_hbmpCompatBMPOld = (HBITMAP)::SelectObject( m_hdcCompatDC, m_hbmpCompatBMP );
		}else{
			// �݊�BMP�̍쐬�Ɏ��s
			// ��������s���J��Ԃ��\���������̂�
			// m_hdcCompatDC��NULL�ɂ��邱�Ƃŉ�ʃo�b�t�@�@�\�����̃E�B���h�E�̂ݖ����ɂ���B
			//	2007.09.29 genta �֐����D������BMP�����
			UseCompatibleDC(FALSE);
		}
		::ReleaseDC( GetHwnd(), hdc );
	}
	return NULL != m_hbmpCompatBMP;
}


/*!
	�݊�������BMP���폜

	@note �����r���[����\���ɂȂ����ꍇ��
		�e�E�B���h�E����\���E�ŏ������ꂽ�ꍇ�ɍ폜�����B
	@date 2007.09.09 Moca �V�K�쐬 
*/
void CEditView::DeleteCompatibleBitmap()
{
	if( m_hbmpCompatBMP ){
		::SelectObject( m_hdcCompatDC, m_hbmpCompatBMPOld );
		::DeleteObject( m_hbmpCompatBMP );
		m_hbmpCompatBMP = NULL;
		m_hbmpCompatBMPOld = NULL;
		m_nCompatBMPWidth = -1;
		m_nCompatBMPHeight = -1;
	}
}



/** ��ʃL���b�V���pCompatibleDC��p�ӂ���

	@param[in] TRUE: ��ʃL���b�V��ON

	@date 2007.09.30 genta �֐���
*/
void CEditView::UseCompatibleDC(BOOL fCache)
{
	// From Here 2007.09.09 Moca �݊�BMP�ɂ���ʃo�b�t�@
	if( fCache ){
		if( m_hdcCompatDC == NULL ){
			HDC			hdc;
			hdc = ::GetDC( GetHwnd() );
			m_hdcCompatDC = ::CreateCompatibleDC( hdc );
			::ReleaseDC( GetHwnd(), hdc );
			DEBUG_TRACE(_T("CEditView::UseCompatibleDC: Created\n"), fCache);
		}
		else {
			DEBUG_TRACE(_T("CEditView::UseCompatibleDC: Reused\n"), fCache);
		}
	}
	else {
		//	CompatibleBitmap���c���Ă��邩������Ȃ��̂ōŏ��ɍ폜
		DeleteCompatibleBitmap();
		if( m_hdcCompatDC != NULL ){
			::DeleteDC( m_hdcCompatDC );
			DEBUG_TRACE(_T("CEditView::UseCompatibleDC: Deleted.\n"));
			m_hdcCompatDC = NULL;
		}
	}
}