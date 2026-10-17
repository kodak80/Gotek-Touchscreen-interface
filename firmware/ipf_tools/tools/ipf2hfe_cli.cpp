/* ipf2hfe_cli.cpp - desktop test front-end for ipf2hfe. */
#include <stdio.h>
#include "ipf2hfe.h"

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s in.ipf out.hfe\n", argv[0]);
        return 2;
    }
    ipf2hfe_stats st;
    int rc = ipf2hfe_convert_file(argv[1], argv[2], NULL, NULL, &st);
    if (rc) {
        fprintf(stderr, "error: %s\n", ipf2hfe_strerror(rc));
        return 1;
    }
    printf("%s: %d cyl x %d side, platform %d, %u bytes\n", argv[2],
           st.cyls, st.sides, st.platform, st.hfe_bytes);
    printf("  variable-density tracks %d, weak tracks %d, noise tracks %d,"
           " 1111-lead bytes re-encoded %d, longest side %d bytes\n",
           st.var_tracks, st.weak_tracks, st.noise_tracks, st.collisions,
           st.max_side_bytes);
    return 0;
}
