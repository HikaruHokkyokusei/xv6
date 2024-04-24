#include "./../kernel/types.h"
#include "./../user/user.h"

int main(void) {
    int runtime = uptime();
    fprintf(1, "I have been running for %d.%d seconds...\n", runtime / 10, runtime % 10);
    exit(0);
}
