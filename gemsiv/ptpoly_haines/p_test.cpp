/* p_test.cpp - point in polygon inside/outside tester.  This is
 * simply testing and display code, the actual algorithms are in ptinpoly.c.
 *
 * Probably the most important thing to set for timings is TEST_RATIO (too
 * low and the timings are untrustworthy, too high and you wait forever).
 * Start low and see how consistent separate runs appear to be.
 *
 * To add a new algorithm to the test suite, add code at the spots marked
 * with '+++'.
 *
 * See Usage() for command line options (or just do "p_test -?").
 *
 * by Eric Haines, 3D/Eye Inc, erich@eye.com
 */

 /* Define TIMER to perform timings on code (need system timing function) */
// #define TIMER

 /* Number of times to try a single point vs. a polygon, per vertex.
  * This should be greater than 1 / ( HZ * approx. single test time in seconds )
  * in order to get a meaningful timings difference.  200 is reasonable for a
  * IBM PC 25 MHz 386 with no FPU, 50000 is good for an HP 720 workstation.
  * Start low and see how consistent separate runs appear to be.
  */

#ifdef TIMER
#define MACHINE_TEST_RATIO 20000000
#else
#define MACHINE_TEST_RATIO     1
#endif

  /* =========== that's all the easy stuff than can be changed  ============ */

#ifdef __GNUC__
#define sscanf_s sscanf
#define sprintf_s(buffer, size, ...) sprintf(buffer, __VA_ARGS__)
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <assert.h>
#include "ptinpoly.h"

#ifdef TIMER
#include <chrono>
#define mGetTime(t)    auto (t) = std::chrono::high_resolution_clock::now();
#endif

#ifdef DISPLAY
#include <starbase.c.h>  /* HP display */
#endif

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifndef HUGE
#define HUGE 1.79769313486232e+308
#endif

#define X 0
#define Y 1

#ifdef DISPLAY
int Display_Tests = 0;
double Display_Scale;
double Display_OffsetX;
double Display_OffsetY;
int Fd;
#endif

typedef struct {
    double time_total;
    int test_ratio;
    int test_times;
    int work;
    const char* name;
    int flag;
} Statistics, * pStatistics;

#define ANGLE_TEST        0
#define BARYCENTRIC_TEST  1
#define CROSSINGS_TEST    2
#define EXTERIOR_TEST     3
#define GRID_TEST         4
#define INCLUSION_TEST    5
#define CROSSMULT_TEST    6
#define PLANE_TEST        7
#define SPACKMAN_TEST     8
#define TRAPEZOID_TEST    9
#define WEILER_TEST      10
/* +++ add new name here and increment TOT_NUM_TESTS +++ */
#define TOT_NUM_TESTS    11

Statistics St[TOT_NUM_TESTS];

const char* TestName[] = {
    "angle",
    "barycentric",
    "crossings",
    "exterior",
    "grid",
    "inclusion",
    "cross-mult",
    "plane",
    "spackman",
    "trapezoid",
    "weiler" };
/* +++ add new name here +++ */

/* minimum & maximum number of polygon vertices to generate */
#define TOT_VERTS 1000
static int Min_Verts = 3;
static int Max_Verts = 6;

/* Test polygons are generated by going CCW in a circle around the origin from
 * the X+ axis around and generating vertices. The radius is how big the
 * circumscribing circle is, the perturbation is how much each vertex is varied.
 * So, radius 1 and perturbation 0 gives a regular, inscribed polygon;
 * radius 0 and perturbation 1 gives a totally random polygon in the
 * space [-1,1)
 */
static double Vertex_Radius = 1.0;
static double Vertex_Perturbation = 0.0;

/* A box is circumscribed around the test polygon.  Making this box bigger
 * is useful for a higher rejection rate.  For example, a ray tracing bounding
 * box usually contains a few polygons, so making the box ratio say 2 or so
 * could simulate this type of box.
 */
static double Box_Ratio = 1.0;

/* for debugging purposes, you might want to set Test_Polygons and Test_Points
 * high (say 1000), and then set the *_Test_Times to 1.  The timings will be
 * useless, but you'll test 1000 polygons each with 1000 points.  You'll also
 * probably want to set the Vertex_Perturbation to something > 0.0.
 */
 /* number of different polygons to try - I like 50 or so; left low for now */
static int Test_Polygons = 20;

