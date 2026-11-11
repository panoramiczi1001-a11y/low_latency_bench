#include <cstdlib>
int f(int n) {
    int *a = (int*)malloc(sizeof(int) * 100);
    for (int i = 0; i < n; i++) a[i] = i;
    int s = 0;
    for (int i = 0; i < n; i++) s += a[i];
    free(a);
    return s;
}

int main(){
        return f(5);
}
