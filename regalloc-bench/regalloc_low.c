// regalloc_low.c
int regalloc_low(int N) {
    int sum = 0;
    for (int i = 1; i <= N; ++i) {
        sum += i;
    }
    return sum;
}
