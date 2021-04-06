#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[]) {
    shmget(2, 4, 5);
    printf(1, "Used to test system calls. \n");

    exit();
}