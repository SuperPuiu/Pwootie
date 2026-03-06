#pragma once
#define _XOPEN_SOURCE 800

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
#define PWOOTIE_VERSION     "0.2-Experimental"

#define unused(x) ((void) (x))

typedef enum ErrorFlags {
		ERR_STANDARD  = (1 << 0),
		ERR_MEMORY    = (1 << 1),
		ERR_NOEXIT    = (1 << 2),
		ERR_WARNING   = (1 << 3)
} ErrorFlags;

typedef struct MemoryStruct {
		char    *Memory;
		size_t  Size;
} MemoryStruct;

typedef struct ResponseStruct {
		FILE *FileStream;
		char *FileName;

		uint32_t FileNameSize;
		CURLcode Response;
		uint8_t  FreeName:1;
} ResponseStruct;

typedef struct ZipMemoryStruct {
		char *Data;
		uint64_t Size;
} ZipMemoryStruct;

typedef struct EnvInfoStruct {
		char *PwootieVersion, *Renderer;
		char *RobloxVersion, *DesktopEnvironment;
		char *SessionType;

		char WineVersion[64];
		char KernelRelease[64], MachineType[64];
} EnvInfoStruct;

typedef struct VersionData {
		char *Version;
		char *ClientVersionUpload; /* GUID */
		char *BootstrapperVersion;
} VersionData;

typedef struct Package {
		char    Name[64];
		char    Checksum[33];

		uint8_t Download:1;

		uint64_t Size;
		uint64_t ZipSize;
} Package;

typedef struct FetchPackagesStruct {
		Package   *PackageList;
		uint32_t  LongestName;
		uint8_t   TotalPackages;
} FetchStruct;

/* System.c */
EnvInfoStruct *FetchEnvInfo(char *StudioVersion);
int32_t       ExecProgram(char *Program, uint8_t Silent, uint8_t Disown, ...);

/* GetVersionData.c */
int8_t GetVersionData(VersionData *Data);

/* Installer.c */
int8_t Install(char *Version, uint8_t CheckVersion);

/* Filesystem.c */
uint64_t QueryDiskSpace();
int32_t  DeleteFile(const char *pathname, const struct stat *sbuf, int32_t type, struct FTW *ftwb);
int8_t   BuildDirectoryTree(char *Path, uint32_t SkipBytes);
int8_t   CopyRelativeDir(char *Old, char *New);
void     ConvertPath(char *Path);
char     *BuildString(uint8_t Elements, ...);
char 				*ReadFileToBuffer(FILE *Ptr, uint32_t *PtrSize);
char     *GetVersionPath(char *Version, uint32_t ExtraBytes);

/* Wine.c */
int8_t  SetupPrefix();
int8_t  SetupWine(uint8_t CheckExistence);
int8_t  AddNewUser(char *restrict UserId, char *restrict Name, char *restrict URL);
char*   GetPrefixPath(uint32_t ExtraBytes);
char*   GetDefaultWineBinary(uint32_t ExtraBytes);
FILE*   GetUserRegistry();
void    RunWineCfg();
void    Run(char *restrict Argument, char *restrict Version);

/* Pwootie.c */
extern FILE* PwootieFile;

int8_t OpenPwootieFile();
void   PwootieExit();
void   PwootieWriteEntry(char *restrict Entry, char *restrict Data);
char   *PwootieReadEntry(char *Entry, uint32_t ExtraBytes);

/* Instructions.c */
char   **ExtractInstructions(FILE *Installer, FetchStruct *Fetched);

/* Error.c */
void   Error(char *String, uint8_t Flags, ...);
void   SetupSignalHandler();

/* CurlWrappers.c */
extern CURLM    *CurlMulti;

void            SetupHandles();
void            ResetMultiCurl(uint16_t Total);
int8_t          CurlMultiSetup(char **Buffers, char **Links, uint16_t Total);
CURLcode        CurlDownload(FILE *File, char *WithURL);
CURLcode        CurlGet(MemoryStruct *Chunk, char *WithURL);
ResponseStruct* CurlDownloadNoFile(char *WithURL, char *DownloadPath);

/* FFlags.c */
int8_t  ApplyFFlag(char *restrict EntryName, char *restrict Data);
int8_t  OutputFFlags(char *EntryName);
int8_t  LoadFFlags(char *Version);
int8_t  CreateFFlags(char *restrict Version, char *restrict OldVersion);
char    *ReadFFlag(char *EntryName);
char    *GetStudioSetting(char *Name);
void    ExitFFlags();

/* WineServer.c */
void StartWineserver();
void KillWineserver();

/* Interactivity.c */
uint8_t AskForConfirmation(char *InitialMessage);
uint8_t AskForOption(uint8_t Options, char **OptionNames);

/* Main.c */
extern char *CDN_URL;
