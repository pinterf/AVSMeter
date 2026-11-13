/////////////////////////////////////////////////////////////////////////////
/* 
DESCRIPTION:
	CFileVersionInfo - Class for getting file version information
	http://www.codeproject.com/file/VersionInfo.asp

NOTES:
	Copyright(C) Armen Hakobyan, 2003
	mailto:armen.h@web.am
	
VERSION HISTORY:
	25 Jul 2003 - Posted the article
	27 Jul 2003 - Added DLLVERSIONINFO2 support to DllGetVersion
	21 Jan 2004 - Added GetFileVersionMajor, GetFileVersionMinor,
				  GetFileVersionBuild, GetFileVersionQFE functions
	29 Jan 2004 - Added GetProductVersionMajor, GetProductVersionMinor,
				  GetProductVersionBuild, GetProductVersionQFE functions
*/
/////////////////////////////////////////////////////////////////////////////

#ifndef __VERINFO_H__ 
#define __VERINFO_H__

#if defined( _MSC_VER ) && ( _MSC_VER >= 1020 )
	#pragma once
#endif

/////////////////////////////////////////////////////////////////////////////

#ifndef	_INC_SHLWAPI
	#include <Shlwapi.h>
#endif
#pragma comment( lib, "shlwapi.lib" )

#ifndef VER_H
	#include <WinVer.h>
#endif

#ifndef _T
	#ifndef _INC_TCHAR
		#include <TChar.h>
	#endif
#endif

#ifndef	ASSERT
	#ifndef _INC_CRTDBG
		#include <CrtDbg.h>
	#endif
	#define ASSERT( x )			_ASSERTE( x )
#endif

#ifndef		_free
	#define _free( p )			{ if( p != NULL ){ free( p ); p = NULL; } }
#endif

#ifndef		ASSERT_RETURN
	#define ASSERT_RETURN( x )	{ ASSERT( 0 ); return x; }
#endif

#ifndef DLLVER_MAJOR_MASK

	typedef struct _DLLVERSIONINFO2 {
		DLLVERSIONINFO	info1;
		DWORD			dwFlags;				// No flags currently defined
		ULONGLONG		ullVersion;
	} DLLVERSIONINFO2;

#endif // DLLVER_MAJOR_MASK

#ifndef MAKEDLLVERULL

	#define MAKEDLLVERULL( major, minor, build, qfe )\
		( ( (ULONGLONG)(major) << 48 ) |			 \
		(   (ULONGLONG)(minor) << 32 ) |			 \
		(   (ULONGLONG)(build) << 16 ) |			 \
		(   (ULONGLONG)(  qfe) <<  0 ) )

#endif // MAKEDLLVERULL


STDAPI_( HRESULT ) DllGetVersion( IN HMODULE hModule, OUT DLLVERSIONINFO* lpDVI );

typedef enum _VI_CP {
	VI_CP_ASCII				= 0,	// 7-bit ASCII 
	VI_CP_JAPAN				= 932,	// Japan (Shift - JIS X-0208) 
	VI_CP_KOREA				= 949,	// Korea (Shift - KSC 5601) 
	VI_CP_TAIWAN			= 950,	// Taiwan (Big5) 
	VI_CP_UNICODE			= 1200,	// Unicode 
	VI_CP_LATIN2			= 1250,	// Latin-2 (Eastern European) 
	VI_CP_CYRILLIC			= 1251,	// Cyrillic 
	VI_CP_MULTILNG			= 1252,	// Multilingual 
	VI_CP_GREEK				= 1253,	// Greek 
	VI_CP_TURKISH			= 1254,	// Turkish 
	VI_CP_HEBREW			= 1255,	// Hebrew 
	VI_CP_ARABIC			= 1256	// Arabic 
} VI_CP;

