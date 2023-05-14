#define USER
#include "param.h"
#include "types.h"
#include "fs/stat.h"
#include "user.h"

#include "fs/fcntl.h"
#include "syscall_gen/syscall_num.h"
#include "memory/memlayout.h"
#include "riscv.h"
#include "memory/buddy.h"

//
// Tests xv6 system calls.  usertests without arguments runs them all
// and usertests <name> runs <name> test. The test runner creates for
// each test a process and based on the exit status of the process,
// the test runner reports "OK" or "FAILED".  Some tests result in
// kernel printing usertrap messages, which can be ignored if test
// prints "OK".
//

#define BUFSZ ((MAXOPBLOCKS + 2) * BSIZE)

char buf[BUFSZ];


char junk1[4096];
int fds[2];
char junk2[4096];
char buf2[4096];
char junk3[4096];




//
// Section with tests that run fairly quickly.  Use -q if you want to
// run just those.  With -q usertests also runs the ones that take a
// fair of time.
//

// what if a string argument crosses over the end of last user page?
void copyinstr3(char *s) {
    sbrk(8192);
    uint64 top = (uint64)sbrk(0);
    if ((top % PGSIZE) != 0) {
        sbrk(PGSIZE - (top % PGSIZE));
    }
    top = (uint64)sbrk(0);
    if (top % PGSIZE) {
        printf("oops\n");
        exit(1);
    }

    char *b = (char *)(top - 1);
    *b = 'x';

    int ret = unlink(b);
    if (ret != -1) {
        printf("unlink(%s) returned %d, not -1\n", b, ret);
        exit(1);
    }

    int fd = open(b, O_CREATE | O_WRONLY);
    if (fd != -1) {
        printf("open(%s) returned %d, not -1\n", b, fd);
        exit(1);
    }

    ret = link(b, b);
    if (ret != -1) {
        printf("link(%s, %s) returned %d, not -1\n", b, b, ret);
        exit(1);
    }

    char *args[] = {"xx", 0};
    ret = exec(b, args);
    if (ret != -1) {
        printf("exec(%s) returned %d, not -1\n", b, fd);
        exit(1);
    }
}






// does the error path in open() for attempt to write a
// directory call iput() in a transaction?
// needs a hacked kernel that pauses just after the namei()
// call in sys_open():
//    if((ip = namei(path)) == 0)
//      return -1;
//    {
//      int i;
//      for(i = 0; i < 10000; i++)
//        yield();
//    }


// More file system tests

void linktest(char *s) {
    enum { SZ = 5 };
    int fd;

    unlink("lf1");
    unlink("lf2");

    fd = open("lf1", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf("%s: create lf1 failed\n", s);
        exit(1);
    }
    if (write(fd, "hello", SZ) != SZ) {
        printf("%s: write lf1 failed\n", s);
        exit(1);
    }
    close(fd);

    if (link("lf1", "lf2") < 0) {
        printf("%s: link lf1 lf2 failed\n", s);
        exit(1);
    }
    unlink("lf1");

    if (open("lf1", 0) >= 0) {
        printf("%s: unlinked lf1 but it is still there!\n", s);
        exit(1);
    }

    fd = open("lf2", 0);
    if (fd < 0) {
        printf("%s: open lf2 failed\n", s);
        exit(1);
    }
    if (read(fd, buf, sizeof(buf)) != SZ) {
        printf("%s: read lf2 failed\n", s);
        exit(1);
    }
    close(fd);

    if (link("lf2", "lf2") >= 0) {
        printf("%s: link lf2 lf2 succeeded! oops\n", s);
        exit(1);
    }

    unlink("lf2");
    if (link("lf2", "lf1") >= 0) {
        printf("%s: link non-existent succeeded! oops\n", s);
        exit(1);
    }

    if (link(".", "lf1") >= 0) {
        printf("%s: link . lf1 succeeded! oops\n", s);
        exit(1);
    }
}

