// Property validation for the AFK solvers in monument_search_core.c.
//
// Independent of the solver: every reported number is re-derived by brute force (per-block disk
// tests, per-window sums), so a regression in the field/trapezoid machinery shows up as a
// mismatch here. Build & run:
//
//   gcc -O2 tools/validate_afk.c monument_search_core.c -o validate_afk -lm
//   ./validate_afk            # exhaustive two-box sweep + 400 random groups + fixed shapes
//
// Exit code 0 = all properties hold, 1 = at least one failure (details are printed).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../monument_search_core.h"

#define BOX 58
#define DISK_R 128
#define SPAN_LIMIT 256
#define WIN (SPAN_LIMIT + 1)
#define BOX_LO (-((BOX + 1) / 2))
#define BOX_HI (BOX_LO + BOX - 1)

static long g_checks;
static long g_failures;

static void check(int ok, const char *name, const char *detail) {
    g_checks++;
    if (!ok) {
        g_failures++;
        printf("FAIL  %-46s %s\n", name, detail ? detail : "");
    }
}

// ---------------------------------------------------------------- brute force coverage

// Multiset coverage of the given AFK points: a box block counts once per box when it is within
// DISK_R of at least one point. Also reports the bounding box of the counted blocks.
static int brute_cover(const MonumentBlockPos *p, int n, const int *px, const int *pz, int npts,
                       int *outMinX, int *outMinZ, int *outMaxX, int *outMaxZ) {
    int total = 0;
    int mnx = 0, mnz = 0, mxx = 0, mxz = 0;
    int have = 0;
    for (int i = 0; i < n; i++) {
        const int x0 = p[i].x + BOX_LO, x1 = p[i].x + BOX_HI;
        const int z0 = p[i].z + BOX_LO, z1 = p[i].z + BOX_HI;
        for (int z = z0; z <= z1; z++) {
            for (int x = x0; x <= x1; x++) {
                int covered = 0;
                for (int k = 0; k < npts && !covered; k++) {
                    const long dx = (long) x - px[k];
                    const long dz = (long) z - pz[k];
                    if (dx * dx + dz * dz <= (long) DISK_R * DISK_R)
                        covered = 1;
                }
                if (!covered)
                    continue;
                total++;
                if (!have) {
                    mnx = mxx = x;
                    mnz = mxz = z;
                    have = 1;
                } else {
                    if (x < mnx) mnx = x;
                    if (x > mxx) mxx = x;
                    if (z < mnz) mnz = z;
                    if (z > mxz) mxz = z;
                }
            }
        }
    }
    if (outMinX) *outMinX = mnx;
    if (outMinZ) *outMinZ = mnz;
    if (outMaxX) *outMaxX = mxx;
    if (outMaxZ) *outMaxZ = mxz;
    return total;
}

// Naive ceiling: largest box-block count any 257x257 window can hold (per-window sum over boxes).
static int brute_ceiling(const MonumentBlockPos *p, int n, int *outX, int *outZ) {
    int minX = p[0].x + BOX_LO, maxX = p[0].x + BOX_HI;
    int minZ = p[0].z + BOX_LO, maxZ = p[0].z + BOX_HI;
    for (int i = 1; i < n; i++) {
        if (p[i].x + BOX_LO < minX) minX = p[i].x + BOX_LO;
        if (p[i].x + BOX_HI > maxX) maxX = p[i].x + BOX_HI;
        if (p[i].z + BOX_LO < minZ) minZ = p[i].z + BOX_LO;
        if (p[i].z + BOX_HI > maxZ) maxZ = p[i].z + BOX_HI;
    }
    const int w = maxX - minX + 1;
    const int h = maxZ - minZ + 1;
    const int ww = w < WIN ? w : WIN;
    const int hh = h < WIN ? h : WIN;
    int best = 0, bx = 0, bz = 0;
    for (int z0 = 0; z0 + hh <= h; z0++) {
        for (int x0 = 0; x0 + ww <= w; x0++) {
            int s = 0;
            for (int i = 0; i < n; i++) {
                const int bx0 = p[i].x + BOX_LO - minX, bx1 = p[i].x + BOX_HI - minX;
                const int bz0 = p[i].z + BOX_LO - minZ, bz1 = p[i].z + BOX_HI - minZ;
                const int ix0 = bx0 > x0 ? bx0 : x0;
                const int ix1 = bx1 < x0 + ww - 1 ? bx1 : x0 + ww - 1;
                const int iz0 = bz0 > z0 ? bz0 : z0;
                const int iz1 = bz1 < z0 + hh - 1 ? bz1 : z0 + hh - 1;
                if (ix0 <= ix1 && iz0 <= iz1)
                    s += (ix1 - ix0 + 1) * (iz1 - iz0 + 1);
            }
            if (s > best) {
                best = s;
                bx = x0;
                bz = z0;
            }
        }
    }
    if (outX) *outX = minX + bx + ww / 2;
    if (outZ) *outZ = minZ + bz + hh / 2;
    return best;
}