typedef enum _VI_STR {
	VI_STR_COMMENTS			= 0,	// Comments
	VI_STR_COMPANYNAME		= 1,	// CompanyName
	VI_STR_FILEDESCRIPTION	= 2,	// FileDescription
	VI_STR_FILEVERSION		= 3,	// FileVersion
	VI_STR_INTERNALNAME		= 4,	// InternalName
	VI_STR_LEGALCOPYRIGHT	= 5,	// LegalCopyright
	VI_STR_LEGALTRADEMARKS	= 6,	// LegalTrademarks
	VI_STR_ORIGINALFILENAME	= 7,	// OriginalFilename
	VI_STR_PRIVATEBUILD		= 8,	// PrivateBuild
	VI_STR_PRODUCTNAME		= 9,	// ProductName
	VI_STR_PRODUCTVERSION	= 10,	// ProductVersion
	VI_STR_SPECIALBUILD		= 11,	// SpecialBuild
	VI_STR_OLESELFREGISTER	= 12	// OLESelfRegister
} VI_STR;

class CFileVersionInfo
{
public:	// Construction/destruction:

	CFileVersionInfo( void );	
	virtual ~CFileVersionInfo( void );

public:	// Implementation:
	
	BOOL	Open( IN LPCTSTR lpszFileName );
	BOOL	Open( IN HINSTANCE hInstance );
	void	Close( void );

	BOOL	QueryStringValue( IN LPCTSTR lpszString, OUT LPTSTR lpszValue, IN INT nBuf ) const;
	BOOL	QueryStringValue( IN INT nIndex, OUT LPTSTR lpszValue, IN INT nBuf ) const;
	LPCTSTR	GetVerStringName( IN INT nIndex );	

	BOOL	SetTrans ( IN LANGID wLID = LANG_NEUTRAL, IN WORD wCP = VI_CP_UNICODE );	
	BOOL	SetTransIndex( IN UINT nIndex = 0 );
	INT		FindTrans( IN LANGID wLID, IN WORD wCP ) const;
	DWORD	GetTransByIndex( IN UINT nIndex  ) const;	

public: // Static members:
	
	static BOOL		GetLIDName( IN WORD wLID, OUT LPTSTR lpszName, IN INT nBuf );
	static BOOL		GetCPName( IN WORD wCP, OUT LPCTSTR* ppszName );
	
public: // Inline members

	inline const	VS_FIXEDFILEINFO& GetVSFFI( void ) const;		
	inline BOOL		IsValid( void ) const;

	inline WORD		GetFileVersionMajor( void ) const;
	inline WORD		GetFileVersionMinor( void ) const;
	inline WORD		GetFileVersionBuild( void ) const;
	inline WORD		GetFileVersionQFE( void )   const;

	inline WORD		GetProductVersionMajor( void ) const;
	inline WORD		GetProductVersionMinor( void ) const;
	inline WORD		GetProductVersionBuild( void ) const;
	inline WORD		GetProductVersionQFE( void )   const;
	
	inline UINT		GetTransCount( void ) const;
	inline UINT		GetCurTransIndex( void ) const;		
	inline LANGID	GetLIDByIndex( IN UINT nIndex ) const;	
	inline WORD		GetCPByIndex( IN UINT nIndex ) const;	
	inline DWORD	GetCurTrans( void ) const;		
	inline LANGID	GetCurLID( void ) const;	
	inline WORD		GetCurCP( void ) const;

protected:

	BOOL	GetVersionInfo( IN LPCTSTR lpszFileName );
	BOOL	QueryVersionTrans( void );

protected: // Members variables
	
	static LPCTSTR	 s_ppszStr[ 13 ];	// String names
	VS_FIXEDFILEINFO m_vsffi;			// Fixed File Info (FFI)

	LPBYTE		m_lpbyVIB;		// Pointer to version info block (VIB)
	LPDWORD		m_lpdwTrans;	// Pointer to translation array in m_lpbyVIB, LOWORD = LangID and HIWORD = CodePage	
	UINT		m_nTransCur;	// Current translation index
	UINT		m_nTransCnt;	// Translations count
	BOOL		m_bValid;		// Version info is loaded	
};

