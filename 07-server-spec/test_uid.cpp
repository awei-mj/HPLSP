#include <unistd.h>
#include <cstdio>

int main()
{
    uid_t uid = getuid();
    uid_t euid = geteuid();
    printf("userid is %d, effective userid is: %d\n", uid, euid);
    return 0;
}
