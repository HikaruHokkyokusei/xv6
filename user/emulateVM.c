#include "./../kernel/types.h"
#include "./../user/user.h"

char rawInp[512];
char *getUserChoice(char *prompt) {
  if (prompt != 0x0)
    printf(prompt);
  return gets(rawInp, 512);
}

int getUserChoiceInt(char *prompt) {
  if (prompt == 0x0)
    prompt = "\n\nMenu:\n"
             "-1. Exit\n"
             " 1. Create New VM\n"
             " 2. Delete a VM\n"
             " 3. Print active VM\n"
             "> ";

  return atoi(getUserChoice(prompt));
}

int main(void) {
  initVMManager();

  int reg1;

  int choice;
  for (;;) {
    choice = getUserChoiceInt(0x0);
    switch (choice) {
      case -1:
        printf("Exiting...\n");
        goto EXIT;

      case 1:
        reg1 = createVM(getUserChoice("Enter workload number: "));
        if (reg1 == -1) {
          printf("Fork Failed.\n");
        } else if (reg1 == -2) {
          printf("Cannot create VM. Max limit reached.\n");
        } else {
          printf("VM successfully created.\n");
        }
        break;

      case 2:
        reg1 = deleteVM(getUserChoiceInt("Enter VM Id to delete: "));
        if (reg1 == -1) {
          printf("Invalid Index.\n");
        } else if (reg1 == 0) {
          printf("VM does not exists or already deleted.\n");
        } else {
          printf("VM successfully deleted.\n");
        }
        break;

      case 3:
        printActiveVM();
        break;

      default:
        break;
    }
  }

  EXIT:
  exit(0);
}