////////////////////////////////////////////////////////////////////////////////

inline BOOL CFileVersionInfo::IsValid( void ) const
	{ return m_bValid; }

inline const VS_FIXEDFILEINFO& CFileVersionInfo::GetVSFFI( void ) const
	{ return m_vsffi; }

////////////////////////////////////////////////////////////////////////////////

inline WORD CFileVersionInfo::GetFileVersionMajor( void ) const
	{ ASSERT( m_bValid ); return HIWORD( m_vsffi.dwFileVersionMS ); }

inline WORD CFileVersionInfo::GetFileVersionMinor( void ) const
	{ ASSERT( m_bValid ); return LOWORD( m_vsffi.dwFileVersionMS ); }

inline WORD CFileVersionInfo::GetFileVersionBuild( void ) const
	{ ASSERT( m_bValid ); return HIWORD( m_vsffi.dwFileVersionLS ); }

inline WORD CFileVersionInfo::GetFileVersionQFE( void ) const
	{ ASSERT( m_bValid ); return LOWORD( m_vsffi.dwFileVersionLS ); }

inline WORD CFileVersionInfo::GetProductVersionMajor( void ) const
	{ ASSERT( m_bValid ); return HIWORD( m_vsffi.dwProductVersionMS ); }

inline WORD CFileVersionInfo::GetProductVersionMinor( void ) const
	{ ASSERT( m_bValid ); return LOWORD( m_vsffi.dwProductVersionMS ); }

inline WORD CFileVersionInfo::GetProductVersionBuild( void ) const
	{ ASSERT( m_bValid ); return HIWORD( m_vsffi.dwProductVersionLS ); }

inline WORD CFileVersionInfo::GetProductVersionQFE( void ) const
	{ ASSERT( m_bValid ); return LOWORD( m_vsffi.dwProductVersionLS ); }

////////////////////////////////////////////////////////////////////////////////
// Translation functions

inline UINT CFileVersionInfo::GetTransCount( void ) const
	{ ASSERT( m_bValid ); return m_nTransCnt; }

inline UINT CFileVersionInfo::GetCurTransIndex( void ) const
	{ ASSERT( m_bValid ); return m_nTransCur; }

inline LANGID CFileVersionInfo::GetLIDByIndex( IN UINT nIndex  ) const
	{ return LOWORD( GetTransByIndex( nIndex ) ); }	

inline WORD CFileVersionInfo::GetCPByIndex( IN UINT nIndex  ) const
	{ return HIWORD( GetTransByIndex( nIndex ) ); }	

inline DWORD CFileVersionInfo::GetCurTrans( void ) const
	{ return GetTransByIndex( GetCurTransIndex() ); }

inline LANGID CFileVersionInfo::GetCurLID( void ) const
	{ return GetLIDByIndex( GetCurTransIndex() ); }

inline WORD CFileVersionInfo::GetCurCP( void ) const
	{ return GetCPByIndex( GetCurTransIndex() ); }
	

