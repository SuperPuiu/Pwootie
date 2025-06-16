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

void BuildDirectoryTree(char *Path) {
  /* I have NO clue how this works. 
   * https://stackoverflow.com/questions/7430248/creating-a-new-directory-in-c */
  char *Separated = strchr(Path + 1, '/');

  while (Separated != NULL) {
    *Separated = '\0';

    if (mkdir(Path, 0755) && errno != EEXIST) {
      printf("[FATAL]: Unable to create %s while creating installation directories.\n", Path);
      exit(EXIT_FAILURE);
    }

    *Separated = '/';
    Separated = strchr(Separated + 1, '/');
  }
}

/* Prototypes are found in Installer.c */
void InstallPackages(FetchStruct *Fetched, VersionData *Client) {
  CURLcode  Response;
  FILE      *Installer;

  uint32_t LengthURL = strlen(CDN_URL), LengthVersion = strlen(Client->ClientVersionUpload);
  uint32_t InstallerLength = strlen(OFFICIAL_INSTALLER), TempDirLength = strlen(TEMP_PWOOTIE_FOLDER);
  uint32_t InstallDirLength = strlen(INSTALL_DIR), HomeLength = strlen(getenv("HOME"));
  uint32_t InstallDirTotal = HomeLength + InstallDirLength + LengthVersion + 3 + 253;
  uint32_t AppSettingsLen = strlen(APP_SETTINGS), SettingsLen = strlen(APP_SETTINGS_DATA);

  /* One extra byte for the null terminator and one extra byte for the dash. InstallDir needs two additonal slashes. */
  char *FullURL         = malloc((LengthURL + LengthVersion + InstallerLength + 2) * sizeof(char));
  char *Official        = malloc((InstallerLength + TempDirLength + 64 + 1) * sizeof(char));
  char *InstallDir      = malloc((InstallDirTotal) * sizeof(char));

  if (!FullURL) {
    printf("[FATAL]: Unable to allocate FullURL during InstallPackages call.\n");
    exit(EXIT_FAILURE);
  } else if (!Official) {
    printf("[FATAL]: Unable to allocate Official during InstallPackages call.\n");
    exit(EXIT_FAILURE);
  } else if (!InstallDir) {
    printf("[FATAL]: Unable to allocate InstallDir during InstallPackages call.\n");
    exit(EXIT_FAILURE);
  }

  /* Construct URL. */
  memcpy(FullURL, CDN_URL, LengthURL);
  memcpy(FullURL + LengthURL, Client->ClientVersionUpload, LengthVersion);
  memcpy(FullURL + LengthURL + LengthVersion + 1, OFFICIAL_INSTALLER, InstallerLength);
  FullURL[LengthURL + LengthVersion] = '-';
  FullURL[LengthURL + LengthVersion + InstallerLength + 1] = '\0';
  
  /* Construct official downloaded path. */
  memcpy(Official, TEMP_PWOOTIE_FOLDER, TempDirLength);
  memcpy(Official + TempDirLength, OFFICIAL_INSTALLER, InstallerLength);
  Official[InstallerLength + TempDirLength] = '\0';
  
  /* Construct installation directory path. */
  memcpy(InstallDir, getenv("HOME"), HomeLength);
  memcpy(InstallDir + HomeLength + 1, INSTALL_DIR, InstallDirLength);
  memcpy(InstallDir + HomeLength + InstallDirLength + 2, Client->ClientVersionUpload, LengthVersion);
  InstallDir[HomeLength] = InstallDir[HomeLength + InstallDirLength + 1] = InstallDir[HomeLength + InstallDirLength + LengthVersion + 2] = '/';
  InstallDir[HomeLength + InstallDirLength + LengthVersion + 3] = '\0';

  /* Download installer in TEMP_PWOOTIE_FOLDER folder. */
  Installer = fopen(Official, "w+");

  if (!Installer) {
    printf("[FATAL]: Unable to create file %s during InstallPackages.\n", Official);
    exit(EXIT_FAILURE);
  }

  Response = CurlDownload(Installer, FullURL);

  if (Response != CURLE_OK) {
    printf("[FATAL]: Failed to download RobloxStudioInstaller.exe.\n");
    exit(EXIT_FAILURE);
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
    printf("[FATAL]: Failed to create AppSettings durign InstallPackages call.\n");
    exit(EXIT_FAILURE);
  }

  fwrite(APP_SETTINGS_DATA, sizeof(char), SettingsLen, AppSettings);
  fclose(AppSettings);

  /* Decompress all zips downloaded during DownloadPackages call. */
  for (uint8_t i = 0; i < Fetched->TotalPackages; i++) {
    uint32_t        ZipLength = strlen(Fetched->PackageList[i].Name), InstructionLength = strlen(Instructions[i]);
    int32_t         Error = 0;
    zip_t           *ZipPointer;
    zip_error_t     ZipError;
    struct zip_stat *ZipStat = calloc(256, sizeof(int));
    zip_file_t      *FileDescriptor;
    
    char *Memory;
    FILE *NewFile;
    
    if (!ZipStat) {
      printf("[FATAL]: Failed to allocate ZipStat for index %i during InstallPackages call.\n", i);
      exit(EXIT_FAILURE);
    }
    
    /* Construct required paths. Official path becomes the path to which the zip file is located. */
    memcpy(InstallDir + HomeLength + InstallDirLength + LengthVersion + 3, Instructions[i], InstructionLength + 1);
    memcpy(Official + TempDirLength, Fetched->PackageList[i].Name, ZipLength);
    Official[TempDirLength + ZipLength] = '\0'; 

    ZipPointer = zip_open(Official, 0, &Error);

    if (!ZipPointer) {
      zip_error_init_with_code(&ZipError, Error);
      printf("[FATAL]: Unable to open zip %s. (%s)\n", Official, zip_error_strerror(&ZipError));
      exit(EXIT_FAILURE);
    }

    /* Read all entries within the zip. */
    int64_t TotalEntries = zip_get_num_entries(ZipPointer, 0);
    
    /* Loop over the entries. */
    for (int64_t Entry = 0; Entry < TotalEntries; Entry++) {
      const char *Name = zip_get_name(ZipPointer, Entry, 0);
      uint32_t NameLength = strlen(Name);
      uint8_t Directory;

      if (!Name) {
        printf("[FATAL]: Error occured while getting entry %li from zip %s.\n", Entry, Official);
        exit(EXIT_FAILURE);
      }
      
      Directory = Name[NameLength - 1] == 92; /* Is the entry a directory? */
      
      /* Grrr, windows paths.. */
      ReplacePathSlashes((char*)Name);

      /* If the entry is a directory (only directories end with /) then we'll build the directory tree. */
      if (Directory) {
        memcpy(InstallDir + HomeLength + InstallDirLength + LengthVersion + InstructionLength + 2, Name, NameLength + 1);
        printf("%s\n", InstallDir);

        BuildDirectoryTree(InstallDir);
        continue;
      }

      /* Needed for entry size. */
      zip_stat_index(ZipPointer, Entry, 0, ZipStat);
      Memory = calloc(ZipStat->size, sizeof(char));
      
      if (!Memory) {
        printf("[FATAL]: Unable to allocate Memory variable for %s during InstallPackages call.\n", Name);
        exit(EXIT_FAILURE);
      }
      
      /* Include string terminator. */
      memcpy(InstallDir + HomeLength + InstallDirLength + InstructionLength + LengthVersion + 3, Name, NameLength + 1);
      NewFile = fopen(InstallDir, "wb");
      
      if (!NewFile) {
        printf("[FATAL]: Unable to open %s to write contents. (errno: %s)\n", InstallDir, strerror(errno));
        exit(EXIT_FAILURE);
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

    printf("[INFO]: Installed package %i out of %i.\n", i + 1, Fetched->TotalPackages);
  }

  /* Free the instructions 2D array. */
  for (uint8_t i = 0; i < Fetched->TotalPackages; i++)
    free(Instructions[i]);
  free(Instructions);
  
  /* Free additional variables. */
  free(FullURL);
  free(InstallDir);
  free(Official);
}

void DownloadPackages(FetchStruct *Fetched, VersionData *Client) {
  uint32_t LengthURL = strlen(CDN_URL), LengthVersion = strlen(Client->ClientVersionUpload), RootPartLength = strlen(TEMP_PWOOTIE_FOLDER);
  /* One extra byte for the null terminator and one extra byte for the dash. 
   * Allocate the maximum possible size to avoid reallocating the buffers every single package download. */
  char *FullURL = malloc((LengthURL + LengthVersion + Fetched->LongestName + 2) * sizeof(char));
  char *ZipFilePath = malloc((RootPartLength + Fetched->LongestName + 2) * sizeof(char));

  if (!FullURL) {
    printf("[FATAL]: Failed to allocate memory for FullURL during DownloadPackages call.\n");
    exit(EXIT_FAILURE);
  } else if (!ZipFilePath) {
    printf("[FATAL]: Failed to allocate memory for ZipFilePath during DownloadPackages call.\n");
    exit(EXIT_FAILURE);
  }
  
  memcpy(ZipFilePath, TEMP_PWOOTIE_FOLDER, RootPartLength);
  ZipFilePath[RootPartLength] = '/';

  memcpy(FullURL, CDN_URL, LengthURL);
  memcpy(FullURL + LengthURL, Client->ClientVersionUpload, LengthVersion);
  FullURL[LengthURL + LengthVersion] = '-';

  /* Create a temp folder for downloads. 
   * The installer function cleans it once installation finishes.*/
  if (mkdir(TEMP_PWOOTIE_FOLDER, 0755) && errno != EEXIST) {
    printf("[FATAL]: Failed to create new directory within /tmp/. (%s)\n", strerror(errno));
    exit(EXIT_FAILURE);
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
        printf("[FATAL]: Unable to open %s.\n", ZipFilePath);
        exit(EXIT_FAILURE);
      }

      if (Response != CURLE_OK) {
        printf("[ATTEMPT %i]: Failed to download %s. (%s)\n", Attempts, Fetched->PackageList[Index].Name, curl_easy_strerror(Response));
        continue;
      }
      
      /* If checksum is not the same, then our package might be corrupted. If that's the case, just close the file and restart. */
      md5File(ZipFile, Checksum);
      ChecksumToString(Checksum, ChecksumBuf);

      if (strcmp(ChecksumBuf, Fetched->PackageList[Index].Checksum) != 0) {
        printf("[ATTEMPT %i]: Checksum is not matching normal %s checksum.\n", Attempts, Fetched->PackageList[Index].Name);
        fclose(ZipFile);
        continue;
      }

      Downloaded = 1;
    }

    if (Downloaded) {
      printf("[INFO]: Downloaded package %i out of %i.\n", Index + 1, Fetched->TotalPackages);
    } else {
      printf("[FATAL]: Failed to download package. Aborting download.\n");
      exit(EXIT_FAILURE);
    }
  }
  
  free(ZipFilePath);
  free(FullURL);
}

