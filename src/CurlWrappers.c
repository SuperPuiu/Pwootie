#include <Shared.h>

CURL *CurlHandle;
CURL *CurlDownloadHandle;

size_t WriteMemoryCallback  (void *Contents, size_t Size, size_t nmemb, void *userp);
size_t WriteFileCallback    (void *Contents, size_t Size, size_t nmemb, FILE *Stream);

/* Initialize the CurlHandle once and never do it again. Pretty sure this is the recommended way to use curl. */
void SetupHandles() {
  CurlHandle = curl_easy_init(), CurlDownloadHandle = curl_easy_init();

  curl_easy_setopt(CurlHandle, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(CurlHandle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  curl_easy_setopt(CurlHandle, CURLOPT_USERAGENT, "PwootieFetch/1.0");

  curl_easy_setopt(CurlDownloadHandle, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(CurlDownloadHandle, CURLOPT_WRITEFUNCTION, WriteFileCallback);
  curl_easy_setopt(CurlDownloadHandle, CURLOPT_USERAGENT, "PwootieDownload/1.0");
}

CURLcode CurlGet(MemoryStruct *Chunk, char *WithURL) {
  curl_easy_setopt(CurlHandle, CURLOPT_URL, WithURL);
  curl_easy_setopt(CurlHandle, CURLOPT_WRITEDATA, (void *)Chunk);
  return curl_easy_perform(CurlHandle);
}

CURLcode CurlDownload(FILE *File, char *WithURL) {
  curl_easy_setopt(CurlDownloadHandle, CURLOPT_URL, WithURL);
  curl_easy_setopt(CurlDownloadHandle, CURLOPT_WRITEDATA, File);
  return curl_easy_perform(CurlDownloadHandle);
}

size_t WriteFileCallback(void *Contents, size_t Size, size_t nmemb, FILE *Stream) {
  return fwrite(Contents, Size, nmemb, Stream); /* Written bytes. */
}

size_t WriteMemoryCallback(void *Contents, size_t Size, size_t nmemb, void *userp) {
  size_t RequiredSize = Size * nmemb;
  MemoryStruct *Memory = (MemoryStruct *)userp;

  char *Pointer = realloc(Memory->Memory, Memory->Size + RequiredSize + 1);

  if (Pointer == NULL) {
    printf("WirteMemoryCallback: not enough memory.\n");
    return 0;
  }

  Memory->Memory = Pointer;
  memcpy(&(Memory->Memory[Memory->Size]), Contents, RequiredSize);
  Memory->Size += RequiredSize;
  Memory->Memory[Memory->Size] = 0;

  return RequiredSize;
}
