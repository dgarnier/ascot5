#include "../particle.h"
#include "../simulate.h"
#include "../endcond.h"
#include "../print.h"
#include "./bmc_wall.h"
#include <stdio.h>
#include <math.h>
#include "../ascot5.h"
#include "../consts.h"
#include "../B_field.h"
#include "../E_field.h"
#include "../boozer.h"
#include "../mhd.h"
#include "../math.h"
#include "../particle.h"
#include "../error.h"
#include "../random.h"
#include "../simulate/mccc/mccc_coefs.h"
#include "../simulate/mccc/mccc.h"
#include "../physlib.h"
#include "../simulate/step/step_gceom.h"
#include "../simulate/step/step_gceom_mhd.h"
#include "../simulate/step/step_gc_rk4.h"

void bmc_simulate_timestep_gc(int n_simd_particles, int n_coll_simd_particles, particle_simd_gc* p, particle_simd_gc* p_coll,
        int n_hermite_knots,
        sim_offload_data* sim_offload,
        offload_package* offload_data,
        real* offload_array,
        real h, int n_rk4_subcycles
    );

void fmc_simulation(
        particle_state* ps,
        sim_offload_data* sim,
        offload_package* offload_data,
        real* offload_array,
        double* mic1_start, double* mic1_end,
        double* mic0_start, double* mic0_end,
        double* host_start, double* host_end,
        int n_mic,
        int n_host,
        real* diag_offload_array_host,
        real* diag_offload_array_mic0,
        real* diag_offload_array_mic1
    );

void init_particles_coll_simd_hermite(int n_simd_particles, int n_hermite_knots,
        particle_simd_gc* p_coll
    );
void copy_particles_simd_to_coll_simd(int n_simd_particles, int n_hermite_knots,
        particle_simd_gc* p, particle_simd_gc* p_coll
    );

void bmc_step_deterministic(particle_simd_gc *p, real *h, B_field_data *Bdata,
                            E_field_data *Edata, plasma_data *pdata, int enable_clmbcol, random_data *rdata, mccc_data *mdata);

void bmc_step_stochastic(particle_simd_gc *p, real *h, B_field_data *Bdata,
                         E_field_data *Edata, plasma_data *pdata, random_data *rdata, mccc_data *mdata);