#include <Shared.h>

#include <sys/stat.h>
#include <errno.h>
#include <zip.h>
#include <md5.h>

#define OFFICIAL_INSTALLER  "RobloxStudioInstaller.exe"
#define APP_SETTINGS        "AppSettings.xml"
#define APP_SETTINGS_DATA   "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<Settings>\r\n        <ContentFolder>content</ContentFolder>\r\n        <BaseUrl>http://www.roblox.com</BaseUrl>\r\n</Settings>\r\n"

/* This is ugly. */
void ChecksumToString(uint8_t *Checksum, char *Buffer) {
  uint8_t BufPosition = 0;
  char l_Buf[3];

  for (uint8_t i = 0; i < 16; i++) {
    sprintf(l_Buf, "%02x", Checksum[i]);
    memcpy(Buffer + BufPosition, l_Buf, 2);
    BufPosition += 2;
  }

  Buffer[BufPosition] = '\0';
}

/* Prototypes are found in Installer.c */
int8_t InstallPackages(FetchStruct *Fetched, char *Version) {
  CURLcode  Response;
  FILE      *Installer;

  uint32_t SettingsLen = strlen(APP_SETTINGS_DATA), LengthVersion = strlen(Version);
  uint32_t InstallerLength = strlen(OFFICIAL_INSTALLER), TempDirLength = strlen(TEMP_PWOOTIE_FOLDER);
  uint32_t InstallDirLength = strlen(INSTALL_DIR), HomeLength = strlen(getenv("HOME"));
  uint32_t InstallDirTotal = HomeLength + InstallDirLength + LengthVersion + 3 + 253;
  uint32_t AppSettingsLen = strlen(APP_SETTINGS);

  /* The 'Official' string is built inside of this function only to reuse it later. Same for 'InstallDir'. */
  char *Official        = malloc((InstallerLength + TempDirLength + 64 + 1) * sizeof(char));
  char *InstallDir      = malloc((InstallDirTotal) * sizeof(char));
  char *FullURL         = BuildString(4, CDN_URL, Version, "-", OFFICIAL_INSTALLER);

  if (!Official) 
    Error("[FATAL]: Unable to allocate Official during InstallPackages call.", ERR_MEMORY);
  else if (!InstallDir)
    Error("[FATAL]: Unable to allocate InstallDir during InstallPackages call.", ERR_MEMORY);
  
  /* Construct official downloaded path. */
  memcpy(Official, TEMP_PWOOTIE_FOLDER, TempDirLength);
  memcpy(Official + TempDirLength, OFFICIAL_INSTALLER, InstallerLength);
  Official[InstallerLength + TempDirLength] = '\0';
  
  /* Construct installation directory path. */
  memcpy(InstallDir, getenv("HOME"), HomeLength);
  memcpy(InstallDir + HomeLength + 1, INSTALL_DIR, InstallDirLength);
  memcpy(InstallDir + HomeLength + InstallDirLength + 2, Version, LengthVersion);
  InstallDir[HomeLength] = InstallDir[HomeLength + InstallDirLength + 1] = InstallDir[HomeLength + InstallDirLength + LengthVersion + 2] = '/';
  InstallDir[HomeLength + InstallDirLength + LengthVersion + 3] = '\0';

  /* Download installer in TEMP_PWOOTIE_FOLDER folder. */
  Installer = fopen(Official, "w+");

  if (!Installer) {
    Error("[ERROR]: Unable to create RobloxStudioInstaller file during InstallPackages call.", ERR_STANDARD | ERR_NOEXIT);
    Error(Official, ERR_STANDARD | ERR_NOEXIT);
    return -1;
  }
  
  Response = CurlDownload(Installer, FullURL);

  if (Response != CURLE_OK) {
    Error("[ERROR]: Failed to download RobloxStudioInstaller.exe.\n", ERR_STANDARD | ERR_NOEXIT);
    Error((char*)curl_easy_strerror(Response), ERR_STANDARD | ERR_NOEXIT);
    return -1;
  }

  /* For this step, we have two ways of proceeding: 
   * 1. Hardcode a 2D array of chars with where the zip files should be extracted. 
   * CONS: If ROBLOX decided to change locations or add new packages we'll have to update the hardcoded array.
   * PROS: Easier to implement. 
   * 2. Get extraction locations directly from RobloxStudioInstaller.exe: 
   * CONS: More difficult to implement. 
   * PROS: We're fixing the problem we'd have with the first solution.
   *
   * I found second solution by looking at the source code for Vinegar. Big thanks to them.
   * I found first implementation by looking at the source code for Cork. Big thanks to them as well.
   * Because we're going with the second solution, you may find the implementation in Instructions.c. */

  char **Instructions = ExtractInstructions(Installer, Fetched);

  /* Initialize all directories we got after extracting the instructions. */
  for (uint8_t Index = 0; Index < Fetched->TotalPackages; Index++) {
    /* Skip empty instructions. */
    if (Instructions[Index][0] == '\0')
      continue;
    uint32_t InstructionLength = strlen(Instructions[Index]);

    /* Add the required path to construct. Add +1 to the length because we want to include the NULL terminator. */
    memcpy(InstallDir + HomeLength + InstallDirLength + LengthVersion + 3, Instructions[Index], InstructionLength + 1);

    /* Build the directory tree. */
    BuildDirectoryTree(InstallDir);
  }
  
  /* Why can't just AppSettings.xml be packaged in a zip file? I don't know! */
  memcpy(InstallDir + HomeLength + InstallDirLength + LengthVersion + 3, APP_SETTINGS, AppSettingsLen);
  InstallDir[HomeLength + InstallDirLength + LengthVersion + AppSettingsLen + 3] = '\0';
  
  FILE *AppSettings = fopen(InstallDir, "w");
  if (!AppSettings) {
    Error("[ERROR]: Failed to create AppSettings durign InstallPackages call.", ERR_STANDARD | ERR_NOEXIT);
    return -1;
  }

  fwrite(APP_SETTINGS_DATA, sizeof(char), SettingsLen, AppSettings);
  fclose(AppSettings);

  /* Decompress all zips downloaded during DownloadPackages call. */
  for (uint8_t i = 0; i < Fetched->TotalPackages; i++) {
    uint32_t        ZipLength = strlen(Fetched->PackageList[i].Name), InstructionLength = strlen(Instructions[i]);
    int32_t         ErrorCode = 0;
    zip_t           *ZipPointer;
    zip_error_t     ZipError;
    struct zip_stat *ZipStat = calloc(256, sizeof(int));
    zip_file_t      *FileDescriptor;
    
    char *Memory;
    FILE *NewFile;
    
    if (!ZipStat) {
      char Buffer[5];
      sprintf(Buffer, "%i", i);

      Error("[ERROR]: Failed to allocate ZipStat for one of the fetched packages during InstallPackages call.", ERR_STANDARD | ERR_NOEXIT);
      Error(Buffer, ERR_STANDARD | ERR_NOEXIT);
      return -1;
    }
    
    /* Construct required paths. Official path becomes the path to which the zip file is located. */
    memcpy(InstallDir + HomeLength + InstallDirLength + LengthVersion + 3, Instructions[i], InstructionLength + 1);
    memcpy(Official + TempDirLength, Fetched->PackageList[i].Name, ZipLength);
    Official[TempDirLength + ZipLength] = '\0'; 

    ZipPointer = zip_open(Official, 0, &ErrorCode);

    if (!ZipPointer) {
      zip_error_init_with_code(&ZipError, ErrorCode);
      Error("[FATAL]: Unable to open one of the zip files.", ERR_STANDARD | ERR_NOEXIT);
      Error(Official, ERR_STANDARD | ERR_NOEXIT);
      Error((char*)zip_error_strerror(&ZipError), ERR_STANDARD | ERR_NOEXIT);

      return -1;
    }

    /* Read all entries within the zip. */
    int64_t TotalEntries = zip_get_num_entries(ZipPointer, 0);
    
    /* Loop over the entries. */
    for (int64_t Entry = 0; Entry < TotalEntries; Entry++) {
      const char *Name = zip_get_name(ZipPointer, Entry, 0);
      uint32_t NameLength = strlen(Name);
      uint8_t Directory;

      if (!Name) {
        char Buffer[32];
        sprintf(Buffer, "%li", Entry);

        Error("[ERROR]: Error occured while getting one of the entries from a zip.", ERR_STANDARD | ERR_NOEXIT);
        Error(Buffer, ERR_STANDARD | ERR_NOEXIT);
        Error(Official, ERR_STANDARD | ERR_NOEXIT);
        return -1;
      }
      
      Directory = Name[NameLength - 1] == 92; /* Is the entry a directory? */
      
      /* Grrr, windows paths.. */
      ReplacePathSlashes((char*)Name);

      /* If the entry is a directory (only directories end with /) then we'll build the directory tree. */
      if (Directory) {
        memcpy(InstallDir + HomeLength + InstallDirLength + LengthVersion + InstructionLength + 2, Name, NameLength + 1);

        BuildDirectoryTree(InstallDir);
        continue;
      }

      /* Needed for entry size. */
      zip_stat_index(ZipPointer, Entry, 0, ZipStat);
      Memory = calloc(ZipStat->size, sizeof(char));
      
      if (!Memory)
        Error("[FATAL]: Unable to allocate Memory variable during InstallPackages call.", ERR_MEMORY);
      
      /* Include string terminator. */
      memcpy(InstallDir + HomeLength + InstallDirLength + InstructionLength + LengthVersion + 3, Name, NameLength + 1);
      NewFile = fopen(InstallDir, "wb");
      
      if (!NewFile) {
        Error("[ERROR]: Unable to open a file to write contents.", ERR_STANDARD | ERR_NOEXIT);
        Error(InstallDir, ERR_STANDARD | ERR_NOEXIT);
        return -1;
      }

      FileDescriptor = zip_fopen_index(ZipPointer, Entry, 0);
      
      /* Read entry content and then write to the file opened earlier. */
      zip_fread(FileDescriptor, Memory, ZipStat->size);
      fwrite(Memory, sizeof(char), ZipStat->size, NewFile);
      
      /* Cleanup. */
      zip_fclose(FileDescriptor);
      fclose(NewFile);
      free(Memory);
    }
    
    free(ZipStat);
    zip_close(ZipPointer);

    printf("[INFO]: Installing package %i out of %i.\r", i + 1, Fetched->TotalPackages);
    fflush(stdout);
  }

  printf("\n");

  /* Free the instructions 2D array. */
  for (uint8_t i = 0; i < Fetched->TotalPackages; i++)
    free(Instructions[i]);
  free(Instructions);
  
  /* Remove temporary directory. We need to reset the path buffer. 
   * We ignore error checking because really, what's the point? We're almost done. */
  Official[InstallerLength + TempDirLength] = '\0';
  remove(Official);

  /* Free additional variables. */
  free(FullURL);
  free(InstallDir);
  free(Official);

  return 0;
}