/* number of different intersection points to try - I like 50 or so */
static int Test_Points = 20;

/* for debugging or constrained test purposes, this value constrains the value
 * of the polygon points and the test points to those on a grid emanating from
 * the origin with this increment.  Points are shifted to the closest grid
 * point.  0.0 means no grid.  NOTE:  by setting this value, a large number
 * of points will be generated exactly on interior (triangle fan) or exterior
 * edges.  Interior edge points will cause problems for the algorithms that
 * generate interior edges (triangle fan).  "On edge" points are arbitrarily
 * determined to be inside or outside the polygon, so results can differ.
 */
static double Constraint_Increment = 0.0;

/* default resolutions */
static int Grid_Resolution = 20;
static int Trapezoid_Bins = 20;

#define Max(a,b) (((a)>(b))?(a):(b))

#define FPRINTF_POLYGON                                                     \
    fprintf( stderr, "point %g %g\n", (float)point[X], (float)point[Y] );   \
    fprintf( stderr, "polygon (%d vertices):\n", numverts );                \
    for ( n = 0; n < numverts; n++ ) {                                      \
        fprintf( stderr, " %g %g\n", (float)pgon[n][X], (float)pgon[n][Y]); \
  }

/* timing functions */
#ifdef TIMER

#define START_TIMER( test_id )                                     \
   /* do the test a bunch of times to get a useful time reading */ \
   mGetTime( timestart );                                          \
   for ( int tcnt = St[test_id].test_times+1; --tcnt; )

#define STOP_TIMER( test_id )                  \
   mGetTime( timestop );                       \
   auto time = timestop - timestart;           \
   /* time in milliseconds */                  \
   St[test_id].time_total += time/std::chrono::milliseconds(1);
#else
#define START_TIMER( test_id )
#define STOP_TIMER( test_id )
#endif

//char *getenv();
void Usage();
void ScanOpts();
void ConstrainPoint();
void BreakString();
#ifdef DISPLAY
void DisplayPolygon();
void DisplayPoint();
#endif

void Usage()
{
    /* +++ add new routine here +++ */
    printf("p_test [options] -{ABCEGIMPSTW}\n");
    printf("  -v minverts [maxverts] = variation in number of polygon vertices\n");
    printf("  -r radius = radius of polygon vertices generated\n");
    printf("  -p perturbation = perturbation of polygon vertices generated\n");
    printf("       These first three determine the type of polygon tested.\n");
    printf("       No perturbation gives regular polygons, while no radius\n");
    printf("       gives random polygons, and a mix gives semi-random polygons\n");
    printf("  -s size = scale of test point box around polygon (1.0 default)\n");
    printf("       A larger size means more points generated outside the\n");
    printf("       polygon.  By default test points are in the bounding box.\n");
    printf("  -b bins = number of y bins for trapezoid test\n");
    printf("  -g resolution = grid resolution for grid test\n");
    printf("  -n polygons = number of polygons to test (default %d)\n",
        Test_Polygons);
    printf("  -i points = number of points to test per polygon (default %d)\n",
        Test_Points);
    printf("  -c increment = constrain polygon and test points to grid\n");
    /* +++ add new routine here +++ */
    printf("  -{ABCEGIMPSTW} = angle/bary/crossings/exterior/grid/inclusion/cross-mult/\n");
    printf("       plane/spackman/trapezoid (bin)/weiler test (default is all)\n");
    printf("  -d = display polygons and points using starbase\n");
}

