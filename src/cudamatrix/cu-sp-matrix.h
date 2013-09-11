#ifndef KALDI_CUDAMATRIX_CU_SP_MATRIX_H_
#define KALDI_CUDAMATRIX_CU_SP_MATRIX_H_

#include <sstream>

#include "cudamatrix/cu-common.h"
#include "matrix/matrix-common.h"
#include "matrix/sp-matrix.h"
#include "cudamatrix/cu-array.h"
#include "cudamatrix/cu-math.h"
#include "cudamatrix/cu-packed-matrix.h"
#include "cudamatrix/cu-matrix.h"

namespace kaldi {

/// TraceSpSp returns tr(A B)
template<typename Real, typename OtherReal>
Real TraceSpSp(const CuSpMatrix<Real> &A, const CuSpMatrix<OtherReal> &B);

template<typename Real>
class CuSpMatrix : public CuPackedMatrix<Real> {
  friend class CuMatrixBase<Real>;
  friend class CuVectorBase<Real>;
  friend class CuSubMatrix<Real>;
  friend class CuRand<Real>;

  template<class R, class S>
  friend R TraceSpSp(const CuSpMatrix<R> &A, const CuSpMatrix<S> &B);
 public:
  
  CuSpMatrix(): CuPackedMatrix<Real>() {}
  
  explicit CuSpMatrix(MatrixIndexT r, MatrixResizeType resize_type = kSetZero)
    : CuPackedMatrix<Real>(r, resize_type) {}

  explicit CuSpMatrix(const SpMatrix<Real> &orig)
    : CuPackedMatrix<Real>(orig) {}

  explicit CuSpMatrix(const CuSpMatrix<Real> &orig)
    : CuPackedMatrix<Real>(orig) {}

  explicit CuSpMatrix(const CuMatrixBase<Real> &orig,
                      SpCopyType copy_type = kTakeLower)
      : CuPackedMatrix<Real>(orig.NumRows(), kUndefined) {
    CopyFromMat(orig, copy_type);
  }

  ~CuSpMatrix() {}  

  inline void Resize(MatrixIndexT nRows, MatrixResizeType resize_type = kSetZero) {
    CuPackedMatrix<Real>::Resize(nRows, resize_type);
  }

  Real FrobeniusNorm() const { return sqrt(TraceSpSp(*this, *this)); }
  
  void CopyFromSp(const CuSpMatrix<Real> &other) {
    CuPackedMatrix<Real>::CopyFromPacked(other);
  }
  void CopyFromSp(const SpMatrix<Real> &other) {
    CuPackedMatrix<Real>::CopyFromPacked(other);
  }

  void CopyFromMat(const CuMatrixBase<Real> &orig,
                   SpCopyType copy_type = kTakeLower);
  
  void CopyToSp(SpMatrix<Real> *dst) const { //added const by hxu
    CuPackedMatrix<Real>::CopyToPacked(dst);
  }

  inline CuValue<Real> operator() (MatrixIndexT r, MatrixIndexT c) {
    if (static_cast<UnsignedMatrixIndexT>(c) >
        static_cast<UnsignedMatrixIndexT>(r))
      std::swap(c, r);
    KALDI_ASSERT(static_cast<UnsignedMatrixIndexT>(r) <
                 static_cast<UnsignedMatrixIndexT>(this->num_rows_));
    return CuValue<Real>(this->data_ + (r * (r+1)) / 2 + c);
  }
  
  inline Real operator() (MatrixIndexT r, MatrixIndexT c) const {
    if (static_cast<UnsignedMatrixIndexT>(c) >
        static_cast<UnsignedMatrixIndexT>(r))
      std::swap(c, r);
    KALDI_ASSERT(static_cast<UnsignedMatrixIndexT>(r) <
                 static_cast<UnsignedMatrixIndexT>(this->num_rows_));
    return CuValue<Real>(this->data_ + (r * (r+1)) / 2 + c); // will be
    // casted to Real.
  }

  void Invert();

  void AddVec2(const Real alpha, const CuVectorBase<Real> &v);

  void AddMat2(const Real alpha, const CuMatrixBase<Real> &M,
               MatrixTransposeType transM, const Real beta);
  
  void AddSp(const Real alpha, const CuSpMatrix<Real> &Ma) {
    this->AddPacked(alpha, Ma);
  }

 protected:
  inline const SpMatrix<Real> &Mat() const {
    return *(reinterpret_cast<const SpMatrix<Real>* >(this));
  }
  inline SpMatrix<Real> &Mat() {
    return *(reinterpret_cast<SpMatrix<Real>* >(this));
  }
  
  

};



/*
template<typename Real>
template<typename OtherReal>
void SpMatrix<Real>::CopyFromCuSp(const CuSpMatrix<Real> &cu) {
   cu.CopyToSp(this);

}
*/ 

//added by hxu
template<typename Real>
SpMatrix<Real>::SpMatrix(const CuSpMatrix<Real> &cu) {
   Resize(cu.NumRows());
   cu.CopyToSp(this);
}



} // namespace

#endif