/*=============================================================================
	FFileManagerLinux.h: Unreal Linux based file manager.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Brandon Reinhart
=============================================================================*/

#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <glob.h>
#include "FFileManagerGeneric.h"

/*-----------------------------------------------------------------------------
	File Manager.
-----------------------------------------------------------------------------*/

// File manager.
class FArchiveFileReader : public FArchive
{
public:
	FArchiveFileReader( FILE* InFile, FOutputDevice* InError, INT InSize )
	:	File			( InFile )
	,	Error			( InError )
	,	Size			( InSize )
	,	Pos				( 0 )
	,	BufferBase		( 0 )
	,	BufferCount		( 0 )
	{
		guard(FArchiveFileReader::FArchiveFileReader);
		fseek( File, 0, SEEK_SET );
		ArIsLoading = ArIsPersistent = 1;
		unguard;
	}
	~FArchiveFileReader()
	{
		guard(FArchiveFileReader::~FArchiveFileReader);
		if( File )
			Close();
		unguard;
	}
	void Precache( INT HintCount )
	{
		guardSlow(FArchiveFileReader::Precache);
		checkSlow(Pos==BufferBase+BufferCount);
		BufferBase = Pos;
		BufferCount = Min( Min( HintCount, (INT)(ARRAY_COUNT(Buffer) - (Pos&(ARRAY_COUNT(Buffer)-1))) ), Size-Pos );
		if( fread( Buffer, BufferCount, 1, File )!=1 && BufferCount!=0 )
		{
			ArIsError = 1;
			Error->Logf( TEXT("fread failed: BufferCount=%i Error=%i"), BufferCount, ferror(File) );
			return;
		}
		unguardSlow;
	}
	void Seek( INT InPos )
	{
		guard(FArchiveFileReader::Seek);
		check(InPos>=0);
		check(InPos<=Size);
		if( fseek(File,InPos,SEEK_SET) )
		{
			ArIsError = 1;
			Error->Logf( TEXT("seek Failed %i/%i: %i %i"), InPos, Size, Pos, ferror(File) );
		}
		Pos         = InPos;
		BufferBase  = Pos;
		BufferCount = 0;
		unguard;
	}
	INT Tell()
	{
		return Pos;
	}
	INT TotalSize()
	{
		return Size;
	}
	UBOOL Close()
	{
		guardSlow(FArchiveFileReader::Close);
		if( File )
			fclose( File );
		File = NULL;
		return !ArIsError;
		unguardSlow;
	}
	void Serialize( void* V, INT Length )
	{
		guardSlow(FArchiveFileReader::Serialize);
		while( Length>0 )
		{
			INT Copy = Min( Length, BufferBase+BufferCount-Pos );
			if( Copy==0 )
			{
				if( Length >= ARRAY_COUNT(Buffer) )
				{
					if( fread( V, Length, 1, File )!=1 )
					{
						ArIsError = 1;
						Error->Logf( TEXT("fread failed: Length=%i Error=%i"), Length, ferror(File) );
					}
					Pos += Length;
					BufferBase += Length;
					return;
				}
				Precache( MAXINT );
				Copy = Min( Length, BufferBase+BufferCount-Pos );
				if( Copy<=0 )
				{
					ArIsError = 1;
					Error->Logf( TEXT("ReadFile beyond EOF %i+%i/%i"), Pos, Length, Size );
				}
				if( ArIsError )
					return;
			}
			appMemcpy( V, Buffer+Pos-BufferBase, Copy );
			Pos       += Copy;
			Length    -= Copy;
			V          = (BYTE*)V + Copy;
		}
		unguardSlow;
	}
protected:
	FILE*			File;
	FOutputDevice*	Error;
	INT				Size;
	INT				Pos;
	INT				BufferBase;
	INT				BufferCount;
	BYTE			Buffer[1024];
};
class FArchiveFileWriter : public FArchive
{
public:
	FArchiveFileWriter( FILE* InFile, FOutputDevice* InError )
	:	File		(InFile)
	,	Error		( InError )
	,	Pos			(0)
	,	BufferCount	(0)
	{
		ArIsSaving = ArIsPersistent = 1;
	}
	~FArchiveFileWriter()
	{
		guard(FArchiveFileWriter::~FArchiveFileWriter);
		if( File )
			Close();
		File = NULL;
		unguard;
	}
	void Seek( INT InPos )
	{
		Flush();
		if( fseek(File,InPos,SEEK_SET) )
		{
			ArIsError = 1;
			Error->Logf( LocalizeError("SeekFailed",TEXT("Core")) );
		}
		Pos = InPos;
	}
	INT Tell()
	{
		return Pos;
	}
	UBOOL Close()
	{
		guardSlow(FArchiveFileWriter::Close);
		Flush();
		if( File && fclose( File ) )
		{
			ArIsError = 1;
			Error->Logf( LocalizeError("WriteFailed",TEXT("Core")) );
		}
		File = NULL;
		return !ArIsError;
		unguardSlow;
	}
	void Serialize( void* V, INT Length )
	{
		Pos += Length;
		INT Copy;
		while( Length > (Copy=ARRAY_COUNT(Buffer)-BufferCount) )
		{
			appMemcpy( Buffer+BufferCount, V, Copy );
			BufferCount += Copy;
			Length      -= Copy;
			V            = (BYTE*)V + Copy;
			Flush();
		}
		if( Length )
		{
			appMemcpy( Buffer+BufferCount, V, Length );
			BufferCount += Length;
		}
	}
	void Flush()
	{
		if( BufferCount && fwrite( Buffer, BufferCount, 1, File )!=1 )
		{
			ArIsError = 1;
			Error->Logf( LocalizeError("WriteFailed",TEXT("Core")) );
		}
		BufferCount=0;
	}
protected:
	FILE*			File;
	FOutputDevice*	Error;
	INT				Pos;
	INT				BufferCount;
	BYTE			Buffer[4096];
};