void ScanOpts(int argc, char* argv[])
{
    float f1;
    int i1;
    int test_flag = FALSE;

    for (argc--, argv++; argc > 0; argc--, argv++) {
        if (**argv == '-') {
            switch (*++(*argv)) {

            case 'v': /* vertex min & max */
                argv++; argc--;
                if (argc && sscanf_s(*argv, "%d", &i1) == 1) {
                    Min_Verts = i1;
                    argv++; argc--;
                    if (argc && sscanf_s(*argv, "%d", &i1) == 1) {
                        Max_Verts = i1;
                    }
                    else {
                        argv--; argc++;
                        Max_Verts = Min_Verts;
                    }
                }
                else {
                    Usage();
                    exit(1);
                }
                break;

            case 'r': /* vertex radius */
                argv++; argc--;
                if (argc && sscanf_s(*argv, "%f", &f1) == 1) {
                    Vertex_Radius = (double)f1;
                }
                else {
                    Usage();
                    exit(1);
                }
                break;

            case 'p': /* vertex perturbation */
                argv++; argc--;
                if (argc && sscanf_s(*argv, "%f", &f1) == 1) {
                    Vertex_Perturbation = (double)f1;
                }
                else {
                    Usage();
                    exit(1);
                }
                break;

            case 's': /* centered box size ratio - higher is bigger */
                argv++; argc--;
                if (argc && sscanf_s(*argv, "%f", &f1) == 1) {
                    Box_Ratio = (double)f1;
                    if (Box_Ratio < 1.0) {
                        fprintf(stderr, "warning: ratio is smaller than 1.0\n");
                    }
                }
                else {
                    Usage();
                    exit(1);
                }
                break;

            case 'b': /* number of bins for trapezoid test */
                argv++; argc--;
                if (argc && sscanf_s(*argv, "%d", &i1) == 1) {
                    Trapezoid_Bins = i1;
                }
                else {
                    Usage();
                    exit(1);
                }
                break;

            case 'g': /* grid resolution for grid test */
                argv++; argc--;
                if (argc && sscanf_s(*argv, "%d", &i1) == 1) {
                    Grid_Resolution = i1;
                }
                else {
                    Usage();
                    exit(1);
                }
                break;

            case 'n': /* number of polygons to test */
                argv++; argc--;
                if (argc && sscanf_s(*argv, "%d", &i1) == 1) {
                    Test_Polygons = i1;
                }
                else {
                    Usage();
                    exit(1);
                }
                break;

            case 'i': /* number of intersections per polygon to test */
                argv++; argc--;
                if (argc && sscanf_s(*argv, "%d", &i1) == 1) {
                    Test_Points = i1;
                }
                else {
                    Usage();
                    exit(1);
                }
                break;

            case 'c': /* constrain increment (0 means don't use) */
                argv++; argc--;
                if (argc && sscanf_s(*argv, "%f", &f1) == 1) {
                    Constraint_Increment = (double)f1;
                }
                else {
                    Usage();
                    exit(1);
                }
                break;

            case 'd': /* display polygon & test points */
#ifdef DISPLAY
                Display_Tests = 1;
#else
                fprintf(stderr,
                    "warning: display mode not compiled in - ignored\n");
#endif
                break;

                /* +++ add new symbol here +++ */
            case 'A': /* do tests specified */
            case 'B':
            case 'C':
            case 'E':
            case 'G':
            case 'I':
            case 'M':
            case 'P':
            case 'S':
            case 'T':
            case 'W':
                test_flag = TRUE;
                if (strchr(*argv, 'A')) {
                    St[ANGLE_TEST].work = 1;
                }
                if (strchr(*argv, 'B')) {
                    St[BARYCENTRIC_TEST].work = 1;
                }
                if (strchr(*argv, 'C')) {
                    St[CROSSINGS_TEST].work = 1;
                }
                if (strchr(*argv, 'E')) {
#ifdef CONVEX
                    St[EXTERIOR_TEST].work = 1;
#else
                    fprintf(stderr,
                        "warning: exterior test for -DCONVEX only - ignored\n");
#endif
                }
                if (strchr(*argv, 'G')) {
                    St[GRID_TEST].work = 1;
                }
                if (strchr(*argv, 'I')) {
#ifdef CONVEX
                    St[INCLUSION_TEST].work = 1;
#else
                    fprintf(stderr,
                        "warning: inclusion test for -DCONVEX only - ignored\n");
#endif
                }
                if (strchr(*argv, 'M')) {
                    St[CROSSMULT_TEST].work = 1;
                }
                if (strchr(*argv, 'P')) {
                    St[PLANE_TEST].work = 1;
                }
                if (strchr(*argv, 'S')) {
                    St[SPACKMAN_TEST].work = 1;
                }
                if (strchr(*argv, 'T')) {
                    St[TRAPEZOID_TEST].work = 1;
                }
                if (strchr(*argv, 'W')) {
                    St[WEILER_TEST].work = 1;
                }
                /* +++ add new symbol test here +++ */
                break;

            default:
                Usage();
                exit(1);
                break;
            }

        }
        else {
            Usage();
            exit(1);
        }
    }

    if (!test_flag) {
        fprintf(stderr,
            "error: no point in polygon tests were specified, e.g. -PCS\n");
        Usage();
        exit(1);
    }
}

