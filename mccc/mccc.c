#define _XOPEN_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include "../ascot5.h"
#include "../particle.h"
#include "../math.h"
#include "mccc.h"
#include "mccc_wiener.h"
#include "mccc_push.h"
#include "mccc_coefs.h"

void mccc_init(){

}

// TODO figure out how to present coefficients which in theory are NSIMD x MAXSPECIES arrays
void mccc_update_fo(particle_simd_fo* p, B_field_data* Bdata, plasma_1d_data* pdata, 
		    real* clogab, real* F, real* Dpara, real* Dperp, real* K, real* nu){

    int i;
    #pragma omp simd
    for(i = 0; i < NSIMD; i++) {
        if(p->running[i]) {
	    // Update background data
	    real* psi;
	    real* rho;

	    B_field_eval_psi(psi, p->r[i], p->phi[i], p->z[i], Bdata);
	    B_field_eval_rho(rho, psi[0], Bdata);

	    real temp[MAX_SPECIES];
	    real dens[MAX_SPECIES];

	    int j;
	    for(j = 0; j < pdata->n_species; j++) {
		temp[j] = plasma_1d_eval_temp(rho[0], j, pdata);
		dens[j] = plasma_1d_eval_dens(rho[0], j, pdata);
	    }

	    real va = sqrt(p->rdot[i]*p->rdot[i] + (p->r[i]*p->phidot[i])*(p->r[i]*p->phidot[i]) + p->zdot[i]*p->zdot[i]);
					        
	    mccc_coefs_clog(p->mass[i],p->charge[i],va,pdata->mass,pdata->charge,dens,temp,&clogab[i*MAX_SPECIES],pdata->n_species);
	    mccc_coefs_fo(p->mass[i],p->charge[i],va,pdata->mass,pdata->charge,dens,temp,&clogab[i*MAX_SPECIES],pdata->n_species,
			  &F[i*MAX_SPECIES],&Dpara[i*MAX_SPECIES],&Dperp[i*MAX_SPECIES],&K[i*MAX_SPECIES],&nu[i*MAX_SPECIES]);


	}
    }

}

