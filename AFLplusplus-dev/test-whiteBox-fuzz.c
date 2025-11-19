/*
 * test-whiteBox.c 的模糊测试版本
 * 支持从文件或标准输入读取两个整数
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

// 判断一个数是否为偶数
bool is_even(int num) {
    return (num % 2 == 0);
}

// 原始逻辑函数（提取出来以便测试）
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

// 方法 1：从文件读取（使用 @@）
int main_file_input(int argc, char **argv) {
    int X, Y;
    FILE *fp;
    
    if (argc < 2) {
        fprintf(stderr, "用法: %s -f <文件>\n", argv[0]);
        return 1;
    }
    
    if (strcmp(argv[1], "-f") != 0) {
        fprintf(stderr, "用法: %s -f <文件>\n", argv[0]);
        return 1;
    }
    
    fp = fopen(argv[2], "r");
    if (fp == NULL) {
        fprintf(stderr, "无法打开文件: %s\n", argv[2]);
        return 1;
    }
    
    if (fscanf(fp, "%d %d", &X, &Y) != 2) {
        fprintf(stderr, "无法读取两个整数\n");
        fclose(fp);
        return 1;
    }
    
    fclose(fp);
    
    test_logic(X, Y);
    return 0;
}

// 方法 2：从标准输入读取文本格式
int main_stdin_text() {
    int X, Y;
    
    if (scanf("%d %d", &X, &Y) != 2) {
        fprintf(stderr, "无法读取两个整数\n");
        return 1;
    }
    
    test_logic(X, Y);
    return 0;
}

// 方法 3：从标准输入读取二进制格式（推荐用于模糊测试）
int main_stdin_binary() {
    int X, Y;
    unsigned char buf[8];
    ssize_t len;
    
    len = read(0, buf, sizeof(buf));
    if (len < 8) {
        // 如果输入不足 8 字节，使用默认值或返回
        return 1;
    }
    
    // 将 8 字节解释为两个 32 位整数
    X = *(int *)&buf[0];
    Y = *(int *)&buf[4];
    
    test_logic(X, Y);
    return 0;
}

int main(int argc, char **argv) {
    // 如果使用 -f 参数，从文件读取
    if (argc >= 3 && strcmp(argv[1], "-f") == 0) {
        return main_file_input(argc, argv);
    }
    
    // 否则从标准输入读取（二进制格式，适合模糊测试）
    return main_stdin_binary();
}

