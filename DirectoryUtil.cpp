#include "DirectoryUtil.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>
#include <direct.h>
#else
#include <dirent.h>
#endif//WIN32

const int DirectoryUtil::msc_nMaxFilePath = 1024;

DirectoryUtil::DirectoryUtil()
{

}

DirectoryUtil::~DirectoryUtil()
{
    //
}

bool DirectoryUtil::CreateDir(const char *szDirPath)
{
    if (
        (szDirPath != NULL)
        && ((int)strlen(szDirPath) < msc_nMaxFilePath)
        )
    {
        char sPath[msc_nMaxFilePath];
        char *pTemp;

        memset(sPath, 0, msc_nMaxFilePath);
        //copy szDirPath to sPath and change '\' to '/'
        for (size_t i = 0; i < strlen(szDirPath); i ++)
        {
            if (*(szDirPath+i) == '\\')
            {
                sPath[i] = '/';
            }
            else
            {
                sPath[i] = *(szDirPath+i);
            }
        }

        //
        pTemp = sPath;
        while (*pTemp == '/')
        {// skip '/' in header
            pTemp ++;
        }
        while (*pTemp != '\0')
        {
            if (*pTemp == '/')
            {
                *pTemp = '\0';
                //create dir
                if (MakeDir(sPath) != 0)
                {
                    *pTemp = '/';
                    break;
                }
                *pTemp = '/';
            }
            pTemp ++;
        }

        if (MakeDir(sPath) == 0)
        {
            return true;
        }
    }

    return false;
}

bool DirectoryUtil::IsDirExist(const char *szDirPath)
{
#ifdef WIN32
    struct _stat64 thestat;

    if (szDirPath != NULL)
    {
        std::string strDirPath = EnsureNoSlashEnd(szDirPath);
        if(::_stat64(strDirPath.c_str(), &thestat) >= 0)
        {
            if((thestat.st_mode&_S_IFDIR))
            {
                return true;
            }
        }
    }

    return false;
#else
    struct stat64 thestat;

    if (szDirPath != NULL)
    {
        std::string strDirPath = EnsureNoSlashEnd(szDirPath);
        if(::stat64(strDirPath.c_str(), &thestat) >= 0)
        {
            if(S_ISDIR(thestat.st_mode))
            {
                return true;
            }
        }
    }

    return false;
#endif
}

std::string DirectoryUtil::EnsureSlashEnd(std::string strDirPath)
{
    if (strDirPath.length() >= 2 && strDirPath.at(1) == ':')
    {//windows path
        if (strDirPath.rfind("\\") != (strDirPath.length()-1))
        {
            return strDirPath + "\\";
        }
    }
    else if (strDirPath.length() >= 1 && strDirPath.at(0) == '/')
    {
        if (strDirPath.rfind("/") != (strDirPath.length()-1))
        {
            return strDirPath + "/";
        }
    }

    return strDirPath;
}

std::string DirectoryUtil::EnsureNoSlashEnd(std::string strDirPath)
{
    if (strDirPath.length() >= 2 && strDirPath.at(1) == ':')
    {//windows path
        if (strDirPath.rfind("\\") == (strDirPath.length()-1))
        {
            return strDirPath.substr(0, strDirPath.length()-1);
        }
    }
    else if (strDirPath.length() >= 1 && strDirPath.at(0) == '/')
    {
        if (strDirPath.rfind("/") == (strDirPath.length()-1))
        {
            return strDirPath.substr(0, strDirPath.length()-1);
        }
    }

    return strDirPath;
}

std::string DirectoryUtil::GetParentDir(std::string strPath)
{
    std::string strKey = "\\";
    if (strPath.length() >= 2 && strPath.at(1) == ':')
    {//windows path
        strKey = "\\";
    }
    else if (strPath.length() >= 1 && strPath.at(0) == '/')
    {
        strKey = "/";
    }

    size_t nPos = strPath.rfind(strKey);
    if (nPos == strPath.length()-1)
    {
         nPos = strPath.rfind(strKey, nPos-1);
    }
    if (nPos != std::string::npos)
    {
        return strPath.substr(0, nPos+1);
    }
    else
    {
        return "";
    }
}


int DirectoryUtil::MakeDir(const char *szDirPath)
{
#ifdef WIN32
    if (szDirPath != NULL)
    {
        struct stat	thestat;
        if (::stat(szDirPath, &thestat) == -1)
        {//not exist,create it
            if (::_mkdir(szDirPath) == -1)
            {
                if(::_chdir(szDirPath) != 0)
                {
                    return ::GetLastError();
                }
            }
        }
        else if(!(thestat.st_mode&_S_IFDIR))
        {//exist, but is file
            // there is a file at this point in the path
            return EEXIST;
        }
        else
        {//exist, ensure
            if(::_chdir(szDirPath) != 0)
            {
                return ::GetLastError();
            }
        }

        return 0;
    }

    return -1;
#else
    if (szDirPath != NULL)
    {
        struct stat	thestat;
        if (::stat(szDirPath, &thestat) == -1)
        {//not exist,create it
            if (::mkdir(szDirPath, (S_IRWXU|S_IRWXG|S_IRWXO)) == -1)
            {
                if(::chdir(szDirPath) != 0)
                {
                    return errno;
                }
            }
        }
        else if(!S_ISDIR(thestat.st_mode))
        {//exist, but is file
            // there is a file at this point in the path
            return EEXIST;
        }
        else
        {//exist, ensure
            if(::chdir(szDirPath) != 0)
            {
                return errno;
            }
        }

        chmod(szDirPath, (S_IRWXU|S_IRWXG|S_IRWXO));

        return 0;
    }

    return -1;
#endif
}
//////////////////////////////////////////////////////////////////////////
