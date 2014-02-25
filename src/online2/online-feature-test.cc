// online/online-feat-test.cc

// 2014  IMSL, PKU-HKUST (author: Wei Shi)

// See ../../COPYING for clarification regarding multiple authors
//
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

#include "online2/online-feature.h"
#include "feat/wave-reader.h"

namespace kaldi {


template<class Real> static void AssertEqual(const Matrix<Real> &A,
                                             const Matrix<Real> &B,
                                             float tol = 0.001) {
  KALDI_ASSERT(A.NumRows() == B.NumRows()&&A.NumCols() == B.NumCols());
  for (MatrixIndexT i = 0;i < A.NumRows();i++)
	for (MatrixIndexT j = 0;j < A.NumCols();j++) {
	  KALDI_ASSERT(std::abs(A(i, j)-B(i, j)) < tol*std::max(1.0, (double) (std::abs(A(i, j))+std::abs(B(i, j)))));
    }
}

void GetOutput(OnlineFeatureInterface *a,
               Matrix<BaseFloat> *output) {
  int32 dim = a->Dim();
  int32 frame_num = 0;
  OnlineCacheFeature cache(a);

  std::vector<Vector<BaseFloat>* > cached_frames;
  while (true) {
    Vector<BaseFloat> garbage(dim);
    cache.GetFrame(frame_num, &garbage);
    cached_frames.push_back(new Vector<BaseFloat>(garbage));
    if (cache.IsLastFrame(frame_num))
      break;
    frame_num++;
  }

  KALDI_ASSERT(cached_frames.size() == a->NumFramesReady());

  output->Resize(cached_frames.size(), dim);
  for(int32 i = 0; i < cached_frames.size(); i++){
    output->CopyRowFromVec(*(cached_frames[i]), i);
    delete cached_frames[i];
  }
  cached_frames.clear();
  cache.ClearCache();
}

// Only generate random length for each piece
bool RandomSplit(int32 wav_dim,
                 std::vector<int32> *piece_dim,
                 int32 num_pieces,
                 int32 trials = 5){
  piece_dim->clear();

  int32 dim_mean = wav_dim / (num_pieces * 2);
  int32 cnt = 0;
  while(true){
    int32 dim_total = 0;
    for(int32 i = 0; i < num_pieces - 1; i++){
      (*piece_dim)[i] = dim_mean + rand() % dim_mean;
      dim_total += (*piece_dim)[i];
    }
    (*piece_dim)[num_pieces - 1] = wav_dim - dim_total;

    if(dim_total > 0 && dim_total < wav_dim)
      break;
    if(++cnt > trials)
      return false;
  }
  return true;
}

// test the OnlineMatrixFeature and OnlineCacheFeature classes.
void TestOnlineMatrixCacheFeature() {
  int32 dim = 2 + rand() % 5; // dimension of features.
  int32 num_frames = 100 + rand() % 100;

  Matrix<BaseFloat> input_feats(num_frames, dim);
  input_feats.SetRandn();

  OnlineMatrixFeature matrix_feats(input_feats);

  Matrix<BaseFloat> output_feats;
  GetOutput(&matrix_feats, &output_feats);
  AssertEqual(input_feats, output_feats);
}

void TestOnlineDeltaFeature() {
  int32 dim = 2 + rand() % 5; // dimension of features.
  int32 num_frames = 100 + rand() % 100;
  DeltaFeaturesOptions opts;
  opts.order = rand() % 3;
  opts.window = 1 + rand() % 3;

  int32 output_dim = dim * (1 + opts.order);

  Matrix<BaseFloat> input_feats(num_frames, dim);
  input_feats.SetRandn();

  OnlineMatrixFeature matrix_feats(input_feats);
  OnlineDeltaFeature delta_feats(opts, &matrix_feats);

  Matrix<BaseFloat> output_feats1;
  GetOutput(&delta_feats, &output_feats1);

  Matrix<BaseFloat> output_feats2(num_frames, output_dim);
  ComputeDeltas(opts, input_feats, &output_feats2);

  KALDI_ASSERT(output_feats1.ApproxEqual(output_feats2));
}

void TestOnlineSpliceFrames() {
  int32 dim = 2 + rand() % 5; // dimension of features.
  int32 num_frames = 100 + rand() % 100;
  OnlineSpliceOptions opts;
  opts.left_context  = 1 + rand() % 4;
  opts.right_context = 1 + rand() % 4;

  int32 output_dim = dim * (1 + opts.left_context + opts.right_context);

  Matrix<BaseFloat> input_feats(num_frames, dim);
  input_feats.SetRandn();

  OnlineMatrixFeature matrix_feats(input_feats);
  OnlineSpliceFrames splice_frame(opts, &matrix_feats);

  Matrix<BaseFloat> output_feats1;
  GetOutput(&splice_frame, &output_feats1);

  Matrix<BaseFloat> output_feats2(num_frames, output_dim);
  SpliceFrames(input_feats, opts.left_context, opts.right_context, &output_feats2);

  //KALDI_ASSERT(output_feats1.ApproxEqual(output_feats2));

}

void TestOnlineMfcc() {
  std::ifstream is("../feat/test_data/test.wav");
  WaveData wave;
  wave.Read(is);
  KALDI_ASSERT(wave.Data().NumRows() == 1);
  SubVector<BaseFloat> waveform(wave.Data(), 0);

  // the parametrization object
  MfccOptions op;
  op.frame_opts.dither = 0.0;
  op.frame_opts.preemph_coeff = 0.0;
  op.frame_opts.window_type = "hamming";
  op.frame_opts.remove_dc_offset = false;
  op.frame_opts.round_to_power_of_two = true;
  op.frame_opts.samp_freq = wave.SampFreq();
  op.mel_opts.low_freq = 0.0;
  op.htk_compat = false;
  op.use_energy = false;  // C0 not energy.
  Mfcc mfcc(op);

  // compute mfcc offline
  Matrix<BaseFloat> mfcc_feats;
  mfcc.Compute(waveform, 1.0, &mfcc_feats, NULL); // vtln not supported

  // compare
  // The test waveform is about 1.44s long, so
  // we try to break it into from 5 pieces to 9(not essential to do so)
  for(int32 num_piece = 5; num_piece < 10; num_piece++) {
    OnlineMfcc online_mfcc(op);
    std::vector<int32> piece_length(num_piece);
    KALDI_ASSERT(RandomSplit(waveform.Dim(), &piece_length, num_piece));

    int32 offset_start = 0;
    for(int32 i = 0; i < num_piece; i++){
      Vector<BaseFloat> wave_piece(
        waveform.Range(offset_start, piece_length[i]));
      online_mfcc.AcceptWaveform(wave.SampFreq(), wave_piece);
      offset_start += piece_length[i];
    }
    online_mfcc.InputFinished();

    Matrix<BaseFloat> online_mfcc_feats;
    GetOutput(&online_mfcc, &online_mfcc_feats);

    AssertEqual(mfcc_feats, online_mfcc_feats);
  }
}

void TestOnlinePlp() {
  std::ifstream is("../feat/test_data/test.wav");
  WaveData wave;
  wave.Read(is);
  KALDI_ASSERT(wave.Data().NumRows() == 1);
  SubVector<BaseFloat> waveform(wave.Data(), 0);

  // the parametrization object
  PlpOptions op;
  op.frame_opts.dither = 0.0;
  op.frame_opts.preemph_coeff = 0.0;
  op.frame_opts.window_type = "hamming";
  op.frame_opts.remove_dc_offset = false;
  op.frame_opts.round_to_power_of_two = true;
  op.frame_opts.samp_freq = wave.SampFreq();
  op.mel_opts.low_freq = 0.0;
  op.htk_compat = false;
  op.use_energy = false;  // C0 not energy.
  Plp plp(op);

  // compute plp offline
  Matrix<BaseFloat> plp_feats;
  plp.Compute(waveform, 1.0, &plp_feats, NULL); // vtln not supported

  // compare
  // The test waveform is about 1.44s long, so
  // we try to break it into from 5 pieces to 9(not essential to do so)
  for(int32 num_piece = 5; num_piece < 10; num_piece++) {
    OnlinePlp online_plp(op);
    std::vector<int32> piece_length(num_piece);
    KALDI_ASSERT(RandomSplit(waveform.Dim(), &piece_length, num_piece));

    int32 offset_start = 0;
    for(int32 i = 0; i < num_piece; i++){
      Vector<BaseFloat> wave_piece(
        waveform.Range(offset_start, piece_length[i]));
      online_plp.AcceptWaveform(wave.SampFreq(), wave_piece);
      offset_start += piece_length[i];
    }
    online_plp.InputFinished();

    Matrix<BaseFloat> online_plp_feats;
    GetOutput(&online_plp, &online_plp_feats);

    AssertEqual(plp_feats, online_plp_feats);
  }
}

}  // end namespace kaldi

int main() {
  using namespace kaldi;
  for (int i = 0; i < 40; i++) {
    TestOnlineMatrixCacheFeature();
    TestOnlineDeltaFeature();
    TestOnlineSpliceFrames();
    TestOnlineMfcc();
    TestOnlinePlp();
  }
  std::cout << "Test OK.\n";
}
