/**
 * @file simulate.h
 * @brief Simulation module interface
 *
 * Interface for calling different simulation modules, @see B_field.h
 */
#ifndef SIMULATE_H
#define SIMULATE_H

#include "ascot5.h"
#include "B_field.h"
#include "E_field.h"
#include "plasma_1d.h"
#include "distributions.h"
#include "wall.h"
#include "diag.h"

typedef struct {
    real t0;
    real tstep;
    real tcollstep;
    real tmax;
    real trstep;
    real emin;
    int active_endcond;
    B_field_offload_data B_offload_data;
    E_field_offload_data E_offload_data;
    plasma_1d_offload_data plasma_offload_data;
    wall_offload_data wall_offload_data;
    dist_rzvv_offload_data dist_offload_data;
    diag_offload_data diag_offload_data;
} sim_offload_data;

typedef struct {
    real t0;
    real tstep;
    real tcollstep;
    real tmax;
    real trstep;
    real emin;
    int active_endcond;
    B_field_data B_data;
    E_field_data E_data;
    plasma_1d_data plasma_data;
    wall_data wall_data;
    dist_rzvv_data dist_data;
    diag_data diag_data;
} sim_data;


#pragma omp declare target
void sim_init(sim_data* sim, sim_offload_data* offload_data);
#pragma omp end declare target

#endif
