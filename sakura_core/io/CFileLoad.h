/*!	@file
	@brief �������o�b�t�@�N���X�ւ̃t�@�C�����̓N���X

	@author Moca
	@date 2002/08/30 �V�K�쐬
*/
/*
	Copyright (C) 2002, Moca, genta
	Copyright (C) 2003, Moca, ryoji

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
#ifndef SAKURA_CFILELOAD_H_
#define SAKURA_CFILELOAD_H_

#include <Windows.h>
#include "CStream.h" //CError_FileOpen
#include "charset/CCodeBase.h"
#include "charset/CCodePage.h"
#include "util/design_template.h"

// VC6�Y�t�̃w�b�_�Œ�`����Ă܂���
#ifndef INVALID_SET_FILE_POINTER
 #define INVALID_SET_FILE_POINTER 0xFFFFFFFF
#endif // INVALID_SET_FILE_POINTER

struct SEncodingConfig;
class CCodeBase;

/*!
	�����R�[�h��ϊ����ăf�[�^���s�P�ʂŎ擾���邽�߂̃N���X
	@note �����I��FileOpen�����o���Ăяo���Ȃ��Ǝg���Ȃ�
		�t�@�C���|�C���^�����L����ƍ���̂ŁA�N���X�̃R�s�[�֎~
*/
class CFileLoad
{
public:

	CFileLoad( const SEncodingConfig& encode );
	~CFileLoad( void );

	//	Jul. 26, 2003 ryoji BOM�����ǉ�
	ECodeType FileOpen( LPCTSTR, bool bBigFile, ECodeType, int, bool* pbBomExist = NULL );		// �w�蕶���R�[�h�Ńt�@�C�����I�[�v������
	void FileClose( void );					// �����I�Ƀt�@�C�����N���[�Y����

	//! 1�s�f�[�^�����[�h���� ���A�N�Z�X�p
	EConvertResult ReadLine(
		CNativeW*	pUnicodeBuffer,	//!< [out] UNICODE�f�[�^�󂯎��o�b�t�@
		CEol*		pcEol			//!< [i/o]
	);

//	�������֐��S
//	cosnt char* ReadAtLine( int, int*, CEol* ); // �w��s�ڂ����[�h����
//	cosnt wchar_t* ReadAtLineW( int, int*, CEol* ); // �w��s�ڂ����[�h����(Unicode��)
//	bool ReadIgnoreLine( void ); // 1�s�ǂݔ�΂�

	//! �t�@�C���̓������擾����
	BOOL GetFileTime( FILETIME*, FILETIME*, FILETIME* ); // inline

	//	Jun. 08, 2003 Moca
	//! �J�����t�@�C���ɂ�BOM�����邩�H
	bool IsBomExist( void ){ return m_bBomExist; }

	//! ���݂̐i�s�����擾����(0% - 100%) �኱�덷���o��
	int GetPercent( void );

	//! �t�@�C���T�C�Y���擾����
	inline LONGLONG GetFileSize( void ){ return m_nFileSize; }

	static const int gm_nBufSizeDef; // ���[�h�p�o�b�t�@�T�C�Y�̏����l
//	static const int gm_nBufSizeMin; // ���[�h�p�o�b�t�@�T�C�Y�̐ݒ�\�ȍŒ�l

protected:
	// Oct. 19, 2002 genta �X�y���~�X�C��
//	void SeekBegin( void );		// �t�@�C���̐擪�ʒu�Ɉړ�����(BOM���l������)
	void Buffering( void );		// �o�b�t�@�Ƀf�[�^�����[�h����
	void ReadBufEmpty( void );	// �o�b�t�@����ɂ���

	// GetLextLine �� �����R�[�h�l����
	const char* GetNextLineCharCode( const char*, int, int*, int*, CEol*, int*, int* );
	EConvertResult ReadLine_core( CNativeW*, CEol* );

	int Read( void*, size_t ); // inline
	DWORD FilePointer( DWORD, DWORD ); // inline

