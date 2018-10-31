#include "psis.hh"

//
//  psis.cpp
//  sts
//
//  Created by Mathieu Fourment on 8/10/2018.
//  Copyright © 2018 Mathieu Fourment. All rights reserved.
//

#include <vector>
#include <cmath>
#include <algorithm>
#include <functional>
#include <numeric>
#include <tuple>
#include <iostream>

double logSum(const double x, const double y){
	const static double maxValue = std::numeric_limits<double>::max();
	const static double logLimit = -maxValue / 100;
	const static double NATS = 400;
	
	const double temp = y - x;
	if(temp > NATS || x < logLimit)
		return y;
	if(temp < -NATS || y < logLimit)
		return x;
	if(temp < 0)
		return x + std::log1p(std::exp(temp));
	return y + std::log1p(std::exp(-temp));
}

std::pair<double, double> static gpdfitnew(const std::vector<double >& x){
//"""Estimate the paramaters for the Generalized Pareto Distribution (GPD)
//Returns empirical Bayes estimate for the parameters of the two-parameter
//generalized Parato distribution given the data.
//Parameters
//----------
//x : ndarray
//One dimensional data array
//sort : bool or ndarray, optional
//If known in advance, one can provide an array of indices that would
//sort the input array `x`. If the input array is already sorted, provide
//False. If True (default behaviour), the array is sorted internally.
//sort_in_place : bool, optional
//If `sort` is True and `sort_in_place` is True, the array is sorted
//in-place (False by default).
//return_quadrature : bool, optional
//If True, quadrature points and weight `ks` and `w` of the marginal posterior distribution of k are also calculated and returned. False by
//default.
//Returns
//-------
//k, sigma : float
//estimated parameter values
//ks, w : ndarray
//Quadrature points and weights of the marginal posterior distribution
//of `k`. Returned only if `return_quadrature` is True.
//Notes
//-----
//This function returns a negative of Zhang and Stephens's k, because it is
//more common parameterisation.
//"""

	size_t n = x.size();
	double PRIOR = 3;
	size_t m = 30 + int(std::sqrt(n));
	std::vector<double> bs(m);
	double denom = PRIOR * x[int(static_cast<double>(n)/4 + 0.5) - 1];
	for (size_t i = 1; i <= m; i++) {
		bs[i-1] = (1.0 - std::sqrt(static_cast<double>(m)/(static_cast<double>(i) - 0.5)))/denom + 1.0/x[n-1];
	}
	
	std::vector<double> ks(m);
	for (int i = 0; i < m; i++) {
		double mean = 0;
		const double b = -bs[i];
		for (int j = 0; j < n; j++) {
			mean += std::log1p(b*x[j]);
		}
		ks[i] = mean/x.size();
	}
	
	std::vector<double> L;
	std::transform(bs.begin(), bs.end(), ks.begin(), std::back_inserter(L),
				   [n](const double b, const double k){return (std::log(-b/k) - k - 1.0)*n;} );
	std::cout << "L " <<L.size() << std::endl;
	std::vector<double> w(m);
	for (int i = 0; i < m; i++) {
		double sum = 0;
		for (int j = 0; j < m; j++) {
			sum += std::exp(L[j] - L[i]);
		}
		w[i] = 1.0/sum;
	}

	// remove negligible weights
	for (int i = 0; i < w.size(); i++) {
		if(w[i] < 10.0*std::numeric_limits<double>::epsilon()){
			w.erase(w.begin()+i);
			bs.erase(bs.begin()+i);
			i--;
		}
	}
	m = w.size();
	
	// normalise w
	const double sum = std::accumulate(w.begin(), w.end(), 0.0);
	std::for_each(w.begin(), w.end(), [sum](double& x){ x /= sum; });

	// posterior mean for b
	double b = 0;
	for (size_t i = 0; i < m; i++) {
		b += bs[i]*w[i];
	}
	
	// Estimate for k, note that we return a negative of Zhang and
	// Stephens's k, because it is more common parameterisation.
	double k = 0;
	for (size_t i = 0; i < x.size(); i++) {
		k += std::log1p(-b*x[i]);
	}
	k = k/x.size();

	// estimate for sigma
	double sigma = -k/b*n / (n - 0);
	// weakly informative prior for k
	const double a = 10;
	k = k*n/(n+a) + a*0.5/(n+a);
// 	std::cout << "k " << k << " sigma " << sigma<< " a "<<a <<" b "<<b<<std::endl;
	return std::make_pair(k, sigma);
}