static HRESULT STDAPICALLTYPE DllGetVersion( IN  HMODULE hModule, 
									  OUT DLLVERSIONINFO* lpDVI )
{
	if( hModule == NULL || 
		::IsBadReadPtr( lpDVI, sizeof( DLLVERSIONINFO* ) ) )
	{
		ASSERT_RETURN( S_FALSE );
	}

	CONST DWORD cbSize = lpDVI->cbSize;

	if(
#ifdef DLLVERSIONINFO2
		(
#endif
		cbSize != sizeof( DLLVERSIONINFO  )
#ifdef DLLVERSIONINFO2
		&& cbSize != sizeof( DLLVERSIONINFO2 ) ) 
#endif
		|| ::IsBadWritePtr( lpDVI, cbSize ) )
	{
		ASSERT_RETURN( S_FALSE );
	}

	::ZeroMemory( lpDVI, cbSize );
	lpDVI->cbSize = cbSize;
	
	CFileVersionInfo fvi;
	if( fvi.Open( hModule ) )
	{
		VS_FIXEDFILEINFO vsffi = fvi.GetVSFFI();

		if( vsffi.dwFileType == VFT_DLL ||
			vsffi.dwFileType == VFT_STATIC_LIB )
		{
			switch( vsffi.dwFileOS )
			{			
			case VOS__WINDOWS32:
			case VOS_NT_WINDOWS32:
				lpDVI->dwPlatformID = DLLVER_PLATFORM_WINDOWS;
				break;
			case VOS_NT:
				lpDVI->dwPlatformID = DLLVER_PLATFORM_NT;
				break;			
			default:
				return ( S_FALSE );
			}
			
			lpDVI->dwMajorVersion = HIWORD( vsffi.dwFileVersionMS );
			lpDVI->dwMinorVersion = LOWORD( vsffi.dwFileVersionMS );
			lpDVI->dwBuildNumber  = HIWORD( vsffi.dwFileVersionLS );
			
		#ifdef DLLVERSIONINFO2

			if( cbSize == sizeof( DLLVERSIONINFO2 ) )
			{
				DLLVERSIONINFO2* lpDVI2 = (DLLVERSIONINFO2*)lpDVI;
				lpDVI2->ullVersion = MAKEDLLVERULL( 
					lpDVI->dwMajorVersion,
					lpDVI->dwMinorVersion,
					lpDVI->dwBuildNumber ,
					LOWORD( vsffi.dwFileVersionLS )
				);
			}

		#endif

			return ( S_OK );
		}
	#ifdef _DEBUG
		else
			ASSERT( 0 );
	#endif

		fvi.Close();
	}

	return ( S_FALSE );
}

/////////////////////////////////////////////////////////////////////////////
// HIWORD( ffi.dwFileVersionMS ) - major
// LOWORD( ffi.dwFileVersionMS ) - minor
// HIWORD( ffi.dwFileVersionLS ) - build
// LOWORD( ffi.dwFileVersionLS ) - QFE
/////////////////////////////////////////////////////////////////////////////

CFileVersionInfo::CFileVersionInfo( void )
				: m_lpbyVIB( NULL )
{		
	Close();
}

CFileVersionInfo::~CFileVersionInfo( void )
{
	Close();
}

LPCTSTR CFileVersionInfo::s_ppszStr[] = { 
	_T( "Comments" ),			_T( "CompanyName" ),      
	_T( "FileDescription" ),	_T( "FileVersion" ),
	_T( "InternalName" ),		_T( "LegalCopyright" ),
	_T( "LegalTrademarks" ),	_T( "OriginalFilename" ),
	_T( "PrivateBuild" ),		_T( "ProductName" ),
	_T( "ProductVersion" ),		_T( "SpecialBuild" ),
	_T( "OLESelfRegister" )
};

////////////////////////////////////////////////////////////////////////////////
// Implementation

BOOL CFileVersionInfo::Open( IN HINSTANCE hInstance )
{
	if( hInstance == NULL )
		ASSERT_RETURN( FALSE );

	TCHAR szFileName[ MAX_PATH ] = { 0 };
	if( ::GetModuleFileName( hInstance, szFileName, MAX_PATH ) )
		return Open( szFileName );

	return FALSE;
};

BOOL CFileVersionInfo::Open( IN LPCTSTR lpszFileName )
{
	if( lpszFileName == NULL )
		ASSERT_RETURN( FALSE );

	Close();
	if( !GetVersionInfo( lpszFileName ) || !QueryVersionTrans() )
		Close();

	return m_bValid;
};