// test concurrent create/link/unlink of the same file
void concreate(char *s) {
    enum { N = 40 };
    char file[3];
    int i, pid, n, fd;
    char fa[N];
    struct {
        ushort inum;
        char name[DIRSIZ];
    } de;

    file[0] = 'C';
    file[2] = '\0';
    for (i = 0; i < N; i++) {
        file[1] = '0' + i;
        unlink(file);
        pid = fork();
        if (pid && (i % 3) == 1) {
            link("C0", file);
        } else if (pid == 0 && (i % 5) == 1) {
            link("C0", file);
        } else {
            fd = open(file, O_CREATE | O_RDWR);
            if (fd < 0) {
                printf("concreate create %s failed\n", file);
                exit(1);
            }
            close(fd);
        }
        if (pid == 0) {
            exit(0);
        } else {
            int xstatus;
            wait(&xstatus);
            if (xstatus != 0)
                exit(1);
        }
    }

    memset(fa, 0, sizeof(fa));
    fd = open(".", 0);
    n = 0;
    while (read(fd, &de, sizeof(de)) > 0) {
        if (de.inum == 0)
            continue;
        if (de.name[0] == 'C' && de.name[2] == '\0') {
            i = de.name[1] - '0';
            if (i < 0 || i >= sizeof(fa)) {
                printf("%s: concreate weird file %s\n", s, de.name);
                exit(1);
            }
            if (fa[i]) {
                printf("%s: concreate duplicate file %s\n", s, de.name);
                exit(1);
            }
            fa[i] = 1;
            n++;
        }
    }
    close(fd);

    if (n != N) {
        printf("%s: concreate not enough files in directory listing\n", s);
        exit(1);
    }

    for (i = 0; i < N; i++) {
        file[1] = '0' + i;
        pid = fork();
        if (pid < 0) {
            printf("%s: fork failed\n", s);
            exit(1);
        }
        if (((i % 3) == 0 && pid == 0) || ((i % 3) == 1 && pid != 0)) {
            close(open(file, 0));
            close(open(file, 0));
            close(open(file, 0));
            close(open(file, 0));
            close(open(file, 0));
            close(open(file, 0));
        } else {
            unlink(file);
            unlink(file);
            unlink(file);
            unlink(file);
            unlink(file);
            unlink(file);
        }
        if (pid == 0)
            exit(0);
        else
            wait(0);
    }
}

// another concurrent link/unlink/create test,
// to look for deadlocks.
void linkunlink(char *s) {
    int pid, i;

    unlink("x");
    pid = fork();
    if (pid < 0) {
        printf("%s: fork failed\n", s);
        exit(1);
    }

    unsigned int x = (pid ? 1 : 97);
    for (i = 0; i < 100; i++) {
        x = x * 1103515245 + 12345;
        if ((x % 3) == 0) {
            close(open("x", O_RDWR | O_CREATE));
        } else if ((x % 3) == 1) {
            link("cat", "x");
        } else {
            unlink("x");
        }
    }

    if (pid)
        wait(0);
    else
        exit(0);
}

