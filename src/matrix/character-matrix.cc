#include "character-matrix.h"
#include <iostream>
int main() {
  CharacterMatrix<float> a(2, 2,1);
  a.SetZero();
  a.Set(10);
  a.Resize(4,3,5);
  std::cout << "row number : " << a.NumRows() << " a(0, 0) = " << a(0,0)<< std::endl ;
  CharacterMatrix<float> b(4,3,2);
  //CharacterMatrix<float> c(3,4,0);
  //c.Transpose(b);
  //std::cout << "row number : " << c.NumRows() << " c(2, 3) = " << c(2,3)<< std::endl ;
  return 0;
}
