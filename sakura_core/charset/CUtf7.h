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
#ifndef SAKURA_CUTF7_D489ED48_E52A_43DD_8124_CB439CA30FC8_H_
#define SAKURA_CUTF7_D489ED48_E52A_43DD_8124_CB439CA30FC8_H_

#include "CCodeBase.h"

class CUtf7 : public CCodeBase{
public:
	//CCodeBase�C���^�[�t�F�[�X
	EConvertResult CodeToUnicode(const CMemory& cSrc, CNativeW* pDst){ return UTF7ToUnicode(cSrc, pDst); }	//!< ����R�[�h �� UNICODE    �ϊ�
	EConvertResult UnicodeToCode(const CNativeW& cSrc, CMemory* pDst){ return UnicodeToUTF7(cSrc, pDst); }	//!< UNICODE    �� ����R�[�h �ϊ�
	void GetBom(CMemory* pcmemBom);	//!< BOM�f�[�^�擾
	void GetEol(CMemory* pcmemEol, EEolType eEolType);

public:
	//����
	static EConvertResult UTF7ToUnicode(const CMemory& cSrc, CNativeW* pDstMem);		// UTF-7     �� Unicode�R�[�h�ϊ� //2007.08.13 kobake �ǉ�
	static EConvertResult UnicodeToUTF7(const CNativeW& cSrc, CMemory* pDstMem);		// Unicode   �� UTF-7�R�[�h�ϊ�
//	static int MemBASE64_Encode( const char*, int, char**, int, int );/* Base64�G���R�[�h */  // convert/convert_util2.h �ֈړ�

protected:

	// 2008.11.10 �ϊ����W�b�N����������
	static int _Utf7SetDToUni_block( const char*, const int, wchar_t* );
	static int _Utf7SetBToUni_block( const char*, const int, wchar_t*, bool* );
	static int Utf7ToUni( const char*, const int, wchar_t*, bool* pbError );

	static int _UniToUtf7SetD_block( const wchar_t* pSrc, const int nSrcLen, char* pDst );
	static int _UniToUtf7SetB_block( const wchar_t* pSrc, const int nSrcLen, char* pDst );
	static int UniToUtf7( const wchar_t* pSrc, const int nSrcLen, char* pDst );


};

#endif /* SAKURA_CUTF7_D489ED48_E52A_43DD_8124_CB439CA30FC8_H_ */
/*[EOF]*/