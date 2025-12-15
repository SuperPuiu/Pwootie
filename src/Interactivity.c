#include <Shared.h>
#include <stdarg.h>

uint8_t AskForConfirmation(char *InitialMessage) {
  printf("%s\n", InitialMessage);
  char Input[64];

  do {
  fgets(Input, 64, stdin);

  if (Input[0] == 'y' || Input[0] == 'Y')
    return 1;
  else if (Input[0] == 'n' || Input[0] == 'N')
    return 0;

  printf("Invalid input. Enter [y] for yes or [n] for no.\n");
  } while (1);
}

uint8_t AskForOption(uint8_t Options, char **OptionNames) {
  printf("Please select one of the following:\n");
  char Input[64];

  for (uint8_t i = 0; i < Options; i++)
    printf("> %s: %u\n", OptionNames[i], i);

  do {
    fgets(Input, 64, stdin);
    Input[strlen(Input) - 1] = '\0'; /* Get rid of the newline. */

    uint8_t Selected = atoi(Input);

    if (Selected > Options)
      printf("Invalid option. Input a number within the provided range.");
    else
      return Selected; /* Even if an error was encountered, 0 is returned by atoi so just return that. */
  } while (1);
}
