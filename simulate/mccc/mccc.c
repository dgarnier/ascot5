/**
 * @author Konsta Sarkimaki konsta.sarkimaki@aalto.fi
 * @file mccc.c
 * @brief Interface for using mccc package within ascot5
 */
#define _XOPEN_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "../../ascot5.h"
#include "../../particle.h"
#include "../../B_field.h"
#include "../../plasma_1d.h"
#include "../../math.h"
#include "../../consts.h"
#include "mccc.h"
#include "mccc_wiener.h"
#include "mccc_push.h"
#include "mccc_coefs.h"

/**
 * @brief Initializes mccc package
 *
 * Initializes lookup tables that provide possibly faster
 * evaluation of collision coefficients.
 * 
 * @todo Not implemented
 */
void mccc_init(){

}

/**
 * @brief Evaluates collision coefficients in fo picture
 *
 * Finds the rho coordinate first and uses it to evaluate plasma parameters
 * that are then used to evaluate Coulomb logarithm and collision coefficients.
 * 
 * The coefficients are returned in arrays whose format is D_is = D[i*s] where
 * i is the particle SIMD position and s is species (maximum is MAX_SPECIES
 * defined in ascot5.h).
 *
 * Operations are performed simultaneously for NSIMD markers.
 *
 * @param p pointer to SIMD_fo struct containing the markers
 * @param Bdata pointer to magnetic field data
 * @param pdata pointer to plasma data
 * @param clogab pointer to array storing the evaluated Coulomb logarithms
 * @param F pointer to array storing the evaluated Fokker-Planck coefficient [m/s^2]
 * @param Dpara pointer to array the evaluated storing parallel diffusion coefficient [m^2/s^3]
 * @param Dperp pointer to array the evaluated storing perpendicular diffusion coefficient [m²/s^3]
 * @param K pointer to array storing the evaluated friction coefficient [m/s^2]
 * @param nu pointer to array storing the evaluated pitch collision frequency [1/s]
 */
void mccc_update_fo(particle_simd_fo* p, B_field_data* Bdata, plasma_1d_data* pdata, 
		    real* clogab, real* F, real* Dpara, real* Dperp, real* K, real* nu){
    int i;
    #pragma omp simd
    for(i = 0; i < NSIMD; i++) {
        if(p->running[i]) {
	    /* Update background data */
	    real temp[MAX_SPECIES];
	    real dens[MAX_SPECIES];

	    // Electron and ion temperature
	    temp[0] = plasma_1d_eval_temp(p->rho[i], 0, pdata)*CONST_KB;
	    temp[1] = plasma_1d_eval_temp(p->rho[i], 1, pdata)*CONST_KB;

	    // Electron density
	    dens[0] = plasma_1d_eval_dens(p->rho[i], 0, pdata);

	    // Ion densities (and temperatures)
	    int j;
	    for(j = 1; j < pdata->n_species; j++) {
		dens[j] = plasma_1d_eval_dens(p->rho[i], j, pdata);
		temp[j] = temp[1];
	    }

	    /* Evaluate coefficients */
	    real va = sqrt(p->rdot[i]*p->rdot[i] + (p->r[i]*p->phidot[i])*(p->r[i]*p->phidot[i]) + p->zdot[i]*p->zdot[i]);
					        
	    mccc_coefs_clog(p->mass[i],p->charge[i],va,pdata->mass,pdata->charge,dens,temp,&clogab[i*MAX_SPECIES],pdata->n_species);
	    mccc_coefs_fo(p->mass[i],p->charge[i],va,pdata->mass,pdata->charge,dens,temp,&clogab[i*MAX_SPECIES],pdata->n_species,
			  &F[i*MAX_SPECIES],&Dpara[i*MAX_SPECIES],&Dperp[i*MAX_SPECIES],&K[i*MAX_SPECIES],&nu[i*MAX_SPECIES]);


	}
    }

}

/**
 * @brief Evaluates collision frequency in gc picture
 *
 * Finds the rho coordinate first and uses it to evaluate plasma parameters
 * that are then used to evaluate Coulomb logarithm and collision coefficients.
 *
 * @param p pointer to SIMD_gc struct containing the markers
 * @param Bdata pointer to magnetic field data
 * @param pdata pointer to plasma data
 * @param nu pointer to pitch collision frequency
 * @param i index of the marker in simd array
 */