void subdir(char *s) {
    int fd, cc;

    unlink("ff");
    if (mkdir("dd") != 0) {
        printf("%s: mkdir dd failed\n", s);
        exit(1);
    }

    fd = open("dd/ff", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf("%s: create dd/ff failed\n", s);
        exit(1);
    }
    write(fd, "ff", 2);
    close(fd);

    if (unlink("dd") >= 0) {
        printf("%s: unlink dd (non-empty dir) succeeded!\n", s);
        exit(1);
    }

    if (mkdir("/dd/dd") != 0) {
        printf("subdir mkdir dd/dd failed\n", s);
        exit(1);
    }

    fd = open("dd/dd/ff", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf("%s: create dd/dd/ff failed\n", s);
        exit(1);
    }
    write(fd, "FF", 2);
    close(fd);

    fd = open("dd/dd/../ff", 0);
    if (fd < 0) {
        printf("%s: open dd/dd/../ff failed\n", s);
        exit(1);
    }
    cc = read(fd, buf, sizeof(buf));
    if (cc != 2 || buf[0] != 'f') {
        printf("%s: dd/dd/../ff wrong content\n", s);
        exit(1);
    }
    close(fd);

    if (link("dd/dd/ff", "dd/dd/ffff") != 0) {
        printf("link dd/dd/ff dd/dd/ffff failed\n", s);
        exit(1);
    }

    if (unlink("dd/dd/ff") != 0) {
        printf("%s: unlink dd/dd/ff failed\n", s);
        exit(1);
    }
    if (open("dd/dd/ff", O_RDONLY) >= 0) {
        printf("%s: open (unlinked) dd/dd/ff succeeded\n", s);
        exit(1);
    }

    if (chdir("dd") != 0) {
        printf("%s: chdir dd failed\n", s);
        exit(1);
    }
    if (chdir("dd/../../dd") != 0) {
        printf("%s: chdir dd/../../dd failed\n", s);
        exit(1);
    }
    if (chdir("dd/../../../dd") != 0) {
        printf("chdir dd/../../dd failed\n", s);
        exit(1);
    }
    if (chdir("./..") != 0) {
        printf("%s: chdir ./.. failed\n", s);
        exit(1);
    }

    fd = open("dd/dd/ffff", 0);
    if (fd < 0) {
        printf("%s: open dd/dd/ffff failed\n", s);
        exit(1);
    }
    if (read(fd, buf, sizeof(buf)) != 2) {
        printf("%s: read dd/dd/ffff wrong len\n", s);
        exit(1);
    }
    close(fd);

    if (open("dd/dd/ff", O_RDONLY) >= 0) {
        printf("%s: open (unlinked) dd/dd/ff succeeded!\n", s);
        exit(1);
    }

    if (open("dd/ff/ff", O_CREATE | O_RDWR) >= 0) {
        printf("%s: create dd/ff/ff succeeded!\n", s);
        exit(1);
    }
    if (open("dd/xx/ff", O_CREATE | O_RDWR) >= 0) {
        printf("%s: create dd/xx/ff succeeded!\n", s);
        exit(1);
    }
    if (open("dd", O_CREATE) >= 0) {
        printf("%s: create dd succeeded!\n", s);
        exit(1);
    }
    if (open("dd", O_RDWR) >= 0) {
        printf("%s: open dd rdwr succeeded!\n", s);
        exit(1);
    }
    if (open("dd", O_WRONLY) >= 0) {
        printf("%s: open dd wronly succeeded!\n", s);
        exit(1);
    }
    if (link("dd/ff/ff", "dd/dd/xx") == 0) {
        printf("%s: link dd/ff/ff dd/dd/xx succeeded!\n", s);
        exit(1);
    }
    if (link("dd/xx/ff", "dd/dd/xx") == 0) {
        printf("%s: link dd/xx/ff dd/dd/xx succeeded!\n", s);
        exit(1);
    }
    if (link("dd/ff", "dd/dd/ffff") == 0) {
        printf("%s: link dd/ff dd/dd/ffff succeeded!\n", s);
        exit(1);
    }
    if (mkdir("dd/ff/ff") == 0) {
        printf("%s: mkdir dd/ff/ff succeeded!\n", s);
        exit(1);
    }
    if (mkdir("dd/xx/ff") == 0) {
        printf("%s: mkdir dd/xx/ff succeeded!\n", s);
        exit(1);
    }
    if (mkdir("dd/dd/ffff") == 0) {
        printf("%s: mkdir dd/dd/ffff succeeded!\n", s);
        exit(1);
    }
    if (unlink("dd/xx/ff") == 0) {
        printf("%s: unlink dd/xx/ff succeeded!\n", s);
        exit(1);
    }
    if (unlink("dd/ff/ff") == 0) {
        printf("%s: unlink dd/ff/ff succeeded!\n", s);
        exit(1);
    }
    if (chdir("dd/ff") == 0) {
        printf("%s: chdir dd/ff succeeded!\n", s);
        exit(1);
    }
    if (chdir("dd/xx") == 0) {
        printf("%s: chdir dd/xx succeeded!\n", s);
        exit(1);
    }

    if (unlink("dd/dd/ffff") != 0) {
        printf("%s: unlink dd/dd/ff failed\n", s);
        exit(1);
    }
    if (unlink("dd/ff") != 0) {
        printf("%s: unlink dd/ff failed\n", s);
        exit(1);
    }
    if (unlink("dd") == 0) {
        printf("%s: unlink non-empty dd succeeded!\n", s);
        exit(1);
    }
    if (unlink("dd/dd") < 0) {
        printf("%s: unlink dd/dd failed\n", s);
        exit(1);
    }
    if (unlink("dd") < 0) {
        printf("%s: unlink dd failed\n", s);
        exit(1);
    }
}

