// -*- mode: C++; c-indent-level: 4; c-basic-offset: 4; indent-tabs-mode: nil; -*-

#include <RcppArmadillo.h>

// [[Rcpp::depends(RcppArmadillo)]]

#include <sys/time.h>
#include <iomanip>
#include <string>
#include <cmath>
#include <vector>
#include <iostream>
#include <algorithm>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <bitset>







Rcpp::IntegerVector rcpp_int_expand(arma::ivec& hapc, const int nSNPs);

arma::imat inflate_fhb(
    arma::imat& rhb,
    Rcpp::IntegerVector& haps_to_get,
    const int nSNPs
);


double print_times(
    double prev,
    int suppressOutput,
    std::string past_text,
    std::string next_text
);

Rcpp::NumericVector rcpp_nth_partial_sort(
    Rcpp::NumericVector x,
    int nth
);

arma::mat Rcpp_build_eMatDH(
    arma::imat& distinctHapsB,
    const arma::mat& gl,
    const int nGrids,
    const int nSNPs,
    const double ref_error,
    const double ref_one_minus_error,
    const bool add_zero_row = false
			    );


void rcpp_internal_make_eMatRead_t_using_binary(
    arma::mat & eMatRead_t,
    arma::imat& rhb,
    const int K,
    const int nSNPs,
    const Rcpp::NumericVector& u,
    const Rcpp::NumericMatrix& ps,
    const int nReads,
    const Rcpp::NumericVector& start,
    const Rcpp::NumericVector& end,
    const Rcpp::NumericVector& nr,
    const double ref_error,
    const int ceil_K_n,
    const int n
);


//' @export
// [[Rcpp::export]]
void Rcpp_haploid_reference_single_forward_version2(
    Rcpp::IntegerVector& gammaSmall_cols_to_get,
    const arma::mat& gl,
    arma::mat& alphaHat_t,
    arma::rowvec& c,
    const arma::mat& transMatRate_t,
    const arma::imat& rhb_t,
    arma::imat& hapMatcher,
    arma::mat& eMatDH,
    const int& nGrids,
    const int& nSNPs,
    const int& K,
    const bool& use_eMatDH,
    double ref_error,
    const bool only_store_alpha_at_gamma_small
) {
    double jump_prob, one_minus_jump_prob, not_jump_prob;
    int s, e, nSNPsLocal, i, iGrid, k;
    double ref_one_minus_error = 1 - ref_error;
    //const int n_which_hapMatcher_0 = which_hapMatcher_0.n_rows;
    //int hapMatcher_0_position = 0;
    //bool continue_hapMatcher_0 = true;
    double dR, dA, prob;
    double run_total = 0;
    arma::mat gl_local(2, 32);
    arma::icolvec dh_col(K);
    arma::colvec prob_col(K);
    arma::colvec eMatDH_col(K);
    arma::colvec alphaHat_t_col_prev(K);
    arma::colvec alphaHat_t_col(K);
    bool grid_has_variant;
    const double double_K = double(K);
    bool calculate_small_gamma_t_col;
    bool store_alpha_for_this_grid;
    //
    for(iGrid = 1; iGrid < nGrids; iGrid++) {
        jump_prob = transMatRate_t(1, iGrid - 1) / double_K;
        not_jump_prob = transMatRate_t(0, iGrid - 1);
        one_minus_jump_prob = (1 - jump_prob);
        s = 32 * iGrid; // 0-based here
        e = 32 * (iGrid + 1) - 1;
        if (e > (nSNPs - 1)) {
            e = nSNPs - 1;
        }
        nSNPsLocal = e - s + 1;
        grid_has_variant = false;        
        for(i = 0; i < nSNPsLocal; i++) {
            gl_local(0, i) = gl(0, i + s);
            gl_local(1, i) = gl(1, i + s);
            if ((gl_local(0, i) != 1) | (gl_local(1, i) != 1)) {
                grid_has_variant = true;
            }
        }
        if (iGrid == 1) {
            grid_has_variant = true; // make sure re-scaling kicks on properly
	    alphaHat_t_col = alphaHat_t.col(0); // initialize with previous value
        }
	store_alpha_for_this_grid = true;
	if (only_store_alpha_at_gamma_small) {
            if (gammaSmall_cols_to_get(iGrid) == 0) {
	      store_alpha_for_this_grid = false;
	    }
	}
        if (use_eMatDH) {
            //
            // yes use eMatDH
            //
            if (grid_has_variant) {
                dh_col = hapMatcher.col(iGrid);
                eMatDH_col = eMatDH.col(iGrid);
		run_total = 0;
                for(k = 0; k < K; k++) {
		    // if dh_col is 0 i.e. need to re-do this prob is 0 so we are OK
                    prob = eMatDH_col(dh_col(k));
                    if (dh_col(k) == 0) {
                        //
                        std::uint32_t tmp(rhb_t(k, iGrid));                
                        //
                        prob = 1;
                        for(int b = 0; b < nSNPsLocal; b++) {
                            dR = gl_local(0, b);
                            dA = gl_local(1, b);
                            if (tmp & (1<<b)) {
                                // alternate
                                prob *= (dR * ref_error + dA * ref_one_minus_error);
                            } else {
                                prob *= (dR * ref_one_minus_error + dA * ref_error);
                            }
                        }
                    }
		    alphaHat_t_col(k) = (jump_prob + not_jump_prob * alphaHat_t_col(k)) * prob;
                    run_total += alphaHat_t_col(k);
                }
                c(iGrid) = 1 / run_total;
		alphaHat_t_col *= c(iGrid); // argh
            } else {
	        alphaHat_t_col = (jump_prob + not_jump_prob * alphaHat_t_col);
	        c(iGrid) = 1;
            }
	    if (store_alpha_for_this_grid) {
	        alphaHat_t.col(iGrid) = alphaHat_t_col;
	    }
        } else {
            //
            // no to eMatDH
            //
            for(k = 0; k < K; k++) {            
                std::uint32_t tmp(rhb_t(k, iGrid));                
                //
                prob = 1;
                for(int b = 0; b < nSNPsLocal; b++) {
                    dR = gl_local(0, b);
                    dA = gl_local(1, b);
                    if (tmp & (1<<b)) {
                        // alternate
                        prob *= (dR * ref_error + dA * ref_one_minus_error);
                    } else {
                        prob *= (dR * ref_one_minus_error + dA * ref_error);
                    }
                }
                alphaHat_t(k, iGrid) = (jump_prob + one_minus_jump_prob * alphaHat_t(k, iGrid - 1)) * prob;
            }
            c(iGrid) = 1 / sum(alphaHat_t.col(iGrid));
            alphaHat_t.col(iGrid) = alphaHat_t.col(iGrid) * c(iGrid);
        }
        //
    }
    return;
}







