#include <Shared.h>
#include <errno.h>

uint8_t Compare(char *Buffer) {
  char *Required = ".rdata";

  for (uint8_t i = 0; i < 5; i++)
    if (Buffer[i] != Required[i])
      return 0;
  return 1;
}

char** ExtractInstructions(FILE *Installer, FetchStruct *Fetched) {
  /* ExtractInstructions's sole purpose is to extract the install instructions found within RobloxStudioInstaller.exe. 
   * I first found this solution while looking at the Vinegar implementation for it, and I find it kinda innovative. 
   * Apparently, the install instructions are beautifully placed next to each other in the .rdata section. How convenient! 
   *
   * Now, to actually EXTRACT those instructions, we have the following solutions:
   * 1. Read the whole file until the Instructions list is completed.
   * CONS: Much slower to skip useless bytes and blindfully searching for the required information.
   * PROS: Easier to implement.
   * 2. Read the header information then jump straight to .rdata and begin searching. 
   * CONS: Harder to implement.
   * PROS: It should be faster than blindfully searching.
   * 
   * Useful resources:
   * https://medium.com/@jasemalsadi/understanding-pe-file-for-reversers-9aa3b59ab13c 
   * https://wiki.osdev.org/PE */

  uint8_t TotalPackages = Fetched->TotalPackages;
  char **Instructions = malloc(TotalPackages * sizeof(char*));
  char *RawData;
  char *PackageName = malloc(Fetched->LongestName * sizeof(char));

  uint32_t PE_Offset, RawDataPointer, RawDataSize;
  uint16_t OptHeaderSize;
  char SectionName[8];

  if (unlikely(!Instructions))
    Error("[FATAL]: Unable to allocate Instructions array in ExtractInstructions call.", ERR_MEMORY);

  for (uint8_t i = 0; i < TotalPackages; i++) {
    Instructions[i] = malloc(128 * sizeof(char)); /* May be wasting a bit of memory here. */

    if (unlikely(!Instructions[i]))
      Error("[FATAL]: Unable to allocate instructions array for a package during ExtractInstructions call.", ERR_MEMORY);
  }

  /* Read only the required information from the MS-DOS header. 
   * We only need the PE_Offset from the MS-DOS header. This is so we skip the STUB PROGRAM which is after the header. */
  fseek(Installer, 60, SEEK_SET);
  fread(&PE_Offset, 4, 1, Installer);

  /* Jump to PE_Offset. This is the start of the PE Header. */
  fseek(Installer, PE_Offset, SEEK_SET);

  /* Skip a bunch more bytes from the header until we reach the bytes which tell us the size of the optional header. */
  fseek(Installer, 20, SEEK_CUR);
  fread(&OptHeaderSize, sizeof(OptHeaderSize), 1, Installer);

  /* Jump over the characteristics part of the header + the optional header. */
  fseek(Installer, 2 + OptHeaderSize, SEEK_CUR);

  /* Now we could skip 40 bytes, as the first section header appears to be the .text section.
   * However, because I like making things more complex, we'll compare each section header name until we find ".rdata". */

  while (fread(&SectionName, 8, 1, Installer)) {
    if (Compare(SectionName))
      break;

    /* Skip the 32 bytes of data in the section header. */
    fseek(Installer, 32, SEEK_CUR);
  }

  /* We already read the first 8 bytes of the section earlier. So we just skip 8 more and then read the raw size and raw data pointer. */
  fseek(Installer, 8, SEEK_CUR);
  fread(&RawDataSize, 4, 1, Installer);
  fread(&RawDataPointer, 4, 1, Installer);

  /* Allocate RawData array. We need it for fast access. */
  RawData = malloc(RawDataSize);

  if (unlikely(RawData == NULL))
    Error("[FATAL]: Unable to allocate RawData array during ExtractInstructions call.\n", ERR_MEMORY);

  /* Jump to the .rdata section. */
  fseek(Installer, RawDataPointer, SEEK_CUR);

  /* Read the ENTIRE .rdata section. */
  fread(RawData, 1, RawDataSize, Installer);

  /* Now we just search for our little silly instructions. */
  /* First we need to find the beginning of the list, which starts with " \0{\" ". */
  for (uint32_t i = 1; i < RawDataSize - 2; i++) {
    if (RawData[i - 1] == '\0' && RawData[i] == '{' && RawData[i + 1] == '"') {
      /* We found the list of instructions! Now we just have to extract what we need.. */
      i += 3;

      for (uint8_t Index = 0; Index < Fetched->TotalPackages; Index++) {
        uint8_t DstIndex = 0, CurrentPackage = 0;

        /* Get the package name. */
        while (RawData[i] != '"') {
          PackageName[DstIndex] = RawData[i];
          i++;
          DstIndex++;
        }

        i++;
        PackageName[DstIndex] = '\0';

        /* Skip characters until we hit the start of the next string. */
        while (RawData[i] != '"')
          i++;
        i++;

        /* Find index we need to store the extraction location at. */
        for (uint8_t _i = 0; _i < Fetched->TotalPackages && CurrentPackage == 0; _i++)
          if (strcmp(Fetched->PackageList[_i].Name, PackageName) == 0)
            CurrentPackage = _i;

        DstIndex = 0;

        /* Copy the extraction location to the right instruction index. */
        while (RawData[i] != '"') {
          Instructions[CurrentPackage][DstIndex] = RawData[i];
          DstIndex++;
          i++;
        }

        Instructions[CurrentPackage][DstIndex] = '\0';
        i++;

        /* Bring the index to the first character of the next package name. */
        while (RawData[i] != '"')
          i++;
        i++;
      }

      break;
    }
  }

  /* We're not out of the woods yet. We need to replace the \\ with /. I love path naming. */
  for (uint8_t i = 0; i < Fetched->TotalPackages; i++)
    ReplacePathSlashes(Instructions[i]);

  free(RawData);
  free(PackageName);
  return Instructions;
}