BOOL CFileVersionInfo::GetVersionInfo( IN LPCTSTR lpszFileName )
{
	DWORD dwDummy = 0;
	DWORD dwSize  = ::GetFileVersionInfoSize( 
		const_cast< LPTSTR >( lpszFileName ), &dwDummy // Set to 0
	);

	if ( dwSize > 0 )
	{		
		m_lpbyVIB = (LPBYTE)malloc( dwSize );

		if ( m_lpbyVIB != NULL && 
			::GetFileVersionInfo( const_cast< LPTSTR >( lpszFileName ), 
			0, dwSize, m_lpbyVIB ) )
		{
			UINT   uLen    = 0;
			LPVOID lpVSFFI = NULL;
			
			if ( ::VerQueryValue( m_lpbyVIB, _T( "\\" ), (LPVOID*)&lpVSFFI, &uLen ) )
			{
				::CopyMemory( &m_vsffi, lpVSFFI, sizeof( VS_FIXEDFILEINFO ) );
				m_bValid = ( m_vsffi.dwSignature == VS_FFI_SIGNATURE );
			}
		}
	}

	return m_bValid;
}

BOOL CFileVersionInfo::QueryVersionTrans( void )
{
	if( m_bValid == FALSE )
		ASSERT_RETURN( FALSE );

	UINT   uLen  = 0;
	LPVOID lpBuf = NULL;
  
	if( ::VerQueryValue( m_lpbyVIB, _T( "\\VarFileInfo\\Translation" ), (LPVOID*)&lpBuf, &uLen ) )
	{
		m_lpdwTrans = (LPDWORD)lpBuf;
		m_nTransCnt = ( uLen / sizeof( DWORD ) );		
	}	
	return (BOOL)( m_lpdwTrans != NULL );
}

void CFileVersionInfo::Close( void )
{
	m_nTransCnt  = 0;
	m_nTransCur  = 0;
	m_bValid	 = FALSE;	
	m_lpdwTrans  = NULL;
		
	::ZeroMemory( &m_vsffi, sizeof( VS_FIXEDFILEINFO ) );
	_free( m_lpbyVIB );
}

BOOL CFileVersionInfo::QueryStringValue( IN  LPCTSTR lpszItem,
										 OUT LPTSTR  lpszValue, 
										 IN  INT     nBuf ) const
{
	if( m_bValid  == FALSE || lpszItem == NULL )
		ASSERT_RETURN( FALSE );
	
	if( lpszValue != NULL && nBuf <= 0 )
		ASSERT_RETURN( FALSE );

	::ZeroMemory( lpszValue, nBuf * sizeof( TCHAR ) );

	TCHAR szSFI[ MAX_PATH ] = { 0 };
	::_stprintf( szSFI, _T( "\\StringFileInfo\\%04X%04X\\%s" ), 
		GetCurLID(), GetCurCP(), lpszItem );

	BOOL   bRes    = FALSE;
	UINT   uLen    = 0;
	LPTSTR lpszBuf = NULL;

	if( ::VerQueryValue( m_lpbyVIB, (LPTSTR)szSFI, (LPVOID*)&lpszBuf, &uLen ) )
	{
		if( lpszValue != NULL && nBuf > 0 )
			bRes = (BOOL)( ::lstrcpyn( lpszValue, lpszBuf, nBuf ) != NULL );
		else
			bRes = TRUE;
	}
	
	return ( bRes );
}

BOOL CFileVersionInfo::QueryStringValue( IN  INT    nIndex, 
										 OUT LPTSTR lpszValue,
										 IN  INT    nBuf ) const
{
	if( nIndex < VI_STR_COMMENTS || 
		nIndex > VI_STR_OLESELFREGISTER ) 
	{ 
		ASSERT_RETURN( FALSE );
	}	
	return QueryStringValue( s_ppszStr[ nIndex ], lpszValue, nBuf );
}

LPCTSTR CFileVersionInfo::GetVerStringName( IN INT nIndex )
{ 
	if( nIndex < VI_STR_COMMENTS || 
		nIndex > VI_STR_OLESELFREGISTER ) 
	{ 
		ASSERT_RETURN( FALSE );
	}
	return (LPCTSTR)s_ppszStr[ nIndex ];
}

