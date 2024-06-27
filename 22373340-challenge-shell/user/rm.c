#include <lib.h>

void remove_path(char *path, int recursive, int force);

void main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("usage: rm [-r|-rf] [file|dir]\n");
        return;
    }

    int recursive = 0;
    int force = 0;
    char *path;

    if (argc == 3 && strcmp(argv[1], "-r") == 0)
    {
        recursive = 1;
        force = 0;
        path = argv[2];
    }
    else if (argc == 3 && strcmp(argv[1], "-rf") == 0)
    {
        recursive = 1;
        force = 1;
        path = argv[2];
    }
    else if (argc == 2)
    {
        recursive = 0;
        force = 0;
        path = argv[1];
    }
    else
    {
        printf("usage: rm [-r|-rf] [file|dir]\n");
        return;
    }

    remove_path(path, recursive, force);
}

void remove_path(char *path, int recursive, int force)
{
    int fd;
    struct Stat path_stat;
    if(recursive == 0 && force == 0){ 
        if ((fd = open(path, O_RDONLY)) < 0){
            printf("rm: cannot remove '%s': No such file or directory\n", path);
            return;
        }
        close(fd);
        stat(path, &path_stat);
        if (path_stat.st_isdir)
        {
            printf("rm: cannot remove '%s': Is a directory\n", path);
            return;
        }
        remove(path);
    }
    else if (recursive == 1 && force == 0){
        if ((fd = open(path, O_RDONLY)) < 0)
        {
            printf("rm: cannot remove '%s': No such file or directory\n", path);
            return;
        }
        close(fd);
        remove(path);
    }
    else if (recursive == 1 && force == 1){
        if ((fd = open(path, O_RDONLY)) < 0)
        {
            return;
        }
        close(fd);
        remove(path);
    }
}