class FFileManagerLinux : public FFileManagerGeneric
{
private:
	TCHAR	ConfigDir[PATH_MAX];

public:
	void Init( UBOOL Startup )
	{
		guard(FFileManagerLinux::Init);

		const ANSICHAR* XdgConfigHome = getenv( "XDG_CONFIG_HOME" );
		if( XdgConfigHome )
			appSprintf( ConfigDir, TEXT("%s/%s/System/"), appFromAnsi(XdgConfigHome), appPackage() );
		else
			appSprintf( ConfigDir, TEXT("%s/.config/%s/System/"), appFromAnsi(getenv("HOME")), appPackage() );

		if( !MakeDirectory( ConfigDir, 1 ) )
			appErrorf( TEXT("Failed to create configuration directory %s"), ConfigDir );

		unguard;
	}

	FArchive* CreateFileReader( const TCHAR* OrigFilename, DWORD Flags, FOutputDevice* Error )
	{
		guard(FFileManagerLinux::CreateFileReader);

		TCHAR FixedFilename[PATH_MAX], Filename[PATH_MAX];
		PathSeparatorFixup( FixedFilename, OrigFilename );

		FILE* File = NULL;
		if( RewriteToConfigPath( Filename, FixedFilename ) )
			File = fopen(TCHAR_TO_ANSI(Filename), "rb");
		if( !File )
		{
			File = fopen(TCHAR_TO_ANSI(FixedFilename), "rb");
			if( !File )
			{
				if( Flags & FILEREAD_NoFail )
					appErrorf(TEXT("Failed to read file: %s"),Filename);
				return NULL;
			}
		}
		fseek( File, 0, SEEK_END );

		return new(TEXT("LinuxFileReader"))FArchiveFileReader(File,Error,ftell(File));

		unguard;
	}
	FArchive* CreateFileWriter( const TCHAR* OrigFilename, DWORD Flags, FOutputDevice* Error )
	{
		guard(FFileManagerLinux::CreateFileWriter);

		TCHAR FixedFilename[PATH_MAX], Filename[PATH_MAX];
		PathSeparatorFixup( FixedFilename, OrigFilename );

		if( Flags & FILEWRITE_EvenIfReadOnly )
			chmod(TCHAR_TO_ANSI(Filename), __S_IREAD | __S_IWRITE);
		if( (Flags & FILEWRITE_NoReplaceExisting) && FileSize(Filename)>=0 )
			return NULL;

		const ANSICHAR* Mode = (Flags & FILEWRITE_Append) ? "ab" : "wb"; 
		FILE* File = NULL;
		if( RewriteToConfigPath( Filename, FixedFilename ) )
			File = fopen(TCHAR_TO_ANSI(Filename),Mode);
		else
			File = fopen(TCHAR_TO_ANSI(FixedFilename),Mode);
		if( !File )
		{
			if( Flags & FILEWRITE_NoFail )
				appErrorf( TEXT("Failed to write: %s"), Filename );
			return NULL;
		}
		if( Flags & FILEWRITE_Unbuffered )
			setvbuf( File, 0, _IONBF, 0 );

		return new(TEXT("LinuxFileWriter"))FArchiveFileWriter(File,Error);

		unguard;
	}
	UBOOL Delete( const TCHAR* OrigFilename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 )
	{
		guard(FFileManagerLinux::Delete);

		TCHAR Filename[PATH_MAX];
		PathSeparatorFixup( Filename, OrigFilename );

		if( EvenReadOnly )
			chmod(TCHAR_TO_ANSI(Filename), __S_IREAD | __S_IWRITE);
		return unlink(TCHAR_TO_ANSI(Filename))==0 || (errno==ENOENT && !RequireExists);

		unguard;
	}
	SQWORD GetGlobalTime( const TCHAR* Filename )
	{
		guard(FFileManagerLinux::GetGlobalTime);

		return 0;
		
		unguard;
	}
	UBOOL SetGlobalTime( const TCHAR* Filename )
	{
		guard(FFileManagerLinux::SetGlobalTime);

		return 0;

		unguard;
	}
	UBOOL MakeDirectory( const TCHAR* OrigPath, UBOOL Tree=0 )
	{
		guard(FFileManagerLinux::MakeDirectory);

		TCHAR Path[PATH_MAX];
		PathSeparatorFixup( Path, OrigPath );

		if( Tree )
		{
			// Strip one element off the path
			// Start at the end
			TCHAR* Ptr = Path + appStrlen( Path ) - 1;
			// Ignore trailing slashes
			while( Ptr >= Path && *Ptr == TEXT('/') ) Ptr--;
			// Look for the last separator
			while( Ptr >= Path && *Ptr != TEXT('/') ) Ptr--;
			if( Ptr >= Path )
			{
				*Ptr = TEXT('\0');
				// Don't go past the root
				if( appStrlen(Path) != 0 && !FFileManagerGeneric::MakeDirectory(Path, 1) )
					return 0;
				*Ptr = TEXT('/');
			}
		}

		return mkdir(TCHAR_TO_ANSI(Path), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)==0 || errno==EEXIST;

		unguard;
	}
	UBOOL DeleteDirectory( const TCHAR* OrigPath, UBOOL RequireExists=0, UBOOL Tree=0 )
	{
		guard(FFileManagerLinux::DeleteDirectory);

		TCHAR Path[PATH_MAX];
		PathSeparatorFixup( Path, OrigPath );

		if( Tree )
			return FFileManagerGeneric::DeleteDirectory( Path, RequireExists, Tree );

		return rmdir(TCHAR_TO_ANSI(Path))==0 || (errno==ENOENT && !RequireExists);

		unguard;
	}
	TArray<FString> FindFiles( const TCHAR* OrigPattern, UBOOL Files, UBOOL Directories )
	{
		guard(FFileManagerLinux::FindFiles);

		TArray<FString> Result;
		TCHAR FixedPattern[PATH_MAX], Pattern[PATH_MAX];
		glob_t GlobBuf;

		PathSeparatorFixup( FixedPattern, OrigPattern );

		// Look in both ConfigDir and the application directory.
		INT GlobFlags = GLOB_NOSORT | GLOB_MARK;
		if( RewriteToConfigPath( Pattern, FixedPattern ) )
		{
			glob( TCHAR_TO_ANSI(Pattern), GlobFlags, NULL, &GlobBuf );
			GlobFlags |= GLOB_APPEND;
		}
		glob( TCHAR_TO_ANSI(FixedPattern), GlobFlags, NULL, &GlobBuf );

		for( size_t i = 0; i < GlobBuf.gl_pathc; i++ )
		{
			const TCHAR* Match = appFromAnsi(GlobBuf.gl_pathv[i]);
			// Strip the filename from the match
			for( const TCHAR* Ptr = Match; *Ptr != TEXT('\0'); Ptr++ )
				if( *Ptr == TEXT('/') )
					Match = Ptr + 1;
			// Directories are GLOB_MARK'd with a trailing slash,
			// so this prevents directories from being added to the result.
			if( appStrlen(Match) != 0 )
				new(Result)FString( Match );
		}

		globfree( &GlobBuf );

		return Result;
		unguard;
	}
	UBOOL SetDefaultDirectory( const TCHAR* Filename )
	{
		guard(FFileManagerLinux::SetDefaultDirectory);
		return chdir(TCHAR_TO_ANSI(Filename))==0;
		unguard;
	}
	FString GetDefaultDirectory()
	{
		guard(FFileManagerLinux::GetDefaultDirectory);
		ANSICHAR Buffer[PATH_MAX]="";
		getcwd( Buffer, ARRAY_COUNT(Buffer) );
		return appFromAnsi( Buffer );
		unguard;
	}
private:
	void PathSeparatorFixup( TCHAR* Dest, const TCHAR* Src )
	{
		appStrcpy( Dest, Src );
		for( TCHAR *Cur = Dest; *Cur != TEXT('\0'); Cur++ )
			if( *Cur == TEXT('\\') )
				*Cur = TEXT('/');
	}
	UBOOL RewriteToConfigPath( TCHAR* Result, const TCHAR* Path )
	{
		// Don't rewrite absolute paths
		if( Path[0] == TEXT('/') )
			return 0;

		appStrcpy( Result, ConfigDir );
		appStrcat( Result, Path );

		return 1;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
