#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>  // 用于 abort()

// 判断一个数是否为偶数
bool is_even(int num) {
    return (num % 2 == 0);
}

int main() {
    int X, Y;

    // 输入两个整数（从标准输入读取，AFL++ 会自动传递测试用例文件内容）
    // 移除提示信息，避免干扰 AFL++ 的执行
    if (scanf("%d %d", &X, &Y) != 2) {
        // 如果读取失败，使用默认值
        X = 0;
        Y = 0;
    }

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

    // 附加逻辑：判断奇偶性的组合（增加条件组合数量，便于覆盖）
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

    /* ========== 新增崩溃代码（放在函数体最后，return 之前） ========== */
    
    // 触发崩溃：通过条件判断控制是否执行非法操作
    // 这里我们设定：当 X 和 Y 都为 0 时触发崩溃（容易构造输入文件）
    // 使用 abort() 确保崩溃能被 AFL++ 可靠检测到
    if (X == 0 && Y == 0) {
        // 使用 abort() 触发 SIGABRT，这是最可靠的崩溃方式
        // AFL++ 可以检测到 SIGABRT、SIGSEGV、SIGFPE 等信号
        abort();  // 直接调用 abort()，确保崩溃
        
        // 备选方案（如果 abort() 不可用）：
        // 方法1：空指针解引用（经典崩溃）
        // int *p = NULL;
        // *p = 100;  // 写入空指针 -> SIGSEGV
        
        // 方法2：除零错误（也会崩溃）
        // int zero = 0;
        // int crash = 1 / zero;  // SIGFPE
        
        // 方法3：使用编译器内置的陷阱指令
        // __builtin_trap();  // 生成非法指令 -> SIGILL
    }

    /* ================================================================== */

    return 0;
}