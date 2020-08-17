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



//' @export
// [[Rcpp::export]]
Rcpp::IntegerVector Rcpp_test(int k, int iGrid, arma::imat& rhb_t) {
  //
  Rcpp::IntegerVector output(32);
  output.fill(-1);
  std::uint32_t tmp(rhb_t(k, iGrid));
  for(int b = 0; b < 32; b++) {
    if (tmp & (1<<b)) {
      output(b) = 1;
    } else {
      output(b) = 0;
    }
  }
  return(output);
}



//' @export
// [[Rcpp::export]]
Rcpp::NumericVector rcpp_nth_partial_sort(
    Rcpp::NumericVector x,
    int nth
) {
    Rcpp::NumericVector y = clone(x);
    std::nth_element(y.begin(), y.begin()+nth, y.end());
    std::sort(y.begin(), y.begin()+nth);
    return(y);
}


//' @export
// [[Rcpp::export]]
arma::mat Rcpp_build_eMatDH(
    arma::imat& distinctHapsB,
    const arma::mat& gl,
    const int nGrids,
    const int nSNPs,
    const double ref_error,
    const double ref_one_minus_error,
    const bool add_zero_row = false
) {
    const int nMaxDH = distinctHapsB.n_rows;
    arma::mat eMatDH;
    int kbump;
    if (add_zero_row) {
        eMatDH = arma::mat(nMaxDH + 1, nGrids);
        kbump = 1;
    } else {
        eMatDH = arma::mat(nMaxDH, nGrids);
        kbump = 0;
    }
    arma::mat gl_local(2, 32);
    //
    for(int iGrid = 0; iGrid < nGrids; iGrid++) {
        //
        int s = 32 * iGrid; // 0-based here
        int e = 32 * (iGrid + 1) - 1;
        if (e > (nSNPs - 1)) {
            e = nSNPs - 1;
        }
        int nSNPsLocal = e - s + 1;
        // now subset
        for(int i = 0; i < nSNPsLocal; i++) {
            gl_local(0, i) = gl(0, i + s);
            gl_local(1, i) = gl(1, i + s);      
        }
        for(int k = 0; k < nMaxDH; k++) {
            //
            std::uint32_t tmp(distinctHapsB(k, iGrid));
            //
            double prob = 1;
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
            eMatDH(kbump + k, iGrid) = prob;
        }
    }
    return(eMatDH);
}


//' @export
// [[Rcpp::export]]
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
) {
    double ref_one_minus_error = 1 - ref_error;
    //
    arma::imat hap;
    int iRead, s, e, j;
    int KL, ik, sh, eh, i;
    double prob, pR, pA;
    Rcpp::IntegerVector to_pass_in(1);
    arma::imat haps;
    for(i = 0; i < ceil_K_n; i++) {
        sh = n * i; 
        eh = n * (i + 1) - 1;
        if (eh > (K - 1)) {
            eh = K - 1;
        }
        KL = eh - sh + 1; // 1-based, length
        Rcpp::IntegerVector haps_to_get(KL);
        for(ik = 0; ik < KL; ik++) {
            haps_to_get(ik) = ik + sh;
        }
        haps = inflate_fhb(rhb, haps_to_get, nSNPs);
        //
        for(ik = 0; ik < KL; ik++) {
            for(iRead = 0; iRead < nReads; iRead++) {
                s = start(iRead) - 1;
                e = end(iRead) - 1;
                prob = 1;
                for(j = 0; j < nr(iRead); j++) {
                    pR = ps(0, s + j);
                    pA = ps(1, s + j);
                    if (haps(u(s + j), ik) == 0) {
                        prob *= (pR * ref_one_minus_error + pA * ref_error);
                    } else {
                        prob *= (pA * ref_one_minus_error + pR * ref_error);
                    }
                }
                eMatRead_t((sh + ik), iRead) = prob;
            }
        }
    }
    return;
}



