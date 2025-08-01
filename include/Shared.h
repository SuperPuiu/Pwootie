#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <curl/curl.h>

/* CDN stands for Content Delivery Network */
#define PACKAGE_MANIFEST    "-rbxPkgManifest.txt"
#define CDN_URL             "https://setup.rbxcdn.com/"
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
char *BuildString(uint8_t Elements, ...);

/* Wine.c */
int8_t SetupPrefix();
int8_t SetupProton(uint8_t CheckExistence);
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

/* CurlWrappers.c */
void SetupHandles();
CURLcode    CurlDownload(FILE *File, char *WithURL);
CURLcode    CurlGet(MemoryStruct *Chunk, char *WithURL);

/* FFlags.c */
int8_t  ApplyFFlag(char *EntryName, char *Data);
int8_t  OutputFFlags(char *EntryName);
int8_t  LoadFFlags(char *Version);
int8_t  CreateFFlags(char *Version, char *OldVersion);
char    *ReadFFlag(char *EntryName);

#endif