void dirfile(char *s) {
    int fd;

    fd = open("dirfile", O_CREATE);
    if (fd < 0) {
        printf("%s: create dirfile failed\n", s);
        exit(1);
    }
    close(fd);
    if (chdir("dirfile") == 0) {
        printf("%s: chdir dirfile succeeded!\n", s);
        exit(1);
    }
    fd = open("dirfile/xx", 0);
    if (fd >= 0) {
        printf("%s: create dirfile/xx succeeded!\n", s);
        exit(1);
    }
    fd = open("dirfile/xx", O_CREATE);
    if (fd >= 0) {
        printf("%s: create dirfile/xx succeeded!\n", s);
        exit(1);
    }
    if (mkdir("dirfile/xx") == 0) {
        printf("%s: mkdir dirfile/xx succeeded!\n", s);
        exit(1);
    }
    if (unlink("dirfile/xx") == 0) {
        printf("%s: unlink dirfile/xx succeeded!\n", s);
        exit(1);
    }
    if (link("README.md", "dirfile/xx") == 0) {
        printf("%s: link to dirfile/xx succeeded!\n", s);
        exit(1);
    }
    if (unlink("dirfile") != 0) {
        printf("%s: unlink dirfile failed!\n", s);
        exit(1);
    }

    fd = open(".", O_RDWR);
    if (fd >= 0) {
        printf("%s: open . for writing succeeded!\n", s);
        exit(1);
    }
    fd = open(".", 0);
    if (write(fd, "x", 1) > 0) {
        printf("%s: write . succeeded!\n", s);
        exit(1);
    }
    close(fd);
}

// test that iput() is called at the end of _namei().
// also tests empty file names.
void iref(char *s) {
    int i, fd;

    for (i = 0; i < NINODE + 1; i++) {
        if (mkdir("irefd") != 0) {
            printf("%s: mkdir irefd failed\n", s);
            exit(1);
        }
        if (chdir("irefd") != 0) {
            printf("%s: chdir irefd failed\n", s);
            exit(1);
        }

        mkdir("");
        link("README.md", "");
        fd = open("", O_CREATE);
        if (fd >= 0)
            close(fd);
        fd = open("xx", O_CREATE);
        if (fd >= 0)
            close(fd);
        unlink("xx");
    }

    // clean up
    for (i = 0; i < NINODE + 1; i++) {
        chdir("..");
        unlink("irefd");
    }

    chdir("/");
}

void validatetest(char *s) {
    int hi;
    uint64 p;

    hi = 1100 * 1024;
    for (p = 0; p <= (uint)hi; p += PGSIZE) {
        // try to crash the kernel by passing in a bad string pointer
        if (link("nosuchfile", (char *)p) != -1) {
            printf("%s: link should not succeed\n", s);
            exit(1);
        }
    }
}


struct test {
    void (*f)(char *);
    char *s;
} quicktests[] = {
    {cowtest, "cowtest"},
    {copyin, "copyin"},
    {copyout, "copyout"},
    {copyinstr1, "copyinstr1"},
    {copyinstr2, "copyinstr2"},
    {copyinstr3, "copyinstr3"},
    {rwsbrk, "rwsbrk"},
    {truncate1, "truncate1"},
    {truncate2, "truncate2"},
    {truncate3, "truncate3"},
    {openiputtest, "openiput"},
    {exitiputtest, "exitiput"},
    {iputtest, "iput"},
    {opentest, "opentest"},
    {writetest, "writetest"},
    {writebig, "writebig"},
    {createtest, "createtest"},
    {dirtest, "dirtest"},
    {exectest, "exectest"},
    {pipe1, "pipe1"},
    {killstatus, "killstatus"},
    {preempt, "preempt"},
    {exitwait, "exitwait"},
    {reparent, "reparent"},
    {twochildren, "twochildren"},
    {forkfork, "forkfork"},
    {forkforkfork, "forkforkfork"},
    {reparent2, "reparent2"},
    {mem, "mem"},
    {sharedfd, "sharedfd"},
    {fourfiles, "fourfiles"},
    {createdelete, "createdelete"},
    {unlinkread, "unlinkread"},
    {linktest, "linktest"},
    {concreate, "concreate"},
    {linkunlink, "linkunlink"},
    {subdir, "subdir"},
    {bigwrite, "bigwrite"},
    {bigfile, "bigfile"},
    {fourteen, "fourteen"},
    {rmdot, "rmdot"},
    {dirfile, "dirfile"},
    {iref, "iref"},
    {forktest, "forktest"},
    {sbrkbasic, "sbrkbasic"},
    {sbrkmuch, "sbrkmuch"},
    {kernmem, "kernmem"},
    {MAXVAplus, "MAXVAplus"},
    {sbrkfail, "sbrkfail"},
    {sbrkarg, "sbrkarg"},
    {validatetest, "validatetest"},
    {bsstest, "bsstest"},
    {bigargtest, "bigargtest"},
    {argptest, "argptest"},
    {stacktest, "stacktest"},
    {textwrite, "textwrite"},
    {pgbug, "pgbug"},
    {sbrkbugs, "sbrkbugs"},
    {sbrklast, "sbrklast"},
    {sbrk8000, "sbrk8000"},
    {badarg, "badarg"},
    {uvmfree, "uvmfree"},

    {0, 0},
};

