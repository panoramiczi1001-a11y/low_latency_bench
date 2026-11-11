int f(int n) {
    int a[100];
    for (int i = 0; i < n; i++) a[i] = i;
    int s = 0;
    for (int i = 0; i < n; i++) s += a[i];
    return s;
}

int main(){
	return f(5);
}
