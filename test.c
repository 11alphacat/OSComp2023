#include <stdio.h>
int main() {
    char buf[100];
    int a = snprintf(buf, 10, "%s\n", "671231111");
    printf("%d %s\n", a, buf);
    return 0;
}