// ---------------------------------------------------------------- random groups

static unsigned long long g_rng = 0x9E3779B97F4A7C15ULL;
static int rnd(int lo, int hi) {
    g_rng = g_rng * 6364136223846793005ULL + 1442695040888963407ULL;
    return lo + (int) ((g_rng >> 33) % (unsigned long long) (hi - lo + 1));
}

static void make_group(MonumentBlockPos *p, int *n) {
    *n = rnd(1, 4);
    const int bx = rnd(-200000, 200000);
    const int bz = rnd(-200000, 200000);
    p[0].x = bx;
    p[0].z = bz;
    const int spread = rnd(0, 1) ? 120 : 420; // tight clusters and span-violating ones
    for (int i = 1; i < *n; i++) {
        // structure positions are 16-block aligned in game; keep that property
        p[i].x = bx + 16 * (rnd(-spread, spread) / 16);
        p[i].z = bz + 16 * (rnd(-spread, spread) / 16);
        if (rnd(0, 5) == 0) { // occasionally a coincident box
            p[i] = p[0];
        }
    }
}

static void validate_group(const MonumentBlockPos *p, int n) {
    char detail[256];
    const int total = n * BOX * BOX;

    // ---- single dummy
    int sx = 0, sz = 0;
    const int single = monument_afk_coverage(p, n, BOX, BOX, &sx, &sz);
    snprintf(detail, sizeof detail, "n=%d single=%d", n, single);
    check(single >= 0, "single >= 0", detail);
    check(single <= total, "single <= n*ax*az", detail);
    {
        int csx[1] = {sx}, csz[1] = {sz};
        const int bf = brute_cover(p, n, csx, csz, 1, NULL, NULL, NULL, NULL);
        snprintf(detail, sizeof detail, "n=%d reported=%d brute=%d", n, single, bf);
        check(bf == single, "single point re-evaluates exactly", detail);
    }
    check(single >= BOX * BOX, "single >= one box", detail);

    // ---- dual dummies (the API is defined for 2..4 monuments; a lone box is trivial)
    int x1 = 0, z1 = 0, x2 = 0, z2 = 0, sd = 0, sdx = 0, sdz = 0;
    int dual = single;
    if (n < 2) {
        snprintf(detail, sizeof detail, "n=%d area=%d", n, single);
        check(single == BOX * BOX, "lone box covers exactly one box", detail);
    } else {
        dual = monument_afk_coverage_dual(p, n, BOX, BOX, &x1, &z1, &x2, &z2, &sd, &sdx, &sdz);
    }
    snprintf(detail, sizeof detail, "n=%d dual=%d single=%d", n, dual, single);
    check(dual >= single, "dual >= single", detail);
    check(dual <= total, "dual <= n*ax*az", detail);
    check(n < 2 || sd == single, "dual reports the single optimum", detail);
    if (n >= 2) {
        int px[2] = {x1, x2}, pz[2] = {z1, z2};
        int mnx, mnz, mxx, mxz;
        const int bf = brute_cover(p, n, px, pz, 2, &mnx, &mnz, &mxx, &mxz);
        snprintf(detail, sizeof detail, "n=%d reported=%d brute=%d bbox=(%d,%d)-(%d,%d)",
                 n, dual, bf, mnx, mnz, mxx, mxz);
        check(bf == dual, "dual points re-evaluate exactly", detail);
        check(mxx - mnx <= SPAN_LIMIT && mxz - mnz <= SPAN_LIMIT,
              "dual counted area fits the 256 span", detail);
        // repeated calls must agree (the refine is deterministic)
        int r1 = 0, r2 = 0, r3 = 0, r4 = 0;
        const int dual2 = monument_afk_coverage_dual(p, n, BOX, BOX, &r1, &r2, &r3, &r4,
                                                     NULL, NULL, NULL);
        snprintf(detail, sizeof detail, "n=%d first=%d second=%d", n, dual, dual2);
        check(dual2 == dual, "dual solve is deterministic", detail);
    }
    if (n >= 2) {
        // reaches() must agree with the exact optimum around the boundary
        const int at = monument_afk_coverage_dual_reaches(p, n, BOX, BOX, dual);
        const int above = monument_afk_coverage_dual_reaches(p, n, BOX, BOX, dual + 1);
        snprintf(detail, sizeof detail, "n=%d dual=%d at=%d above=%d", n, dual, at, above);
        check(at == 1 && above == 0, "reaches() matches the exact optimum", detail);
    }
    {
        // hill climbing from random points must never beat the reported optimum
        int best = single;
        for (int start = 0; start < 6; start++) {
            int px = p[0].x + rnd(-180, 180);
            int pz = p[0].z + rnd(-180, 180);
            int cur = brute_cover(p, n, &px, &pz, 1, NULL, NULL, NULL, NULL);
            for (int step = 16; step >= 1; step /= 2) {
                int improved = 1;
                while (improved) {
                    improved = 0;
                    const int dirs[4][2] = {{step, 0}, {-step, 0}, {0, step}, {0, -step}};
                    for (int d = 0; d < 4; d++) {
                        int nx = px + dirs[d][0], nz = pz + dirs[d][1];
                        int v = brute_cover(p, n, &nx, &nz, 1, NULL, NULL, NULL, NULL);
                        if (v > cur) {
                            cur = v;
                            px = nx;
                            pz = nz;
                            improved = 1;
                        }
                    }
                }
            }
            if (cur > best)
                best = cur;
        }
        snprintf(detail, sizeof detail, "n=%d solver=%d hillclimb=%d", n, single, best);
        check(best <= single, "single hill climb never beats the solver", detail);
    }

    // ---- ceiling
    int cx = 0, cz = 0;
    const int ceiling = monument_group_max_area(p, n, BOX, BOX, &cx, &cz);
    int bcx = 0, bcz = 0;
    const int brute = brute_ceiling(p, n, &bcx, &bcz);
    snprintf(detail, sizeof detail, "n=%d ceiling=%d brute=%d", n, ceiling, brute);
    check(ceiling == brute, "ceiling equals the brute-force window max", detail);
    snprintf(detail, sizeof detail, "n=%d ceiling=%d dual=%d single=%d", n, ceiling, dual, single);
    check(ceiling >= dual, "ceiling >= dual", detail);
    check(ceiling >= single, "ceiling >= single", detail);
    check(ceiling <= total, "ceiling <= n*ax*az", detail);
    {
        // the cheap prefilter bound must dominate the true ceiling and never exceed the cap
        const int ub = monument_group_upper_bound(p, n, BOX, BOX);
        snprintf(detail, sizeof detail, "n=%d ceiling=%d bound=%d total=%d", n, ceiling, ub, total);
        check(ub >= ceiling, "gate upper bound >= brute ceiling", detail);
        check(ub <= total, "gate upper bound <= n*ax*az", detail);
    }
    {
        // the winning window centre must sit inside the group's block extent
        int mnx = p[0].x + BOX_LO, mxx = p[0].x + BOX_HI;
        int mnz = p[0].z + BOX_LO, mxz = p[0].z + BOX_HI;
        for (int i = 1; i < n; i++) {
            if (p[i].x + BOX_LO < mnx) mnx = p[i].x + BOX_LO;
            if (p[i].x + BOX_HI > mxx) mxx = p[i].x + BOX_HI;
            if (p[i].z + BOX_LO < mnz) mnz = p[i].z + BOX_LO;
            if (p[i].z + BOX_HI > mxz) mxz = p[i].z + BOX_HI;
        }
        snprintf(detail, sizeof detail, "n=%d gate=(%d,%d) extent=(%d,%d)-(%d,%d)",
                 n, cx, cz, mnx, mnz, mxx, mxz);
        check(cx >= mnx && cx <= mxx && cz >= mnz && cz <= mxz,
              "gate centre inside the group extent", detail);
    }
}