void ConstrainPoint(double* pt)
{
    double val;

    if (Constraint_Increment > 0.0) {
        pt[X] -=
            (val = fmod(pt[X], Constraint_Increment));
        if (fabs(val) > Constraint_Increment * 0.5) {
            pt[X] += (val > 0.0) ? Constraint_Increment :
                -Constraint_Increment;
        }
        pt[Y] -=
            (val = fmod(pt[Y], Constraint_Increment));
        if (fabs(val) > Constraint_Increment * 0.5) {
            pt[Y] += (val > 0.0) ? Constraint_Increment :
                -Constraint_Increment;
        }
    }
}

/* break long strings into 80 or less character output.  Not foolproof, but
 * good enough.
 */
void BreakString(char* str)
{
    int length;
    int i, last_space, col;

    length = (int)strlen(str);
    last_space = 0;
    col = 0;
    for (i = 0; i < length; i++) {
        if (str[i] == ' ') {
            last_space = i;
        }
        if (col == 79) {
            str[last_space] = '\n';
            col = i - last_space;
            last_space = 0;
        }
        else {
            col++;
        }
    }
}

#ifdef DISPLAY
/* ================================= display routines ====================== */
/* Currently for HP Starbase - pretty easy to modify */
void DisplayPolygon(pgon, numverts, id)
double pgon[][2];
int numverts;
{
    static int init_flag = 0;
    int i;
    char str[256];

    if (!init_flag) {
        init_flag = 1;
        /* make things big enough to avoid clipping */
        Display_Scale = 0.45 / (Vertex_Radius + Vertex_Perturbation);
        Display_OffsetX = Display_Scale + 0.05;
        Display_OffsetY = Display_Scale + 0.10;

        Fd = DevOpen(OUTDEV, INIT);
        shade_mode(Fd, CMAP_FULL | INIT, 0);
        background_color(Fd, 0.1, 0.2, 0.4);
        line_color(Fd, 1.0, 0.9, 0.7);
        text_color(Fd, 1.0, 1.0, 0.2);
        character_height(Fd, 0.08);
        marker_type(Fd, 3);
    }
    clear_view_surface(Fd);

    move2d(Fd,
        (float)(pgon[numverts - 1][X] * Display_Scale + Display_OffsetX),
        (float)(pgon[numverts - 1][Y] * Display_Scale + Display_OffsetY));
    for (i = 0; i < numverts; i++) {
        draw2d(Fd,
            (float)(pgon[i][X] * Display_Scale + Display_OffsetX),
            (float)(pgon[i][Y] * Display_Scale + Display_OffsetY));
    }

    sprintf_s(str, 256, "%4d sides, %3d of %d\n", numverts, id + 1, Test_Polygons);
    text2d(Fd, 0.01, 0.01, str, VDC_TEXT, 0);
    flush_buffer(Fd);
}

void DisplayPoint(point, hilit)
double point[2];
int hilit;
{
    float clist[2];

    if (hilit) {
        marker_color(Fd, 1.0, 0.0, 0.0);
    }
    else {
        marker_color(Fd, 0.2, 1.0, 1.0);
    }

    clist[0] = (float)(point[0] * Display_Scale + Display_OffsetX);
    clist[1] = (float)(point[1] * Display_Scale + Display_OffsetY);
    polymarker2d(Fd, clist, 1, 0);
    flush_buffer(Fd);
}

int DevOpen(dev_kind, init_mode)
int dev_kind, init_mode;
{
    char* dev, * driver;
    int fildes;

    if (dev_kind == OUTDEV) {
        dev = getenv("OUTINDEV");
        if (!dev) dev = getenv("OUTDEV");
        if (!dev) dev = "/dev/crt";
        driver = getenv("OUTDRIVER");
        if (!driver) driver = "hp98731";
    }
    else {
        dev = getenv("OUTINDEV");
        if (!dev) dev = getenv("INDEV");
        if (!dev) dev = "/dev/hil2";
        driver = getenv("INDRIVER");
        if (!driver) driver = "hp-hil";
    }

    /* driver? we don't need no stinking driver... */
    fildes = gopen(dev, dev_kind, NULL, init_mode);

    return(fildes);
}
#endif


