#include <Shared.h>

int main(int argc, char **argv) {
  curl_global_init(CURL_GLOBAL_ALL);

  VersionData Data;

  SetupHandles();
  GetVersionData(&Data);

  if (argc > 1) {
    if (strcmp(argv[1], "reinstall") == 0) {
      Install(&Data, 0);
    }
  } else {

  }
  
  Install(&Data, 1);
  printf("%s %s %s\n", Data.Version, Data.ClientVersionUpload, Data.BootstrapperVersion);

  /* The second argument can be the token we need to log into studio. */
  Run(argc > 1 ? argv[1] : NULL, Data.ClientVersionUpload);

  /* Cleanup */
  free(Data.Version);
  free(Data.ClientVersionUpload);
  free(Data.BootstrapperVersion);

  curl_global_cleanup();
  PwootieExit();

  return 0;
}
