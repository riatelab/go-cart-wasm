/* Program for the construction of cartograms. The program needs polygon     */
/* coordinates and a value for the (relative) target area for each region as */
/* input, calculates the new coordinates, and writes these coordinates       */
/* either to a file or, if using the -s flag, to stdout.                     */
/* Postscript images of the original map and the cartogram are created if    */
/* using the -e flag.                                                        */

/* If you use output created by this program, please acknowledge the use of  */
/* this code and its first publication in:                                   */
/* Michael T. Gastner, Vivien Seguy and Pratyush More, "A fast flow-based    */
/* algorithm for creating density-equalizing map  projections", Proc. Nat.   */
/* Acad. Sci. USA, 115(10):E2156-E2164 (2018), DOI: 10.1073/pnas.1712674115. */

/* The input coordinates in the command-line argument after the -g flag must */
/* be in ArcInfo "generate" format of the type:                              */

/* 2302 Maine02              ID for region followed by optional description. */
/* 0.302204 -0.188090        Pairs of x-, y-coordinates. Orientation along   */
/* 0.302716 -0.187835        outer boundaries must be clockwise. If a        */
/* ...                       polygon has a hole, the inner boundary must be  */
/* 0.303897 -0.193159        anticlockwise.                                  */
/* 0.302204 -0.188090        Regions are not permitted to overlap. This is   */
/* END                       *not* checked by this code!                     */
/* 2301 Maine01                                                              */
/* 0.333358 -0.200693                                                        */
/* ...                                                                       */
/* 0.333358 -0.200693                                                        */
/* END                                                                       */
/* 2301 Maine01              IDs can repeat if a region consists of          */
/* 0.334699 -0.204771        multiple polygons.                              */
/* ...                                                                       */
/* 0.334699 -0.204771                                                        */
/* END                       Each polygon terminates with END.               */
/* END                       One more END signals the end of the file.       */

/* The target area in the command-line argument after the -a flag must be    */
/* given in the space-delimited format                                       */
/* region_ID target_area (optional comment)                                  */
/* For example,                                                              */

/* 1 9.0 Alabama                                                             */
/* 4 11.0 Arizona                                                            */
/* 5 6.0 Arkansas                                                            */
/* ...                                                                       */

/* Output:                                                                   */
/* (1) Unless overwritten by the -s flag, the output coordinates are written */
/*     to cartogram.gen in ArcInfo "generate" format. If the -s flag is      */
/*     the output is redirected to stdout.                                   */
/* (2) If the -e flag is used, two postscript images are produced.           */
/*     - A representation of the original input is prepared as map.eps.      */
/*     - an image of the cartogram is printed to cartogram.eps.              */
/* (3) The relative area errors are printed to area_error.dat.               */

/******************************** Inclusions. ********************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fftw3.h>
#include <string.h>
#include <getopt.h>
//#include <time.h>
#include "cartogram.h"

/**************************** Variable definitions. **************************/
/* Variables for map. */

double *area_err;
double *cart_area, map_maxx, map_maxy, map_minx, map_miny,
        *target_area, *region_perimeter;

int max_id, n_poly, *n_polycorn, *n_polyinreg, n_reg, *polygon_id, *poly_is_hole,
        **polyinreg, *region_id, *region_id_inv, *region_na;
size_t projsize;
POINT **cartcorn, **origcorn, **polycorn, *proj, *proj2, *projtmp, *projinit;
BOOLEAN use_perimeter_threshold;

/* Variables for digitizing the density. */

double *rho_ft, *rho_init;
fftw_plan plan_fwd;
int lx, ly;
rgb_color* color;