int8_t DownloadPackages(FetchStruct *Fetched, char *Version) {
  uint32_t LengthURL = strlen(CDN_URL), LengthVersion = strlen(Version), RootPartLength = strlen(TEMP_PWOOTIE_FOLDER);
  /* One extra byte for the null terminator and one extra byte for the dash. 
   * Allocate the maximum possible size to avoid reallocating the buffers every single package download. */
  char *FullURL = malloc((LengthURL + LengthVersion + Fetched->LongestName + 2) * sizeof(char));
  char *ZipFilePath = malloc((RootPartLength + Fetched->LongestName + 2) * sizeof(char));

  if (!FullURL)
    Error("[FATAL]: Failed to allocate memory for FullURL during DownloadPackages call.", ERR_MEMORY);
  else if (!ZipFilePath)
    Error("[FATAL]: Failed to allocate memory for ZipFilePath during DownloadPackages call.", ERR_MEMORY);
  
  memcpy(ZipFilePath, TEMP_PWOOTIE_FOLDER, RootPartLength);
  ZipFilePath[RootPartLength] = '/';

  memcpy(FullURL, CDN_URL, LengthURL);
  memcpy(FullURL + LengthURL, Version, LengthVersion);
  FullURL[LengthURL + LengthVersion] = '-';

  /* Create a temp folder for downloads. 
   * The installer function cleans it once installation finishes.*/
  if (mkdir(TEMP_PWOOTIE_FOLDER, 0755) && errno != EEXIST) {
    Error("[FATAL]: Failed to create new directory within /tmp/.", ERR_STANDARD | ERR_NOEXIT);
    return -1;
  }

  /* Loop over all packages and download the require content for them. 
   * The link is structured in the following way: CDN + ClientVersionUpload + '-' + PackageName. */
  for (uint32_t Index = 0; Index < Fetched->TotalPackages; Index++) {
    uint32_t  PackageNameLength = strlen(Fetched->PackageList[Index].Name);
    uint8_t   Downloaded = 0; /* Must be set to 1 when the package downloaded successfully. */

    /* The extra byte is obviously the dash. Well I suppose it's obvious. */
    memcpy(FullURL + LengthURL + LengthVersion + 1, Fetched->PackageList[Index].Name, PackageNameLength);
    FullURL[LengthURL + LengthVersion + PackageNameLength + 1] = '\0';
    
    memcpy(ZipFilePath + RootPartLength + 1, Fetched->PackageList[Index].Name, PackageNameLength);
    ZipFilePath[RootPartLength + PackageNameLength + 1] = '\0';

    /* Download the zip and check checksum. If we fail thrice, terminate the program. /tmp/ is cleaned every boot anyways, I hope. */
    for (uint8_t Attempts = 0; Attempts < 3 && !Downloaded; Attempts++) {
      FILE*     ZipFile = fopen(ZipFilePath, "w+");
      CURLcode  Response = CurlDownload(ZipFile, FullURL);
      uint8_t   Checksum[16];
      char      ChecksumBuf[33];
      
      if (!ZipFile) {
        Error("[FATAL]: Unable to open a zip file.", ERR_STANDARD | ERR_NOEXIT);
        Error(ZipFilePath, ERR_STANDARD | ERR_NOEXIT);
        goto error;
      }

      if (Response != CURLE_OK) {
        printf("\n[ATTEMPT %i]: Failed to download %s. (%s)\n", Attempts, Fetched->PackageList[Index].Name, curl_easy_strerror(Response));
        continue;
      }
      
      /* If checksum is not the same, then our package might be corrupted. If that's the case, just close the file and restart. */
      md5File(ZipFile, Checksum);
      ChecksumToString(Checksum, ChecksumBuf);

      if (strcmp(ChecksumBuf, Fetched->PackageList[Index].Checksum) != 0) {
        printf("\n[ATTEMPT %i]: Checksum is not matching normal %s checksum.\n", Attempts, Fetched->PackageList[Index].Name);
        fclose(ZipFile);
        continue;
      }

      Downloaded = 1;
    }

    if (Downloaded) {
      printf("[INFO]: Downloaded package %i out of %i.\r", Index + 1, Fetched->TotalPackages);
      fflush(stdout);
    } else {
      Error("[ERROR]: Failed to download package. Aborting download.\n", ERR_STANDARD | ERR_NOEXIT);
      goto error;
    }
  }

  printf("\n");
  
  free(ZipFilePath);
  free(FullURL);
  return 0;

error:
  free(ZipFilePath);
  free(FullURL);
  return -1;
}

