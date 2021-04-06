#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[]) {
    printf(1, "Used to test system calls. %d\n", shmget(0, 2000, 1));

    exit();
}