void mccc_collfreq_gc(particle_simd_gc* p, B_field_data* Bdata, plasma_1d_data* pdata, 
		    real* nu, int i){

    /* Update background data */
    real B[3];
    real xi;

    B[0] = p->B_r[i];
    B[1] = p->B_phi[i];
    B[2] = p->B_z[i];

    real temp[MAX_SPECIES];
    real dens[MAX_SPECIES];

    // Electron and ion temperature
    temp[0] = plasma_1d_eval_temp(p->rho[i], 0, pdata)*CONST_KB;
    temp[1] = plasma_1d_eval_temp(p->rho[i], 1, pdata)*CONST_KB;

    // Electron density
    dens[0] = plasma_1d_eval_dens(p->rho[i], 0, pdata);

    // Ion densities (and temperatures)
    int j;
    for(j = 1; j < pdata->n_species; j++) {
	dens[j] = plasma_1d_eval_dens(p->rho[i], j, pdata);
	temp[j] = temp[1];
    }

    /* Evaluate coefficients */
    real Bnorm = math_norm(B);
    real t = 2*p->mu[i]*Bnorm*p->mass[i];
    real va = sqrt(p->vpar[i]*p->vpar[i] + t*t);
    xi = p->vpar[i]/va;

    real clogab[MAX_SPECIES];
    real Dparab[MAX_SPECIES];
    real Kb[MAX_SPECIES];
    real nub[MAX_SPECIES];
    real DXb[MAX_SPECIES];
    mccc_coefs_clog(p->mass[i],p->charge[i],va,pdata->mass,pdata->charge,dens,temp,clogab,pdata->n_species);
    mccc_coefs_gcfixed(p->mass[i],p->charge[i],va,xi,pdata->mass,pdata->charge,dens,temp,Bnorm,clogab,pdata->n_species,
		       Dparab,DXb,Kb,nub);

    *nu = 0;
    for(j = 0; j < pdata->n_species; j++) {
        *nu += nub[j];
    }


}

/**
 * @brief Evaluates collision coefficients in gc picture
 *
 * Finds the rho coordinate first and uses it to evaluate plasma parameters
 * that are then used to evaluate Coulomb logarithm and collision coefficients.
 * 
 * The coefficients are returned in arrays whose format is D_is = D[i*s] where
 * i is the particle SIMD position and s is species (maximum is MAX_SPECIES
 * defined in ascot5.h).
 *
 * Operations are performed simultaneously for NSIMD markers.
 *
 * @param p pointer to SIMD_gc struct containing the markers
 * @param Bdata pointer to magnetic field data
 * @param pdata pointer to plasma data
 * @param clogab pointer to array storing the evaluated Coulomb logarithms
 * @param Dpara pointer to array the evaluated storing parallel diffusion coefficient [m^2/s^3]
 * @param DX pointer to array storing the classical diffusion coefficient [m^2/s]
 * @param K pointer to array storing the evaluated friction coefficient [m/s^2]
 * @param nu pointer to array storing the evaluated pitch collision frequency [1/s]
 * @param dQ pointer to array storing the evaluated derivative dQ/dv [1/s]
 * @param dDpara pointer to array storing the evaluated derivative dDpara/dv [m/s^2]
 *
 */
