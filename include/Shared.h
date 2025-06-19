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
void GetVersionData(VersionData *Data);
void GetCDNVersion(MemoryStruct *VersionStruct);

/* Installer.c */
void Install(VersionData *Data, uint8_t CheckVersion);

/* Utilities.c */
void DeleteDirectories();
void ReplacePathSlashes(char *Path);

/* Wine.c */
void SetupPrefix();
void Run(char *Argument, char *Version);

/* Pwootie.c */
void PwootieExit();
uint8_t OpenPwootieFile();
void PwootieWriteEntry(char *Entry, char *Data);
char *PwootieReadEntry(char *Entry);

/* Instructions.c */
char **ExtractInstructions(FILE *Installer, FetchStruct *Fetched);

/* CurlWrappers.c */
void SetupHandles();
CURLcode    CurlDownload(FILE *File, char *WithURL);
CURLcode    CurlGet(MemoryStruct *Chunk, char *WithURL);

#endif
