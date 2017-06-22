/**
 * @file diagnostics.h
 * @brief Header file for diagnostics.c
 */
#ifndef DIAG_H
#define DIAG_H
#include "ascot5.h"
#include "particle.h"
#include "diag_orb.h"

typedef struct{
    int temp;

} diag_offload_data;

typedef struct{
    int diag_orb_collect;
    int diag_debug_collect;
    int diag_dist4D_collect;
    diag_orb_data* orbits;

} diag_data;

#pragma omp declare target
void diag_init(diag_data* data, diag_offload_data* offload_data);

void diag_update_gc(diag_data* d, particle_simd_gc* p_f, particle_simd_gc* p_i);

void diag_update_fo(diag_data* d, particle_simd_fo* p_f, particle_simd_fo* p_i);

void diag_update_ml(diag_data* d, particle_simd_ml* p_f, particle_simd_ml* p_i);

void diag_write(diag_data* d);

void diag_clean(diag_data* d);
#pragma omp end declare target

#endif