int doCartogram (char *map_file_name, char *area_file_name)
{
    BOOLEAN inv, use_area_file, use_gen, use_std;
    char *json_file_name = NULL, *map_file_type = "json";
    double cart_tot_area, correction_factor, init_tot_area, mae;
    int opt, i, integration, j;

    use_gen = TRUE;
    use_area_file = TRUE;
    inv = FALSE;
    use_std = FALSE;
    use_perimeter_threshold = TRUE;


    /***************************** Read input data. ****************************/

    /* Read the original polygon coordinates. If there is more than one        */
    /* region, fill the lx-times-ly grid with density and print a map.         */
    /* rho_ft[] will be filled with the Fourier transform of the initial       */
    /* density.                                                                */

    if (fill_with_density1(map_file_name, area_file_name, inv)) {
        fprintf(stderr,
                "WARNING: There is only one region. The output cartogram will\n");
        fprintf(stderr,
                "         simply be an affine transformation of the input map.\n");
    }
    if (max_area_err(area_err, cart_area, polycorn, &init_tot_area) <=
            MAX_PERMITTED_AREA_ERROR) {            /* No need to compute anything. */

        /* Coordinates in .gen/.json format and area errors. */

        if(strcmp(map_file_type, "gen") == 0){
            output_to_gen(use_std, polycorn);
        }else if(strcmp(map_file_type, "json") == 0){
            output_to_geojson(use_std, polycorn, map_file_name);
        }
        output_error();

        return 0;
    }

    /*************** Allocate memory for the projected positions. **************/

    projsize = lx * ly * sizeof(POINT);
    proj = (POINT*) malloc(projsize);
    cartcorn = (POINT**) malloc(n_poly * sizeof(POINT*));
    for (i=0; i<n_poly; i++)
        cartcorn[i] = (POINT*) malloc(n_polycorn[i] * sizeof(POINT));

    /* proj[i*ly+j] will store the current position of the point that started  */
    /* at (i+0.5, j+0.5).                                                      */
    projinit = (POINT*) malloc(projsize);
    for (i = 0; i < lx; i++) {
        for (j = 0; j < ly; j++) {
            projinit[i * ly + j].x = i + 0.5;
            projinit[i * ly + j].y = j + 0.5;
        }
    }
    memcpy(proj, projinit, projsize);

    /************** First integration of the equations of motion. **************/

    fprintf(stderr, "Starting integration 1\n");
    ffb_integrate();
    project(FALSE);  /* FALSE because we do not need to project the graticule. */
    mae = max_area_err(area_err, cart_area, cartcorn, &cart_tot_area);
    fprintf(stderr, "max. abs. area error: %f\n", mae);

    /********* Additional integrations to come closer to target areas. *********/

    proj2 = (POINT*) malloc(projsize);
    integration = 1;
    while (mae > MAX_PERMITTED_AREA_ERROR) {
        fill_with_density2();

        /* Copy the current graticule before resetting. We will construct the    */
        /* final graticule by interpolating proj2 on the basis of proj.          */

        projtmp = proj2;
        proj2 = proj;
        proj = projtmp;
        memcpy(proj, projinit, projsize);
        integration++;
        fprintf(stderr, "Starting integration %d\n", integration);
        ffb_integrate();
        project(TRUE);     /* TRUE because we need to project the graticule too. */

        /* Overwrite proj with proj2. */

        projtmp = proj;
        proj = proj2;
        proj2 = projtmp;
        mae = max_area_err(area_err, cart_area, cartcorn, &cart_tot_area);
        fprintf(stderr, "max. abs. area error: %f\n", mae);
    }

    /* Rescale all areas to perfectly match the total area before the          */
    /* integrations started.                                                   */

    correction_factor = sqrt(init_tot_area / cart_tot_area);
    fprintf(stderr, "correction_factor = %f\n", correction_factor);
    for (i=0; i<n_poly; i++) {
        for (j = 0; j < n_polycorn[i]; j++) {
            cartcorn[i][j].x = correction_factor * (cartcorn[i][j].x - 0.5 * lx) + 0.5 * lx;
            cartcorn[i][j].y = correction_factor * (cartcorn[i][j].y - 0.5 * ly) + 0.5 * ly;
        }
    }

    /* Run max_area_err() once more so that we print correct absolute areas in */
    /* output_error().                                                      */

    max_area_err(area_err, cart_area, cartcorn, &cart_tot_area);

    /* Print the cartogram in eps-format. Set the final argument to TRUE if    */
    /* you want to add the graticule.                                          */

    /* Print additional output files: Coordinates in .gen/.json format, area errors, */
    /* the graticules from the inverse transform. */

    rescale_map_inv();

    if(strcmp(map_file_type, "gen") == 0){
        output_to_gen(use_std, cartcorn);
    }else if(strcmp(map_file_type, "json") == 0){
        output_to_geojson(use_std, cartcorn, map_file_name);
    }
    output_error();

    /******************************* Free memory. ******************************/

    fftw_destroy_plan(plan_fwd);
    fftw_free(rho_ft);
    fftw_free(rho_init);
    for (i=0; i<n_poly; i++)
        free(polycorn[i]);
    free(polycorn);
    for (i=0; i<n_poly; i++)
        free(cartcorn[i]);
    free(cartcorn);
    free(n_polycorn);
    free(polygon_id);
    free(region_id);
    free(region_id_inv);
    for (i=0; i<n_reg; i++)
        free(polyinreg[i]);
    free(polyinreg);
    free(n_polyinreg);
    free(proj);
    free(proj2);
    free(target_area);
    free(area_err);
    free(cart_area);
    if (use_gen) {
        free(map_file_name);
    }
    if (use_area_file) {
        free(area_file_name);
    }
    return 0;
}
