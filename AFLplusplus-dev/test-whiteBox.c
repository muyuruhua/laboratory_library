#include <stdio.h>
#include <stdbool.h>

// 判断一个数是否为偶数
bool is_even(int num) {
    return (num % 2 == 0);
}

int main() {
    int X, Y;

    // 输入两个整数
    printf("请输入两个整数 X 和 Y（用空格隔开）：");
    scanf("%d %d", &X, &Y);

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

    return 0;
}