struct test slowtests[] = {
    {bigdir, "bigdir"},
    {manywrites, "manywrites"},
    {badwrite, "badwrite"},
    {execout, "execout"},
    {diskfull, "diskfull"},
    {outofinodes, "outofinodes"},

    {0, 0},
};

//
// drive tests
//

// run each test in its own process. run returns 1 if child's exit()
// indicates success.
int run(void f(char *), char *s) {
    int pid;
    int xstatus;

    printf("test %s: ", s);
    if ((pid = fork()) < 0) {
        printf("runtest: fork error\n");
        exit(1);
    }
    if (pid == 0) {
        f(s);
        exit(0);
    } else {
        wait(&xstatus);
        if (xstatus != 0)
            printf("FAILED\n");
        else
            printf("OK\n");
        return xstatus == 0;
    }
}

int runtests(struct test *tests, char *justone) {
    for (struct test *t = tests; t->s != 0; t++) {
        if ((justone == 0) || strcmp(t->s, justone) == 0) {
            if (!run(t->f, t->s)) {
                printf("SOME TESTS FAILED\n");
                return 1;
            }
        }
    }
    return 0;
}

//
// use sbrk() to count how many free physical memory pages there are.
// touches the pages to force allocation.
// because out of memory with lazy allocation results in the process
// taking a fault and being killed, fork and report back.
//
int countfree() {
    int fds[2];

    if (pipe(fds) < 0) {
        printf("pipe() failed in countfree()\n");
        exit(1);
    }

    int pid = fork();

    if (pid < 0) {
        printf("fork failed in countfree()\n");
        exit(1);
    }

    if (pid == 0) {
        close(fds[0]);

        while (1) {
            uint64 a = (uint64)sbrk(4096);
            if (a == 0xffffffffffffffff) {
                break;
            }

            // modify the memory to make sure it's really allocated.
            *(char *)(a + 4096 - 1) = 1;

            // report back one more page.
            if (write(fds[1], "x", 1) != 1) {
                printf("write() failed in countfree()\n");
                exit(1);
            }
        }

        exit(0);
    }

    close(fds[1]);

    int n = 0;
    while (1) {
        char c;
        int cc = read(fds[0], &c, 1);
        if (cc < 0) {
            printf("read() failed in countfree()\n");
            exit(1);
        }
        if (cc == 0)
            break;
        n += 1;
    }

    close(fds[0]);
    wait((int *)0);

    return n;
}

int drivetests(int quick, int continuous, char *justone) {
    do {
        printf("usertests starting\n");
        int free0 = countfree();
        int free1 = 0;
        if (runtests(quicktests, justone)) {
            if (continuous != 2) {
                return 1;
            }
        }
        if (!quick) {
            if (justone == 0)
                printf("usertests slow tests starting\n");
            if (runtests(slowtests, justone)) {
                if (continuous != 2) {
                    return 1;
                }
            }
        }
        if ((free1 = countfree()) < free0) {
            printf("FAILED -- lost some free pages %d (out of %d)\n", free1, free0);
            if (continuous != 2) {
                return 1;
            }
        }
    } while (continuous);
    return 0;
}

int main(int argc, char *argv[]) {
    int continuous = 0;
    int quick = 0;
    char *justone = 0;

    // print_pgtable();
    if (argc == 2 && strcmp(argv[1], "-q") == 0) {
        quick = 1;
    } else if (argc == 2 && strcmp(argv[1], "-c") == 0) {
        continuous = 1;
    } else if (argc == 2 && strcmp(argv[1], "-C") == 0) {
        continuous = 2;
    } else if (argc == 2 && argv[1][0] != '-') {
        justone = argv[1];
    } else if (argc > 1) {
        printf("Usage: usertests [-c] [-C] [-q] [testname]\n");
        exit(1);
    }
    if (drivetests(quick, continuous, justone)) {
        exit(1);
    }
    printf("ALL TESTS PASSED\n");
    exit(0);
}
