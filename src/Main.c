#include <Shared.h>

int main(int argc, char **argv) {
  OpenPwootieFile();
  curl_global_init(CURL_GLOBAL_ALL);

  VersionData Data = {NULL, NULL, NULL};

  SetupHandles();
  GetVersionData(&Data);

  char *StudioVersion = Data.ClientVersionUpload;
  char *ForcedVersion = PwootieReadEntry("forced_version");
  
  /* If we have a ForcedVersion, then use that. */
  StudioVersion = ForcedVersion ? ForcedVersion : StudioVersion;

  if (argc > 1) {
    if (strcmp(argv[1], "reinstall") == 0) {
      if (argc < 3) {
        printf("[INFO]: No reinstall option specified. You must either specify proton or studio to be reinstalled.\n");
        goto exit;
      }

      if (strcmp(argv[2], "proton") == 0)
        SetupProton(0);
      else if (strcmp(argv[2], "studio") == 0)
        Install(StudioVersion, 0);
      else
        printf("[INFO]: Unknown reinstall option. (available options: proton, studio)\n");
      
      goto exit;
    } else if (strcmp(argv[1], "fflags") == 0) {
      if (LoadFFlags(StudioVersion) == -1) {
        Error("[INFO]: Failed to load fflags.\n", ERR_STANDARD | ERR_NOEXIT);
        goto exit;
      }
      
      if (argc == 2) {
        printf("[INFO]: fflags require one or more parameters. See the documentation for more information.\n");
        goto exit;
      }
      
      if (strcmp(argv[2], "apply") == 0)
        ApplyFFlag(argv[3], argv[4]);
      else if (strcmp(argv[2], "read") == 0)
        OutputFFlags(argv[3]);
      else
        printf("[INFO]: Unknown fflags option. (available options: apply, read)\n");

      goto exit;
    } else if (strcmp(argv[1], "user") == 0) {
      if (argc < 3) {
        printf("[INFO]: No user option specified. (available options: add)\n");
        goto exit;
      }

      if (strcmp(argv[2], "add") == 0) {
        if (argc < 5) {
          printf("[INFO]: Command 'user add' called with wrong number of arguments. (userid and name are required)\n");
          goto exit;
        }

        AddNewUser(argv[3], argv[4], argc >= 5 ? argv[5] : NULL);
      } else {
        printf("[INFO]: Unknown user option. (available options: add)\n");
      }

      goto exit;
    } else if (strcmp(argv[1], "cookie") == 0) {
      if (strcmp(argv[3], "read") == 0) {

      } else if (strcmp(argv[3], "write") == 0) {
        if (argc < 3) {
          printf("[INFO]: Command 'cookie write' called with wrong number of arguments. (cookie is required)\n");
          goto exit;
        }

        goto exit;
      } else {
        printf("[INFO]: No cookie option specified. (available options: read, write)\n");
        goto exit;
      }
    }

    /* The first argument can be the token we need to log into studio. */
    Run(argv[1], StudioVersion);
  } else {
    Install(StudioVersion, 1);
    Run(NULL, StudioVersion);
  }

exit:
  /* Cleanup */
  if (Data.Version)
    free(Data.Version);
  
  if (Data.ClientVersionUpload)
    free(Data.ClientVersionUpload);
  
  if (Data.ClientVersionUpload)
    free(Data.BootstrapperVersion);
  
  if (ForcedVersion)
    free(ForcedVersion);

  curl_global_cleanup();
  PwootieExit();
  return 0;
}
