/* glue_test.cpp - exercise gti_ipf.h (convert, then cache hit) on Linux. */
#include "Arduino.h"
#include "gti_ipf.h"

static void prog(void *, int done, int total)
{
    if (done == total) printf("  progress %d/%d\n", done, total);
}

int main(int argc, char **argv)
{
    if (argc != 3) { printf("usage: glue_test <sdroot> </path/on/sd.ipf>\n"); return 2; }
    fs::FS sd(argv[1]);
    String hfe;
    bool a = gtiIpfPrepare(sd, argv[2], hfe, prog, nullptr);
    printf("first: %d %s\n", a, hfe.c_str());
    String hfe2;
    bool b = gtiIpfPrepare(sd, argv[2], hfe2);
    printf("second: %d %s\n", b, hfe2.c_str());
    return (a && b && hfe.s == hfe2.s) ? 0 : 1;
}
