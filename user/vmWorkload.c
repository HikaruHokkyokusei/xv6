#include "./../kernel/types.h"
#include "./../kernel/stat.h"
#include "./../user/user.h"

void workload0() {
  for(;;)
    ;
}

void workload1() {

}

void workload2() {

}

int
main(int argc, char *argv[])
{
  if (argc <= 1) {
    exit(0);
  }

  switch (atoi(argv[1])) {
    case 0:
      workload0();
      break;

    case 1:
      workload1();
      break;

    case 2:
      workload2();
      break;

    default:
      exit(1);
  }

  exit(0);
}