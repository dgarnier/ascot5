/**
 * @file test_wall_3d.c
 * @brief Test program for 3D wall collision functions
 */
#include <stdio.h>
#include <stdlib.h>
#include "ascot5.h"
#include "wall_3d.h"

void test_collisions(wall_3d_data wdata, real* offload_array) {
    real xmin = -1.5;
    real xmax = 1.5;
    real ymin = -1.5;
    real ymax = 1.5;
    real zmin = -1.5;
    real zmax = 1.5;
    int i;
    for(i = 0; i < 100; i++) {
        real q1[3], q2[3];
        q1[0] = ((real)rand()/(real)RAND_MAX)*(xmax-xmin) + xmin;
        q2[0] = ((real)rand()/(real)RAND_MAX)*(xmax-xmin) + xmin;
        q1[1] = ((real)rand()/(real)RAND_MAX)*(ymax-ymin) + ymin;
        q2[1] = ((real)rand()/(real)RAND_MAX)*(ymax-ymin) + ymin;
        q1[2] = ((real)rand()/(real)RAND_MAX)*(zmax-zmin) + zmin;
        q2[2] = ((real)rand()/(real)RAND_MAX)*(zmax-zmin) + zmin;

        real w;
        int j;
        for(j = 0; j < wdata.n; j++) {
            w = wall_3d_tri_collision(q1, q2, &offload_array[9*j],
                    &offload_array[9*j+3], &offload_array[9*j+6]);
            if(w >= 0) {
                break;
            }
        }

        if(j == wdata.n) {
            j = -1;
        }
        printf("%lf %lf %lf %lf %lf %lf %lf %d\n", q1[0], q1[1], q1[2],
               q2[0], q2[1], q2[2], w, j);
    }
}

void test_tree(wall_3d_data* wdata, real* offload_array) {
    int i;
    int ncell = wdata->ngrid*wdata->ngrid*wdata->ngrid;
    printf("%d\n", ncell);
    for(i = 0; i < ncell; i++) {
        int ntris = wdata->tree_array[wdata->tree_array[i]];
        printf("%d\n", ntris);
        int j;
        for(j = 0; j < ntris; j++) {
            printf(" %d\n", wdata->tree_array[wdata->tree_array[i]+1+j]);
        }
    }
}

void test_tri_in_cube(void) {
    real t1[3] = {-1,-1,0.5};
    real t2[3] = {2,2,0.5};
    real t3[3] = {2,-1,0.5};
    real bb1[3] = {0,0,0};
    real bb2[3] = {1,1,1};

    printf("%d\n", wall_3d_tri_in_cube(t1, t2, t3, bb1, bb2));
}

int main(int argc, char** argv) {
    wall_3d_offload_data offload_data;
    real* offload_array;
    wall_3d_init_offload(&offload_data, &offload_array);

    wall_3d_data wdata;
    wall_3d_init(&wdata, &offload_data, offload_array);

    //test_collisions(wdata, offload_array);
    test_tree(&wdata, offload_array);
    //test_tri_in_cube();

    return 0;
}

