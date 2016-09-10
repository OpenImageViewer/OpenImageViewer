#include <string>
#include <windef.h>
#include <windows.h>
#include <Shlobj.h>
#include "StringUtility.h"
#include <tchar.h>
#include "API\defs.h"
typedef std::basic_string<OIVCHAR> tstring;

class Utility
{
public:
	static tstring GetExePath()
	{
		TCHAR ownPth[MAX_PATH];

		// Will contain exe path
		HMODULE hModule = GetModuleHandle(NULL);
		if (hModule != NULL)
		{
			// When passing NULL to GetModuleHandle, it returns handle of exe itself
			GetModuleFileName(hModule, ownPth, (sizeof(ownPth)));

			// Use above module handle to get the path using GetModuleFileName()
			return tstring(ownPth);
		}

		return tstring();

	}

	static tstring GetExeFolder()
	{
		tstring filePath = Utility::GetExePath();
		int idx = filePath.find_last_of(TEXT("\\"));
		filePath.erase(idx, filePath.length() - idx);
		return filePath;
	}

	static tstring GetAppDataFolder()
	{
		TCHAR szPath[MAX_PATH];

		if (SUCCEEDED(SHGetFolderPath(NULL,
			CSIDL_APPDATA | CSIDL_FLAG_CREATE,
			NULL,
			0,
			szPath)))
		{
			tstring result = szPath;
			result += TEXT("\\UniOgre");
			EnsurePath(result);
			return result;
		}

		return tstring();
	}

	static void EnsurePath(const tstring& path)
	{

		if (DirectoryExists(path) == false)
		{
			SHCreateDirectoryEx(NULL, path.c_str(), NULL);
		}
	}


	static bool DirectoryExists(const tstring& path)
	{
		struct _stat64i32 info;
		if (_tstat(path.c_str(), &info) != 0)
			return false;

		else if (info.st_mode & S_IFDIR)  // S_ISDIR() doesn't exist on my windows 

			//a directory
			return true;
		else
			// a file 
			return false;
	}

	static void find_files(tstring wrkdir, std::list<tstring> &lstFileData)
	{
		
		tstring wrkdirtemp = wrkdir;
		if( !wrkdirtemp.empty() && (wrkdirtemp[wrkdirtemp.length()-1] != L'\\')  )
		{
			wrkdirtemp += TEXT("\\");
		}

		WIN32_FIND_DATA file_data = {0};
		HANDLE hFile = FindFirstFile( (wrkdirtemp + TEXT("*")).c_str(), &file_data );

		if( hFile == INVALID_HANDLE_VALUE )
		{
			return;
		}

		do
		{
			if( file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			{
				if ((_tcscmp(file_data.cFileName, TEXT(".")) != 0) &&
					(_tcscmp(file_data.cFileName, TEXT("..")) != 0))
				{
					tstring directoryName = wrkdirtemp + file_data.cFileName;
					find_files( directoryName, lstFileData );
				}
			}
			else
			{
				if( (file_data.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) == 0 )
				{
					lstFileData.push_back(wrkdirtemp + file_data.cFileName);
				}
			}
		}
		while( FindNextFile( hFile, &file_data ) ) ;

		FindClose( hFile );
	}


};