std::vector<double> gpinv(std::vector<double> p, const double k, const double sigma){
//"""Inverse Generalised Pareto distribution function."""
	if (sigma <= 0){
		std::vector<double> x(p.size(), NAN);
		return x;
	}

	std::vector<bool> ok(p.size());
	for (size_t i = 0; i < p.size(); i++) {
		ok[i] = p[i] > 0 && p[i] < 1;
	}
	const int allok = std::accumulate(ok.begin(), ok.end(), 0);
	std::vector<double> x = p;
	if (allok == ok.size()){
		if (std::abs(k) < std::numeric_limits<double>::epsilon()){
			std::for_each(x.begin(), x.end(), [sigma](double& pp){ pp = -std::log1p(pp)*sigma; });
		}
		else{
			std::for_each(x.begin(), x.end(),
						  [k, sigma](double& pp){ pp = std::expm1(-k*std::log1p(-pp))/k*sigma; });
		}
	}
	else{
		if (std::abs(k) < std::numeric_limits<double>::epsilon()){
			for (size_t i = 0; i < p.size(); i++) {
				if (ok[i]) {
					x[i] = -std::log1p(-p[i])*sigma;
				}
			}
		}
		else{
			for (size_t i = 0; i < p.size(); i++) {
				if (ok[i]) {
					x[i] = std::expm1(-k*std::log1p(-p[i]))/k*sigma;
				}
			}
		}
		for (size_t i = 0; i < x.size(); i++) {
			if (p[i] == 0) {
				x[i] = 0;
			}
		}
		if (k >= 0){
			for (size_t i = 0; i < x.size(); i++) {
				if (p[i] == 1) {
					x[i] = std::numeric_limits<double>::infinity();
				}
			}
		}
		else{
			for (size_t i = 0; i < x.size(); i++) {
				if (p[i] == 1) {
					x[i] = -sigma/k;
				}
			}
		}
	}
	return x;
}

double psislw(std::vector<double>& lw, double Reff=1.0){
//"""Pareto smoothed importance sampling (PSIS).
//Parameters
//----------
//lw : ndarray
//Array of size n x m containing m sets of n log weights. It is also
//possible to provide one dimensional array of length n.
//Reff : scalar, optional
//relative MCMC efficiency ``N_eff / N``
//overwrite_lw : bool, optional
//If True, the input array `lw` is smoothed in-place, assuming the array
//is F-contiguous. By default, a new array is allocated.
//Returns
//-------
//lw_out : ndarray
//smoothed log weights
//kss : ndarray
//Pareto tail indices
//"""

	size_t n = lw.size();
	int m = 1;

	// precalculate constants
	const int cutoff_ind = std::ceil(std::min(0.2 * n, 3.0 * std::sqrt(n / Reff))) - 1;
	double cutoffmin = std::log(std::nextafter(0.0,1.0));
	const const double k_min = 1.0/3;
	double k;
	double sigma;

	// improve numerical accuracy
	double max = *max_element(std::begin(lw), std::end(lw));
	std::for_each(lw.begin(), lw.end(), [max](double& w) { w -= max; });

	std::vector<std::pair<double, int>> v(lw.size());
	for (int i = 0; i < lw.size(); i++) {
		v[i] = std::make_pair(lw[i], i);
	}
	std::sort(v.begin(), v.end());
	
	// divide log weights into body and right tail
	double xcutoff = std::max(v[v.size()-cutoff_ind-1].first, cutoffmin);
	std::cout << xcutoff<<std::endl;
	double expxcutoff = std::exp(xcutoff);
	std::vector<double> x2;
	for (int i = v.size()-cutoff_ind; i < v.size(); i++) {
		x2.push_back(v[i].first);
	}
	size_t n2 = x2.size();

	if (n2 <= 4){
		// not enough tail samples for gpdfitnew
		return std::numeric_limits<double>::infinity();
	}
	else{
		std::for_each(x2.begin(), x2.end(), [expxcutoff](double& w) { w = std::exp(w) - expxcutoff; });
		// fit generalized Pareto distribution to the right tail samples
		std::tie(k, sigma) = gpdfitnew(x2);
		std::cout << "k " << k << " sigma "<< sigma <<std::endl;
	}
	
	if (k >= k_min){
		// no smoothing if short tail or GPD fit failed
		// compute ordered statistic for the fit
		std::vector<double> sti;
		double acc = 0.5;
		for (int i = 0; i < n2; i++) {
			sti.push_back(acc/n2);
			acc += 1.0;
		}
		std::vector<double> qq = gpinv(sti, k, sigma);
		
		// place the smoothed tail into the output array
		int j= 0;
		for (int i = v.size()-cutoff_ind; i < v.size(); i++, j++) {
			lw[v[i].second] = std::log(qq[j] + expxcutoff);
		}
		
		// truncate smoothed values to the largest raw weight 0
		std::for_each(lw.begin(), lw.end(), [](double& xx){ xx = std::min(xx, 0.0); });
	}
	// renormalize weights
	double sumlogs = lw[0];
	for (size_t i = 1; i < lw.size(); i++) {
		sumlogs = logSum(sumlogs, lw[i]);
	}
	std::for_each(lw.begin(), lw.end(), [sumlogs](double& xx){ xx -= sumlogs; });

	return k;
}

