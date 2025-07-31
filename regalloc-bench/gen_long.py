N = 1000
# 宣告函式參數
param_list = ", ".join([f"int a{i}" for i in range(1, N+1)])
print(f"int regalloc_long(int x, {param_list}) {{")
print("    int y = x;")
for i in range(1, N+1):
    print(f"    y = y + a{i};")
print("    return y;")
print("}")