FetchStruct* FetchPackages(VersionData *Client) {
  MemoryStruct ManifestContent; 
  
  /* Last time I counted there were 33 packages. Hopefully I didn't count them wrong. */
  uint8_t PackageArraySize = 33, CurrentPackage = 0;
  uint32_t LengthURL = strlen(CDN_URL), LengthVersion = strlen(Client->ClientVersionUpload), ManifestLength = strlen(PACKAGE_MANIFEST);
  
  uint8_t *FullURL = malloc((LengthURL + LengthVersion + ManifestLength + 1) * sizeof(uint8_t)); /* One extra byte for null terminator. */
  Package *PackagesData = malloc(sizeof(Package) * PackageArraySize);
  FetchStruct *ReturnStruct = malloc(sizeof(FetchStruct));

  if (!FullURL) {
    printf("[FATAL]: Failed to allocate FullURL within GetPackages call.\n");
    exit(EXIT_FAILURE);
  } else if (!PackagesData) {
    printf("[FATAL]: Failed to allocate PackagesData within GetPackages call.\n");
    exit(EXIT_FAILURE);
  } else if (!ReturnStruct) {
    printf("[FATAL]: Failed to allocate ReturnStruct within GetPackages call.\n");
    exit(EXIT_FAILURE);
  }

  ManifestContent.Memory = malloc(1);
  ManifestContent.Size = 0;
  
  /* Construct URL */
  memcpy(FullURL, CDN_URL, LengthURL);
  memcpy(FullURL + LengthURL, Client->ClientVersionUpload, LengthVersion);
  memcpy(FullURL + LengthURL + LengthVersion, PACKAGE_MANIFEST, ManifestLength);
  FullURL[LengthURL + LengthVersion + ManifestLength] = '\0';
  
  /* Get the manifest containing all the packages information needed. */
  CURLcode Response = CurlGet(&ManifestContent, (char*)FullURL);

  if (Response != CURLE_OK) {
    printf("[FATAL]: GetPackages failed: %s\n", curl_easy_strerror(Response));
    exit(EXIT_FAILURE);
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
        printf("[FATAL]: Unable to reallocate PackagesData from GetPackages function.\n");
        exit(EXIT_FAILURE);
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
}
