// cudamatrix/cuda-matrix-test.cc

// Copyright 2010  Karel Vesely
//           2013  Lucas Ondel
//           2013  Johns Hopkins University (author: Daniel Povey)

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


#include <iostream>
#include <vector>
#include <cstdlib>

#include "base/kaldi-common.h"
#include "util/common-utils.h"
#include "cudamatrix/cu-matrix-lib.h"

using namespace kaldi;


namespace kaldi {

/*
 * INITIALIZERS
 */
template<typename Real> 
static void InitRand(VectorBase<Real> *v) {
  for (MatrixIndexT i = 0; i < v->Dim(); i++)
	(*v)(i) = RandGauss();
}



template<typename Real> 
static void InitRand(MatrixBase<Real> *M) {
  do {
    for (MatrixIndexT i = 0;i < M->NumRows();i++)
      for (MatrixIndexT j = 0;j < M->NumCols();j++)
        (*M)(i, j) = RandGauss();
  } while (M->NumRows() != 0 && M->Cond() > 100);
}



template<typename Real> 
static void RandGaussMatrix(MatrixBase<Real>* mat) {
  for(int32 r=0; r<mat->NumRows(); r++)
    for(int32 c=0; c<mat->NumCols(); c++)
      (*mat)(r,c) = RandGauss();
}



template<typename Real> 
static void RandZeroToOneMatrix(MatrixBase<Real>* mat) {
  for(int32 r=0; r<mat->NumRows(); r++)
    for(int32 c=0; c<mat->NumCols(); c++)
      (*mat)(r,c) = RandUniform();
}




/*
 * ASSERTS
 */
template<typename Real> 
static void AssertEqual(const MatrixBase<Real> &A,
                        const MatrixBase<Real> &B,
                        float tol = 0.001) {
  KALDI_ASSERT(A.NumRows() == B.NumRows()&&A.NumCols() == B.NumCols());
  for (MatrixIndexT i = 0;i < A.NumRows();i++) {
    for (MatrixIndexT j = 0;j < A.NumCols();j++) {
      KALDI_ASSERT(std::abs(A(i, j)-B(i, j)) <= tol*std::max(1.0, (double) (std::abs(A(i, j))+std::abs(B(i, j)))));
    }
  }
}

template<typename Real> 
static void AssertEqual(const CuMatrixBase<Real> &A,
                        const CuMatrixBase<Real> &B,
                        float tol = 0.001) {
  Real Anorm = A.FrobeniusNorm(), Bnorm = B.FrobeniusNorm();
  CuMatrix<Real> diff(A);
  diff.AddMat(-1.0, B);
  Real diff_norm = diff.FrobeniusNorm();
  if (diff_norm > tol * 0.5 * (Anorm + Bnorm)) {
    KALDI_LOG << "A = " << A;
    KALDI_LOG << "B = " << B;
    KALDI_ERR << "Matrices differ, " << diff_norm << " > " << tol << " * 0.5 *  ( "
              << Anorm << " + " << Bnorm << " ). ";
  }
}
  


template<typename Real>
static bool ApproxEqual(const MatrixBase<Real> &A,
                        const MatrixBase<Real> &B, Real tol = 0.001) {
  KALDI_ASSERT(A.NumRows() == B.NumRows());
  MatrixBase<Real> diff(A);
  diff.AddSp(1.0, B);
  Real a = std::max(A.Max(), -A.Min()), b = std::max(B.Max(), -B.Min),
      d = std::max(diff.Max(), -diff.Min());
  return (d <= tol * std::max(a, b));
}



template<typename Real> 
static void AssertEqual(VectorBase<Real> &A, VectorBase<Real> &B, float tol = 0.001) {
  KALDI_ASSERT(A.Dim() == B.Dim());
  for (MatrixIndexT i=0; i < A.Dim(); i++)
    KALDI_ASSERT(std::abs(A(i)-B(i)) <= tol);
}



template<typename Real> 
static bool ApproxEqual(VectorBase<Real> &A, VectorBase<Real> &B, float tol = 0.001) {
  KALDI_ASSERT(A.Dim() == B.Dim());
  for (MatrixIndexT i=0; i < A.Dim(); i++)
    if (std::abs(A(i)-B(i)) > tol) return false;
  return true;
}



static void AssertEqual(std::vector<int32> &A, std::vector<int32> &B) {
  KALDI_ASSERT(A.size() == B.size());
  for (size_t i=0; i < A.size(); i++)
    KALDI_ASSERT(A[i] == B[i]);
}



/*
 * Unit tests
 */

template<typename Real> 
static void UnitTestCuMatrixTraceMatMat() {
  for (int32 i = 0; i < 5; i++) {
    int32 M = 100 + rand() % 200, N = 100 + rand() % 200;
    CuMatrix<Real> A(M, N);
    A.SetRandn();
    if (i % 2 == 0) {
      CuMatrix<Real> B(M, N);
      B.SetRandn();
      Real r1 = TraceMatMat(A, B, kTrans),
          r2 = TraceMatMat(Matrix<Real>(A), Matrix<Real>(B), kTrans),
          r3 = TraceMatMat(Matrix<Real>(A), Matrix<Real>(B, kTrans), kNoTrans);
      Matrix<Real> X(B, kTrans);
      KALDI_LOG << "Xsum = " << X.Sum();
      Matrix<Real> Y(B, kTrans);
      KALDI_LOG << "Ysum = " << Y.Sum();
      KALDI_LOG << "Bsum = " << B.Sum();
      KALDI_ASSERT(ApproxEqual(r1, r2));
      KALDI_ASSERT(ApproxEqual(r2, r3));
    } else {
      CuMatrix<Real> B(N, M);
      B.SetRandn();
      Real r1 = TraceMatMat(A, B, kNoTrans),
          r2 = TraceMatMat(Matrix<Real>(A), Matrix<Real>(B), kNoTrans),
          r3 = TraceMatMat(Matrix<Real>(A), Matrix<Real>(B, kTrans), kTrans);
      KALDI_ASSERT(ApproxEqual(r1, r2));
      KALDI_ASSERT(ApproxEqual(r2, r3));
    }
  }
}
      



/*
 * CuMatrix
 */
template<typename Real> 
static void UnitTestCuMatrixApplyLog() {
  int32 M = 100 + rand() % 200, N = 100 + rand() % 200;
  Matrix<Real> H(M, N);
  H.SetRandn();
  H.MulElements(H); // make numbers positive

  CuMatrix<Real> D(H);

  D.ApplyLog();
  H.ApplyLog();

  Matrix<Real> H2(D);

  AssertEqual(H,H2);
}


template<typename Real> 
static void UnitTestCuMatrixSigmoid() {
  for (int32 i = 0; i < 3; i++) {
    int32 M = 100 + rand() % 200, N = 100 + rand() % 200;
    Matrix<Real> H(M, N);
    H.SetRandn();
    H.MulElements(H); // make numbers positive

    CuMatrix<Real> D(H);
    CuMatrix<Real> E(M, N);

    E.Sigmoid(D);
    H.Sigmoid(H);

    Matrix<Real> H2(E);

    AssertEqual(H, H2);
  }
}

template<typename Real> 
static void UnitTestCuMatrixScale() {
  int32 M = 100 + rand() % 200, N = 100 + rand() % 200;
  Matrix<Real> H(M, N);
  H.SetRandn();

  BaseFloat scale = -1 + (0.33 * (rand() % 5));
  CuMatrix<Real> D(H);
  D.Scale(scale);
  H.Scale(scale);
  Matrix<Real> E(D);

  AssertEqual(H, E);
}

template<typename Real> 
static void UnitTestCuMatrixAdd() {
  int32 M = 100 + rand() % 200, N = 100 + rand() % 200;
  Matrix<Real> H(M, N);
  H.SetRandn();

  BaseFloat offset = -1 + (0.33 * (rand() % 5));
  CuMatrix<Real> D(H);
  D.Add(offset);
  H.Add(offset);
  Matrix<Real> E(D);

  AssertEqual(H, E);
}


template<typename Real> 
static void UnitTestCuMatrixSoftHinge() {
  int32 M = 100 + rand() % 200, N = 100 + rand() % 200;
  Matrix<Real> H(M, N);
  H.SetRandn();
  H.MulElements(H); // make numbers positive

  CuMatrix<Real> D(H);
  CuMatrix<Real> E(M, N);

  E.SoftHinge(D);
  H.SoftHinge(H);
  
  Matrix<Real> H2(E);

  AssertEqual(H,H2);
}


template<typename Real> 
static void UnitTestCuMatrixSet() {
  for (int32 i = 0; i < 3; i++) {
    BaseFloat value= 0.333;
    int32 dimM = 10 + rand() % 600, dimN = 10 + rand() % 400;
    CuMatrix<Real> m1(dimM, dimN);
    Matrix<Real> m2(dimM, dimN);
    m1.Set(value);
    m2.Set(value);
    Matrix<Real> m3(m1);
    AssertEqual(m2, m3);
  }
}


template<typename Real> 
static void UnitTestCuMatrixApplyPow() {

  for (int32 i = 0; i < 3; i++) {
    BaseFloat pow = 0.5 * (rand() % 6);
    
    Matrix<Real> H(10 + rand() % 600, 10 + rand() % 20);
    H.SetRandn();
    H.Row(0).Set(0.0);
    if (i == 2) { Matrix<Real> tmp(H, kTrans); H = tmp; }
    
    if (pow != 1.0 && pow != 2.0 && pow != 3.0)
      H.MulElements(H); //make numbers positive
    
    CuMatrix<Real> cH(H);

    cH.ApplyPow(pow);

    H.ApplyPow(pow);
    Matrix<Real> H2(cH);
    AssertEqual(H, H2);
  }
}


template<typename Real>
static void UnitTestCuMatrixCopyRowsFromVec() {
  for (MatrixIndexT p = 0; p < 10; p++) {
    int32 num_rows = 100 + rand() % 255, num_cols;
    if (p <= 2) num_cols = 128;
    else if (p <= 4) num_cols = 256;
    else num_cols = 100 + rand() % 200;

    int32 vec_dim;
    if (p % 2 == 0) vec_dim = num_cols;
    else vec_dim = num_cols * num_rows;

    CuVector<Real> cu_vec(vec_dim);
    cu_vec.SetRandn();
    Vector<Real> vec(cu_vec);

    CuMatrix<Real> cu_mat(num_rows, num_cols);
    cu_mat.CopyRowsFromVec(cu_vec);
    Matrix<Real> mat(num_rows, num_cols);
    mat.CopyRowsFromVec(vec);

    Matrix<Real> mat2(cu_mat);
    AssertEqual(mat, mat2);
  }
}


template<typename Real>
static void UnitTestCuMatrixCopyRows() {
  for (MatrixIndexT p = 0; p < 10; p++) {
    MatrixIndexT num_rows1 = 10 + rand() % 10,
        num_rows2 = 10 + rand() % 10,
        num_cols = 10 + rand() % 10;
    CuMatrix<Real> M(num_rows1, num_cols);
    M.SetRandn();
    
    CuMatrix<Real> N(num_rows2, num_cols), O(num_rows2, num_cols);
    std::vector<int32> reorder(num_rows2);
    for (int32 i = 0; i < num_rows2; i++)
      reorder[i] = -1 + (rand() % (num_rows1 + 1));
    
    N.CopyRows(M, reorder);

    for (int32 i = 0; i < num_rows2; i++)
      for (int32 j = 0; j < num_cols; j++)
        if (reorder[i] < 0) O(i, j) = 0;
        else O(i, j) = M(reorder[i], j);
    
    AssertEqual(N, O);
  }
}


template<typename Real>
void UnitTestCuMatrixCopyCross() {
  for (int32 i = 0; i < 10; i++) {
    int32 M = 100 + rand() % 255, N = 100 + rand() % 255;
    if (rand() % 3 == 0) { M = 0; N = 0; }
    CuMatrix<Real> mat1(M, N);
    mat1.SetRandn();
    if (i % 2 == 0) {
      CuMatrix<float> mat2(M, N);
      mat2.CopyFromMat(mat1);
      CuMatrix<Real> mat3(M, N);
      mat3.CopyFromMat(mat2);
      AssertEqual(mat1, mat3);
    } else {
      CuMatrix<float> mat2(N, M);
      mat2.CopyFromMat(mat1, kTrans);
      CuMatrix<Real> mat3(M, N);
      mat3.CopyFromMat(mat2, kTrans);
      AssertEqual(mat1, mat3);
    }
  }
}

template<typename Real> void UnitTestCuMatrixCopyCross2() {
  for (int32 i = 0; i < 10; i++) {
    int32 M = 100 + rand() % 255, N = 100 + rand() % 255;
    if (rand() % 3 == 0) { M = 0; N = 0; }
    CuMatrix<Real> mat1(M, N);
    mat1.SetRandn();
    Matrix<float> mat2(M, N);
    mat2.CopyFromMat(mat1);
    CuMatrix<Real> mat3(M, N);
    mat3.CopyFromMat(mat2);
    AssertEqual(mat1, mat3);
  }
}

template<typename Real>
static void UnitTestCuMatrixCopyCols() {
  for (MatrixIndexT p = 0; p < 10; p++) {
    MatrixIndexT num_cols1 = 10 + rand() % 10,
        num_cols2 = 10 + rand() % 10,
        num_rows = 10 + rand() % 10;
    CuMatrix<Real> M(num_rows, num_cols1);
    M.SetRandn();
    
    CuMatrix<Real> N(num_rows, num_cols2), O(num_rows, num_cols2);
    std::vector<int32> reorder(num_cols2);
    for (int32 i = 0; i < num_cols2; i++)
      reorder[i] = -1 + (rand() % (num_cols1 + 1));
    
    N.CopyCols(M, reorder);
    
    for (int32 i = 0; i < num_rows; i++)
      for (int32 j = 0; j < num_cols2; j++)
        if (reorder[j] < 0) O(i, j) = 0;
        else O(i, j) = M(i, reorder[j]);
    AssertEqual(N, O);
  }
}


template<typename Real> 
static void UnitTestCuMatrixApplyFloor() {

  for (int32 i = 0; i < 3; i++) {
    BaseFloat floor = 0.33 * (rand() % 6);
    
    Matrix<Real> H(10 + rand() % 600, 10 + rand() % 20);
    H.SetRandn();
    if (i == 2) { Matrix<Real> tmp(H, kTrans); H = tmp; }
    
    CuMatrix<Real> cH(H);

    cH.ApplyFloor(floor);

    H.ApplyFloor(floor);
    Matrix<Real> H2(cH);

    AssertEqual(H, H2);
  }
}


template<typename Real> 
static void UnitTestCuMatrixApplyHeaviside() {

  for (int32 i = 0; i < 3; i++) {
    Matrix<Real> H(10 + rand() % 600, 10 + rand() % 20);
    H.SetRandn();
    H.Row(0).Set(0.0);
    if (i == 2) { Matrix<Real> tmp(H, kTrans); H = tmp; }


    CuMatrix<Real> cH(H);

    cH.ApplyHeaviside();
    H.ApplyHeaviside();
    Matrix<Real> H2(cH);
    AssertEqual(H, H2);
  }
}



template<typename Real> 
static void UnitTestCuMatrixMulElements() {
  for (int32 i = 0; i < 4; i++) {
    MatrixIndexT dimM = 100 + rand() % 256, dimN = 100 + rand() % 256;
  
    Matrix<Real> Ha(dimM, dimN);
    Matrix<Real> Hb(dimM, dimN);
    RandGaussMatrix(&Ha);
    RandGaussMatrix(&Hb);

    CuMatrix<Real> Da(dimM, dimN);
    CuMatrix<Real> Db(dimM, dimN);
    Da.CopyFromMat(Ha);
    Db.CopyFromMat(Hb);

    Da.MulElements(Db);
    Ha.MulElements(Hb);

    Matrix<Real> Ha2(dimM, dimN);
    Da.CopyToMat(&Ha2);

    AssertEqual(Ha,Ha2);
  }
}

template<typename Real> 
static void UnitTestCuMatrixMax() {
  Matrix<Real> Ha(100,100);
  Matrix<Real> Hb(100,100);
  RandGaussMatrix(&Ha);
  RandGaussMatrix(&Hb);

  CuMatrix<Real> Da(100,100);
  CuMatrix<Real> Db(100,100);
  Da.CopyFromMat(Ha);
  Db.CopyFromMat(Hb);

  Da.Max(Db);
  Ha.Max(Hb);

  Matrix<Real> Ha2(100,100);
  Da.CopyToMat(&Ha2);

  AssertEqual(Ha,Ha2);
}



template<typename Real> 
static void UnitTestCuMatrixMulColsVec() {
  Matrix<Real> Hm(100,99);
  Vector<Real> Hv(99);
  RandGaussMatrix(&Hm);
  InitRand(&Hv);

  CuMatrix<Real> Dm(100,99);
  CuVector<Real> Dv(99);
  Dm.CopyFromMat(Hm);
  Dv.CopyFromVec(Hv);

  Dm.MulColsVec(Dv);
  Hm.MulColsVec(Hv);

  Matrix<Real> Hm2(100,99);
  Dm.CopyToMat(&Hm2);

  AssertEqual(Hm,Hm2);
}



template<typename Real> 
static void UnitTestCuMatrixMulRowsVec() {
  for (int32 i = 0; i < 5; i++) {
    int32 dimM = 100 + rand() % 200, dimN = 100 + rand() % 200;
    Matrix<Real> Hm(dimM, dimN);
    Vector<Real> Hv(dimM);
    RandGaussMatrix(&Hm);
    InitRand(&Hv);

    CuMatrix<Real> Dm(dimM, dimN);
    CuVector<Real> Dv(dimM);
    Dm.CopyFromMat(Hm);
    Dv.CopyFromVec(Hv);
    
    Dm.MulRowsVec(Dv);
    Hm.MulRowsVec(Hv);
    
    Matrix<Real> Hm2(dimM, dimN);
    Dm.CopyToMat(&Hm2);

    AssertEqual(Hm,Hm2);
  }
}

template<typename Real> static void UnitTestCuMatrixAddDiagVecMat() {
  for (int p = 0; p < 4; p++) {
    MatrixIndexT dimM = 100 + rand() % 255, dimN = 100 + rand() % 255;
    //MatrixIndexT dimM = 10 + rand() % 2, dimN = 10 + rand() % 2;
    Real alpha = 0.43243, beta = 1.423;
    CuMatrix<Real> M(dimM, dimN), N(dimM, dimN);
    M.SetRandn();
    N.SetRandn();
    MatrixTransposeType trans = (p % 2 == 0 ? kNoTrans : kTrans);
    if (trans == kTrans)
      N.Transpose();

    KALDI_ASSERT(M.Sum() != 0.0);
    KALDI_ASSERT(N.Sum() != 0.0);
    
    CuVector<Real> V(dimM);
    V.SetRandn();

    KALDI_ASSERT(V.Sum() != 0.0);

    CuMatrix<Real> Mcheck(M);

    for (int32 r = 0; r < dimM; r++) {
      CuSubVector<Real> Mcheckrow(Mcheck, r);
      CuVector<Real> Nrow(dimN);
      if (trans == kTrans) Nrow.CopyColFromMat(N, r);
      else Nrow.CopyFromVec(N.Row(r));
      Mcheckrow.Scale(beta);
      Mcheckrow.AddVec(alpha * V(r), Nrow);
    }
    
    M.AddDiagVecMat(alpha, V, N, trans, beta);
    AssertEqual(M, Mcheck);
    KALDI_ASSERT(M.Sum() != 0.0);
  }
}


template<typename Real> 
static void UnitTestCuMatrixDivRowsVec() {
  Matrix<Real> Hm(100,99);
  Vector<Real> Hv(100);
  RandGaussMatrix(&Hm);
  InitRand(&Hv);

  CuMatrix<Real> Dm(100,99);
  CuVector<Real> Dv(100);
  Dm.CopyFromMat(Hm);
  Dv.CopyFromVec(Hv);

  Dm.DivRowsVec(Dv);
  Hv.InvertElements();
  Hm.MulRowsVec(Hv);

  Matrix<Real> Hm2(100,99);
  Dm.CopyToMat(&Hm2);

  AssertEqual(Hm,Hm2);
}



template<typename Real> 
static void UnitTestCuMatrixAddMat() {
  Matrix<Real> Ha(100,100);
  Matrix<Real> Hb(100,100);
  RandGaussMatrix(&Ha);
  RandGaussMatrix(&Hb);

  CuMatrix<Real> Da(100,100);
  CuMatrix<Real> Db(100,100);
  Da.CopyFromMat(Ha);
  Db.CopyFromMat(Hb);

  Da.AddMat(0.5,Db);
  Ha.AddMat(0.5,Hb);

  Matrix<Real> Ha2(100,100);
  Da.CopyToMat(&Ha2);

  AssertEqual(Ha,Ha2);
}

template<typename Real> 
static void UnitTestCuMatrixSum() {
  int32 M = 100 + rand() % 300, N = 100 + rand() % 300;
  CuMatrix<Real> A(M, N);
  A.SetRandn();
  Matrix<Real> mA(A);
  KALDI_ASSERT(ApproxEqual(mA.Sum(), A.Sum()));
}


template<typename Real> 
static void UnitTestCuMatrixAddVecToCols() {
  Matrix<Real> Hm(100,99);
  Vector<Real> Hv(100);
  RandGaussMatrix(&Hm);
  InitRand(&Hv);

  CuMatrix<Real> Dm(100,99);
  CuVector<Real> Dv(100);
  Dm.CopyFromMat(Hm);
  Dv.CopyFromVec(Hv);

  Dm.AddVecToCols(0.5,Dv);
  Hm.AddVecToCols(0.5,Hv);

  Matrix<Real> Hm2(100,99);
  Dm.CopyToMat(&Hm2);

  AssertEqual(Hm,Hm2);
}



template<typename Real> 
static void UnitTestCuMatrixAddVecToRows() {
  Matrix<Real> Hm(100,99);
  Vector<Real> Hv(99);
  RandGaussMatrix(&Hm);
  InitRand(&Hv);

  CuMatrix<Real> Dm(100,99);
  CuVector<Real> Dv(99);
  Dm.CopyFromMat(Hm);
  Dv.CopyFromVec(Hv);

  Dm.AddVecToRows(0.5,Dv);
  Hm.AddVecToRows(0.5,Hv);

  Matrix<Real> Hm2(100,99);
  Dm.CopyToMat(&Hm2);

  AssertEqual(Hm,Hm2);
}



template<typename Real> 
static void UnitTestCuMatrixAddMatMat() {
  Matrix<Real> Ha(200,100);
  Matrix<Real> Hb(100,200);
  Matrix<Real> Hc1(200,200);
  Matrix<Real> Hc2(100,100);
  RandGaussMatrix(&Ha);
  RandGaussMatrix(&Hb);

  CuMatrix<Real> Da(200,100);
  CuMatrix<Real> Db(100,200);
  Da.CopyFromMat(Ha);
  Db.CopyFromMat(Hb);
  CuMatrix<Real> Dc1(200,200);
  CuMatrix<Real> Dc2(100,100);

  Dc1.AddMatMat(0.5f,Da,kNoTrans,Db,kNoTrans,0.0f);
  Dc2.AddMatMat(0.5f,Da,kTrans,Db,kTrans,0.0f);
  Hc1.AddMatMat(0.5f,Ha,kNoTrans,Hb,kNoTrans,0.0f);
  Hc2.AddMatMat(0.5f,Ha,kTrans,Hb,kTrans,0.0f);

  Matrix<Real> Hc1a(200,200);
  Matrix<Real> Hc2a(100,100);
  Dc1.CopyToMat(&Hc1a);
  Dc2.CopyToMat(&Hc2a);

  AssertEqual(Hc1,Hc1a);
  AssertEqual(Hc2,Hc2a);
}

template<typename Real>
static void UnitTestCuMatrixCopyFromMat() {
  for (MatrixIndexT i = 1; i < 10; i++) {
    MatrixIndexT dim = 5 * i + rand() % 10;
    
    Matrix<Real> A(dim, dim);
    A.SetRandn();
    CuMatrix<Real> E(A);    
    CuMatrix<Real> B(dim, dim);
    B.CopyFromMat(E);

    AssertEqual<Real>(B, E);
  }
}

template<typename Real>
static void UnitTestCuMatrixCopyFromTp() {
  for (MatrixIndexT i = 1; i < 10; i++) {
    MatrixIndexT dim = 5 * i + rand() % 10;
    TpMatrix<Real> A(dim);
    A.SetRandn();
    CuTpMatrix<Real> E(A);
    Matrix<Real> B(dim, dim);
    CuMatrix<Real> C(dim, dim);
    B.CopyFromTp(A, kNoTrans);
    C.CopyFromTp(E, kNoTrans);
    CuMatrix<Real> D(B);
    AssertEqual<Real>(D, C);
  }
}

template<typename Real>
static void UnitTestCuMatrixAddMatTp() {
  for (MatrixIndexT i = 1; i < 10; i++) {
    MatrixIndexT dim = 5 * i + rand() % 10;
    
    Matrix<Real> A(dim, dim);
    Matrix<Real> B(dim, dim);
    TpMatrix<Real> C(dim);
    A.SetRandn();
    B.SetRandn();
    C.SetRandn();
    CuMatrix<Real> D(A);
    CuMatrix<Real> E(B);
    CuTpMatrix<Real> F(C);
    
    A.AddMatTp(1.0, B, kNoTrans, C, kNoTrans, 1.0);
    D.AddMatTp(1.0, E, kNoTrans, F, kNoTrans, 1.0);

    CuMatrix<Real> G(A);
    AssertEqual<Real>(G, D);
  }
}

template<typename Real>
static void UnitTestCuMatrixAddTpMat() {
  for (MatrixIndexT i = 1; i < 10; i++) {
    MatrixIndexT dim = 5 * i + rand() % 10;
    
    Matrix<Real> A(dim, dim);
    Matrix<Real> B(dim, dim);
    TpMatrix<Real> C(dim);
    A.SetRandn();
    B.SetRandn();
    C.SetRandn();
    CuMatrix<Real> D(A);
    CuMatrix<Real> E(B);
    CuTpMatrix<Real> F(C);
    
    A.AddTpMat(1.0, C, kNoTrans, B, kNoTrans, 1.0);
    D.AddTpMat(1.0, F, kNoTrans, E, kNoTrans, 1.0);

    CuMatrix<Real> G(A);
    AssertEqual<Real>(G, D);
  }
}

/*
 * CuVector unit tests
 */
template<typename Real> 
static void UnitTestCuVectorAddVec() {
  Vector<Real> Hv(777);
  Vector<Real> Hw(777);
  InitRand(&Hv);
  InitRand(&Hw);

  CuVector<Real> Dv(777);
  CuVector<Real> Dw(777);
  Dv.CopyFromVec(Hv);
  Dw.CopyFromVec(Hw);

  Dv.AddVec(0.1,Dw,0.9);
  Hv.Scale(0.9);
  Hv.AddVec(0.1,Hw);

  Vector<Real> Hv2(777);
  Dv.CopyToVec(&Hv2);
  
  AssertEqual(Hv,Hv2);
}



template<typename Real> 
static void UnitTestCuVectorAddRowSumMat() {
 const int32 X=4321, Y=19;
  Real alpha=0.1, beta=0.7;

  Matrix<Real> Hm(X,Y);
  Vector<Real> Hv(Y);
  Vector<Real> Hv_accu(Y);
  RandGaussMatrix(&Hm);
  InitRand(&Hv);

  CuMatrix<Real> Dm(X,Y);
  CuVector<Real> Dv(Y);
  Dm.CopyFromMat(Hm);
  Dv.CopyFromVec(Hv);

  Dv.AddRowSumMat(alpha,Dm,beta);
  
  Hv_accu.SetZero();
  Hv_accu.AddRowSumMat(1.0, Hm);
  Hv.Scale(beta);
  Hv.AddVec(alpha,Hv_accu);

  Vector<Real> Hv2(Y);
  Dv.CopyToVec(&Hv2);

  AssertEqual(Hv,Hv2);
}



template<typename Real> 
static void UnitTestCuVectorAddRowSumMatLarge() {
  Matrix<Real> Hm(1000,990);
  Vector<Real> Hv(990);
  Vector<Real> Hv_accu(990);
  RandGaussMatrix(&Hm);
  InitRand(&Hv);

  CuMatrix<Real> Dm(1000,990);
  CuVector<Real> Dv(990);
  Dm.CopyFromMat(Hm);
  Dv.CopyFromVec(Hv);

  Dv.AddRowSumMat(0.5,Dm,0.7);
  
  Hv_accu.SetZero();
  Hv_accu.AddRowSumMat(1.0, Hm);
  Hv.Scale(0.7);
  Hv.AddVec(0.5,Hv_accu);

  Vector<Real> Hv2(990);
  Dv.CopyToVec(&Hv2);

  AssertEqual(Hv,Hv2);
}



template<typename Real> 
static void UnitTestCuVectorAddColSumMat() {
  const int32 X=19, Y=4321;
  Real alpha=0.5, beta=0.7;

  Matrix<Real> Hm(X,Y);
  Vector<Real> Hv(X);
  Vector<Real> Hv_accu(X);
  RandGaussMatrix(&Hm);
  InitRand(&Hv);

  CuMatrix<Real> Dm(X,Y);
  CuVector<Real> Dv(X);
  Dm.CopyFromMat(Hm);
  Dv.CopyFromVec(Hv);

  Dv.AddColSumMat(alpha,Dm,beta);
  
  Hv_accu.SetZero();
  Hv_accu.AddColSumMat(1.0, Hm);
  Hv.Scale(beta);
  Hv.AddVec(alpha, Hv_accu);

  Vector<Real> Hv2(X);
  Dv.CopyToVec(&Hv2);

  AssertEqual(Hv,Hv2);
}

template<typename Real> 
static void UnitTestCuSubMatrix() {
  for (int32 iter = 0 ; iter < 10; iter++) {
    int32 M1 = 1 + rand () % 10, M2 = 1 + rand() % 1, M3 = 1 + rand() % 10, M = M1 + M2 + M3,
        N1 = 1 + rand () % 10, N2 = 1 + rand() % 1, N3 = 1 + rand() % 10, N = N1 + N2 + N3,
        m = rand() % M2, n = rand() % N2;
    CuMatrix<Real> mat(M, N);
    mat.SetRandn();
    CuSubMatrix<Real> submat1(mat, M1, M2,
                              N1, N2),
        submat2 = mat.Range(M1, M2, N1, N2);
    Real f1 = mat(M1 + m, N1 + n), f2 = submat1(m, n), f3 = submat2(m, n);
    KALDI_ASSERT(f1 == f2);
    KALDI_ASSERT(f2 == f3);
  }
}



template<typename Real> 
static void UnitTestCuVectorAddColSumMatLarge() {
  Matrix<Real> Hm(1000,990);
  Vector<Real> Hv(1000);
  Vector<Real> Hv_accu(1000);
  RandGaussMatrix(&Hm);
  InitRand(&Hv);

  CuMatrix<Real> Dm(1000,990);
  CuVector<Real> Dv(1000);
  Dm.CopyFromMat(Hm);
  Dv.CopyFromVec(Hv);

  Dv.AddColSumMat(0.5, Dm, 0.7);
  
  Hv_accu.SetZero();
  Hv_accu.AddColSumMat(1.0, Hm);
  Hv.Scale(0.7);
  Hv.AddVec(0.5,Hv_accu);

  Vector<Real> Hv2(1000);
  Dv.CopyToVec(&Hv2);

  AssertEqual(Hv,Hv2);
}



template<typename Real> 
static void UnitTestCuVectorInvertElements() {
  Vector<Real> Hv(777);
  InitRand(&Hv);

  CuVector<Real> Dv(777);
  Dv.CopyFromVec(Hv);

  Dv.InvertElements();
  Hv.InvertElements();

  Vector<Real> Hv2(777);
  Dv.CopyToVec(&Hv2);
  
  AssertEqual(Hv,Hv2);
}

template<class Real>
static void UnitTestCuMatrixIO() {
  for (int32 i = 0; i < 10; i++) {
    int32 dimM = 100 + rand() % 255, dimN = 10 + rand() % 20;
    if (i % 2 == 0) std::swap(dimM, dimN);
    if (i % 5 == 0) { dimM = 0; dimN = 0; }
    CuMatrix<Real> mat(dimM, dimN);
    mat.SetRandn();
    std::ostringstream os;
    bool binary = (i % 4 < 2);
    mat.Write(os, binary);

    CuMatrix<Real> mat2;
    std::istringstream is(os.str());
    mat2.Read(is, binary);
    AssertEqual(mat, mat2);
  }
}


template<typename Real>
static void UnitTestCuVectorAddTpVec() {
  Vector<Real> Hv(777);
  InitRand(&Hv);
  CuVector<Real> Dv(777);
  Dv.CopyFromVec(Hv);
  Vector<Real> Hv1(777);
  InitRand(&Hv1);
  CuVector<Real> Dv1(777);
  Dv1.CopyFromVec(Hv1);

  TpMatrix<Real> Hm(777);
  Hm.SetRandn();
  CuTpMatrix<Real> Dm(Hm);

  //gpu
  Dv.AddTpVec(1.0,Dm,kNoTrans,Dv1,1.0);
  //cpu
  Hv.AddTpVec(1.0,Hm,kNoTrans,Hv1,1.0);

  Vector<Real> Hv2(777);
  Dv.CopyToVec(&Hv2);

  AssertEqual(Hv,Hv2);
}

template<typename Real> 
static void UnitTestCuApproxEqual() {
  Real tol = 0.1;
  for (int32 i = 0; i < 10; i++) {
    int32 M = 1 + rand() % 10, N = 1 + rand() % 10;
    CuMatrix<Real> A(M, N), B(M, N);
    A.SetRandn();
    B.SetRandn();
    Matrix<Real> diff(A), Bm(B);
    diff.AddMat(-1.0, Bm);
    Real norm = diff.FrobeniusNorm();
    KALDI_ASSERT( (norm <= tol) == (A.ApproxEqual(B, tol)));
    tol *= 2.0;
  }
}

template<typename Real> 
static void UnitTestCuVectorMulTp() {
  Vector<Real> Hv(777);
  InitRand(&Hv);
  CuVector<Real> Dv(777);
  Dv.CopyFromVec(Hv);

  TpMatrix<Real> Hm(777);
  Hm.SetRandn();
  CuTpMatrix<Real> Dm(Hm);

  //gpu
  Dv.MulTp(Dm,kNoTrans);
  //cpu
  Hv.MulTp(Hm,kNoTrans);

  Vector<Real> Hv2(777);
  Dv.CopyToVec(&Hv2);

  AssertEqual(Hv,Hv2);
}

template<typename Real, typename OtherReal> 
static void UnitTestCuCopy() {
  for (int32 i = 0; i < 10; i++) {
    int32 M = 1 + rand() % 10, N = 1 + rand() % 10;
    CuMatrix<Real> A(M, N);
    CuMatrix<OtherReal> B(A, kTrans);
    CuMatrix<Real> C(B, kTrans);
    CuMatrix<Real> D(N, M);
    D.CopyFromMat(C, kTrans);
    CuMatrix<OtherReal> E(N, M);
    E.CopyFromMat(D, kNoTrans);
    CuMatrix<Real> F(M, N);
    F.CopyFromMat(E, kTrans);

    Matrix<OtherReal> G(M, N);
    G.CopyFromMat(F, kNoTrans);
    CuMatrix<Real> H(N, M);
    H.CopyFromMat(G, kTrans);
    Matrix<OtherReal> I(M, N);
    I.CopyFromMat(H, kTrans);
    CuMatrix<Real> J(I, kTrans);
    Matrix<OtherReal> K(J, kTrans);
    CuMatrix<Real> L(K, kNoTrans);
    
    KALDI_ASSERT(A.ApproxEqual(L));
  }

}

template<typename Real> 
static void UnitTestCuSigmoid() {
  Matrix<Real> Hi(100,111);
  Matrix<Real> Ho(100,111);
  RandGaussMatrix(&Hi);

  CuMatrix<Real> Di(100,111);
  CuMatrix<Real> Do(100,111);
  Di.CopyFromMat(Hi);

  //gpu
  Do.Sigmoid(Di);
  //cpu
  for(MatrixIndexT r=0; r < Hi.NumRows(); r++) {
    for(MatrixIndexT c=0; c < Hi.NumCols(); c++) {
      Ho(r, c) = 1.0/(1.0+exp(-Hi(r, c)));
    }
  }

  Matrix<Real> Ho2(100,111);
  Do.CopyToMat(&Ho2);

  AssertEqual(Ho,Ho2);
}



template<typename Real> 
static void UnitTestCuDiffSigmoid() {
  Matrix<Real> Hi(100,111);
  Matrix<Real> Ho(100,111);
  Matrix<Real> Hy(100,111);
  RandGaussMatrix(&Hi);
  RandZeroToOneMatrix(&Hy);

  CuMatrix<Real> Di(100,111);
  CuMatrix<Real> Do(100,111);
  CuMatrix<Real> Dy(100,111);
  Di.CopyFromMat(Hi);
  Dy.CopyFromMat(Hy);

  //gpu
  Do.DiffSigmoid(Dy, Di);
  //cpu
  for(MatrixIndexT r=0; r<Ho.NumRows(); r++) {
    for(MatrixIndexT c=0; c<Ho.NumCols(); c++) {
      Ho(r, c) = Hy(r, c)*(1.0 - Hy(r, c)) * Hi(r, c);
    }
  }

  Matrix<Real> Ho2(100,111);
  Do.CopyToMat(&Ho2);

  AssertEqual(Ho,Ho2);
}



template<typename Real> 
static void UnitTestCuSoftmax() {

  for (int32 i = 0; i < 5; i++) {
    int row = 100 + rand() % 400;
    int col = 100 + rand() % 500;

    Matrix<Real> Hi(row,col);
    Matrix<Real> Ho(row,col);
    RandGaussMatrix(&Hi);
    Hi.Scale(5.0);
  
    CuMatrix<Real> Di(row, col);
    CuMatrix<Real> Do(row, col);
    Di.CopyFromMat(Hi);

    //gpu
    Do.ApplySoftMaxPerRow(Di);
    //cpu
    Ho.CopyFromMat(Hi);
    for(MatrixIndexT r=0; r<Ho.NumRows(); r++) {
      Ho.Row(r).ApplySoftMax();
    }

    Matrix<Real> Ho2(Do);

    AssertEqual(Ho,Ho2,0.00001);
  }
}



template<typename Real> 
static void UnitTestCuFindRowMaxId() {
  for (int32 i = 0; i < 5; i++) {
    int32 dimM = 100 + rand() % 200, dimN = 100 + rand() % 200;
    Matrix<Real> Hi(dimM, dimN);
    RandGaussMatrix(&Hi);

    CuMatrix<Real> Di(dimM, dimN);
    Di.CopyFromMat(Hi);

    std::vector<int32> Hmax(dimM);
    CuArray<int32> Dmax(dimN);

    //gpu
    Di.FindRowMaxId(&Dmax);

    //cpu
    for(MatrixIndexT r=0; r<Hi.NumRows(); r++) {
      Real max=-1e20; int32 idx=-1;
      for(MatrixIndexT c=0; c<Hi.NumCols(); c++) {
        if(Hi(r,c) > max) { idx=c; max=Hi(r,c); }
      }
      Hmax[r] = idx;
    }

    std::vector<int32> Hmax2(dimM);
    Dmax.CopyToVec(&Hmax2);

    AssertEqual(Hmax,Hmax2);
  }
}



template<typename Real> 
static void UnitTestCuDiffXent() {
  int32 X=100, Y=111;
  //nnet output / diff
  Matrix<Real> Hi(X,Y);
  RandZeroToOneMatrix(&Hi);
  CuMatrix<Real> Di(X,Y);
  Di.CopyFromMat(Hi);
  //target vector
  std::vector<int32> Htgt(X);
  for(int32 i=0; i<X; i++) {
    Htgt[i] = rand()%Y;
  }
  CuArray<int32> Dtgt(X);
  Dtgt.CopyFromVec(Htgt);
  //logpost vector
  Vector<Real> Hlogpost(X);
  CuVector<Real> Dlogpost(X);
  
  //gpu
  Di.DiffXent(Dtgt, &Dlogpost);
  //cpu
  for(MatrixIndexT r=0; r<Hi.NumRows(); r++) {
    int32 col_tgt = Htgt[r];
    Hlogpost(r) = log(Hi(r, col_tgt));
    Hi(r, col_tgt) -= 1.0;
  }

  Matrix<Real> Hi2(X,Y);
  Di.CopyToMat(&Hi2);
  Vector<Real> Hlogpost2(X);
  Dlogpost.CopyToVec(&Hlogpost2);

  AssertEqual(Hi,Hi2);
  AssertEqual(Hlogpost,Hlogpost2);
}

template<typename Real> void UnitTestCheck() {
  Matrix<Real> Hi(100,111);
  RandGaussMatrix(&Hi);

  CuMatrix<Real> Di(100,111);
  Di.CopyFromMat(Hi);

  CuMatrix<Real> Dj(Di);
  KALDI_LOG << Dj.NumRows() << '\n';
 

}

template<typename Real>
void UnitTestSwapCu2Cu() {
  Matrix<Real> Hi(100,111);
  RandGaussMatrix(&Hi);
  CuMatrix<Real> Di(100,111);
  Di.CopyFromMat(Hi);

  Matrix<Real> Hi2(110,121);
  RandGaussMatrix(&Hi2);
  CuMatrix<Real> Di2(110,121);
  Di2.CopyFromMat(Hi2);

  Di.Swap(&Di2);
  Matrix<Real> Hf(Di.NumRows(), Di.NumCols());
  Di.CopyToMat(&Hf);
  Matrix<Real> Hf2(Di2.NumRows(), Di2.NumCols());
  Di2.CopyToMat(&Hf2);
  AssertEqual(Hi,Hf2);
  AssertEqual(Hi2,Hf);
}

template<typename Real>
void UnitTestSwapCu2M() {
  Matrix<Real> Hi(100,111);
  RandGaussMatrix(&Hi);
  CuMatrix<Real> Di(100,111);
  Di.CopyFromMat(Hi);

  Matrix<Real> Hi2(110,121);
  RandGaussMatrix(&Hi2);
  Matrix<Real> Di2(110,121);
  Di2.CopyFromMat(Hi2);

  Di.Swap(&Hi2);
  Matrix<Real> Hf(Di.NumRows(), Di.NumCols());
  Di.CopyToMat(&Hf);
  AssertEqual(Di2,Hf);
  AssertEqual(Hi2,Hi);
}


template<typename Real>
void UnitTestCuTanh() {
  Matrix<Real> H(100,110);
  RandGaussMatrix(&H);
  CuMatrix<Real> D(100,110);
  D.CopyFromMat(H);
  
  //gpu
  CuMatrix<Real> Di(100,110);
  Di.Tanh(D);
  Matrix<Real> Df(Di.NumRows(), Di.NumCols());
  Di.CopyToMat(&Df);

  //cpu
  Matrix<Real> Hf(H.NumRows(), H.NumCols());
  Hf.Tanh(H);
  AssertEqual(Df,Hf);
}

template<typename Real> 
static void UnitTestCuDiffTanh() {
  Matrix<Real> Hi(100,111);
  Matrix<Real> Ho(100,111);
  Matrix<Real> Hy(100,111);
  RandGaussMatrix(&Hi);
  RandZeroToOneMatrix(&Hy);

  CuMatrix<Real> Di(100,111);
  CuMatrix<Real> Do(100,111);
  CuMatrix<Real> Dy(100,111);
  Di.CopyFromMat(Hi);
  Dy.CopyFromMat(Hy);

  //gpu
  Do.DiffTanh(Dy, Di);
  //cpu
  for(MatrixIndexT r=0; r<Ho.NumRows(); r++) {
    for(MatrixIndexT c=0; c<Ho.NumCols(); c++) {
      Ho(r, c) = (1.0 - Hy(r, c)*Hy(r, c)) * Hi(r, c);
    }
  }

  Matrix<Real> Ho2(100,111);
  Do.CopyToMat(&Ho2);

  AssertEqual(Ho,Ho2);
}

// just need this for testing function below.  Compute n!!
static int32 DoubleFactorial(int32 i) {
  if (i <= 0) { return 1; } else { return i * DoubleFactorial(i - 2); }
}

template <typename Real>
static void UnitTestCuMatrixSetRandn() {

  { // First test consistency when called twice.
    int32 dimM = 100 + rand() % 200, dimN = 100 + rand() % 200;
    Matrix<Real> M(dimM, dimN), N(dimM, dimN);
    srand(104);
    M.SetRandn();
    srand(104);
    N.SetRandn();
    AssertEqual(M, N);
  }
    
  for (MatrixIndexT i = 0; i < 5; i++) {
    MatrixIndexT rows = 100 + rand() % 50, cols = 100 + rand() % 50;
    CuMatrix<Real> M(rows, cols);
    M.SetRandn();

    for (MatrixIndexT pow = 1; pow < 5; pow++) {
      // test moments 1 through 4 of
      // the distribution.
      CuMatrix<Real> Mpow(M);
      Mpow.ApplyPow(pow);
      Real observed_moment = Mpow.Sum() / (rows * cols);
      // see http://en.wikipedia.org/wiki/Normal_distribution#Moments,
      // note that mu = 0 and sigma = 1.
      Real expected_moment = (pow % 2 == 1 ? 0 : DoubleFactorial(pow - 1));
      Real k = 10.0; // This is just a constant we use to give us some wiggle
                     // room before rejecting the distribution... e.g. 20 sigma,
                     // quite approximately.
      Real allowed_deviation = k * pow / sqrt(static_cast<Real>(rows * cols));
      // give it a bit more wiggle room for higher powers.. this is quite
      // unscientific, it would be better to involve the absolute moments or
      // something like that, and use one of those statistical inequalities,
      // but it involves the gamma function and it's too much hassle to implement.
      Real lower_bound = expected_moment - allowed_deviation,
          upper_bound = expected_moment + allowed_deviation;
      KALDI_ASSERT(observed_moment >= lower_bound && observed_moment <= upper_bound);
    }
  }
}


template <typename Real>
static void UnitTestCuMatrixSetRandUniform() {
  for (MatrixIndexT i = 0; i < 5; i++) {
    MatrixIndexT rows = 180 + rand() % 200, cols = 200 + rand() % 200;
    CuMatrix<Real> M(rows, cols);
    M.SetRandUniform();

    M.Add(-0.5); // we'll be testing the central moments, so
    // center it around zero first.
    // Got these moments from http://mathworld.wolfram.com/UniformDistribution.html
    Vector<Real> central_moments(5);
    central_moments(0) = 0.0;
    central_moments(1) = 0.0;
    central_moments(2) = 1.0 / 12; // times (b - a)^2, which equals 1.
    central_moments(3) = 0.0;
    central_moments(4) = 1.0 / 80; // times (b - a)^4, which equals 1.

    for (MatrixIndexT pow = 1; pow < central_moments.Dim(); pow++) {
      CuMatrix<Real> Mpow(M);
      Mpow.ApplyPow(pow);
      Real observed_moment = Mpow.Sum() / (rows * cols);
      // see http://en.wikipedia.org/wiki/Normal_distribution#Moments,
      // note that mu = 0 and sigma = 1.
      Real expected_moment = central_moments(pow);
      Real k = 20.0; // This is just a constant we use to give us some wiggle
                     // room before rejecting the distribution... e.g. 10 sigma,
                     // quite approximately.
      Real allowed_deviation = k / sqrt(static_cast<Real>(rows * cols));
      Real lower_bound = expected_moment - allowed_deviation,
          upper_bound = expected_moment + allowed_deviation;
      if (!(observed_moment >= lower_bound && observed_moment <= upper_bound)) {
        KALDI_LOG << "Random matrix is " << M;
        KALDI_ERR << "Bad observed " << pow <<  "'th moment " << observed_moment
                  << ", expected " << expected_moment << ", allowed range "
                  << lower_bound << " to " << upper_bound;
      }
    }
  }
}



template<typename Real> void CudaMatrixUnitTest() {
  //test CuMatrix<Real> methods by cross-check with Matrix
  UnitTestCuMatrixCopyCross<Real>();
  UnitTestCuMatrixCopyCross2<Real>();
  UnitTestCuMatrixApplyLog<Real>();
  UnitTestCuMatrixSetRandn<Real>();
  UnitTestCuMatrixSetRandUniform<Real>();
  UnitTestCuMatrixScale<Real>();
  UnitTestCuMatrixSigmoid<Real>();
  UnitTestCuMatrixTraceMatMat<Real>();
  UnitTestCuMatrixSoftHinge<Real>();
  UnitTestCuMatrixApplyPow<Real>();
  UnitTestCuMatrixSet<Real>();
  UnitTestCuMatrixAdd<Real>();
  UnitTestCuMatrixApplyFloor<Real>();
  UnitTestCuMatrixApplyHeaviside<Real>();
  UnitTestCuMatrixMulElements<Real>();
  UnitTestCuMatrixMax<Real>();
  UnitTestCuMatrixMulColsVec<Real>();
  UnitTestCuMatrixMulRowsVec<Real>();
  UnitTestCuMatrixDivRowsVec<Real>();
  UnitTestCuMatrixAddMat<Real>();
  UnitTestCuMatrixSum<Real>();
  UnitTestCuMatrixAddVecToCols<Real>();
  UnitTestCuMatrixAddVecToRows<Real>();
  UnitTestCuMatrixAddMatMat<Real>();
  UnitTestCuMatrixCopyFromMat<Real>();
  UnitTestCuMatrixCopyFromTp<Real>();
  UnitTestCuMatrixAddMatTp<Real>();
  UnitTestCuMatrixCopyCols<Real>();
  UnitTestCuMatrixCopyRows<Real>();
  UnitTestCuMatrixCopyRowsFromVec<Real>();
  UnitTestCuMatrixAddTpMat<Real>();
  //test CuVector<Real> methods
  UnitTestCuVectorAddVec<Real>();
  UnitTestCuVectorAddRowSumMat<Real>();
  UnitTestCuVectorAddRowSumMatLarge<Real>();
  UnitTestCuVectorAddColSumMat<Real>();
  UnitTestCuVectorAddColSumMatLarge<Real>();
  UnitTestCuSubMatrix<Real>();
  UnitTestCuVectorInvertElements<Real>();
  UnitTestCuMatrixIO<Real>();
  UnitTestCuSigmoid<Real>();
  UnitTestCuApproxEqual<Real>();
  UnitTestCuCopy<Real, float>();
#if HAVE_CUDA == 1  
  if (CuDevice::Instantiate().DoublePrecisionSupported())
#endif
    UnitTestCuCopy<Real, double>();
  UnitTestCuDiffSigmoid<Real>();
  UnitTestCuFindRowMaxId<Real>();
  UnitTestCuSoftmax<Real>();
  UnitTestCuDiffXent<Real>();
  UnitTestCheck<Real>();
  UnitTestSwapCu2Cu<Real>();
  UnitTestSwapCu2M<Real>();
  UnitTestCuMatrixAddDiagVecMat<Real>();
  UnitTestCuTanh<Real>();
  UnitTestCuDiffTanh<Real>();
  UnitTestCuVectorAddTpVec<Real>();
  UnitTestCuVectorMulTp<Real>();
}


} // namespace kaldi


int main() {
  for (int32 loop = 0; loop < 2; loop++) {
#if HAVE_CUDA == 1
    if (loop == 0)
      CuDevice::Instantiate().SelectGpuId(-1); // -1 means no GPU
    else
      CuDevice::Instantiate().SelectGpuId(-2); // -2 .. automatic selection
#endif

    kaldi::CudaMatrixUnitTest<float>();

    CuDevice::Instantiate().PrintMemoryUsage();
    
#if HAVE_CUDA == 1
    if (CuDevice::Instantiate().DoublePrecisionSupported()) {
      kaldi::CudaMatrixUnitTest<double>();
    } else {
      KALDI_WARN << "Double precision not supported";
    }
#else
    kaldi::CudaMatrixUnitTest<double>();
#endif

    if (loop == 0)
      KALDI_LOG << "Tests without GPU use succeeded.\n";
    else
      KALDI_LOG << "Tests with GPU use (if available) succeeded.\n";
  }
#if HAVE_CUDA == 1
  CuDevice::Instantiate().PrintProfile();
#endif
  return 0;
}