FetchStruct* FetchPackages(char *Version) {
  MemoryStruct ManifestContent; 
  
  /* Last time I counted there were 33 packages. Hopefully I didn't count them wrong. */
  uint8_t PackageArraySize = 33, CurrentPackage = 0;

  char *FullURL = BuildString(3, CDN_URL, Version, PACKAGE_MANIFEST);
  Package *PackagesData = malloc(sizeof(Package) * PackageArraySize);
  FetchStruct *ReturnStruct = malloc(sizeof(FetchStruct));

  if (!PackagesData)
    Error("[FATAL]: Failed to allocate PackagesData within GetPackages call.", ERR_MEMORY);
  else if (!ReturnStruct)
    Error("[FATAL]: Failed to allocate ReturnStruct within GetPackages call.", ERR_MEMORY);
  
  ManifestContent.Memory = malloc(1);
  ManifestContent.Size = 0;
  
  /* Get the manifest containing all the packages information needed. */
  CURLcode Response = CurlGet(&ManifestContent, (char*)FullURL);

  if (Response != CURLE_OK) {
    Error("[ERROR]: GetPackages call failed to download the Manifest file.", ERR_STANDARD | ERR_NOEXIT);
    Error((char*)curl_easy_strerror(Response), ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }

  /* Construct PackagesData. Inside the for loop we're also allocating more space for PackagesData. 
   * We're skipping first 3 bytes, aka "v0\n"*/
  for (uint32_t i = 4; i < ManifestContent.Size; i++) {
    uint32_t SizePosition = 0, ZipSizePosition = 0, NamePosition = 0, ChecksumPosition = 0;
    /* Sizes need to be converted into numbers. */
    char SizeBuf[64], ZipSizeBuf[64];

    if (PackageArraySize == CurrentPackage) {
      /* Allocate PackageArraySize * 2 more space for packages, to avoid making a lot of expensive malloc calls. 
       * We might be wasting a bit of memory here. */
      PackageArraySize *= 2;
      Package *l_PackagesData = realloc(PackagesData, sizeof(Package) * PackageArraySize);
      
      if (!l_PackagesData) {
        Error("[ERROR]: Unable to reallocate PackagesData from GetPackages function.", ERR_STANDARD | ERR_NOEXIT);
        goto error;
      }

      PackagesData = l_PackagesData;

      // printf("[DEBUG]: Reallocated PackagesData. (new size: %i)\n", PackageArraySize);
    }
    
    /* Maybe I could've did this section in a prettier way. Whatever. 
     * We subtract current position by one because we have to clean the carriage return character. */
    /* First read the name of the package. */
    for (; ManifestContent.Memory[i] != '\n'; i++, NamePosition++)
      PackagesData[CurrentPackage].Name[NamePosition] = ManifestContent.Memory[i];
    PackagesData[CurrentPackage].Name[NamePosition - 1] = '\0';

    i++;

    /* Next read the checksum. */
    for (; ManifestContent.Memory[i] != '\n'; i++, ChecksumPosition++)
      PackagesData[CurrentPackage].Checksum[ChecksumPosition] = ManifestContent.Memory[i];
    PackagesData[CurrentPackage].Checksum[ChecksumPosition - 1] = '\0';

    i++;

    /* Read the uncompressed size. */
    for (; ManifestContent.Memory[i] != '\n'; i++, SizePosition++)
      SizeBuf[SizePosition] = ManifestContent.Memory[i];
    SizeBuf[SizePosition - 1] = '\0';

    i++;

    /* Read the compressed size. */
    for (; ManifestContent.Memory[i] != '\n'; i++, ZipSizePosition++)
      ZipSizeBuf[ZipSizePosition] = ManifestContent.Memory[i];
    ZipSizeBuf[ZipSizePosition - 1] = '\0';

    PackagesData[CurrentPackage].Size    = atoi(SizeBuf);
    PackagesData[CurrentPackage].ZipSize = atoi(ZipSizeBuf);
    
    /* Update LongestPackageName. */
    if (ReturnStruct->LongestName < NamePosition)
      ReturnStruct->LongestName = NamePosition;

    CurrentPackage++;
  }

  ReturnStruct->TotalPackages = CurrentPackage;
  ReturnStruct->PackageList   = PackagesData;

  free(FullURL);
  free(ManifestContent.Memory);
  return ReturnStruct;

error:
  free(PackagesData);
  free(FullURL);
  free(ManifestContent.Memory);
  return NULL;
}
