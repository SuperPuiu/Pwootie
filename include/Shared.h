#ifndef __UTIL_H__
#define __UTIL_H__
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <curl/curl.h>
#include <ftw.h>

/* https://stackoverflow.com/questions/1668013/can-likely-unlikely-macros-be-used-in-user-space-code */
#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif

/* CDN stands for Content Delivery Network */
#define PACKAGE_MANIFEST    "-rbxPkgManifest.txt"
#define CLIENT_SETTINGS_URL "https://clientsettingscdn.roblox.com/v2/client-version/WindowsStudio64" 
#define TEMP_PWOOTIE_FOLDER "/tmp/pwootie/"
#define PWOOTIE_DATA        "PwootieData.txt"
#define INSTALL_DIR         ".robloxstudio"

#define unused(x) ((void) (x))

typedef enum ErrorFlags {
  ERR_STANDARD  = (1 << 0),
  ERR_MEMORY    = (1 << 1),
  ERR_NOEXIT    = (1 << 2)
} ErrorFlags;

typedef struct MemoryStruct {
  char    *Memory;
  size_t  Size;
} MemoryStruct;

typedef struct VersionData {
  char *Version;
  char *ClientVersionUpload; /* GUID */
  char *BootstrapperVersion;
} VersionData;

typedef struct Package {
  char    Name[64];
  char    Checksum[64];
  int64_t Size;
  int64_t ZipSize;
} Package;

typedef struct FetchPackagesStruct {
  Package   *PackageList;
  uint32_t  LongestName;
  uint8_t   TotalPackages;
} FetchStruct;

/* GetVersionData.c */
int8_t GetVersionData(VersionData *Data);
int8_t GetCDNVersion(MemoryStruct *VersionStruct);

/* Installer.c */
int8_t Install(char *Version, uint8_t CheckVersion);

/* Filesystem.c */
void ReplacePathSlashes(char *Path);
int8_t BuildDirectoryTree(char *Path);
uint64_t QueryDiskSpace();
int32_t DeleteFile(const char *pathname, const struct stat *sbuf, int32_t type, struct FTW *ftwb);
char *BuildString(uint8_t Elements, ...);

/* Wine.c */
int8_t SetupPrefix();
int8_t SetupProton(uint8_t CheckExistence);
int8_t AddNewUser(char *UserId, char *Name, char *URL);
void RunWineCfg();
void Run(char *Argument, char *Version);

/* Pwootie.c */
extern FILE* PwootieFile;

void PwootieExit();
void PwootieWriteEntry(char *Entry, char *Data);
int8_t OpenPwootieFile();
char *PwootieReadEntry(char *Entry);

/* Instructions.c */
char **ExtractInstructions(FILE *Installer, FetchStruct *Fetched);

/* Error.c */
void Error(char *String, uint8_t Flags);
void SetupSignalHandler();

/* CurlWrappers.c */
void          SetupHandles();
void          ResetMultiCurl(uint16_t Total);
int32_t       CurlGetHandleFromMessage(CURLMsg *Message);
int8_t        CurlMultiSetup(FILE **Files, char **Links, uint16_t Total);
CURLcode      CurlDownload(FILE *File, char *WithURL);
CURLcode      CurlGet(MemoryStruct *Chunk, char *WithURL);
extern CURLM  *CurlMulti;

/* FFlags.c */
int8_t  ApplyFFlag(char *EntryName, char *Data);
int8_t  OutputFFlags(char *EntryName);
int8_t  LoadFFlags(char *Version);
int8_t  CreateFFlags(char *Version, char *OldVersion);
char    *ReadFFlag(char *EntryName);

/* Main.c */
extern char *CDN_URL;

#endif
