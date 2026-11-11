#include <vector>
int f(int n) {
    std::vector<int> v;
    for (int i = 0; i < n; i++) v.push_back(i);
    int s = 0;
    for (int x : v) s += x;
    return s;
}

int main(){
        return f(5);
}