//' @export
// [[Rcpp::export]]
void Rcpp_haploid_reference_single_backward_version2(
    arma::mat& alphaHat_t,
    arma::mat& betaHat_t,
    arma::mat& gamma_t,
    arma::mat& gammaSmall_t,
    Rcpp::IntegerVector& gammaSmall_cols_to_get,
    Rcpp::NumericVector& dosage,    
    const int& nGrids,
    const arma::mat& transMatRate_t,
    arma::mat& eMatDH,
    arma::imat& hapMatcher,
    const int& nSNPs,
    const int& K,
    const bool& use_eMatDH,
    const arma::imat& rhb_t,
    double ref_error,
    const arma::mat& gl,
    arma::rowvec& c,
    arma::mat& distinctHapsIE,
    bool return_betaHat_t,
    bool return_dosage,
    bool return_gamma_t,
    bool return_gammaSmall_t,
    const int nMaxDH
) {
    // 
    //
    double ref_one_minus_error = 1 - ref_error;    
    double jump_prob, one_minus_jump_prob, not_jump_prob;
    int b, s, e, nSNPsLocal, i, iGrid, k, dh;
    const double double_K = double(K);
    arma::mat gl_local(2, 32);
    double dR, dA, prob, val, gk;
    arma::colvec ematcol(K);
    iGrid = nGrids - 1;
    arma::colvec betaHat_t_col(K);
    arma::colvec gamma_t_col(K);
    bool calculate_small_gamma_t_col, grid_has_variant;
    arma::vec matched_gammas(nMaxDH + 1);
    arma::colvec e_times_b(K);
    arma::vec dosageL(32);
    dosageL.fill(0);
    arma::icolvec dh_col(K);
    arma::colvec eMatDH_col(nMaxDH + 1);
    arma::colvec prob_col(K);
    double sum_e_times_b = 0;
    double prev_val = -1; // argh          
    //
    //
    for(iGrid = nGrids - 1; iGrid >= 0; --iGrid) {
        if (iGrid == (nGrids - 1)) {
            betaHat_t_col.fill(1);
        } else {
            jump_prob = transMatRate_t(1, iGrid) / double_K;
            one_minus_jump_prob = (1 - jump_prob);
            not_jump_prob = transMatRate_t(0, iGrid);
            s = 32 * (iGrid + 1); // 0-based here
            e = 32 * (iGrid + 1 + 1) - 1;
            if (e > (nSNPs - 1)) {
                e = nSNPs - 1;
            }
	    grid_has_variant = false;
            nSNPsLocal = e - s + 1;
            for(i = 0; i < nSNPsLocal; i++) {
                gl_local(0, i) = gl(0, i + s);
                gl_local(1, i) = gl(1, i + s);
		if ((gl_local(0, i) != 1) | (gl_local(1, i) != 1)) {
		  grid_has_variant = true;
		}
            }
	    //
	    // use eMatDH
	    //
	    if (use_eMatDH) {
	        if (grid_has_variant) {
		    dh_col = hapMatcher.col(iGrid + 1);		  
		    eMatDH_col = eMatDH.col(iGrid + 1);
		    sum_e_times_b = 0;
		    for(k = 0; k < K; k++) {
                        prob = eMatDH_col(dh_col(k));
			if (dh_col(k) == 0) {
			    //
			    std::uint32_t tmp(rhb_t(k, iGrid + 1));                
			    //
			    prob = 1;
			    for(int b = 0; b < nSNPsLocal; b++) {
			      dR = gl_local(0, b);
			      dA = gl_local(1, b);
			      if (tmp & (1<<b)) {
                                // alternate
                                prob *= (dR * ref_error + dA * ref_one_minus_error);
			      } else {
                                prob *= (dR * ref_one_minus_error + dA * ref_error);
			      }
			    }
			    //prob_col(k) = prob;
			}
			//ematcol(k) = prob;		    
			e_times_b(k) = betaHat_t_col(k) * prob;
			sum_e_times_b += e_times_b(k);
		    }
		    val = jump_prob * sum_e_times_b;
		    betaHat_t_col = ((not_jump_prob) * e_times_b + val);
		    prev_val = -1;
		} else {
		    // so here prob is uniformly 1, can then super simplify
		    // also, "val", can work out math, is the same if two consecutive no variants, so avoid another fullsum
		    if (prev_val == -1) {
		         val = jump_prob * sum(betaHat_t_col);
		    } else {
		     	double J1 = (transMatRate_t(1, iGrid + 1) / double_K);
		     	double N1 = transMatRate_t(0, iGrid + 1);
		     	val = prev_val * (N1 * jump_prob / J1 + double_K * jump_prob);
		    }
		    betaHat_t_col = ((not_jump_prob) * betaHat_t_col + val);
		    prev_val = val * c(iGrid); // c should not be relevant though, should be 1, has no variants
                }
	    } else {
	        for(k = 0; k < K; k++) {
		    //
		  std::uint32_t tmp(rhb_t(k, iGrid + 1));
		  //
		  prob = 1;
		  for(b = 0; b < nSNPsLocal; b++) {
		      dR = gl_local(0, b);
		      dA = gl_local(1, b);
		      if (tmp & (1<<b)) {
			// alternate
			prob *= (dR * ref_error + dA * ref_one_minus_error);
		      } else {
			prob *= (dR * ref_one_minus_error + dA * ref_error);
		      }
		  }
		  ematcol(k) = prob;
		}
		e_times_b = betaHat_t_col % ematcol;
		val = jump_prob * sum(e_times_b);
		betaHat_t_col = ((not_jump_prob) * e_times_b + val);
	    }
        }
	//
	// all this was to give us the unnormalized beta column that we can now work with
	// now with build gammas and dosages etc
        //
        calculate_small_gamma_t_col = false;
        if (return_gammaSmall_t) {
            if (gammaSmall_cols_to_get(iGrid) >= 0) {
                calculate_small_gamma_t_col = true;
            }
        }
        if (return_dosage | return_gamma_t | calculate_small_gamma_t_col) {
            gamma_t_col = alphaHat_t.col(iGrid) % betaHat_t_col; // betaHat_t_col includes effect of c, so this is accurate and does not need more c
        }
        if (return_dosage) {
            s = 32 * (iGrid); // 0-based here
            e = 32 * (iGrid + 1) - 1;
            if (e > (nSNPs - 1)) {
                e = nSNPs - 1;
            }
            nSNPsLocal = e - s + 1;
            matched_gammas.fill(0);
            dosageL.fill(0);
	    dh_col = hapMatcher.col(iGrid);
            if (use_eMatDH) {
                for(k = 0; k < K; k++) {
                    // some of the dh will be 0 and go to matched_gammas 0th entry, but that is OK we do not use that
		    // they are dealt with afterwards
		    matched_gammas(dh_col(k)) += gamma_t_col(k);
                    if (dh_col(k) == 0) {
		        gk = gamma_t_col(k);
                        std::uint32_t tmp(rhb_t(k, iGrid));
                        for(b = 0; b < nSNPsLocal; b++) {
                            if (tmp & (1<<b)) {
                                // alternate
                                dosageL(b) += gk * (ref_one_minus_error);
                            } else {
                                // reference
                                dosageL(b) += gk * (ref_error);	    
                            }
                        }
                    }
                }
                for(b = 0; b < nSNPsLocal; b++) {
                    for(dh = 0; dh < nMaxDH; dh++) {
                        dosageL(b) += distinctHapsIE(dh, s + b) * matched_gammas(dh + 1);
                    }
                    dosage(s + b) = dosageL(b);
                }
            } else {
                for(k = 0; k < K; k++) {
                    gk = gamma_t_col(k);
                    std::uint32_t tmp(rhb_t(k, iGrid));
                    for(b = 0; b < nSNPsLocal; b++) {
                        if (tmp & (1<<b)) {
                            // alternate
                            dosageL(b) += gk * (ref_one_minus_error);
                        } else {
                            // reference
                            dosageL(b) += gk * (ref_error);
                        }
                }
                }
                // put back
                for(b = 0; b < nSNPsLocal; b++) {
                    dosage(s + b) = dosageL(b);
                }
            }
        }
        // add in c here to betaHat_t_col (as long as not 1!)
	if (c(iGrid) != 1) {
	    betaHat_t_col *= c(iGrid);
	}
        if (return_betaHat_t) {
            betaHat_t.col(iGrid) = betaHat_t_col;
        }
        if (return_gamma_t) {
            gamma_t.col(iGrid) = gamma_t_col;
        }
        if (calculate_small_gamma_t_col) {
            gammaSmall_t.col(gammaSmall_cols_to_get(iGrid)) = gamma_t_col;
        }
    }
    return;
}







