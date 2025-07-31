N = 100
print("int compute(int x, int i) { return x + i; }")
print("int regalloc_high(int x) {")
for i in range(1, N+1):
    print(f"    int v{i} = compute(x, {i});")
print("    int total =", " + ".join([f"v{i}" for i in range(1, N+1)]) + ";")
print("    return total;")
print("}")