/* test program - see Usage() for command line options */
int main(int argc, char* argv[])
{
    int i, j, k, n, numverts, inside_flag, inside_tot;
    int numrec = 0;
    double pgon[TOT_VERTS][2], point[2], angle, ran_offset;
    double rangex, rangey, scale, minx, maxx, diffx, miny, maxy, diffy;
    double offx, offy;
    char str[256], * strplus;
    GridSet grid_set;
    pPlaneSet p_plane_set = NULL;
    pSpackmanSet p_spackman_set = NULL;
    TrapezoidSet trap_set;

#ifdef CONVEX
    pPlaneSet p_ext_set = NULL;
    pInclusionAnchor p_inc_anchor = NULL;
#endif

    //SRAN();

    ScanOpts(argc, argv);

    for (i = 0; i < TOT_NUM_TESTS; i++) {
        St[i].time_total = 0.0;
        if (i == ANGLE_TEST) {
            /* angle test is real slow, so test it fewer times */
            St[i].test_ratio = MACHINE_TEST_RATIO / 10;
        }
        else {
            St[i].test_ratio = MACHINE_TEST_RATIO;
        }
        St[i].name = TestName[i];
        St[i].flag = 0;
    }

    inside_tot = 0;

#ifdef CONVEX
    if (Vertex_Perturbation > 0.0 && Max_Verts > 3) {
        fprintf(stderr,
            "warning: vertex perturbation is > 0.0, which is exciting\n");
        fprintf(stderr,
            "    when using convex-only algorithms!\n");
    }
#endif

    if (Min_Verts == Max_Verts) {
        sprintf_s(str, 256, "\nPolygons with %d vertices, radius %g, "
            "perturbation +/- %g, bounding box scale %g",
            Min_Verts, Vertex_Radius, Vertex_Perturbation, Box_Ratio);
    }
    else {
        sprintf_s(str, 256, "\nPolygons with %d to %d vertices, radius %g, "
            "perturbation +/- %g, bounding box scale %g",
            Min_Verts, Max_Verts, Vertex_Radius, Vertex_Perturbation,
            Box_Ratio);
    }
    strplus = &str[strlen(str)];
    if (St[TRAPEZOID_TEST].work) {
        sprintf_s(strplus, 256, ", %d trapezoid bins", Trapezoid_Bins);
        strplus = &str[strlen(str)];
    }
    if (St[GRID_TEST].work) {
        sprintf_s(strplus, 256, ", %d grid resolution", Grid_Resolution);
        strplus = &str[strlen(str)];
    }
#ifdef CONVEX
    sprintf_s(strplus, 256, ", convex");
    strplus = &str[strlen(str)];
#ifdef HYBRID
    sprintf_s(strplus, 256, ", hybrid");
    strplus = &str[strlen(str)];
#endif
#endif
#ifdef SORT
    if (St[PLANE_TEST].work || St[SPACKMAN_TEST].work) {
        sprintf_s(strplus, 256, ", using triangles sorted by edge lengths");
        strplus = &str[strlen(str)];
#ifdef CONVEX
        sprintf_s(strplus, 256, " and areas");
        strplus = &str[strlen(str)];
#endif
    }
#endif
#ifdef RANDOM
    if (St[EXTERIOR_TEST].work) {
        sprintf_s(strplus, 256, ", exterior edges' order randomized");
        strplus = &str[strlen(str)];
    }
#endif
    sprintf_s(strplus, 256, ".\n");
    strplus = &str[strlen(str)];
    BreakString(str);
    printf("%s", str);

    printf(
        " Testing %d polygons with %d points\n", Test_Polygons, Test_Points);

#ifdef TIMER
    printf("doing timings");
    fflush(stdout);
#endif
    for (i = 0; i < Test_Polygons; i++) {

        /* make an arbitrary polygon fitting 0-1 range in x and y */
        numverts = Min_Verts +
            (int)(RAN01() * (double)(Max_Verts - Min_Verts + 1));

        /* add a random offset to the angle so that each polygon is not in
         * some favorable (or unfavorable) alignment.
         */
        ran_offset = 2.0 * M_PI * RAN01();
        minx = miny = 99999.0;
        maxx = maxy = -99999.0;
        for (j = 0; j < numverts; j++) {
            angle = 2.0 * M_PI * (double)j / (double)numverts + ran_offset;
            pgon[j][X] = cos(angle) * Vertex_Radius +
                (RAN01() * 2.0 - 1.0) * Vertex_Perturbation;
            pgon[j][Y] = sin(angle) * Vertex_Radius +
                (RAN01() * 2.0 - 1.0) * Vertex_Perturbation;

            ConstrainPoint(pgon[j]);

            if (pgon[j][X] < minx) minx = pgon[j][X];
            if (pgon[j][X] > maxx) maxx = pgon[j][X];
            if (pgon[j][Y] < miny) miny = pgon[j][Y];
            if (pgon[j][Y] > maxy) maxy = pgon[j][Y];
        }

        offx = (maxx + minx) / 2.0;
        offy = (maxy + miny) / 2.0;
        if ((diffx = maxx - minx) > (diffy = maxy - miny)) {
            scale = 2.0 / (Box_Ratio * diffx);
            rangex = 1.0;
            rangey = diffy / diffx;
        }
        else {
            scale = 2.0 / (Box_Ratio * diffy);
            rangex = diffx / diffy;
            rangey = 1.0;
        }

        for (j = 0; j < numverts; j++) {
            pgon[j][X] = (pgon[j][X] - offx) * scale;
            pgon[j][Y] = (pgon[j][Y] - offy) * scale;
        }

        /* Set up number of times to test a point against a polygon, for
         * the sake of getting a reasonable timing.  We already know how
         * most of these will perform, so scale their # tests accordingly.
         */
        for (j = 0; j < TOT_NUM_TESTS; j++) {
            if ((j == GRID_TEST) || (j == TRAPEZOID_TEST)) {
                St[j].test_times = Max(St[j].test_ratio /
                    (int)sqrt((double)numverts), 1);
            }
            else {
                St[j].test_times = Max(St[j].test_ratio / numverts, 1);
            }
        }

        /* set up tests */
#ifdef CONVEX
        if (St[EXTERIOR_TEST].work) {
            p_ext_set = ExteriorSetup(pgon, numverts);
        }
#endif

        if (St[GRID_TEST].work) {
            GridSetup(pgon, numverts, Grid_Resolution, &grid_set);
        }

#ifdef CONVEX
        if (St[INCLUSION_TEST].work) {
            p_inc_anchor = InclusionSetup(pgon, numverts);
        }
#endif

        if (St[PLANE_TEST].work) {
            p_plane_set = PlaneSetup(pgon, numverts);
        }

        if (St[SPACKMAN_TEST].work) {
            p_spackman_set = SpackmanSetup(pgon, numverts, &numrec);
        }

        if (St[TRAPEZOID_TEST].work) {
            TrapezoidSetup(pgon, numverts, Trapezoid_Bins, &trap_set);
        }

#ifdef DISPLAY
        if (Display_Tests) {
            DisplayPolygon(pgon, numverts, i);
        }
#endif

        /* now try # of points against it */
        for (j = 0; j < Test_Points; j++) {
            point[X] = RAN01() * rangex * 2.0 - rangex;
            point[Y] = RAN01() * rangey * 2.0 - rangey;

            ConstrainPoint(point);

#ifdef DISPLAY
            if (Display_Tests) {
                DisplayPoint(point, TRUE);
            }
#endif

            if (St[ANGLE_TEST].work) {
                START_TIMER(ANGLE_TEST)
                    St[ANGLE_TEST].flag = AngleTest(pgon, numverts, point);
                STOP_TIMER(ANGLE_TEST)
            }
            if (St[BARYCENTRIC_TEST].work) {
                START_TIMER(BARYCENTRIC_TEST)
                    St[BARYCENTRIC_TEST].flag =
                    BarycentricTest(pgon, numverts, point);
                STOP_TIMER(BARYCENTRIC_TEST)
            }
            if (St[CROSSINGS_TEST].work) {
                START_TIMER(CROSSINGS_TEST)
                    St[CROSSINGS_TEST].flag =
                    CrossingsTest(pgon, numverts, point);
                STOP_TIMER(CROSSINGS_TEST)
            }
#ifdef CONVEX
            if (St[EXTERIOR_TEST].work) {
                START_TIMER(EXTERIOR_TEST)
                    St[EXTERIOR_TEST].flag =
                    ExteriorTest(p_ext_set, numverts, point);
                STOP_TIMER(EXTERIOR_TEST)
            }
#endif
            if (St[GRID_TEST].work) {
                START_TIMER(GRID_TEST)
                    St[GRID_TEST].flag = GridTest(&grid_set, point);
                STOP_TIMER(GRID_TEST)
            }
#ifdef CONVEX
            if (St[INCLUSION_TEST].work) {
                START_TIMER(INCLUSION_TEST)
                    St[INCLUSION_TEST].flag =
                    InclusionTest(p_inc_anchor, point);
                STOP_TIMER(INCLUSION_TEST)
            }
#endif
            if (St[CROSSMULT_TEST].work) {
                START_TIMER(CROSSMULT_TEST)
                    St[CROSSMULT_TEST].flag = CrossingsMultiplyTest(
                        pgon, numverts, point);
                STOP_TIMER(CROSSMULT_TEST)
            }
            if (St[PLANE_TEST].work) {
                START_TIMER(PLANE_TEST)
                    St[PLANE_TEST].flag =
                    PlaneTest(p_plane_set, numverts, point);
                STOP_TIMER(PLANE_TEST)
            }
            if (St[SPACKMAN_TEST].work) {
                START_TIMER(SPACKMAN_TEST)
                    St[SPACKMAN_TEST].flag =
                    SpackmanTest(pgon[0], p_spackman_set, numrec, point);
                STOP_TIMER(SPACKMAN_TEST)
            }
            if (St[TRAPEZOID_TEST].work) {
                START_TIMER(TRAPEZOID_TEST)
                    St[TRAPEZOID_TEST].flag =
                    TrapezoidTest(pgon, numverts, &trap_set, point);
                STOP_TIMER(TRAPEZOID_TEST)
            }
            if (St[WEILER_TEST].work) {
                START_TIMER(WEILER_TEST)
                    St[WEILER_TEST].flag =
                    WeilerTest(pgon, numverts, point);
                STOP_TIMER(WEILER_TEST)
            }
            /* +++ add new procedure call here +++ */

                    /* reality check if crossings test is used */
            if (St[CROSSINGS_TEST].work) {
                for (k = 0; k < TOT_NUM_TESTS; k++) {
                    if (St[k].work &&
                        (St[k].flag != St[CROSSINGS_TEST].flag)) {
                        fprintf(stderr,
                            "%s test says %s, crossings test says %s\n",
                            St[k].name,
                            St[k].flag ? "INSIDE" : "OUTSIDE",
                            St[CROSSINGS_TEST].flag ? "INSIDE" : "OUTSIDE");
                        FPRINTF_POLYGON;
                    }
                }
            }

            /* see if any flag is TRUE (i.e. the test point is inside) */
            for (k = 0, inside_flag = 0
                ; k < TOT_NUM_TESTS && !inside_flag
                ; k++) {
                inside_flag = St[k].flag;
            }
            inside_tot += inside_flag;

            /* turn off highlighting for this point */
#ifdef DISPLAY
            if (Display_Tests) {
                DisplayPoint(point, FALSE);
            }
#endif
        }

        /* clean up test structures */
#ifdef CONVEX
        if (St[EXTERIOR_TEST].work) {
            ExteriorCleanup(p_ext_set);
            p_ext_set = NULL;
        }
#endif

        if (St[GRID_TEST].work) {
            GridCleanup(&grid_set);
        }

#ifdef CONVEX
        if (St[INCLUSION_TEST].work) {
            InclusionCleanup(p_inc_anchor);
            p_inc_anchor = NULL;
        }
#endif

        if (St[PLANE_TEST].work) {
            PlaneCleanup(p_plane_set);
            p_plane_set = NULL;
        }

        if (St[SPACKMAN_TEST].work) {
            SpackmanCleanup(p_spackman_set);
            p_spackman_set = NULL;
        }

        if (St[TRAPEZOID_TEST].work) {
            TrapezoidCleanup(&trap_set);
        }

#ifdef TIMER
        /* print a "." every polygon done to give the user a warm feeling */
        printf(".");
        fflush(stdout);
#endif
    }

    printf("\n%g %% of all points were inside polygons\n",
        (float)inside_tot * 100.0 / (float)(Test_Points * Test_Polygons));

#ifdef TIMER
    for (i = 0; i < TOT_NUM_TESTS; i++) {
        if (St[i].work) {
            printf("  %s test time: %g nanoseconds per test\n",
                St[i].name,
                (float)(1000000.0 * St[i].time_total / ((double)St[i].test_times * (double)Test_Points * (double)Test_Polygons)));
        }
    }
#endif
    return 0;
}
