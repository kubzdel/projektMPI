#include <sys/types.h> /* for open(2) */
#include <sys/stat.h> /* for open(2) */
#include <fcntl.h> /* for open(2) */
#include <unistd.h> /* for read(2), close(2) */
#define DEVURANDOM "/dev/urandom"
int random_int()
{
    int rnum = 0;
    int fd = open(DEVURANDOM, O_RDONLY);
    if (fd != -1)
    {
        (void) read(fd, (void *)&rnum, sizeof(int));
        (void) close(fd);
    }
    return rnum;
}

inline int max_int(int a, int b)
{
	return (a > b ? a : b);
}

int random(int min, int max)
{
    int tmp;
    if (max>=min)
        max-= min;
    else
    {
        tmp= min - max;
        min= max;
        max= tmp;
    }
    return max ? (rand() % max + min) : min;
}