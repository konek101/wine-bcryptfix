/*
 * Win32 kernel functions
 *
 * Copyright 1995 Martin von Loewis, Sven Verdoolaege, and Cameron Heide
 */

#include <errno.h>
#include <sys/errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include "windows.h"
#include "winbase.h"
#include "winerror.h"
#include "file.h"
#include "device.h"
#include "process.h"
#include "heap.h"
#include "debug.h"

DWORD ErrnoToLastError(int errno_num);

static int TranslateCreationFlags(DWORD create_flags);
static int TranslateAccessFlags(DWORD access_flags);
static int TranslateShareFlags(DWORD share_flags);

/***********************************************************************
 *              ReadFileEx                (KERNEL32.)
 */
typedef
VOID
(CALLBACK *LPOVERLAPPED_COMPLETION_ROUTINE)(
    DWORD dwErrorCode,
    DWORD dwNumberOfBytesTransfered,
    LPOVERLAPPED lpOverlapped
    );

BOOL32 WINAPI ReadFileEx(HFILE32 hFile, LPVOID lpBuffer, DWORD numtoread,
			 LPOVERLAPPED lpOverlapped, 
			 LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{

    FIXME(file, "file %d to buf %p num %ld %p func %p stub\n",
	  hFile, lpBuffer, numtoread, lpOverlapped, lpCompletionRoutine);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}


/*************************************************************************
 * CreateFile32A [KERNEL32.45]  Creates or opens a file or other object
 *
 * Creates or opens an object, and returns a handle that can be used to
 * access that object.
 *
 * PARAMS
 *
 * filename	[I] pointer to filename to be accessed
 * access	[I] access mode requested
 * sharing	[I] share mode
 * security	[I] pointer to security attributes
 * creation	[I] ?
 * attributes	[I] ?
 * template	[I] handle to file with attributes to copy
 *
 * RETURNS
 *   Success: Open handle to specified file
 *   Failure: INVALID_HANDLE_VALUE
 *
 * NOTES
 *  Should call SetLastError() on failure.
 *
 * BUGS
 *
 * Doesn't support character devices, pipes, template files, or a
 * lot of the 'attributes' flags yet.
 */
HFILE32 WINAPI CreateFile32A(LPCSTR filename, DWORD access, DWORD sharing,
                             LPSECURITY_ATTRIBUTES security, DWORD creation,
                             DWORD attributes, HANDLE32 template)
{
    int access_flags, create_flags, share_flags;
    HFILE32 to_dup = HFILE_ERROR32; /* handle to dup */

    /* Translate the various flags to Unix-style.
     */
    access_flags = TranslateAccessFlags(access);
    create_flags = TranslateCreationFlags(creation);
    share_flags = TranslateShareFlags(sharing);

    if(template)
        FIXME(file, "template handles not supported.\n");

    if(!filename)
      return HFILE_ERROR32;
    /* If the name starts with '\\?\' or '\\.\', ignore the first 4 chars.
     */
    if(!strncmp(filename, "\\\\?\\", 4) || !strncmp(filename, "\\\\.\\", 4))
    {
        if (filename[2] == '.')
            return DEVICE_Open( filename+4, access_flags | create_flags );

        filename += 4;
	if (!strncmp(filename, "UNC", 3))
	{
            FIXME(file, "UNC name (%s) not supported.\n",filename);
            SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
            return HFILE_ERROR32;
	}	
    }

    /* If the name still starts with '\\', it's a UNC name.
     */
    if(!strncmp(filename, "\\\\", 2))
    {
        FIXME(file, "UNC names not supported.\n");
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
        return HFILE_ERROR32;
    }

    /* If the name is either CONIN$ or CONOUT$, give them duplicated stdin
     * or stdout, respectively. The lower case version is also allowed. Most likely
     * this should be a case ignore string compare.
     */
    if(!strcasecmp(filename, "CONIN$"))
	to_dup = GetStdHandle( STD_INPUT_HANDLE );
    else if(!strcasecmp(filename, "CONOUT$"))
	to_dup = GetStdHandle( STD_OUTPUT_HANDLE );

    if(to_dup != HFILE_ERROR32)
    {
	HFILE32 handle;
	if (!DuplicateHandle( GetCurrentProcess(), to_dup, GetCurrentProcess(),
			      &handle, access, FALSE, 0 ))
	    handle = HFILE_ERROR32;
	return handle;
    }
    return FILE_Open( filename, access_flags | create_flags,share_flags );
}


/*************************************************************************
 *              CreateFile32W              (KERNEL32.48)
 */
HFILE32 WINAPI CreateFile32W(LPCWSTR filename, DWORD access, DWORD sharing,
                             LPSECURITY_ATTRIBUTES security, DWORD creation,
                             DWORD attributes, HANDLE32 template)
{
    LPSTR afn = HEAP_strdupWtoA( GetProcessHeap(), 0, filename );
    HFILE32 res = CreateFile32A( afn, access, sharing, security, creation,
                                 attributes, template );
    HeapFree( GetProcessHeap(), 0, afn );
    return res;
}

static int TranslateAccessFlags(DWORD access_flags)
{
    int rc = 0;

    switch(access_flags)
    {
        case GENERIC_READ:
            rc = O_RDONLY;
            break;

        case GENERIC_WRITE:
            rc = O_WRONLY;
            break;

        case (GENERIC_READ | GENERIC_WRITE):
            rc = O_RDWR;
            break;
    }

    return rc;
}

static int TranslateCreationFlags(DWORD create_flags)
{
    int rc = 0;

    switch(create_flags)
    {
        case CREATE_NEW:
            rc = O_CREAT | O_EXCL;
            break;

        case CREATE_ALWAYS:
            rc = O_CREAT | O_TRUNC;
            break;

        case OPEN_EXISTING:
            rc = 0;
            break;

        case OPEN_ALWAYS:
            rc = O_CREAT;
            break;

        case TRUNCATE_EXISTING:
            rc = O_TRUNC;
            break;
    }

    return rc;
}
static int TranslateShareFlags(DWORD share_flags)
/* 
OPEN_SHARE_DENYNONE	FILE_SHARE_READ | FILE_SHARE_WRITE
OPEN_SHARE_DENYREAD	FILE_SHARE_WRITE
OPEN_SHARE_DENYREADWRITE	0
OPEN_SHARE_DENYWRITE	FILE_SHARE_READ
*/
{
  switch(share_flags)
    {
    case FILE_SHARE_READ | FILE_SHARE_WRITE: 
      return OF_SHARE_DENY_NONE;
    case FILE_SHARE_WRITE: 
      return OF_SHARE_DENY_READ;
    case FILE_SHARE_READ:
      return OF_SHARE_DENY_WRITE;
    case 0:
      return OF_SHARE_EXCLUSIVE;
    default:
    }
  FIXME(file,"unknown sharing flags 0x%04lx\n",share_flags);
  return OF_SHARE_EXCLUSIVE;
}
/**************************************************************************
 *              SetFileAttributes16	(KERNEL.421)
 */
BOOL16 WINAPI SetFileAttributes16( LPCSTR lpFileName, DWORD attributes )
{
    return SetFileAttributes32A( lpFileName, attributes );
}


/**************************************************************************
 *              SetFileAttributes32A	(KERNEL32.490)
 */
BOOL32 WINAPI SetFileAttributes32A(LPCSTR lpFileName, DWORD attributes)
{
    struct stat buf;
    DOS_FULL_NAME full_name;

    if (!DOSFS_GetFullName( lpFileName, TRUE, &full_name ))
        return FALSE;

    TRACE(file,"(%s,%lx)\n",lpFileName,attributes);
    if (attributes & FILE_ATTRIBUTE_NORMAL) {
      attributes &= ~FILE_ATTRIBUTE_NORMAL;
      if (attributes)
        FIXME(file,"(%s):%lx illegal combination with FILE_ATTRIBUTE_NORMAL.\n",
	      lpFileName,attributes);
    }
    if(stat(full_name.long_name,&buf)==-1)
    {
        SetLastError(ErrnoToLastError(errno));
        return FALSE;
    }
    if (attributes & FILE_ATTRIBUTE_READONLY)
    {
        buf.st_mode &= ~0222; /* octal!, clear write permission bits */
        attributes &= ~FILE_ATTRIBUTE_READONLY;
    }
    attributes &= ~(FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM);
    if (attributes)
        FIXME(file,"(%s):%lx attribute(s) not implemented.\n",
	      lpFileName,attributes);
    if (-1==chmod(full_name.long_name,buf.st_mode))
    {
        SetLastError(ErrnoToLastError(errno));
        return FALSE;
    }
    return TRUE;
}


/**************************************************************************
 *              SetFileAttributes32W	(KERNEL32.491)
 */
BOOL32 WINAPI SetFileAttributes32W(LPCWSTR lpFileName, DWORD attributes)
{
    LPSTR afn = HEAP_strdupWtoA( GetProcessHeap(), 0, lpFileName );
    BOOL32 res = SetFileAttributes32A( afn, attributes );
    HeapFree( GetProcessHeap(), 0, afn );
    return res;
}


/**************************************************************************
 *              SetFileApisToOEM   (KERNEL32.645)
 */
VOID WINAPI SetFileApisToOEM(void)
{
  /*FIXME(file,"(): stub!\n");*/
}


/**************************************************************************
 *              SetFileApisToANSI   (KERNEL32.644)
 */
VOID WINAPI SetFileApisToANSI(void)
{
    /*FIXME(file,"(): stub\n");*/
}


/******************************************************************************
 * AreFileApisANSI [KERNEL32.105]  Determines if file functions are using ANSI
 *
 * RETURNS
 *    TRUE:  Set of file functions is using ANSI code page
 *    FALSE: Set of file functions is using OEM code page
 */
BOOL32 WINAPI AreFileApisANSI(void)
{
    FIXME(file,"(void): stub\n");
    return TRUE;
}

