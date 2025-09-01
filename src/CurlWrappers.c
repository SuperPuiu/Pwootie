#include <Shared.h>
#define CURL_ARRAY_SIZE 32

CURL *CurlHandle;
CURL *CurlDownloadHandle;
CURL *CurlDownloadArray[CURL_ARRAY_SIZE];

CURLM *CurlMulti;

size_t WriteMemoryCallback  (void *Contents, size_t Size, size_t nmemb, void *userp);
size_t WriteFileCallback    (void *Contents, size_t Size, size_t nmemb, FILE *Stream);

void SetupHandles() {
  CurlHandle = curl_easy_init(), CurlDownloadHandle = curl_easy_init();

  curl_easy_setopt(CurlHandle, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(CurlHandle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  curl_easy_setopt(CurlHandle, CURLOPT_USERAGENT, "PwootieFetch/1.0");

  curl_easy_setopt(CurlDownloadHandle, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(CurlDownloadHandle, CURLOPT_WRITEFUNCTION, WriteFileCallback);
  curl_easy_setopt(CurlDownloadHandle, CURLOPT_USERAGENT, "PwootieDownload/1.0");
  
  CurlMulti = curl_multi_init();

  for (uint32_t CurrentIndex = 0; CurrentIndex < CURL_ARRAY_SIZE; CurrentIndex++) {
    CurlDownloadArray[CurrentIndex] = curl_easy_init();

    curl_easy_setopt(CurlDownloadArray[CurrentIndex], CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(CurlDownloadArray[CurrentIndex], CURLOPT_WRITEFUNCTION, WriteFileCallback);
    curl_easy_setopt(CurlDownloadArray[CurrentIndex], CURLOPT_USERAGENT, "PwootieDownload/1.0");
  }
}

void ResetMultiCurl(uint16_t Total) {
  for (uint16_t CurrentIndex = 0; CurrentIndex < Total; CurrentIndex++)
    curl_multi_remove_handle(CurlMulti, CurlDownloadArray[CurrentIndex]);
}

CURLcode CurlGet(MemoryStruct *Chunk, char *WithURL) {
  curl_easy_setopt(CurlHandle, CURLOPT_URL, WithURL);
  curl_easy_setopt(CurlHandle, CURLOPT_WRITEDATA, (void *)Chunk);
  return curl_easy_perform(CurlHandle);
}

int32_t CurlGetHandleFromMessage(CURLMsg *Message) {
  int32_t Index = -1;

  if (Message->msg == CURLMSG_DONE) {
    for (Index = 0; Index < CURL_ARRAY_SIZE; Index++) {
      if (Message->easy_handle == CurlDownloadArray[Index])
        break;
    }
  }

  return Index;
}

int8_t CurlMultiSetup(FILE **Files, char **Links, uint16_t Total) {
  if (Total > CURL_ARRAY_SIZE)
    return -1;

  for (uint16_t CurrentIndex = 0; CurrentIndex < Total; CurrentIndex++) {
    curl_easy_setopt(CurlDownloadArray[CurrentIndex], CURLOPT_URL, Links[CurrentIndex]);
    curl_easy_setopt(CurlDownloadArray[CurrentIndex], CURLOPT_WRITEDATA, Files[CurrentIndex]);
    curl_multi_add_handle(CurlMulti, CurlDownloadArray[CurrentIndex]);
  }

  return 0;
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
