// regalloc_high.c
int compute(int x, int i) { return x + i; }

int regalloc_high(int x) {
    int v1  = compute(x, 1);
    int v2  = compute(x, 2);
    int v3  = compute(x, 3);
    int v4  = compute(x, 4);
    int v5  = compute(x, 5);
    int v6  = compute(x, 6);
    int v7  = compute(x, 7);
    int v8  = compute(x, 8);
    int v9  = compute(x, 9);
    int v10 = compute(x, 10);
    int v11 = compute(x, 11);
    int v12 = compute(x, 12);
    int v13 = compute(x, 13);
    int v14 = compute(x, 14);
    int v15 = compute(x, 15);
    int v16 = compute(x, 16);
    int v17 = compute(x, 17);
    int v18 = compute(x, 18);
    int v19 = compute(x, 19);
    int v20 = compute(x, 20);

    // 所有變數最後一起被使用
    int total = v1 + v2 + v3 + v4 + v5 +
                v6 + v7 + v8 + v9 + v10 +
                v11 + v12 + v13 + v14 + v15 +
                v16 + v17 + v18 + v19 + v20;

    return total;
}