// int main(){
// 	static const double arr[] = {-21.02,-22.00,-19.16,-22.91,-22.88,-23.10,-18.61,-21.92,-21.76,-23.81,-18.04,-16.59,-20.91,-21.67,-20.97,-21.02,-22.62,-22.64,-22.60,-21.37,-21.29,-20.32,-21.24,-21.28,-21.11,-23.01,-20.67,-21.93,-20.65,-21.36,-23.65,-19.35,-21.44,-21.03,-21.21,-25.21,-25.23,-25.24,-21.27,-22.07,-19.90,-20.97,-22.50,-20.73,-20.71,-20.77,-18.79,-20.91,-21.34,-21.61,-22.54,-20.30,-22.38,-20.11,-20.00,-20.81,-20.46,-20.53,-20.47,-20.49,-26.75,-21.79,-20.91,-19.76,-20.16,-15.74,-19.73,-23.63,-20.31,-21.86,-22.18,-21.14,-21.10,-21.24,-22.60,-21.33,-21.30,-22.74,-20.51,-23.30,-23.35,-22.39,-23.25,-20.40,-20.27,-19.93,-20.92,-22.48,-20.80,-20.84,-22.21,-22.02,-23.06,-22.14,-19.81,-21.04,-20.66,-22.39,-21.35,-20.44,-19.32,-20.82,-19.29,-20.91,-20.84,-19.51,-19.50,-22.03,-21.23,-23.20,-21.94,-22.44,-18.11,-17.21,-20.53,-21.50,-24.03,-22.08,-25.42,-23.14,-23.04,-21.36,-20.95,-24.87,-20.77,-21.52,-23.53,-23.69,-19.56,-22.32,-19.02,-21.81,-23.68,-21.29,-21.48,-20.34,-20.78,-20.92,-20.00,-20.36,-20.44,-20.15,-23.63,-21.59,-21.42,-21.65,-19.97,-21.48,-24.94,-21.16,-23.41,-20.98,-21.58,-22.23,-21.31,-21.46,-19.25,-22.25,-21.63,-20.65,-21.43,-20.59,-20.50,-20.41,-20.61,-24.22,-21.09,-20.90,-20.30,-20.41,-20.96,-19.12,-21.10,-21.00,-19.24,-17.63,-19.92,-19.41,-19.83,-19.82,-19.87,-20.00,-27.68,-19.67,-19.76,-19.73,-21.12,-19.84,-19.88,-20.93,-19.83,-19.83,-19.67,-22.83,-20.29,-22.14,-21.82,-22.13,-21.12,-20.62,-21.32,-20.90,-21.79,-20.92,-20.37,-22.99,-23.15,-20.17,-20.60,-20.35,-20.90,-20.72,-17.97,-20.73,-20.74,-20.88,-20.15,-19.08,-20.14,-20.21,-22.83,-22.63,-16.20,-19.87,-19.92,-20.01,-19.84,-21.12,-21.15,-19.82,-17.68,-21.27,-20.03,-20.47,-19.84,-20.99,-21.03,-21.04,-20.87,-20.95,-22.14,-22.27,-22.18,-20.75,-26.39,-19.98,-20.53,-25.95,-21.27,-21.42,-21.42,-21.51,-21.39,-19.87,-19.73,-20.88,-21.96,-23.65,-22.41,-21.31,-22.12,-21.18,-24.41,-20.85,-22.85,-23.02,-23.88,-20.98,-21.06,-15.23,-18.75,-21.76,-21.76,-20.76,-19.64,-19.26,-19.41,-20.68,-19.82,-22.78,-20.37,-24.43,-24.39,-21.78,-22.39,-21.86,-21.15,-21.06,-21.03,-21.12,-20.92,-21.41,-20.94,-22.85,-20.94,-23.02,-20.19,-20.38,-20.03,-19.96,-21.82,-19.91,-19.79,-19.84,-23.61,-23.73,-23.70,-20.06,-20.92,-22.06,-21.96,-20.88,-20.59,-18.48,-19.52,-20.23,-23.50,-18.97,-23.69,-22.76,-18.68,-19.55,-20.57,-20.03,-21.61,-21.12,-22.88,-22.21,-21.68,-21.75,-20.76,-19.69,-22.43,-17.73,-22.58,-23.57,-20.90,-20.83,-20.89,-18.56,-22.07,-20.21,-22.39,-21.33,-22.43,-22.69,-22.00,-22.55,-20.93,-20.94,-21.03,-21.03,-20.57,-20.58,-18.99,-20.66,-19.93,-21.64,-21.72,-19.92,-21.77,-21.53,-23.11,-23.18,-19.09,-20.04,-22.52,-20.85,-17.90,-22.29,-23.15,-20.35,-19.81,-21.07,-22.85,-24.74,-21.68,-21.32,-22.69,-21.16,-20.39,-22.26,-21.19,-21.98,-21.78,-21.17,-23.45,-22.61,-22.49,-20.53,-19.93,-20.55,-20.71,-23.57,-20.56,-19.50,-19.47,-20.35,-19.79,-21.46,-20.09,-19.68,-20.72,-20.58,-21.29,-21.60,-25.13,-20.48,-20.41,-20.39,-20.28,-20.36,-19.32,-20.48,-22.68,-19.74,-20.46,-20.49,-21.76,-19.60,-19.79,-20.78,-23.74,-20.95,-20.89,-20.94,-20.94,-22.20,-20.00,-19.49,-19.37,-19.25,-19.56,-20.86,-20.92,-19.43,-20.15,-20.25,-20.58,-20.95,-22.81,-23.53,-22.83,-21.08,-22.25,-22.31,-21.45,-19.64,-18.24,-21.89,-22.74,-21.89,-22.74,-21.23,-22.02,-18.84,-20.39,-20.47,-20.39,-20.46,-19.48,-19.93,-20.09,-20.44,-20.65,-19.62,-20.68,-21.28,-22.26,-21.49,-21.55,-20.45,-19.66,-20.98,-21.33,-19.04,-23.91,-21.40,-21.30,-21.39,-20.76,-19.61,-18.98,-20.57,-20.23,-20.65,-20.39,-20.49,-21.09,-20.77,-20.64,-20.61,-20.62,-20.50,-20.78,-19.49,-20.59,-20.81,-22.63,-20.55,-20.15,-20.14,-20.37,-26.28,-21.17,-21.32,-21.87,-21.49,-20.87,-22.51,-20.11,-22.18,-23.55,-19.72,-24.50,-21.85,-21.83,-20.26,-20.55,-19.81,-22.05,-22.08,-21.48,-22.23,-22.14,-22.02,-22.13,-21.77,-20.46,-20.50,-20.65,-20.68,-22.54,-19.37,-20.78,-23.24,-20.14,-20.02,-20.92,-19.66,-20.78,-20.93,-20.94,-20.56,-20.67,-21.65,-20.52,-20.80,-20.60,-20.74,-20.67,-22.89,-21.26,-19.52,-21.54,-20.85,-21.44,-21.55,-21.24,-21.56,-21.53,-20.51,-22.30,-21.83,-21.41,-21.96,-20.43,-20.63,-17.87,-21.66,-21.53,-21.15,-21.65,-21.66,-21.68,-21.41,-20.95,-20.12,-21.51,-22.09,-19.70,-22.83,-22.95,-22.81,-20.82,-22.64,-20.62,-21.93,-19.81,-21.87,-21.90,-21.19,-20.49,-21.78,-20.34,-20.35,-20.45,-20.24,-21.78,-21.57,-21.39,-23.17,-21.56,-22.76,-20.59,-21.87,-22.00,-22.09,-20.49,-22.18,-22.95,-21.30,-20.27,-23.20,-22.16,-30.03,-21.21,-20.44,-18.21,-22.25,-22.16,-19.83,-20.42,-19.81,-19.86,-19.92,-19.89,-20.00,-20.47,-19.81,-23.08,-23.09,-22.90,-19.48,-19.57,-20.48,-19.55,-22.45,-19.57,-23.98,-23.53,-23.56,-20.33,-20.02,-21.70,-20.01,-21.86,-21.83,-21.77,-21.19,-20.38,-21.66,-20.41,-19.99,-20.28,-19.56,-21.12,-19.92,-19.89,-20.39,-19.80,-20.81,-22.00,-21.26,-16.49,-21.07,-20.75,-21.10,-21.18,-18.98,-21.43,-19.77,-24.24,-24.43,-19.07,-19.05,-21.23,-18.77,-21.46,-21.66,-19.59,-20.29,-20.74,-19.59,-20.73,-20.62,-21.46,-19.74,-22.11,-19.58,-22.10,-20.70,-20.93,-20.69,-20.72,-24.29,-19.54,-20.70,-20.53,-20.56,-20.48,-22.76,-25.38,-18.90,-20.21,-20.10,-19.07,-20.16,-22.08,-22.16,-22.02,-19.77,-20.74,-20.28,-22.49,-22.34,-21.35,-21.39,-21.33,-21.61,-21.45,-21.55,-21.36,-23.26,-21.53,-23.93,-21.03,-20.72,-20.84,-18.74,-20.57,-21.25,-20.27,-21.26,-20.29,-20.97,-22.54,-22.55,-22.68,-22.50,-22.62,-22.51,-19.44,-20.82,-19.57,-19.40,-19.93,-19.38,-21.68,-21.17,-21.30,-21.27,-19.72,-21.36,-22.08,-19.75,-19.36,-23.88,-21.29,-23.81,-23.77,-23.72,-22.16,-22.45,-22.24,-22.24,-22.12,-22.25,-23.60,-23.60,-23.78,-23.65,-23.71,-23.03,-20.01,-24.05,-19.88,-21.60,-22.20,-24.32,-23.23,-23.18,-23.16,-23.07,-20.32,-20.47,-21.04,-20.48,-20.19,-20.84,-21.93,-22.80,-22.79,-22.83,-22.79,-20.24,-23.23,-23.05,-23.38,-21.61,-20.33,-13.85,-17.66,-15.98,-21.99,-22.06,-19.19,-19.50,-21.32,-19.59,-19.78,-19.57,-20.44,-19.92,-20.39,-17.17,-20.95,-21.50,-20.86,-20.95,-23.34,-23.28,-20.26,-21.52,-21.83,-22.80,-22.32,-22.69,-21.93,-20.89,-20.57,-21.82,-20.99,-19.86,-20.67,-26.28,-20.24,-20.38,-20.89,-23.74,-20.92,-20.70,-23.50,-23.54,-23.53,-17.76,-21.22,-21.29,-23.52,-21.60,-20.30,-21.11,-20.94,-21.04,-20.95,-21.06,-20.38,-20.48,-19.84,-20.38,-20.49,-20.43,-20.49,-20.50,-20.57,-23.54,-20.40,-20.35,-23.34,-21.92,-20.61,-20.75,-20.61,-20.12,-20.40,-20.48,-20.34,-20.18,-19.98,-24.32,-21.08,-21.67,-20.19,-21.70,-20.99,-20.97,-24.15,-21.37,-20.68,-20.52,-20.51,-20.33,-20.77,-20.71,-20.70,-20.19,-20.62,-18.72,-19.22,-21.27,-21.61,-20.55,-21.81,-21.65,-21.70,-22.26,-23.31,-20.77,-20.35,-23.15,-18.97,-21.78,-20.93,-20.79,-21.24,-25.44,-25.35,-20.75,-21.25,-21.32,-21.50,-23.27,-20.92,-21.76,-21.12,-20.87,-20.56,-21.24,-19.65,-20.85,-20.64,-20.64,-21.49,-19.91,-19.99,-19.99,-20.19,-20.52,-19.99,-19.99,-20.06,-19.69,-18.92,-19.21,-19.21,-20.13,-19.44,-21.77,-19.95,-19.49,-23.89,-18.36,-18.73,-20.26,-21.52,-25.37,-25.32,-18.41,-24.35,-24.34,-24.33,-24.45,-21.22,-21.11,-21.22,-18.56,-20.98,-21.07,-24.00,-22.38,-20.89,-20.93,-20.88,-19.69,-20.82,-20.68,-20.77,-22.73,-20.80,-20.82,-21.27,-19.45,-22.54,-23.15,-23.26,-23.17,-19.51,-22.99,-21.64,-19.53,-21.51,-21.85,-21.08,-21.14,-21.12,-20.69,-21.11,-20.82,-20.80,-21.11};
// 	// normalized
// 	//static const double arr[] = {-8.33882,-9.31882,-6.47882,-10.22882,-10.19882,-10.41882,-5.92882,-9.23882,-9.07882,-11.12882,-5.35882,-3.90882,-8.22882,-8.98882,-8.28882,-8.33882,-9.93882,-9.95882,-9.91882,-8.68882,-8.60882,-7.63882,-8.55882,-8.59882,-8.42882,-10.32882,-7.98882,-9.24882,-7.96882,-8.67882,-10.96882,-6.66882,-8.75882,-8.34882,-8.52882,-12.52882,-12.54882,-12.55882,-8.58882,-9.38882,-7.21882,-8.28882,-9.81882,-8.04882,-8.02882,-8.08882,-6.10882,-8.22882,-8.65882,-8.92882,-9.85882,-7.61882,-9.69882,-7.42882,-7.31882,-8.12882,-7.77882,-7.84882,-7.78882,-7.80882,-14.06882,-9.10882,-8.22882,-7.07882,-7.47882,-3.05882,-7.04882,-10.94882,-7.62882,-9.17882,-9.49882,-8.45882,-8.41882,-8.55882,-9.91882,-8.64882,-8.61882,-10.05882,-7.82882,-10.61882,-10.66882,-9.70882,-10.56882,-7.71882,-7.58882,-7.24882,-8.23882,-9.79882,-8.11882,-8.15882,-9.52882,-9.33882,-10.37882,-9.45882,-7.12882,-8.35882,-7.97882,-9.70882,-8.66882,-7.75882,-6.63882,-8.13882,-6.60882,-8.22882,-8.15882,-6.82882,-6.81882,-9.34882,-8.54882,-10.51882,-9.25882,-9.75882,-5.42882,-4.52882,-7.84882,-8.81882,-11.34882,-9.39882,-12.73882,-10.45882,-10.35882,-8.67882,-8.26882,-12.18882,-8.08882,-8.83882,-10.84882,-11.00882,-6.87882,-9.63882,-6.33882,-9.12882,-10.99882,-8.60882,-8.79882,-7.65882,-8.09882,-8.23882,-7.31882,-7.67882,-7.75882,-7.46882,-10.94882,-8.90882,-8.73882,-8.96882,-7.28882,-8.79882,-12.25882,-8.47882,-10.72882,-8.29882,-8.89882,-9.54882,-8.62882,-8.77882,-6.56882,-9.56882,-8.94882,-7.96882,-8.74882,-7.90882,-7.81882,-7.72882,-7.92882,-11.53882,-8.40882,-8.21882,-7.61882,-7.72882,-8.27882,-6.43882,-8.41882,-8.31882,-6.55882,-4.94882,-7.23882,-6.72882,-7.14882,-7.13882,-7.18882,-7.31882,-14.99882,-6.98882,-7.07882,-7.04882,-8.43882,-7.15882,-7.19882,-8.24882,-7.14882,-7.14882,-6.98882,-10.14882,-7.60882,-9.45882,-9.13882,-9.44882,-8.43882,-7.93882,-8.63882,-8.21882,-9.10882,-8.23882,-7.68882,-10.30882,-10.46882,-7.48882,-7.91882,-7.66882,-8.21882,-8.03882,-5.28882,-8.04882,-8.05882,-8.19882,-7.46882,-6.39882,-7.45882,-7.52882,-10.14882,-9.94882,-3.51882,-7.18882,-7.23882,-7.32882,-7.15882,-8.43882,-8.46882,-7.13882,-4.99882,-8.58882,-7.34882,-7.78882,-7.15882,-8.30882,-8.34882,-8.35882,-8.18882,-8.26882,-9.45882,-9.58882,-9.49882,-8.06882,-13.70882,-7.29882,-7.84882,-13.26882,-8.58882,-8.73882,-8.73882,-8.82882,-8.70882,-7.18882,-7.04882,-8.19882,-9.27882,-10.96882,-9.72882,-8.62882,-9.43882,-8.49882,-11.72882,-8.16882,-10.16882,-10.33882,-11.19882,-8.29882,-8.37882,-2.54882,-6.06882,-9.07882,-9.07882,-8.07882,-6.95882,-6.57882,-6.72882,-7.99882,-7.13882,-10.09882,-7.68882,-11.74882,-11.70882,-9.09882,-9.70882,-9.17882,-8.46882,-8.37882,-8.34882,-8.43882,-8.23882,-8.72882,-8.25882,-10.16882,-8.25882,-10.33882,-7.50882,-7.69882,-7.34882,-7.27882,-9.13882,-7.22882,-7.10882,-7.15882,-10.92882,-11.04882,-11.01882,-7.37882,-8.23882,-9.37882,-9.27882,-8.19882,-7.90882,-5.79882,-6.83882,-7.54882,-10.81882,-6.28882,-11.00882,-10.07882,-5.99882,-6.86882,-7.88882,-7.34882,-8.92882,-8.43882,-10.19882,-9.52882,-8.99882,-9.06882,-8.07882,-7.00882,-9.74882,-5.04882,-9.89882,-10.88882,-8.21882,-8.14882,-8.20882,-5.87882,-9.38882,-7.52882,-9.70882,-8.64882,-9.74882,-10.00882,-9.31882,-9.86882,-8.24882,-8.25882,-8.34882,-8.34882,-7.88882,-7.89882,-6.30882,-7.97882,-7.24882,-8.95882,-9.03882,-7.23882,-9.08882,-8.84882,-10.42882,-10.49882,-6.40882,-7.35882,-9.83882,-8.16882,-5.21882,-9.60882,-10.46882,-7.66882,-7.12882,-8.38882,-10.16882,-12.05882,-8.99882,-8.63882,-10.00882,-8.47882,-7.70882,-9.57882,-8.50882,-9.29882,-9.09882,-8.48882,-10.76882,-9.92882,-9.80882,-7.84882,-7.24882,-7.86882,-8.02882,-10.88882,-7.87882,-6.81882,-6.78882,-7.66882,-7.10882,-8.77882,-7.40882,-6.99882,-8.03882,-7.89882,-8.60882,-8.91882,-12.44882,-7.79882,-7.72882,-7.70882,-7.59882,-7.67882,-6.63882,-7.79882,-9.99882,-7.05882,-7.77882,-7.80882,-9.07882,-6.91882,-7.10882,-8.09882,-11.05882,-8.26882,-8.20882,-8.25882,-8.25882,-9.51882,-7.31882,-6.80882,-6.68882,-6.56882,-6.87882,-8.17882,-8.23882,-6.74882,-7.46882,-7.56882,-7.89882,-8.26882,-10.12882,-10.84882,-10.14882,-8.39882,-9.56882,-9.62882,-8.76882,-6.95882,-5.55882,-9.20882,-10.05882,-9.20882,-10.05882,-8.54882,-9.33882,-6.15882,-7.70882,-7.78882,-7.70882,-7.77882,-6.79882,-7.24882,-7.40882,-7.75882,-7.96882,-6.93882,-7.99882,-8.59882,-9.57882,-8.80882,-8.86882,-7.76882,-6.97882,-8.29882,-8.64882,-6.35882,-11.22882,-8.71882,-8.61882,-8.70882,-8.07882,-6.92882,-6.29882,-7.88882,-7.54882,-7.96882,-7.70882,-7.80882,-8.40882,-8.08882,-7.95882,-7.92882,-7.93882,-7.81882,-8.09882,-6.80882,-7.90882,-8.12882,-9.94882,-7.86882,-7.46882,-7.45882,-7.68882,-13.59882,-8.48882,-8.63882,-9.18882,-8.80882,-8.18882,-9.82882,-7.42882,-9.49882,-10.86882,-7.03882,-11.81882,-9.16882,-9.14882,-7.57882,-7.86882,-7.12882,-9.36882,-9.39882,-8.79882,-9.54882,-9.45882,-9.33882,-9.44882,-9.08882,-7.77882,-7.81882,-7.96882,-7.99882,-9.85882,-6.68882,-8.09882,-10.55882,-7.45882,-7.33882,-8.23882,-6.97882,-8.09882,-8.24882,-8.25882,-7.87882,-7.98882,-8.96882,-7.83882,-8.11882,-7.91882,-8.05882,-7.98882,-10.20882,-8.57882,-6.83882,-8.85882,-8.16882,-8.75882,-8.86882,-8.55882,-8.87882,-8.84882,-7.82882,-9.61882,-9.14882,-8.72882,-9.27882,-7.74882,-7.94882,-5.18882,-8.97882,-8.84882,-8.46882,-8.96882,-8.97882,-8.99882,-8.72882,-8.26882,-7.43882,-8.82882,-9.40882,-7.01882,-10.14882,-10.26882,-10.12882,-8.13882,-9.95882,-7.93882,-9.24882,-7.12882,-9.18882,-9.21882,-8.50882,-7.80882,-9.09882,-7.65882,-7.66882,-7.76882,-7.55882,-9.09882,-8.88882,-8.70882,-10.48882,-8.87882,-10.07882,-7.90882,-9.18882,-9.31882,-9.40882,-7.80882,-9.49882,-10.26882,-8.61882,-7.58882,-10.51882,-9.47882,-17.34882,-8.52882,-7.75882,-5.52882,-9.56882,-9.47882,-7.14882,-7.73882,-7.12882,-7.17882,-7.23882,-7.20882,-7.31882,-7.78882,-7.12882,-10.39882,-10.40882,-10.21882,-6.79882,-6.88882,-7.79882,-6.86882,-9.76882,-6.88882,-11.29882,-10.84882,-10.87882,-7.64882,-7.33882,-9.01882,-7.32882,-9.17882,-9.14882,-9.08882,-8.50882,-7.69882,-8.97882,-7.72882,-7.30882,-7.59882,-6.87882,-8.43882,-7.23882,-7.20882,-7.70882,-7.11882,-8.12882,-9.31882,-8.57882,-3.80882,-8.38882,-8.06882,-8.41882,-8.49882,-6.29882,-8.74882,-7.08882,-11.55882,-11.74882,-6.38882,-6.36882,-8.54882,-6.08882,-8.77882,-8.97882,-6.90882,-7.60882,-8.05882,-6.90882,-8.04882,-7.93882,-8.77882,-7.05882,-9.42882,-6.89882,-9.41882,-8.01882,-8.24882,-8.00882,-8.03882,-11.60882,-6.85882,-8.01882,-7.84882,-7.87882,-7.79882,-10.07882,-12.69882,-6.21882,-7.52882,-7.41882,-6.38882,-7.47882,-9.39882,-9.47882,-9.33882,-7.08882,-8.05882,-7.59882,-9.80882,-9.65882,-8.66882,-8.70882,-8.64882,-8.92882,-8.76882,-8.86882,-8.67882,-10.57882,-8.84882,-11.24882,-8.34882,-8.03882,-8.15882,-6.05882,-7.88882,-8.56882,-7.58882,-8.57882,-7.60882,-8.28882,-9.85882,-9.86882,-9.99882,-9.81882,-9.93882,-9.82882,-6.75882,-8.13882,-6.88882,-6.71882,-7.24882,-6.69882,-8.99882,-8.48882,-8.61882,-8.58882,-7.03882,-8.67882,-9.39882,-7.06882,-6.67882,-11.19882,-8.60882,-11.12882,-11.08882,-11.03882,-9.47882,-9.76882,-9.55882,-9.55882,-9.43882,-9.56882,-10.91882,-10.91882,-11.09882,-10.96882,-11.02882,-10.34882,-7.32882,-11.36882,-7.19882,-8.91882,-9.51882,-11.63882,-10.54882,-10.49882,-10.47882,-10.38882,-7.63882,-7.78882,-8.35882,-7.79882,-7.50882,-8.15882,-9.24882,-10.11882,-10.10882,-10.14882,-10.10882,-7.55882,-10.54882,-10.36882,-10.69882,-8.92882,-7.64882,-1.16882,-4.97882,-3.29882,-9.30882,-9.37882,-6.50882,-6.81882,-8.63882,-6.90882,-7.09882,-6.88882,-7.75882,-7.23882,-7.70882,-4.48882,-8.26882,-8.81882,-8.17882,-8.26882,-10.65882,-10.59882,-7.57882,-8.83882,-9.14882,-10.11882,-9.63882,-10.00882,-9.24882,-8.20882,-7.88882,-9.13882,-8.30882,-7.17882,-7.98882,-13.59882,-7.55882,-7.69882,-8.20882,-11.05882,-8.23882,-8.01882,-10.81882,-10.85882,-10.84882,-5.07882,-8.53882,-8.60882,-10.83882,-8.91882,-7.61882,-8.42882,-8.25882,-8.35882,-8.26882,-8.37882,-7.69882,-7.79882,-7.15882,-7.69882,-7.80882,-7.74882,-7.80882,-7.81882,-7.88882,-10.85882,-7.71882,-7.66882,-10.65882,-9.23882,-7.92882,-8.06882,-7.92882,-7.43882,-7.71882,-7.79882,-7.65882,-7.49882,-7.29882,-11.63882,-8.39882,-8.98882,-7.50882,-9.01882,-8.30882,-8.28882,-11.46882,-8.68882,-7.99882,-7.83882,-7.82882,-7.64882,-8.08882,-8.02882,-8.01882,-7.50882,-7.93882,-6.03882,-6.53882,-8.58882,-8.92882,-7.86882,-9.12882,-8.96882,-9.01882,-9.57882,-10.62882,-8.08882,-7.66882,-10.46882,-6.28882,-9.09882,-8.24882,-8.10882,-8.55882,-12.75882,-12.66882,-8.06882,-8.56882,-8.63882,-8.81882,-10.58882,-8.23882,-9.07882,-8.43882,-8.18882,-7.87882,-8.55882,-6.96882,-8.16882,-7.95882,-7.95882,-8.80882,-7.22882,-7.30882,-7.30882,-7.50882,-7.83882,-7.30882,-7.30882,-7.37882,-7.00882,-6.23882,-6.52882,-6.52882,-7.44882,-6.75882,-9.08882,-7.26882,-6.80882,-11.20882,-5.67882,-6.04882,-7.57882,-8.83882,-12.68882,-12.63882,-5.72882,-11.66882,-11.65882,-11.64882,-11.76882,-8.53882,-8.42882,-8.53882,-5.87882,-8.29882,-8.38882,-11.31882,-9.69882,-8.20882,-8.24882,-8.19882,-7.00882,-8.13882,-7.99882,-8.08882,-10.04882,-8.11882,-8.13882,-8.58882,-6.76882,-9.85882,-10.46882,-10.57882,-10.48882,-6.82882,-10.30882,-8.95882,-6.84882,-8.82882,-9.16882,-8.39882,-8.45882,-8.43882,-8.00882,-8.42882,-8.13882,-8.11882,-8.42882};
// 	std::vector<double> vec (arr, arr + sizeof(arr) / sizeof(arr[0]) );
// 	double k = psislw(vec);
// 	std::cout << "c("<<vec[0];
// 	for(int i = 1; i < vec.size(); i++){
// 		std::cout << "," << vec[i];
// 	}
// // 	for(int i = 0; i < vec.size(); i++){
// // 		std::cout << arr[i] << " "<<vec[i] << std::endl;
// // 	}
// }