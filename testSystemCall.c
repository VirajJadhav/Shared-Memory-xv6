#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[]) {
    printf(1, "Value returned from system call: %d\n", getvalue());

    exit();
}