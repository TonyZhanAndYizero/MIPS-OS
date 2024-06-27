#include <lib.h>

void main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("usage: touch [filename]\n");
        return;
    }
    int r = open(argv[1], O_RDONLY); // 创建文件
    if (r >= 0)
    {
        close(r);
        return;
    }
    else
    { // 不存在则创建
        if (create(argv[1], FTYPE_REG) < 0)
        {
            printf("touch: cannot touch '%s': No such file or directory\n", argv[1]);
            return;
        }
    }
}