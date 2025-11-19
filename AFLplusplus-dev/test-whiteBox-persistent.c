/*
 * test-whiteBox.c 的持久模式模糊测试版本
 * 速度提升 10-20 倍
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// 兼容性代码（允许在没有 AFL++ 时编译）
#ifndef __AFL_FUZZ_TESTCASE_LEN
  ssize_t fuzz_len;
  unsigned char fuzz_buf[1024000];
  #define __AFL_FUZZ_TESTCASE_LEN fuzz_len
  #define __AFL_FUZZ_TESTCASE_BUF fuzz_buf
  #define __AFL_FUZZ_INIT() void sync(void);
  #define __AFL_LOOP(x) \
    ((fuzz_len = read(0, fuzz_buf, sizeof(fuzz_buf))) > 0 ? 1 : 0)
  #define __AFL_INIT() sync()
#endif

__AFL_FUZZ_INIT();

// 判断一个数是否为偶数
bool is_even(int num) {
    return (num % 2 == 0);
}

// 测试逻辑函数
void test_logic(int X, int Y) {
    // 逻辑组合 1：X > 0 并且 Y > 0
    if (X > 0 && Y > 0) {
        printf("情况1：X 和 Y 都是正数\n");
    }
    // 逻辑组合 2：X > 0 并且 Y <= 0
    else if (X > 0 && Y <= 0) {
        printf("情况2：X 是正数，Y 是非正数\n");
    }
    // 逻辑组合 3：X <= 0 并且 Y > 0
    else if (X <= 0 && Y > 0) {
        printf("情况3：X 是非正数，Y 是正数\n");
    }
    // 逻辑组合 4：X <= 0 并且 Y <= 0
    else {
        printf("情况4：X 和 Y 都是非正数\n");
    }

    // 附加逻辑：判断奇偶性的组合
    bool x_even = is_even(X);
    bool y_even = is_even(Y);

    if (x_even && y_even) {
        printf("附加情况A：X 和 Y 都是偶数\n");
    } else if (x_even && !y_even) {
        printf("附加情况B：X 是偶数，Y 是奇数\n");
    } else if (!x_even && y_even) {
        printf("附加情况C：X 是奇数，Y 是偶数\n");
    } else {
        printf("附加情况D：X 和 Y 都是奇数\n");
    }
}

int main(int argc, char **argv) {
    unsigned char *buf;
    int len;
    int X, Y;
    
    __AFL_INIT();
    buf = __AFL_FUZZ_TESTCASE_BUF;  // 必须在 __AFL_LOOP 之前赋值！
    
    // 持久模式循环
    while (__AFL_LOOP(10000)) {  // 10000 次迭代后重启进程
        
        len = __AFL_FUZZ_TESTCASE_LEN;  // 不要直接在函数调用中使用宏！
        
        // 检查最小输入长度（需要至少 8 字节存储两个 int）
        if (len < 8) continue;
        
        // 从输入缓冲区解析两个整数
        // 方法 1：直接解释为二进制（推荐，更快）
        X = *(int *)&buf[0];
        Y = *(int *)&buf[4];
        
        // 方法 2：从文本解析（如果需要文本格式）
        // char temp[256];
        // if (len < sizeof(temp)) {
        //     memcpy(temp, buf, len);
        //     temp[len] = '\0';
        //     if (sscanf(temp, "%d %d", &X, &Y) != 2) continue;
        // } else {
        //     continue;
        // }
        
        // 调用测试逻辑
        test_logic(X, Y);
        
        // 重置状态（如果有全局变量需要重置）
        // 这个例子中没有全局状态，所以不需要重置
        
    }
    
    return 0;
}