	/* �����o�I�u�W�F�N�g */
	const SEncodingConfig* m_pEencoding;

//	LPTSTR	m_pszFileName;	// �t�@�C����
	HANDLE	m_hFile;		// �t�@�C���n���h��
	LONGLONG	m_nFileSize;	// �t�@�C���T�C�Y(64bit)
	LONGLONG	m_nFileDataLen;	// �t�@�C���f�[�^������BOM�����������o�C�g��
	LONGLONG	m_nReadLength;	// ���݂܂łɃ��[�h�����f�[�^�̍��v�o�C�g��(BOM�����܂܂Ȃ�)
	int		m_nLineIndex;	// ���݃��[�h���Ă���_���s(0�J�n)
	ECodeType	m_CharCode;		// �����R�[�h
	CCodeBase*	m_pCodeBase;	////
	EEncodingTrait	m_encodingTrait;
	CMemory			m_memEols[3];
	bool	m_bEolEx;		//!< CR/LF�ȊO��EOL���L����
	int		m_nMaxEolLen;	//!< EOL�̒���
	bool	m_bBomExist;	// �t�@�C����BOM���t���Ă��邩 Jun. 08, 2003 Moca 
	int		m_nFlag;		// �����R�[�h�̕ϊ��I�v�V����
	//	Jun. 13, 2003 Moca
	//	��Ԃ�enum�Ƃ��Ă킩��₷���D
	enum enumFileLoadMode{
		FLMODE_CLOSE = 0, //!< �������
		FLMODE_OPEN, //!< �t�@�C���I�[�v���̂�
		FLMODE_READY, //!< ���A�N�Z�XOK
		FLMODE_READBUFEND //!<�t�@�C���̏I�[�܂Ńo�b�t�@�ɓ��ꂽ
	};
	enumFileLoadMode	m_eMode;		// ���݂̓ǂݍ��ݏ��

	// �ǂݍ��݃o�b�t�@�n
	char*	m_pReadBuf;			// �ǂݍ��݃o�b�t�@�ւ̃|�C���^
	int		m_nReadBufSize;		// �ǂݍ��݃o�b�t�@�̎��ۂɊm�ۂ��Ă���T�C�Y
	int		m_nReadDataLen;		// �ǂݍ��݃o�b�t�@�̗L���f�[�^�T�C�Y
	int		m_nReadBufOffSet;	// �ǂݍ��݃o�b�t�@���̃I�t�Z�b�g(���̍s���ʒu)
//	int		m_nReadBufSumSize;	// ���܂łɃo�b�t�@�ɓǂݍ��񂾃f�[�^�̍��v�T�C�Y
	CMemory m_cLineBuffer;
	CNativeW m_cLineTemp;
	int		m_nReadOffset2;
	EConvertResult m_nTempResult;

private:
	DISALLOW_COPY_AND_ASSIGN(CFileLoad);
}; // class CFileLoad

// �C�����C���֐��S

// public
inline BOOL CFileLoad::GetFileTime( FILETIME* pftCreate, FILETIME* pftLastAccess, FILETIME* pftLastWrite ){
	return ::GetFileTime( m_hFile, pftCreate, pftLastAccess, pftLastWrite );
}

// protected
inline int CFileLoad::Read( void* pBuf, size_t nSize )
{
	DWORD ReadSize;
	if( !::ReadFile( m_hFile, pBuf, nSize, &ReadSize, NULL ) )
		throw CError_FileRead();
	return (int)ReadSize;
}

// protected
inline DWORD CFileLoad::FilePointer( DWORD offset, DWORD origin )
{
	DWORD fp;
	if( INVALID_SET_FILE_POINTER == ( fp = ::SetFilePointer( m_hFile, offset, NULL, FILE_BEGIN ) ) )
		throw CError_FileRead();
	return fp;
}


#endif /* SAKURA_CFILELOAD_H_ */

