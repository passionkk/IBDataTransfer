#ifndef _DIRECTORY_UTIL_H_
#define _DIRECTORY_UTIL_H_

#include <string>
#include <vector>

#ifndef UINT64
#ifdef WIN32
typedef unsigned __int64    UINT64;
#else
typedef unsigned long long	UINT64;
#endif
#endif//UINT64

#ifndef INT64
#ifdef WIN32
typedef __int64				INT64;
#else
typedef signed long long	INT64;
#endif
#endif//INT64

/**
 * szDirPath should format like:
 * in windows
 *      C:\1\ not C:\1
 * in linux
 *      /home/1/ not /home/1
 */

class DirectoryUtil
{
public:
    /**
     * create directory. 
     * if parent dir is not exist, create it.
     * 
     * return true when create directory success, or directory is already exist.
     */
    static bool CreateDir(const char *szDirPath);

    static bool IsDirExist(const char *szDirPath);

    static std::string EnsureSlashEnd(std::string strDirPath);

    /**
     * eg.
     * strDirPath = C:\1\2\;    return C:\1\2
     * strDirPath = C:\1\2;     return C:\1\2
     * strDirPath = /home/1/2/; return /home/1/2
     * strDirPath = /home/1/2;  return /home/1/2
     */
    static std::string EnsureNoSlashEnd(std::string strDirPath);

    /**
     * 
     * @strPath
     * strPath can be a file path, or a directory path.
     */
    static std::string GetParentDir(std::string strPath);

private:
    DirectoryUtil();
    ~DirectoryUtil();
protected:
    static int MakeDir(const char *szDirPath);
private:
    static const int    msc_nMaxFilePath;
};

#endif//_DIRECTORY_UTIL_H_