void mccc_update_gc(particle_simd_gc* p, B_field_data* Bdata, plasma_1d_data* pdata, 
		    real* clogab, real* Dpara, real* DX, real* K, real* nu, real* dQ, real* dDpara){
    int i;
    #pragma omp simd
    for(i = 0; i < NSIMD; i++) {
        if(p->running[i]) {

	    /* Update background data */
	    real B[3];
	    real xi;

	    B[0] = p->B_r[i];
	    B[1] = p->B_phi[i];
	    B[2] = p->B_z[i];

	    real temp[MAX_SPECIES];
	    real dens[MAX_SPECIES];

	    // Electron and ion temperature
	    temp[0] = plasma_1d_eval_temp(p->rho[i], 0, pdata)*CONST_KB;
	    temp[1] = plasma_1d_eval_temp(p->rho[i], 1, pdata)*CONST_KB;

	    // Electron density
	    dens[0] = plasma_1d_eval_dens(p->rho[i], 0, pdata);

	    // Ion densities (and temperatures)
	    int j;
	    for(j = 1; j < pdata->n_species; j++) {
		dens[j] = plasma_1d_eval_dens(p->rho[i], j, pdata);
		temp[j] = temp[1];
	    }

	    /* Evaluate coefficients */
	    real Bnorm = math_norm(B);
	    real t = 2*p->mu[i]*Bnorm*p->mass[i];
	    real va = sqrt(p->vpar[i]*p->vpar[i] + t*t);
	    xi = p->vpar[i]/va;
					        
	    mccc_coefs_clog(p->mass[i],p->charge[i],va,pdata->mass,pdata->charge,dens,temp,&clogab[i*MAX_SPECIES],pdata->n_species);
	    mccc_coefs_gcadaptive(p->mass[i],p->charge[i],va,xi,pdata->mass,pdata->charge,dens,temp,Bnorm,&clogab[i*MAX_SPECIES],pdata->n_species,
				  &Dpara[i*MAX_SPECIES],&DX[i*MAX_SPECIES],&K[i*MAX_SPECIES],&nu[i*MAX_SPECIES],&dQ[i*MAX_SPECIES],&dDpara[i*MAX_SPECIES]);

	}
    }

}

/**
 * @brief Evaluates collisions in fo picture with fixed time-step
 *
 * This function first evaluates collision coefficients (see mccc_update_fo)
 * and then evaluates collisions using Euler-Maruyama method and updates
 * marker state.
 *
 * Operations are performed simultaneously for NSIMD markers.
 *
 * @param p pointer to SIMD_fo struct containing the markers
 * @param Bdata pointer to magnetic field data
 * @param pdata pointer to plasma data
 * @param h pointer to time step values [s]
 * @param err pointer to error flags
 */
void mccc_step_fo_fixed(particle_simd_fo* p, B_field_data* Bdata, plasma_1d_data* pdata, real* h,  int* err){
    int i;
    real rnd[3*NSIMD];
    mccc_wiener_boxmuller(rnd, 3*NSIMD);

    #pragma omp simd
    for(i = 0; i < NSIMD; i++) {
        if(p->running[i]) {

	    /* Update background data */
	    real temp[MAX_SPECIES];
	    real dens[MAX_SPECIES];
	    
	    plasma_1d_eval_densandtemp(p->rho[i], pdata, dens, temp);
	    int j;
	    for(j = 0; j < pdata->n_species; j++) {
		temp[j] = temp[j]*CONST_KB;
	    }
	    
	    /* Evaluate coefficients */
	    real va = sqrt(p->rdot[i]*p->rdot[i] + (p->r[i]*p->phidot[i])*(p->r[i]*p->phidot[i]) + p->zdot[i]*p->zdot[i]);

	    real clogab[MAX_SPECIES];
	    real Fb[MAX_SPECIES];
	    real Dparab[MAX_SPECIES];
	    real Dperpb[MAX_SPECIES];
	    real Kb[MAX_SPECIES];
	    real nub[MAX_SPECIES];
	    mccc_coefs_clog(p->mass[i],p->charge[i],va,pdata->mass,pdata->charge,dens,temp,clogab,pdata->n_species);
	    mccc_coefs_fo(p->mass[i],p->charge[i],va,pdata->mass,pdata->charge,dens,temp,clogab,pdata->n_species,
			  Fb,Dparab,Dperpb,Kb,nub);

	    
	    real vin[3];
	    real vout[3];
			
	    real F = 0;
	    real Dpara = 0;
	    real Dperp = 0;
	    for(j=0; j<pdata->n_species; j=j+1){
		F = F + Fb[j];
		Dpara = Dpara + Dparab[j];
		Dperp = Dperp + Dperpb[j];
	    }

	    /* Evaluate collisions */		
	    vin[0] = p->rdot[i] * cos(p->phi[i]) - (p->phidot[i]*p->r[i]) * sin(p->phi[i]);
	    vin[1] = p->rdot[i] * sin(p->phi[i]) + (p->phidot[i]*p->r[i]) * cos(p->phi[i]);
	    vin[2] = p->zdot[i];
			
	    mccc_push_foEM(F,Dpara,Dperp,h[i],&rnd[i*3],vin,vout,&err[i]);
			
	    /* Update particle */
	    #if A5_CCOL_NOENERGY
		real vnorm = va/sqrt(vout[0]*vout[0] + vout[1]*vout[1] + vout[2]*vout[2]);
	        vout[0] *= vnorm;
		vout[1] *= vnorm;
		vout[2] *= vnorm;
	    #endif
	    #if A5_CCOL_NOPITCH
		real vnorm = sqrt(vout[0]*vout[0] + vout[1]*vout[1] + vout[2]*vout[2])/va;
		vout[0] = vin[0] * vnorm;
		vout[1] = vin[1] * vnorm;
		vout[2] = vin[2] * vnorm;
	    #endif
	    p->rdot[i] = vout[0] * cos(p->phi[i]) + vout[1] * sin(p->phi[i]);
	    p->phidot[i] = (-vout[0] * sin(p->phi[i]) + vout[1] * cos(p->phi[i]) ) / p->r[i];
	    p->zdot[i] = vout[2];

	}
    }
}

