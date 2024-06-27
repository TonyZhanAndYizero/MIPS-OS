#include <lib.h>

void create_directory(char *path, int recursive);

void main(int argc, char **argv)
{
    if (argc != 2 && !(argc == 3 && strcmp(argv[1], "-p") == 0))
    {
        printf("usage: mkdir [-p] [dirname]\n");
        return;
    }

    int recursive = 0;
    char *dir;

    if (argc == 3)
    {
        recursive = 1;
        dir = argv[2];
    }
    else
    {
        dir = argv[1];
    }

    create_directory(dir, recursive);
}

void create_directory(char *path, int recursive)
{
    int r = open(path, O_RDONLY);

    if(recursive){
        if(r >= 0){
            close(r);
            return;
        }
        else
        {
            char *p;
            for (p = path; *p; p++)
            {
                
                if (*p == '/')
                {
                    *p = 0;
                    if (create(path, FTYPE_DIR) < 0)
                    {
                        
                        int err = open(path, O_RDONLY);
                        if (err < 0)
                        {
                            printf("mkdir: cannot create directory '%s': No such file or directory\n", path);
                            return;
                        }
                        close(err);
                    }
                    *p = '/';
                }
            }

            if (create(path, FTYPE_DIR) < 0)
            {
                printf("mkdir: cannot create directory '%s': No such file or directory\n", path);
            }
        }
    }
    else
    {
        if (r >= 0)
        {
            printf("mkdir: cannot create directory '%s': File exists\n", path);
            close(r);
            return;
        }
        else
        {
            if (create(path, FTYPE_DIR) < 0)
            {
                printf("mkdir: cannot create directory '%s': No such file or directory\n", path);
            }
        }
    }
}
