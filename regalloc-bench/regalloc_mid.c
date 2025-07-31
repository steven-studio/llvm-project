// regalloc_mid.c
int f1(int x) { return x + 1; }
int f2(int x) { return x * 2; }
int f3(int x) { return x - 3; }

int regalloc_mid(int x) {
    int a = f1(x);
    int b = f2(x);
    int c = a + b;
    int d = f3(c);
    int e = d + b;
    return e;
}