/**
 * @brief Evaluates collisions in gc picture with fixed time-step
 *
 * This function first evaluates collision coefficients (see mccc_update_gc)
 * and then evaluates collisions using Euler-Maruyama method and updates
 * marker state.
 *
 * Operations are performed simultaneously for NSIMD markers.
 *
 * @param p pointer to SIMD_gc struct containing the markers
 * @param Bdata pointer to magnetic field data
 * @param pdata pointer to plasma data
 * @param h pointer to time step values [s]
 * @param err pointer to error flags
 */
void mccc_step_gc_fixed(particle_simd_gc* p, B_field_data* Bdata, plasma_1d_data* pdata, real* h, int* err){
    int i;
    real rnd[5*NSIMD];
    mccc_wiener_boxmuller(rnd, 5*NSIMD);

    #pragma omp simd
    for(i = 0; i < NSIMD; i++) {
        if(p->running[i]) {
	    /* Update background data */
	    real B[3];
	    real xiin;

	    B[0] = p->B_r[i];
	    B[1] = p->B_phi[i];
	    B[2] = p->B_z[i];
		
	    real temp[MAX_SPECIES];
	    real dens[MAX_SPECIES];
		
	    int j;
	    plasma_1d_eval_densandtemp(p->rho[i], pdata, dens, temp);
	    for(j = 0; j < pdata->n_species; j++) {
		temp[j] = temp[j]*CONST_KB;
	    }
	        
	    /* Evaluate coefficients */
	    real Bnorm = math_norm(B);
	    
	    real tmp = 2*p->mu[i]*Bnorm/p->mass[i];
	    real vin = sqrt(p->vpar[i]*p->vpar[i] + tmp);
	    xiin = p->vpar[i]/vin;
	
	    real clogab[MAX_SPECIES];
	    real Dparab[MAX_SPECIES];
	    real Kb[MAX_SPECIES];
	    real nub[MAX_SPECIES];
	    real DXb[MAX_SPECIES];
	    mccc_coefs_clog(p->mass[i],p->charge[i],vin,pdata->mass,pdata->charge,dens,temp,clogab,pdata->n_species);
	    mccc_coefs_gcfixed(p->mass[i],p->charge[i],vin,xiin,pdata->mass,pdata->charge,dens,temp,Bnorm,clogab,pdata->n_species,
			       Dparab,DXb,Kb,nub);

	    real phi0 = p->phi[i];
	    real R0   = p->r[i];
	    real z0   = p->z[i];

	    real xiout;
		
	    real vout;
	    real Xin[3];
	    real Xout[3];
	    real cutoff = 0.1*sqrt(temp[0]/p->mass[i]);
			
	    real Dpara = 0;
	    real K = 0;
	    real nu = 0;
	    real DX = 0;
	    for(j=0; j<pdata->n_species; j=j+1){				
		Dpara = Dpara + Dparab[j];
		K = K + Kb[j];
		nu = nu + nub[j];
		DX = DX + DXb[j];
	    }	        
		        
	    xiin = p->vpar[i]/vin;
	    Xin[0] = p->r[i]*cos(p->phi[i]);
	    Xin[1] = p->r[i]*sin(p->phi[i]);
	    Xin[2] = p->z[i];
		
	    
	    /* Evaluate collisions */
	    mccc_push_gcEM(K,nu,Dpara,DX,B,h[i],&rnd[i*5], 
			   vin,&vout,xiin,&xiout,Xin,Xout,cutoff,&err[i]);
		    
	    /* Update particle */
	    #if A5_CCOL_NOENERGY
	        vout = vin;
	    #endif
	    #if A5_CCOL_NOPITCH
	        xiout = xiin;
	    #endif
	    #if A5_CCOL_NOGCDIFF
	        Xout[0] = Xin[0];
		Xout[1] = Xin[1];
		Xout[2] = Xin[2];
	    #endif
	    
	    p->r[i] = sqrt(Xout[0]*Xout[0] + Xout[1]*Xout[1]);
	    p->z[i] = Xout[2];

	    /* Evaluate phi and pol angles so that they are cumulative */
	    real axis_r = B_field_get_axis_r(Bdata);
	    real axis_z = B_field_get_axis_z(Bdata);
	    p->pol[i] += atan2( (R0-axis_r) * (p->z[i]-axis_z) - (z0-axis_z) * (p->r[i]-axis_r), 
				(R0-axis_r) * (p->r[i]-axis_r) + (z0-axis_z) * (p->z[i]-axis_z) );
	    real tphi = fmod(phi0 , CONST_2PI );
	    if(tphi < 0){tphi = CONST_2PI+tphi;}
	    tphi = fmod(atan2(Xout[1],Xout[0])+CONST_2PI,CONST_2PI) -  tphi;
	    p->phi[i] = phi0 + tphi;

	    /* Evaluate magnetic field (and gradient) and rho at new position */
	    real B_dB[12];
	    B_field_eval_B_dB(B_dB, p->r[i], p->phi[i], p->z[i], Bdata);
	    p->B_r[i]        = B_dB[0];
	    p->B_r_dr[i]     = B_dB[1];
	    p->B_r_dphi[i]   = B_dB[2];
	    p->B_r_dz[i]     = B_dB[3];

	    p->B_phi[i]      = B_dB[4];
	    p->B_phi_dr[i]   = B_dB[5];
	    p->B_phi_dphi[i] = B_dB[6];
	    p->B_phi_dz[i]   = B_dB[7];

	    p->B_z[i]        = B_dB[8];
	    p->B_z_dr[i]     = B_dB[9];
	    p->B_z_dphi[i]   = B_dB[10];
	    p->B_z_dz[i]     = B_dB[11];

	    real psi[1];
	    real rho[1];
	    B_field_eval_psi(psi, p->r[i], p->phi[i], p->z[i], Bdata);
	    B_field_eval_rho(rho, psi[0], Bdata);
	    p->rho[i] = rho[0];

	    Bnorm = math_normc(B_dB[0], B_dB[4], B_dB[8]);
	    p->mu[i] = (1-xiout*xiout)*p->mass[i]*vout*vout/(2*Bnorm);
	    p->vpar[i] = vout*xiout;
	}
    }
}

