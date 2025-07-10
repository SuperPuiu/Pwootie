#include <Shared.h>

int main(int argc, char **argv) {
  OpenPwootieFile();
  curl_global_init(CURL_GLOBAL_ALL);

  VersionData Data;
  char *StudioVersion = Data.ClientVersionUpload;
  char *ForcedVersion = PwootieReadEntry("forced_version");
  
  /* If we have a ForcedVersion, then use that. */
  StudioVersion = ForcedVersion ? ForcedVersion : StudioVersion;

  SetupHandles();
  GetVersionData(&Data);

  if (argc > 1) {
    if (strcmp(argv[1], "reinstall") == 0) {
      if (strcmp(argv[2], "proton") == 0)
        SetupProton(0);
      else if (strcmp(argv[2], "studio"))
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
    }

    /* The first argument can be the token we need to log into studio. */
    Run(argv[1], StudioVersion);
  } else {
    Install(StudioVersion, 1);
    Run(NULL, StudioVersion);
  }

exit:
  /* Cleanup */
  free(Data.Version);
  free(Data.ClientVersionUpload);
  free(Data.BootstrapperVersion);
  
  if (ForcedVersion)
    free(ForcedVersion);

  curl_global_cleanup();
  PwootieExit();
  return 0;
}