INT CFileVersionInfo::FindTrans( IN LANGID wLID,
								 IN WORD   wCP ) const
{	
	if( m_bValid == FALSE )
		ASSERT_RETURN( -1 );

	for( UINT n = 0; n < m_nTransCnt; n++ )
	{
		if( LOWORD( m_lpdwTrans[ n ] ) == wLID && 
			HIWORD( m_lpdwTrans[ n ] ) == wCP  )
		{		
			return n;
		}
	}
	return -1;
}

BOOL CFileVersionInfo::SetTrans( IN LANGID wLID /*LANG_NEUTRAL*/, 
								 IN WORD   wCP  /*WSLVI_CP_UNICODE*/ )
{	
	if( m_bValid == FALSE )
		ASSERT_RETURN( FALSE );

	if( GetCurLID() == wLID && GetCurCP() == wCP )
		return TRUE;

	INT nPos = FindTrans( wLID, wCP );
	if( nPos != -1 ) m_nTransCur = nPos;

	return ( m_nTransCur == (UINT)nPos );
}

DWORD CFileVersionInfo::GetTransByIndex( IN UINT nIndex ) const
{
	if( m_bValid == FALSE || nIndex > m_nTransCnt )
		ASSERT_RETURN( 0 );

	return m_lpdwTrans[ nIndex ];
}

BOOL CFileVersionInfo::SetTransIndex( IN UINT nIndex /*0*/ )
{
	if( m_bValid == FALSE )
		ASSERT_RETURN( FALSE );

	if( m_nTransCur == nIndex )
		return TRUE;

	if(nIndex <= m_nTransCnt)
		m_nTransCur = nIndex;
	
	return ( m_nTransCur == nIndex );
}

/////////////////////////////////////////////////////////////////////////////
// Static members

// If the LID identifier is unknown, it returns a 
// default string ("Language Neutral"):

BOOL CFileVersionInfo::GetLIDName( IN  WORD   wLID, 
								   OUT LPTSTR lpszName, 
								   IN  INT    nBuf )
{
	if( lpszName == NULL || nBuf <= 0 )
		ASSERT_RETURN( FALSE );

	return (BOOL)::VerLanguageName( wLID, lpszName, nBuf );
}

// If the CP identifier is unknown, it returns a 
// default string ("Unknown"):

BOOL CFileVersionInfo::GetCPName( IN  WORD	   wCP,
								  OUT LPCTSTR* ppszName )
{
	if( ppszName == NULL )
		ASSERT_RETURN( FALSE );

	BOOL bRes = TRUE;	
	*ppszName  = NULL;

	switch ( wCP )
	{
		case VI_CP_ASCII:	 *ppszName = _T( "7-bit ASCII" );				break;
		case VI_CP_JAPAN:	 *ppszName = _T( "Japan (Shift – JIS X-0208)" );break;
		case VI_CP_KOREA:	 *ppszName = _T( "Korea (Shift – KSC 5601)" );	break;
		case VI_CP_TAIWAN:	 *ppszName = _T( "Taiwan (Big5)" );				break;
		case VI_CP_UNICODE:	 *ppszName = _T( "Unicode" );					break;
		case VI_CP_LATIN2:	 *ppszName = _T( "Latin-2 (Eastern European)" );break;
		case VI_CP_CYRILLIC: *ppszName = _T( "Cyrillic" );					break;
		case VI_CP_MULTILNG: *ppszName = _T( "Multilingual" );				break;
		case VI_CP_GREEK:	 *ppszName = _T( "Greek" );						break;
		case VI_CP_TURKISH:	 *ppszName = _T( "Turkish" );					break;
		case VI_CP_HEBREW:	 *ppszName = _T( "Hebrew" );					break;
		case VI_CP_ARABIC:	 *ppszName = _T( "Arabic" );					break;		
		default:			 *ppszName = _T( "Unknown" ); bRes = FALSE;		break;
	}
	return bRes;
}


/////////////////////////////////////////////////////////////////////////////
#endif //__VER_H__
/////////////////////////////////////////////////////////////////////////////