//' @export
// [[Rcpp::export]]
void Rcpp_haploid_reference_single_forward(
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
    int s, e, nSNPsLocal, i, iGrid, k, dh;
    double ref_one_minus_error = 1 - ref_error;
    const double double_K = double(K);
    double dR, dA, prob;
    arma::mat gl_local(2, 32);    
    //
    for(iGrid = 1; iGrid < nGrids; iGrid++) {
        jump_prob = transMatRate_t(1, iGrid - 1) / double_K;
        one_minus_jump_prob = (1 - jump_prob);
        not_jump_prob = transMatRate_t(0, iGrid - 1);
        s = 32 * iGrid; // 0-based here
        e = 32 * (iGrid + 1) - 1;
        if (e > (nSNPs - 1)) {
            e = nSNPs - 1;
        }
        nSNPsLocal = e - s + 1;
        for(i = 0; i < nSNPsLocal; i++) {
            gl_local(0, i) = gl(0, i + s);
            gl_local(1, i) = gl(1, i + s);
        }
        if (use_eMatDH) {
            //
            // yes use eMatDH
            //
            for(k = 0; k < K; k++) {
                dh = hapMatcher(k, iGrid);
                if (dh > 0) {
                    prob = eMatDH(dh - 1, iGrid);
                } else {
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
                alphaHat_t(k, iGrid) = (jump_prob + not_jump_prob * alphaHat_t(k, iGrid - 1)) * prob;
            }
            c(iGrid) = 1 / sum(alphaHat_t.col(iGrid));
            alphaHat_t.col(iGrid) = alphaHat_t.col(iGrid) * c(iGrid);
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
                alphaHat_t(k, iGrid) = (jump_prob + not_jump_prob * alphaHat_t(k, iGrid - 1)) * prob;
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
void Rcpp_haploid_reference_single_backward(
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
    bool calculate_small_gamma_t_col;
    arma::vec matched_gammas(nMaxDH);
    arma::colvec e_times_b(K);
    arma::vec dosageL(32);
    dosageL.fill(0);
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
            nSNPsLocal = e - s + 1;
            for(i = 0; i < nSNPsLocal; i++) {
                gl_local(0, i) = gl(0, i + s);
                gl_local(1, i) = gl(1, i + s);      
            }
            for(k = 0; k < K; k++) {
                if (use_eMatDH) {
                    dh = hapMatcher(k, iGrid + 1);
                } else {
                    dh = 0;
                }
                if (dh > 0) {
                    prob = eMatDH(dh - 1, iGrid + 1);
                } else {
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
                }
                ematcol(k) = prob;
            }
            e_times_b = betaHat_t_col % ematcol;
            val = jump_prob * sum(e_times_b);
            betaHat_t_col = ((not_jump_prob) * e_times_b + val);
        }
        //
        // build dosages, possibly save gamma
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
            if (use_eMatDH) {
                for(k = 0; k < K; k++) {
                    gk = gamma_t_col(k);
                    dh = hapMatcher(k, iGrid); // recall 0 means no match else 1-based
                    if (dh > 0) {
                        matched_gammas(dh - 1) += gk;
                    } else {
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
                //distinctHapsIE_local = distinctHapsIE.cols(s, e);
                for(b = 0; b < nSNPsLocal; b++) {
                    for(dh = 0; dh < nMaxDH; dh++) {
                        dosageL(b) += distinctHapsIE(dh, s + b) * matched_gammas(dh);
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
        // add in c here to betaHat_t_col
        betaHat_t_col *= c(iGrid);
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
void Rcpp_haploid_dosage_versus_refs(
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
    const int nMaxDH = distinctHapsB.n_rows;
    double ref_one_minus_error = 1 - ref_error;
    bool only_store_alpha_at_gamma_small;
    if (return_gammaSmall_t & (!return_gamma_t) & (!return_dosage) & (!return_betaHat_t)) {
        only_store_alpha_at_gamma_small = true;
    } else {
        only_store_alpha_at_gamma_small = false;
    }
    //
    //
    //
    double one_over_K = 1 / (double)K;
    arma::rowvec c = arma::zeros(1, nGrids);
    int i, k, dh, b;
    arma::mat gl_local(2, 32);
    double prob;
    //
    //
    //
    arma::mat eMatDH;
    if (use_eMatDH) {
        next_section="make eMatDH";
        prev=print_times(prev, suppressOutput, prev_section, next_section);
        prev_section=next_section;
        eMatDH = Rcpp_build_eMatDH(distinctHapsB, gl, nGrids, nSNPs, ref_error, ref_one_minus_error);
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
            prob = eMatDH(dh - 1, iGrid);
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
    Rcpp_haploid_reference_single_forward(gammaSmall_cols_to_get, gl, alphaHat_t, c, transMatRate_t, rhb_t, hapMatcher, eMatDH, nGrids, nSNPs, K, use_eMatDH, ref_error, only_store_alpha_at_gamma_small);
    //
    // run normal backward progression
    //
    next_section="run backward normal";
    prev=print_times(prev, suppressOutput, prev_section, next_section);
    prev_section=next_section;
    Rcpp_haploid_reference_single_backward(alphaHat_t, betaHat_t, gamma_t, gammaSmall_t, gammaSmall_cols_to_get, dosage,     nGrids, transMatRate_t, eMatDH, hapMatcher, nSNPs, K, use_eMatDH, rhb_t, ref_error, gl, c, distinctHapsIE, return_betaHat_t, return_dosage, return_gamma_t, return_gammaSmall_t, nMaxDH);
    //
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












