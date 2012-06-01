// lat/sausages.cc

// Copyright 2012  Johns Hopkins University (Author: Daniel Povey)

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.

#include "lat/sausages.h"
#include "lat/lattice-functions.h"

namespace kaldi {

// this is Figure 6 in the paper.
void MinimumBayesRisk::MbrDecode() {
  
  for (size_t counter = 0; ; counter++) {
    NormalizeEps(&R_);
    AccStats(); // writes to gamma_
    double delta_Q = 0.0; // change in objective function.

    one_best_times_.clear();
    
    // Caution: q in the line below is (q-1) in the algorithm
    // in the paper; both R_ and gamma_ are indexed by q-1.
    for (size_t q = 0; q < R_.size(); q++) {
      // sort gamma_[i] in reverse order so most likely one is first.
      const vector<pair<int32, BaseFloat> > &this_gamma = gamma_[q];
      double old_gamma = 0, new_gamma = this_gamma[0].second;
      int32 rq = R_[q], rhat = this_gamma[0].first; // rq: old word, rhat: new.
      for (size_t j = 0; j < this_gamma.size(); j++)
        if (this_gamma[j].first == rq) old_gamma = this_gamma[j].second;
      delta_Q += (old_gamma - new_gamma); // will be 0 or negative; a bound on
      // change in error.
      if (rq != rhat)
        KALDI_VLOG(2) << "Changing word " << rq << " to " << rhat;
      R_[q] = rhat;
      if (rhat != 0) one_best_times_.push_back(times_[q]);
    }
    KALDI_VLOG(2) << "Iter = " << counter << ", delta-Q = " << delta_Q;
    if (delta_Q == 0) break;
    if (counter > 100) {
      KALDI_WARN << "Iterating too many times in MbrDecode; stopping.";
      break;
    }
  }
  RemoveEps(&R_);
}


struct Int32IsZero {
  bool operator() (int32 i) { return (i == 0); }
};
// static 
void MinimumBayesRisk::RemoveEps(std::vector<int32> *vec) {
  Int32IsZero pred;
  vec->erase(std::remove_if (vec->begin(), vec->end(), pred),
             vec->end());
}

// static
void MinimumBayesRisk::NormalizeEps(std::vector<int32> *vec) {
  RemoveEps(vec);
  vec->resize(1 + vec->size() * 2);
  int32 s = vec->size();
  for (int32 i = s/2 - 1; i >= 0; i--) {
    (*vec)[i*2 + 1] = (*vec)[i];
    (*vec)[i*2 + 2] = 0;
  }
  (*vec)[0] = 0;
}

double MinimumBayesRisk::EditDistance(int32 N, int32 Q,
                                      Vector<double> &alpha,
                                      Matrix<double> &alpha_dash,
                                      Vector<double> &alpha_dash_arc) {
  alpha(1) = 0.0; // = log(1).  Line 5.
  alpha_dash(1, 0) = 0.0; // Line 5.
  for (int32 q = 1; q <= Q; q++) 
    alpha_dash(1, q) = alpha_dash(1, q-1) + l(0, r(q)); // Line 7.
  for (int32 n = 2; n <= N; n++) {
    double alpha_n = kLogZeroDouble;
    for (size_t i = 0; i < pre_[n].size(); i++) {
      const Arc &arc = arcs_[pre_[n][i]];
      alpha_n = LogAdd(alpha_n, alpha(arc.start_node) + arc.loglike);
    }
    alpha(n) = alpha_n; // Line 10.
    // Line 11 omitted: matrix was initialized to zero.
    for (size_t i = 0; i < pre_[n].size(); i++) {
      const Arc &arc = arcs_[pre_[n][i]];
      int32 s_a = arc.start_node, w_a = arc.word;
      BaseFloat p_a = arc.loglike;
      for (int32 q = 0; q <= Q; q++) {
        if (q == 0) {
          alpha_dash_arc(q) = // line 15.
              alpha_dash(s_a, q) + l(w_a, 0) + delta();
        } else {  // a1,a2,a3 are the 3 parts of min expression of line 17.
          int32 r_q = r(q);
          double a1 = alpha_dash(s_a, q-1) + l(w_a, r_q),
              a2 = alpha_dash(s_a, q) + l(w_a, 0) + delta(),
              a3 = alpha_dash_arc(q-1) + l(0, r_q);
          alpha_dash_arc(q) = std::min(a1, std::min(a2, a3));
        }
        // line 19:
        alpha_dash(n, q) += exp(alpha(s_a) + p_a - alpha(n)) * alpha_dash_arc(q);
      }
    }
  }
  return alpha_dash(N, Q); // line 23.
}

// Figure 5 in the paper.
void MinimumBayesRisk::AccStats() {
  using std::map;
  
  int32 N = static_cast<int32>(pre_.size()) - 1,
      Q = static_cast<int32>(R_.size());

  Vector<double> alpha(N+1); // index (1...N)
  Matrix<double> alpha_dash(N+1, Q+1); // index (1...N, 0...Q)
  Vector<double> alpha_dash_arc(Q+1); // index 0...Q
  Matrix<double> beta_dash(N+1, Q+1); // index (1...N, 0...Q)
  Vector<double> beta_dash_arc(Q+1); // index 0...Q
  vector<char> b_arc(Q+1); // integer in {1,2,3}; index 1...Q
  vector<map<int32, double> > gamma(Q+1); // temp. form of gamma.
  // index 1...Q [word] -> occ.

  // The tau arrays below are the sums over words of the tau_b
  // and tau_e timing quantities mentioned in Appendix C of
  // the paper... we are using these to get averaged times for
  // the sausage bins, not specifically for the 1-best output.
  Vector<double> tau_b(Q+1), tau_e(Q+1);

  double Ltmp = EditDistance(N, Q, alpha, alpha_dash, alpha_dash_arc); 
  if (L_ != 0 && Ltmp > L_) { // L_ != 0 is to rule out 1st iter.
    KALDI_WARN << "Edit distance increased: " << Ltmp << " > "
               << L_;
  }
  L_ = Ltmp;
  KALDI_VLOG(2) << "L = " << L_;
  // omit line 10: zero when initialized.
  beta_dash(N, Q) = 1.0; // Line 11.
  for (int32 n = N; n >= 2; n--) {
    for (size_t i = 0; i < pre_[n].size(); i++) {
      const Arc &arc = arcs_[pre_[n][i]];
      int32 s_a = arc.start_node, w_a = arc.word;
      BaseFloat p_a = arc.loglike;
      alpha_dash_arc(0) = alpha_dash(s_a, 0) + l(w_a, 0) + delta(); // line 14.
      for (int32 q = 1; q <= Q; q++) { // this loop == lines 15-18.
        int32 r_q = r(q);
        double a1 = alpha_dash(s_a, q-1) + l(w_a, r_q),
            a2 = alpha_dash(s_a, q) + l(w_a, 0) + delta(),
            a3 = alpha_dash_arc(q-1) + l(0, r_q);
        if (a1 <= a2) {
          if (a1 <= a3) { b_arc[q] = 1; alpha_dash_arc(q) = a1; }
          else { b_arc[q] = 3; alpha_dash_arc(q) = a3; }
        } else {
          if (a2 <= a3) { b_arc[q] = 2; alpha_dash_arc(q) = a2; }
          else { b_arc[q] = 3; alpha_dash_arc(q) = a3; }
        }
      }
      beta_dash_arc.SetZero(); // line 19.
      for (int32 q = Q; q >= 1; q--) {
        // line 21:
        beta_dash_arc(q) += exp(alpha(s_a) + p_a - alpha(n)) * beta_dash(n, q);
        switch (static_cast<int>(b_arc[q])) { // lines 22 and 23:
          case 1:
            beta_dash(s_a, q-1) += beta_dash_arc(q);
            // next: gamma(q, w(a)) += beta_dash_arc(q)
            AddToMap(w_a, beta_dash_arc(q), &(gamma[q]));
            // next: accumulating times, see decl for tau_b,tau_e
            tau_b(q) += state_times_[s_a] * beta_dash_arc(q);
            tau_e(q) += state_times_[n] * beta_dash_arc(q);
            break;
          case 2:
            beta_dash(s_a, q) += beta_dash_arc(q);
            break;
          case 3:
            beta_dash_arc(q-1) += beta_dash_arc(q);
            // next: gamma(q, epsilon) += beta_dash_arc(q)
            AddToMap(0, beta_dash_arc(q), &(gamma[q]));
            // next: accumulating times, see decl for tau_b,tau_e
            // WARNING: there was an error in Appendix C.  If we followed
            // the instructions there the next line would say state_times_[sa], but
            // it would be wrong.  I will try to publish an erratum.
            tau_b(q) += state_times_[n] * beta_dash_arc(q);
            tau_e(q) += state_times_[n] * beta_dash_arc(q);
            break;
          default:
            KALDI_ERR << "Invalid b_arc value"; // error in code.
        }
      }
      beta_dash_arc(0) += exp(alpha(s_a) + p_a - alpha(n)) * beta_dash(n, 0);
      beta_dash(s_a, 0) += beta_dash_arc(0); // line 26.
    }
  }
  beta_dash_arc.SetZero(); // line 29.
  for (int32 q = Q; q >= 1; q--) {
    beta_dash_arc(q) += beta_dash(1, q);
    beta_dash_arc(q-1) += beta_dash_arc(q);
    AddToMap(0, beta_dash_arc(q), &(gamma[q]));
    // the statements below are actually redundant because
    // state_times_[1] is zero.
    tau_b(q) += state_times_[1] * beta_dash_arc(q);
    tau_e(q) += state_times_[1] * beta_dash_arc(q);
  }
  for (int32 q = 1; q <= Q; q++) { // a check (line 35)
    double sum = 0.0;
    for (map<int32, double>::iterator iter = gamma[q].begin();
         iter != gamma[q].end(); ++iter) sum += iter->second;
    if (fabs(sum - 1.0) > 0.1)
      KALDI_WARN << "sum of gamma[" << q << ",s] is " << sum;
  }
  // The next part is where we take gamma, and convert
  // to the class member gamma_, which is using a different
  // data structure and indexed from zero, not one.
  gamma_.clear();
  gamma_.resize(Q);
  for (int32 q = 1; q <= Q; q++) {
    for (map<int32, double>::iterator iter = gamma[q].begin();
         iter != gamma[q].end(); ++iter)
      gamma_[q-1].push_back(std::make_pair<int32, BaseFloat>(iter->first,
                                                             iter->second));
    // sort gamma_[q-1] from largest to smallest posterior.
    GammaCompare comp;
    std::sort(gamma_[q-1].begin(), gamma_[q-1].end(), comp);
  }
  // We do the same conversion for the state times tau_b and tau_e:
  // they get turned into the times_ data member, which has zero-based
  // indexing.
  times_.clear();
  times_.resize(Q);
  for (int32 q = 1; q <= Q; q++) {
    times_[q-1].first = tau_b(q);
    times_[q-1].second = tau_e(q);
    if (times_[q-1].first > times_[q-1].second)
      KALDI_WARN << "Times out of order";
    if (q > 1 && times_[q-2].second > times_[q-1].first)
      KALDI_WARN << "Times out of order (2)";
  }  
}

MinimumBayesRisk::MinimumBayesRisk(const CompactLattice &clat_in) {
  CompactLattice clat(clat_in); // copy.

  CreateSuperFinal(&clat); // Add super-final state to clat... this is
  // one of the requirements of the MBR algorithm, as mentioned in the
  // paper (i.e. just one final state).
  
  // Topologically sort the lattice, if not already sorted.
  kaldi::uint64 props = clat.Properties(fst::kFstProperties, false);
  if (!(props & fst::kTopSorted)) {
    if (fst::TopSort(&clat) == false)
      KALDI_ERR << "Cycles detected in lattice.";
  }
  CompactLatticeStateTimes(clat, &state_times_); // work out times of
  // the states in clat
  state_times_.push_back(0); // we'll convert to 1-based numbering.
  for (size_t i = state_times_.size()-1; i > 0; i--)
    state_times_[i] = state_times_[i-1];
  
  // Now we convert the information in "clat" into a special internal
  // format (pre_, post_ and arcs_) which allows us to access the
  // arcs preceding any given state.
  // Note: in our internal format the states will be numbered from 1,
  // which involves adding 1 to the OpenFst states.
  int32 N = clat.NumStates();
  pre_.resize(N+1);

  // Careful: "Arc" is a class-member struct, not an OpenFst type of arc as one
  // would normally assume.
  for (int32 n = 1; n <= N; n++) {
    for (fst::ArcIterator<CompactLattice> aiter(clat, n-1);
         !aiter.Done();
         aiter.Next()) {
      const CompactLatticeArc &carc = aiter.Value();
      Arc arc; // in our local format.
      arc.word = carc.ilabel; // == carc.olabel
      arc.start_node = n;
      arc.end_node = carc.nextstate + 1; // convert to 1-based.
      arc.loglike = - (carc.weight.Weight().Value1() +
                       carc.weight.Weight().Value2());
      // loglike: sum graph/LM and acoustic cost, and negate to
      // convert to loglikes.  We assume acoustic scaling is already done.

      pre_[arc.end_node].push_back(arcs_.size()); // record index of this arc.
      arcs_.push_back(arc);
    }
  }

  // We don't need to look at clat.Start() or clat.Final(state):
  // we know clat.Start() == 0 since it's topologically sorted,
  // and clat.Final(state) is Zero() except for One() at the last-
  // numbered state, thanks to CreateSuperFinal and the topological
  // sorting.

  { // Now set R_ to one best in the FST.
    RemoveAlignmentsFromCompactLattice(&clat); // will be more efficient
    // in best-path if we do this.
    Lattice lat;
    ConvertLattice(clat, &lat); // convert from CompactLattice to Lattice.
    fst::VectorFst<fst::StdArc> fst;
    ConvertLattice(lat, &fst); // convert from lattice to normal FST.
    fst::VectorFst<fst::StdArc> fst_shortest_path;
    fst::ShortestPath(fst, &fst_shortest_path); // take shortest path of FST.
    std::vector<int32> alignment, words;
    fst::TropicalWeight weight;
    GetLinearSymbolSequence(fst_shortest_path, &alignment, &words, &weight);
    KALDI_ASSERT(alignment.empty()); // we removed the alignment.
    R_ = words;
    L_ = 0.0; // Set current edit-distance to 0 [just so we know
    // when we're on the 1st iter.]
  }
  
  MbrDecode();
  
}


}  // namespace kaldi