//' @export
// [[Rcpp::export]]
void Rcpp_haploid_dosage_versus_refs_version2(
    const arma::mat& gl,
    arma::mat& alphaHat_t,
    arma::mat& betaHat_t,
    arma::mat& gamma_t,
    arma::mat& gammaSmall_t,
    Rcpp::NumericVector& dosage,
    const arma::mat& transMatRate_t,
    const arma::imat& rhb_t,
    double ref_error,
    const bool use_eMatDH,
    arma::imat& distinctHapsB,
    arma::mat& distinctHapsIE,
    arma::imat& hapMatcher,
    Rcpp::IntegerVector& gammaSmall_cols_to_get,
    const int suppressOutput = 1,
    bool return_betaHat_t = true,
    bool return_dosage = true,
    bool return_gamma_t = true,
    bool return_gammaSmall_t = false
) {
    //
    double prev=clock();
    timeval atime;
    timeval btime;
    if (suppressOutput == 0) {
        gettimeofday(&atime, 0);
        std::cout << "==== start at ==== " << std::endl;
        char buffer[30];
        struct timeval tv;
        time_t curtime;
        gettimeofday(&tv, NULL);
        curtime=tv.tv_sec;
        strftime(buffer,30,"%m-%d-%Y  %T.",localtime(&curtime));
        printf("%s%ld\n",buffer,tv.tv_usec);
        std::cout << "================== " << std::endl;        
    }
    for(int j = 0; j < 4; j++) {
    std::string prev_section="Null";
    std::string next_section="Initialize variables";
    prev=print_times(prev, suppressOutput, prev_section, next_section);
    prev_section=next_section;
    //
    const int K = rhb_t.n_rows;
    const double double_K = double(K);
    const int nGrids = alphaHat_t.n_cols;
    const int nSNPs = gl.n_cols;
    double one_over_K = 1 / (double)K;
    double ref_one_minus_error = 1 - ref_error;
    arma::rowvec c = arma::zeros(1, nGrids);
    arma::mat gl_local(2, 32);
    int i, k, dh, b;
    double prob, dR, dA, jump_prob, gk, one_minus_jump_prob, val;
    bool calculate_small_gamma_t_col;
    bool only_store_alpha_at_gamma_small;
    if (return_gammaSmall_t & (!return_gamma_t) & (!return_dosage) & (!return_betaHat_t)) {
        only_store_alpha_at_gamma_small = true;
    } else {
        only_store_alpha_at_gamma_small = false;
    }
    //
    //
    //
    arma::mat distinctHapsIE_local;
    arma::colvec e_times_b(K);
    arma::vec dosageL(32);
    dosageL.fill(0);
    const int nMaxDH = distinctHapsB.n_rows;
    arma::vec matched_gammas(nMaxDH);
    arma::mat eMatDH;
    if (use_eMatDH) {
        next_section="make eMatDH";
        prev=print_times(prev, suppressOutput, prev_section, next_section);
        prev_section=next_section;
        const bool add_zero_row_true = true;
        eMatDH = Rcpp_build_eMatDH(distinctHapsB, gl, nGrids, nSNPs, ref_error, ref_one_minus_error, add_zero_row_true);
    }
    //
    // initialize alphaHat_t
    //
    next_section="initialize alphaHat_t";
    prev=print_times(prev, suppressOutput, prev_section, next_section);
    prev_section=next_section;
    int iGrid = 0;
    int s = 32 * iGrid; // 0-based here
    int e = 32 * (iGrid + 1) - 1;
    if (e > (nSNPs - 1)) {
      e = nSNPs - 1;
    }
    int nSNPsLocal = e - s + 1;
    // now subset
    for(i = 0; i < nSNPsLocal; i++) {
      gl_local(0, i) = gl(0, i + s);
      gl_local(1, i) = gl(1, i + s);      
    }
    for(k = 0; k < K; k++) {
        if (use_eMatDH) {
            dh = hapMatcher(k, iGrid);
        } else {
            dh = 0;
        }
        if (dh > 0) {
            prob = eMatDH(dh, iGrid);
        } else {
            //
            std::uint32_t tmp(rhb_t(k, iGrid));            
            //
            prob = 1;
            for(int b = 0; b < nSNPsLocal; b++) {
                double dR = gl_local(0, b);
                double dA = gl_local(1, b);
                if (tmp & (1<<b)) {
                    // alternate
                    prob *= (dR * ref_error + dA * ref_one_minus_error);
                } else {
                    prob *= (dR * ref_one_minus_error + dA * ref_error);
                }
            }
        }
        alphaHat_t(k, iGrid) = prob * one_over_K;
    }
    c(0) = 1 / sum(alphaHat_t.col(0));
    alphaHat_t.col(0) = alphaHat_t.col(0) * c(0);
    //
    // run forward
    //
    next_section="run forward";
    prev=print_times(prev, suppressOutput, prev_section, next_section);
    prev_section=next_section;
    Rcpp_haploid_reference_single_forward_version2(gammaSmall_cols_to_get, gl, alphaHat_t, c, transMatRate_t, rhb_t, hapMatcher, eMatDH, nGrids, nSNPs, K, use_eMatDH, ref_error, only_store_alpha_at_gamma_small);
    //
    // run backward algorithm
    //
    arma::colvec ematcol(K);
    iGrid = nGrids - 1;
    arma::colvec betaHat_t_col(K);
    arma::colvec gamma_t_col(K);
    double not_jump_prob;
    //
    // run normal backward progression
    //
    next_section="run backward normal";
    prev=print_times(prev, suppressOutput, prev_section, next_section);
    prev_section=next_section;
    Rcpp_haploid_reference_single_backward_version2(alphaHat_t, betaHat_t, gamma_t, gammaSmall_t, gammaSmall_cols_to_get, dosage,     nGrids, transMatRate_t, eMatDH, hapMatcher, nSNPs, K, use_eMatDH, rhb_t, ref_error, gl, c, distinctHapsIE, return_betaHat_t, return_dosage, return_gamma_t, return_gammaSmall_t, nMaxDH);
    // done now
    //
    next_section="done";
    prev=print_times(prev, suppressOutput, prev_section, next_section);
    prev_section=next_section;
    }
    if (suppressOutput == 0) {
        gettimeofday(&btime, 0);
        std::cout << "==== end at ==== " << std::endl;
        //std::cout << btime << std::endl;
        long seconds  = btime.tv_sec  - atime.tv_sec;
        long useconds = btime.tv_usec - atime.tv_usec;
        long mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
        printf("EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEElapsed time: %ld milliseconds\n", mtime);
        char buffer[30];
        struct timeval tv;
        time_t curtime;
        gettimeofday(&tv, NULL);
        curtime=tv.tv_sec;
        strftime(buffer,30,"%m-%d-%Y  %T.",localtime(&curtime));
        printf("%s%ld\n",buffer,tv.tv_usec);
        std::cout << "================ " << std::endl;        
    }
    return;
}