void mccc_update_gc(particle_simd_gc* p, B_field_data* Bdata, plasma_1d_data* pdata, 
		    real* clogab, real* Dpara, real* DX, real* K, real* nu, real* dQ, real* dDpara){
    int i;
    #pragma omp simd
    for(i = 0; i < NSIMD; i++) {
        if(p->running[i]) {
	    // Update background data
	    real* psi;
	    real* rho;
	    real B[3];
	    real xi;

	    B_field_eval_B(B, p->r[i], p->phi[i], p->z[i], Bdata);
	    B_field_eval_psi(psi, p->r[i], p->phi[i], p->z[i], Bdata);
	    B_field_eval_rho(rho, psi[0], Bdata);

	    real temp[MAX_SPECIES];
	    real dens[MAX_SPECIES];

	    int j;
	    for(j = 0; j < pdata->n_species; j++) {
		temp[j] = plasma_1d_eval_temp(rho[0], j, pdata);
		dens[j] = plasma_1d_eval_dens(rho[0], j, pdata);
	    }

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

void mccc_step_fo_fixed(particle_simd_fo* p, B_field_data* Bdata, plasma_1d_data* pdata, real* h,  int* err){
    int i;
    #pragma omp simd
    for(i = 0; i < NSIMD; i++) {
        if(p->running[i]) {
	    // Update background data
	    real psi[1];
	    real rho[1];

	    B_field_eval_psi(psi, p->r[i], p->phi[i], p->z[i], Bdata);
	    B_field_eval_rho(rho, psi[0], Bdata);

	    real temp[MAX_SPECIES];
	    real dens[MAX_SPECIES];

	    int j;
	    for(j = 0; j < pdata->n_species; j++) {
		temp[j] = plasma_1d_eval_temp(rho[0], j, pdata)*CONST_KB;
		dens[j] = plasma_1d_eval_dens(rho[0], j, pdata);
	    }

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

	    real rnd[3];
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

	    rnd[0] = 1-2*drand48();
	    rnd[1] = 1-2*drand48();
	    rnd[2] = 1-2*drand48();
			
	    vin[0] = p->rdot[i] * cos(p->phi[i]) - (p->phidot[i]*p->r[i]) * sin(p->phi[i]);
	    vin[1] = p->rdot[i] * sin(p->phi[i]) + (p->phidot[i]*p->r[i]) * cos(p->phi[i]);
	    vin[2] = p->zdot[i];
			
	    mccc_push_foEM(F,Dpara,Dperp,h[i],rnd,vin,vout,err);
			
	    p->rdot[i] = vout[0] * cos(p->phi[i]) + vout[1] * sin(p->phi[i]);
	    p->phidot[i] = (-vout[0] * sin(p->phi[i]) + vout[1] * cos(p->phi[i]) ) / p->r[i];
	    p->zdot[i] = vout[2];
	}
    }
}

void mccc_step_gc_fixed(particle_simd_gc* p, B_field_data* Bdata, plasma_1d_data* pdata, real* h, int* err){
    int i;
    #pragma omp simd
    for(i = 0; i < NSIMD; i++) {
        if(p->running[i]) {
	    // Update background data
	    real psi[1];
	    real rho[1];
	    real B[3];
	    real xiin;

	    B_field_eval_B(B, p->r[i], p->phi[i], p->z[i], Bdata);
	    B_field_eval_psi(psi, p->r[i], p->phi[i], p->z[i], Bdata);
	    B_field_eval_rho(rho, psi[0], Bdata);
		
	    real temp[MAX_SPECIES];
	    real dens[MAX_SPECIES];
		
	    int j;
	    for(j = 0; j < pdata->n_species; j++) {
		temp[j] = plasma_1d_eval_temp(rho[0], j, pdata)*CONST_KB;
		dens[j] = plasma_1d_eval_dens(rho[0], j, pdata);
	    }
	        
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
		
	    int tindex;
	    real dW[5];
	    real xiout;
		
	    real vout;
	    real Xin[3];
	    real Xout[3];
	    real cutoff = 0; //TODO give me a real value!
			
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
		
	    real rnd[5];
	    rnd[0] = 1-2*drand48();
	    rnd[1] = 1-2*drand48();
	    rnd[2] = 1-2*drand48();
	    rnd[3] = 1-2*drand48();
	    rnd[4] = 1-2*drand48();
	    
	    mccc_push_gcEM(K,nu,Dpara,DX,B,h[i],rnd, 
			   vin,&vout,xiin,&xiout,Xin,Xout,cutoff,&err[i]);
		        
	    p->mu[i] = (1-xiout*xiout)*p->mass[i]*vout*vout/(2*Bnorm);
	    p->vpar[i] = vout*xiout;
	    p->r[i] = sqrt(Xout[0]*Xout[0] + Xout[1]*Xout[1]);
	    p->phi[i] = atan2(Xout[1],Xout[0]);
	    p->z[i] = Xout[2];
	}
    }
}

void mccc_step_gc_adaptive(particle_simd_gc* p, B_field_data* Bdata, plasma_1d_data* pdata, real* hin, real* hout, mccc_wienarr** w, real tol, int* err){
	int i;
#pragma omp simd
	for(i = 0; i < NSIMD; i++) {
	    if(p->running[i]) {
		// Update background data
		real psi[1];
		real rho[1];
		real B[3];
		real xiin;

		B_field_eval_B(B, p->r[i], p->phi[i], p->z[i], Bdata);
		
		B_field_eval_psi(psi, p->r[i], p->phi[i], p->z[i], Bdata);
		
		B_field_eval_rho(rho, psi[0], Bdata);
		
		real temp[MAX_SPECIES];
		real dens[MAX_SPECIES];
		
		int j;
		for(j = 0; j < pdata->n_species; j++) {
		    temp[j] = plasma_1d_eval_temp(rho[0], j, pdata)*CONST_KB;
		    dens[j] = plasma_1d_eval_dens(rho[0], j, pdata);
		}
	        
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
		
		int tindex;
		real dW[5];
		real xiout;
		
		real vout;
		real Xin[3];
		real Xout[3];
		real cutoff = 0; //TODO give me a real value!
			
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
		
		real t = w[i]->time[0];
		mccc_wiener_generate(w[i], t+hin[i], &tindex, &err[i]);
		dW[0] = w[i]->wiener[tindex*5 + 0] - w[i]->wiener[0];
		dW[1] = w[i]->wiener[tindex*5 + 1] - w[i]->wiener[1];
		dW[2] = w[i]->wiener[tindex*5 + 2] - w[i]->wiener[2];
		dW[3] = w[i]->wiener[tindex*5 + 3] - w[i]->wiener[3];
		dW[4] = w[i]->wiener[tindex*5 + 4] - w[i]->wiener[4];
		        
		xiin = p->vpar[i]/vin;
		Xin[0] = p->r[i]*cos(p->phi[i]);
		Xin[1] = p->r[i]*sin(p->phi[i]);
		Xin[2] = p->z[i];
			
		
		real kappa_k, kappa_d[2];
		mccc_push_gcMI(K,nu,Dpara,DX,B,hin[i],dW,dQ,dDpara, 
			       vin,&vout,xiin,&xiout,Xin,Xout,cutoff,tol,&kappa_k,kappa_d,&err[i]);
		        
		p->mu[i] = (1-xiout*xiout)*p->mass[i]*vout*vout/(2*Bnorm);
		p->vpar[i] = vout*xiout;
		p->r[i] = sqrt(Xout[0]*Xout[0] + Xout[1]*Xout[1]);
		p->phi[i] = atan2(Xout[1],Xout[0]);
		p->z[i] = Xout[2];

		int rejected = 0;
		if(kappa_k > 1 || kappa_d[0] > 1 || kappa_d[1] > 1){rejected = 1;tindex=0;}
			
		
		
		// Different time step estimates are used depending which error estimate dominates
		// This scheme automatically takes care of time step reduction (increase) when time step is rejected (accepted)
		int ki, kmax;
		int windex;
		real dWopt[2];
		dWopt[0] = 0.9*fabs(dW[3])*pow(kappa_d[0],-1.0/3);
		dWopt[1] = 0.9*fabs(dW[4])*pow(kappa_d[1],-1.0/3);
		real alpha = fabs(dW[3]);
		if(alpha < fabs(dW[4])){alpha = fabs(dW[4]);}
		alpha = alpha/sqrt(hin[i]);

		if(kappa_k > kappa_d[0] || kappa_k > kappa_d[1]) {
		    real dti = 0.8*hin[i]/sqrt(kappa_k);
		    if(1.5*hin[i] < dti){dti = 1.5*hin[i];}
		    for(ki=1; ki < 4; ki=ki+1){
			mccc_wiener_generate(w[i], t+ki*dti/3, &windex, &err[i]);
			dW[3] = fabs(w[i]->wiener[3 + windex*5] - w[i]->wiener[3 + tindex*5]);
			if(dW[3] > dWopt[0]){break;}
			dW[4] = fabs(w[i]->wiener[4 + windex*5] - w[i]->wiener[4 + tindex*5]);
			if(dW[4] > dWopt[1]){break;}
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
		    else if (alpha > 2) {
			kmax = 4;
		    }

		    for(ki=1; ki < 4; ki=ki+1){
			mccc_wiener_generate(w[i], t+ki*hin[i]/3, &windex, &err[i]);
			dW[3] = abs(w[i]->wiener[3 + windex*w[i]->Ndim] - w[i]->wiener[3 + tindex*w[i]->Ndim]);
			if(dW[3] > dWopt[0]){exit;}
			dW[4] = abs(w[i]->wiener[4 + windex*w[i]->Ndim] - w[i]->wiener[4 + tindex*w[i]->Ndim]);
			if(dW[4] > dWopt[1]){exit;}
		    }
		    if(ki == 1){
			hout[i] = (hin[i]/3);
		    }
		    else{
			hout[i] = (ki-1)*(hin[i]/3);
		    }
		}
		
		if(rejected){hout[i] = -hout[i];}
		//printf("%g %g %d\n",hin[i],hout[i],ki);
	    }
	}
}

/** Prints error description if err != 0.
 *
 * input:
 *
 * int err -- error flag
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