/**
 * @brief Evaluates collisions in gc picture with adaptive time-step
 *
 * This function first evaluates collision coefficients (see mccc_update_gc)
 * and then evaluates collisions using Milstein method and updates
 * marker state irrespective whether time-step was accepted. Returns suggestion
 * for the next time-step which has minus sign if the time-step was rejected.
 *
 * Operations are performed simultaneously for NSIMD markers.
 *
 * @param p pointer to SIMD_gc struct containing the markers
 * @param Bdata pointer to magnetic field data
 * @param pdata pointer to plasma data
 * @param hin pointer to values for current time-step [s]
 * @param hout pointer to values for next time-step [s]
 * @param w NSIMD array of pointers to Wiener structs 
 * @param tol integration error tolerance
 * @param err pointer to error flags
 *
 * @todo There is no need to check for error when collision frequency is low and other processes determine adaptive time-step
 */
void mccc_step_gc_adaptive(particle_simd_gc* p, B_field_data* Bdata, plasma_1d_data* pdata, real* hin, real* hout, mccc_wienarr** w, real tol, int* err){
    int i;
    real rand5[5*NSIMD];
    mccc_wiener_boxmuller(rand5, 5*NSIMD);

    /* Error estimates */
    real kappa_k[NSIMD], kappa_d0[NSIMD], kappa_d1[NSIMD];
    real dWopt0[NSIMD], dWopt1[NSIMD], alpha[NSIMD];
    int tindex[NSIMD];

    #pragma omp simd
    for(i = 0; i < NSIMD; i++) {
	if(p->running[i]) {
	    /* Update background data */
	    real B[3];
	    real xiin;

	    B[0] = p->B_r[i];
	    B[1] = p->B_phi[i];
	    B[2] = p->B_z[i];
		
	    real temp[MAX_SPECIES];
	    real dens[MAX_SPECIES];
		
	    int j;
	    plasma_1d_eval_densandtemp(p->rho[i], pdata, dens, temp);
	    for(j = 0; j < pdata->n_species; j++) {
		temp[j] = temp[j]*CONST_KB;
	    }
	        
	    /* Evaluate coefficients */
	    real Bnorm = math_norm(B);
	    real tmp = 2*p->mu[i]*Bnorm/p->mass[i];
	    real vin = sqrt(p->vpar[i]*p->vpar[i] + tmp);
	    xiin = p->vpar[i]/vin;
	
	    real clogab[MAX_SPECIES];
	    real dQb[MAX_SPECIES];
	    real dDparab[MAX_SPECIES];
	    real Dparab[MAX_SPECIES];
	    real Kb[MAX_SPECIES];
	    real nub[MAX_SPECIES];
	    real DXb[MAX_SPECIES];
	    mccc_coefs_clog(p->mass[i],p->charge[i],vin,pdata->mass,pdata->charge,dens,temp,clogab,pdata->n_species);
	    mccc_coefs_gcadaptive(p->mass[i],p->charge[i],vin,xiin,pdata->mass,pdata->charge,dens,temp,Bnorm,clogab,pdata->n_species,
				  Dparab,DXb,Kb,nub,dQb,dDparab);
	        
	    real dW[5];
	    real xiout;
		
	    real vout;
	    real Xin[3];
	    real Xout[3];
	    real cutoff = 0.1*sqrt(temp[0]/p->mass[i]);

	    real phi0 = p->phi[i];
	    real R0   = p->r[i];
	    real z0   = p->z[i];
			
	    real Dpara = 0;
	    real K = 0;
	    real nu = 0;
	    real DX = 0;
	    real dQ = 0;
	    real dDpara = 0;
	    for(j=0; j<pdata->n_species; j=j+1){				
		Dpara = Dpara + Dparab[j];
		K = K + Kb[j];
		nu = nu + nub[j];
		DX = DX + DXb[j];
		dQ = dQ + dQb[j];
		dDpara = dDpara + dDparab[j];
	    }
		
	    /* Evaluate collisions */
	    real t = w[i]->time[0];
	    mccc_wiener_generate(w[i], t+hin[i], &tindex[i], &rand5[i*MCCC_NDIM], &err[i]);
	    dW[0] = w[i]->wiener[tindex[i]*MCCC_NDIM + 0] - w[i]->wiener[0];
	    dW[1] = w[i]->wiener[tindex[i]*MCCC_NDIM + 1] - w[i]->wiener[1];
	    dW[2] = w[i]->wiener[tindex[i]*MCCC_NDIM + 2] - w[i]->wiener[2];
	    dW[3] = w[i]->wiener[tindex[i]*MCCC_NDIM + 3] - w[i]->wiener[3];
	    dW[4] = w[i]->wiener[tindex[i]*MCCC_NDIM + 4] - w[i]->wiener[4];
	    
	    xiin = p->vpar[i]/vin;
	    Xin[0] = p->r[i]*cos(p->phi[i]);
	    Xin[1] = p->r[i]*sin(p->phi[i]);
	    Xin[2] = p->z[i];
			
	    mccc_push_gcMI(K,nu,Dpara,DX,B,hin[i],dW,dQ,dDpara, 
			   vin,&vout,xiin,&xiout,Xin,Xout,cutoff,tol, 
			   &kappa_k[i], &kappa_d0[i], &kappa_d1[i],&err[i]);

	    /* Needed for finding the next time step (for the old method, see end of function)*/
	    /*dWopt0[i] = 0.9*fabs(dW[3])*pow(kappa_d0[i],-1.0/3);
	    dWopt1[i] = 0.9*fabs(dW[4])*pow(kappa_d1[i],-1.0/3);
	    alpha[i]  = fabs(dW[3]);
	    if(alpha[i] < fabs(dW[4])) {
		alpha[i] = fabs(dW[4]);
	    }
	    alpha[i] = alpha[i]/sqrt(hin[i]);*/

	    /* Update particle */
	    #if A5_CCOL_NOENERGY
	        vout = vin;
	    #endif
	    #if A5_CCOL_NOPITCH
	        xiout = xiin;
	    #endif
	    #if A5_CCOL_NOGCDIFF
	        Xout[0] = Xin[0];
		Xout[1] = Xin[1];
		Xout[2] = Xin[2];
	    #endif
	    p->mu[i] = (1-xiout*xiout)*p->mass[i]*vout*vout/(2*Bnorm);
	    p->vpar[i] = vout*xiout;
	    p->r[i] = sqrt(Xout[0]*Xout[0] + Xout[1]*Xout[1]);
	    p->z[i] = Xout[2];

	    /* Evaluate phi and pol angles so that they are cumulative */
	    real axis_r = B_field_get_axis_r(Bdata);
	    real axis_z = B_field_get_axis_z(Bdata);
	    p->pol[i] += atan2( (R0-axis_r) * (p->z[i]-axis_z) - (z0-axis_z) * (p->r[i]-axis_r), 
				(R0-axis_r) * (p->r[i]-axis_r) + (z0-axis_z) * (p->z[i]-axis_z) );
	    real tphi = fmod(phi0 , CONST_2PI );
	    if(tphi < 0){tphi = CONST_2PI+tphi;}
	    tphi = fmod(atan2(Xout[1],Xout[0])+CONST_2PI,CONST_2PI) -  tphi;
	    p->phi[i] = phi0 + tphi;

	    /* Evaluate magnetic field (and gradient) at new position */
	    real B_dB[12];
	    B_field_eval_B_dB(B_dB, p->r[i], p->phi[i], p->z[i], Bdata);
	    p->B_r[i]        = B_dB[0];
	    p->B_r_dr[i]     = B_dB[1];
	    p->B_r_dphi[i]   = B_dB[2];
	    p->B_r_dz[i]     = B_dB[3];

	    p->B_phi[i]      = B_dB[4];
	    p->B_phi_dr[i]   = B_dB[5];
	    p->B_phi_dphi[i] = B_dB[6];
	    p->B_phi_dz[i]   = B_dB[7];

	    p->B_z[i]        = B_dB[8];
	    p->B_z_dr[i]     = B_dB[9];
	    p->B_z_dphi[i]   = B_dB[10];
	    p->B_z_dz[i]     = B_dB[11];

	    real psi[1];
	    real rho[1];
	    B_field_eval_psi(psi, p->r[i], p->phi[i], p->z[i], Bdata);
	    B_field_eval_rho(rho, psi[0], Bdata);
	    p->rho[i] = rho[0];

	    int rejected = 0;
	    if(kappa_k[i] > 1 || kappa_d0[i] > 1 || kappa_d1[i] > 1) {
		rejected = 1;
		tindex[i]=0;
	    }

	    /* Needed for finding the next time step */
	    if(kappa_k[i] >= kappa_d0[i] && kappa_k[i] >= kappa_d1[i]) {
		hout[i] = 0.8*hin[i]/sqrt(kappa_k[i]);
	    }
	    else if(kappa_d0[i] >= kappa_k[i] && kappa_d0[i] >= kappa_d1[i]) {
		hout[i] = pow(0.9*fabs(dW[3])*pow(kappa_d0[i],-1.0/3),2.0);
	    }
	    else {
		hout[i] = pow(0.9*fabs(dW[4])*pow(kappa_d1[i],-1.0/3),2.0);
	    }

	    /* Negative value indicates time step was rejected*/
	    if(rejected){
		hout[i] = -hout[i];
	    }
	    
	}
    }

    return; // More accurate but probably less efficient method below
    /* Choose next time step (This loop can be vectorized if there is a 
       suitable tool for drawing random numbers) */
    for(i = 0; i < NSIMD; i++) {
	if(p->running[i]) {
	    real t = w[i]->time[0];

	    /* Check whether time step was accepted and find value for the next time-step */
	    int rejected = 0;
	    if(kappa_k[i] > 1 || kappa_d0[i] > 1 || kappa_d1[i] > 1) {
		rejected = 1;
		tindex[i]=0;
	    }
		
	    /* Different time step estimates are used depending which error estimate dominates
	     * This scheme automatically takes care of time step reduction (increase) when 
	     * time step is rejected (accepted) */
	    int ki, kmax;
	    int windex;
	    real dW[2];

	    if(kappa_k[i] > kappa_d0[i] || kappa_k[i] > kappa_d1[i]) {
		real dti = 0.8*hin[i]/sqrt(kappa_k[i]);
		if(1.5*hin[i] < dti){dti = 1.5*hin[i];}
		kmax = 4;
		for(ki=1; ki < kmax; ki=ki+1){
		    mccc_wiener_boxmuller(&rand5[i*MCCC_NDIM], MCCC_NDIM);
		    mccc_wiener_generate(w[i], t+ki*dti/3, &windex, &rand5[i*MCCC_NDIM], &err[i]);
		    dW[0] = fabs(w[i]->wiener[3 + windex*MCCC_NDIM] 
				 - w[i]->wiener[3 + tindex[i]*MCCC_NDIM]);
		    if(dW[0] > dWopt0[i]) {
			kmax = 0; // Exit loop
		    }
		    else {
			dW[1] = fabs(w[i]->wiener[4 + windex*MCCC_NDIM] 
				     - w[i]->wiener[4 + tindex[i]*MCCC_NDIM]);
			if(dW[1] > dWopt1[i]) {
			    kmax = 0; // Exit loop
			}
		    }
		}
		if(ki == 1){
		    hout[i] = (dti/3);
		}
		else{
		    hout[i] = (ki-1)*(dti/3);
		}
	    }
	    else{
		kmax = 6;
		if (rejected) {
		    kmax = 2;
		}
		else if (alpha[i] > 2) {
		    kmax = 4;
		}

		for(ki=1; ki < kmax; ki=ki+1){
		    mccc_wiener_boxmuller(&rand5[i*MCCC_NDIM], MCCC_NDIM);
		    mccc_wiener_generate(w[i], t+ki*hin[i]/3, &windex, &rand5[i*MCCC_NDIM], &err[i]);
		    dW[0] = abs(w[i]->wiener[3 + windex*MCCC_NDIM] - w[i]->wiener[3 + tindex[i]*MCCC_NDIM]);
		    if(dW[0] > dWopt0[i]) {
			kmax = 0; // Exit loop
		    }
		    else{
			dW[1] = abs(w[i]->wiener[4 + windex*MCCC_NDIM] - w[i]->wiener[4 + tindex[i]*MCCC_NDIM]);
			if(dW[1] > dWopt1[i]) {
			    kmax = 0; // Exit loop
			}
		    }
		    
		}
		if(ki == 1){
		    hout[i] = (hin[i]/3);
		}
		else{
		    hout[i] = (ki-1)*(hin[i]/3);
		}
	    }
		
	    /* Negative value indicates time step was rejected*/
	    if(rejected){
		hout[i] = -hout[i];
	    }
	}
    }
}

/**
 * @brief Prints error description
 *
 * @param err error flag
 */
void mccc_printerror(int err){

    if(err == 0){
	return;
    }
    else if(err == MCCC_WIENER_EXCEEDEDCAPACITY){
	printf("Error: Number of slots in Wiener array exceeded.\n");
    }
    else if(err == MCCC_WIENER_NOASSOCIATEDPROCESS){
	printf("Error: No associated process found.\n");
    }
    else if(err == MCCC_PUSH_ISNAN){
	printf("Error: Collision operator yields NaN or Inf.\n");
    }
    else{
	printf("Error: Unknown error\n");
    }    


}