// ---------------------------------------------------------------- exhaustive two-box sweep

// For n = 2 the prefilter bound equals the exact ceiling, so every 16-aligned pair distance can
// be checked exhaustively (this is the theorem behind monument_group_upper_bound; a regression
// here means the closed form drifted from the sliding-window truth). Also samples non-aligned
// distances: the formula takes raw |dx|/|dz|, not just the 16-aligned game positions.
static void exhaustive_two_box(void) {
    char detail[256];
    for (int dx = 0; dx <= 432; dx += 16) {
        for (int dz = 0; dz <= 432; dz += 16) {
            MonumentBlockPos pair[2] = {{100000, -200000}, {100000 + dx, -200000 + dz}};
            int gx = 0, gz = 0;
            const int ceiling = monument_group_max_area(pair, 2, BOX, BOX, &gx, &gz);
            int bx = 0, bz = 0;
            const int brute = brute_ceiling(pair, 2, &bx, &bz);
            const int ub = monument_group_upper_bound(pair, 2, BOX, BOX);
            snprintf(detail, sizeof detail, "d=(%d,%d) core=%d brute=%d bound=%d",
                     dx, dz, ceiling, brute, ub);
            check(ceiling == brute, "2-box: core ceiling matches brute", detail);
            check(ub >= ceiling, "2-box: bound >= ceiling", detail);
            check(ub == ceiling, "2-box: bound is exact", detail);
        }
    }
    for (int t = 0; t < 200; t++) {
        const int dx = rnd(0, 500), dz = rnd(0, 500);
        MonumentBlockPos pair[2] = {{-50000, 80000}, {-50000 + dx, 80000 + dz}};
        int gx = 0, gz = 0;
        const int ceiling = monument_group_max_area(pair, 2, BOX, BOX, &gx, &gz);
        int bx = 0, bz = 0;
        const int brute = brute_ceiling(pair, 2, &bx, &bz);
        const int ub = monument_group_upper_bound(pair, 2, BOX, BOX);
        snprintf(detail, sizeof detail, "d=(%d,%d) core=%d brute=%d bound=%d",
                 dx, dz, ceiling, brute, ub);
        check(ceiling == brute, "2-box raw: core ceiling matches brute", detail);
        check(ub >= ceiling, "2-box raw: bound >= ceiling", detail);
    }
}

// ---------------------------------------------------------------- fixed shapes

static void fixed_shapes(void) {
    char detail[256];

    // one box: the disk always covers it completely
    MonumentBlockPos one[1] = {{0, 0}};
    int x = 0, z = 0;
    const int a = monument_afk_coverage(one, 1, BOX, BOX, &x, &z);
    snprintf(detail, sizeof detail, "area=%d", a);
    check(a == BOX * BOX, "single box = 58*58", detail);

    // two coincident boxes behave like one
    MonumentBlockPos same[2] = {{0, 0}, {0, 0}};
    int x1, z1, x2, z2, sd, sx, sz;
    const int d = monument_afk_coverage_dual(same, 2, BOX, BOX, &x1, &z1, &x2, &z2, &sd, &sx, &sz);
    snprintf(detail, sizeof detail, "dual=%d single=%d", d, sd);
    // multiset semantics: two coincident boxes each contribute, like every other area here
    check(d == sd && d == 2 * BOX * BOX, "coincident boxes count per box", detail);

    // two boxes exactly 257 apart: no single 257 window holds both, so the ceiling is one box
    MonumentBlockPos far[2] = {{0, 0}, {320, 0}}; // 320 blocks apart, boxes 58 wide
    const int c = monument_group_max_area(far, 2, BOX, BOX, &x1, &z1);
    snprintf(detail, sizeof detail, "ceiling=%d", c);
    check(c == BOX * BOX, "far-apart boxes: ceiling = one box", detail);
}

int main(int argc, char **argv) {
    const int groups = argc > 1 ? atoi(argv[1]) : 400;

    fixed_shapes();
    exhaustive_two_box();
    for (int i = 0; i < groups; i++) {
        MonumentBlockPos p[4];
        int n = 0;
        make_group(p, &n);
        validate_group(p, n);
    }

    printf("\n%ld checks, %ld failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
