// Copyright (C) 2008-today The SG++ project
// This file is part of the SG++ project. For conditions of distribution and
// use, please see the copyright notice provided with SG++ or at
// sgpp.sparsegrids.org

#ifndef ALGORITHMEVALUATIONTRANSPOSED_HPP
#define ALGORITHMEVALUATIONTRANSPOSED_HPP

#include <sgpp/base/grid/GridStorage.hpp>
#include <sgpp/base/datatypes/DataVector.hpp>
#include <sgpp/base/operation/hash/common/basis/LinearBoundaryBasis.hpp>

#include <sgpp/globaldef.hpp>

#include <utility>


namespace sgpp {
namespace base {

/**
 * Basic algorithm for getting all affected basis functions.
 * This implicitly assumes a tensor-product approach and local support.
 * No grid points on the border are supported.
 */
template<class BASIS>
class AlgorithmEvaluationTransposed {
 public:
  explicit AlgorithmEvaluationTransposed(GridStorage& storage) :
    storage(storage) {
  }

  ~AlgorithmEvaluationTransposed() {
  }

  /**
   * Returns evaluations of all basis functions that are non-zero at a given evaluation point.
   * For a given evaluation point \f$x\f$, it stores tuples (std::pair) of
   * \f$(i,\phi_i(x))\f$ in the result vector for all basis functions that are non-zero.
   * If one wants to evaluate \f$f_N(x)\f$, one only has to compute
   * \f[ \sum_{r\in\mathbf{result}} \alpha[r\rightarrow\mathbf{first}] \cdot r\rightarrow\mathbf{second}. \f]
   *
   * @param basis a sparse grid basis
   * @param point evaluation point within the domain
   * @param alpha the coefficient of the regarded ansatzfunction
   * @param result vector that will contain the local support of the given ansatzfuction for all evaluations points
   */
  void operator()(BASIS& basis, const DataVector& point, double alpha, DataVector& result) {
    GridStorage::grid_iterator working(storage);

    const size_t bits = sizeof(index_t) * 8;  // how many levels can we store in a index_type?
    const size_t dim = storage.getDimension();

    // Check for bounding box
    BoundingBox* bb = storage.getBoundingBox();
    DataVector newPoint(dim);

    for (size_t d = 0; d < dim; d++) {
      if (!bb->isContainingPoint(d, point[d])) {
        return;
      }

      newPoint[d] = bb->transformPointToUnitCube(d, point[d]);
    }

    index_t* source = new index_t[dim];

    for (size_t d = 0; d < dim; d++) {
      // This does not really work on grids with borders.
      const double temp = std::floor(newPoint[d] * static_cast<double>(1 << (bits - 2))) * 2.0;

      if (newPoint[d] == 1.0) {
        source[d] = static_cast<index_t>(temp - 1);
      } else {
        source[d] = static_cast<index_t>(temp + 1);
      }
    }

    rec(basis, newPoint, 0, 1.0, working, source, alpha, result);
    delete[] source;
  }

 protected:
  GridStorage& storage;

  /**
   * Recursive traversal of the "tree" of basis functions for evaluation, used in operator().
   * For a given evaluation point \f$x\f$, it stores tuples (std::pair) of
   * \f$(i,\phi_i(x))\f$ in the result vector for all basis functions that are non-zero.
   *
   * @param basis a sparse grid basis
   * @param point evaluation point within the domain
   * @param current_dim the dimension currently looked at (recursion parameter)
   * @param value the value of the evaluation of the current basis function up to (excluding) dimension current_dim (product of the evaluations of the one-dimensional ones)
   * @param working iterator working on the GridStorage of the basis
   * @param source array of indices for each dimension (identifying the indices of the current grid point)
   * @param alpha the coefficient of current ansatzfunction
   * @param result vector that will contain the local support of the given ansatzfuction for all evaluations points
   */
  void rec(BASIS& basis, DataVector& point, size_t current_dim,
           double value, GridStorage::grid_iterator& working,
           index_t* source, double alpha,
           DataVector& result) {
    const unsigned int BITS_IN_BYTE = 8;
    // maximum possible level for the index type
    const level_t max_level = static_cast<level_t>(sizeof(index_t) * BITS_IN_BYTE - 1);
    const index_t src_index = source[current_dim];

    level_t work_level = 1;

    while (true) {
      const size_t seq = working.seq();
      bool hint;

      if (storage.isInvalidSequenceNumber(seq)) {
        working.resetToLevelOne(current_dim);
        break;
      } else {
        index_t work_index;
        level_t temp;

        working.get(current_dim, temp, work_index);
        hint = working.hint();

        const double new_value = basis.eval(work_level, work_index, point[current_dim]) * value;

        if (current_dim == storage.getDimension() - 1) {
          result[seq] += alpha * new_value;
        } else {
          rec(basis, point, current_dim + 1, new_value, working, source, alpha, result);
          if (!hint) working.resetToLevelOne(current_dim+1);
        }
      }

      if (working.hint()) {
        break;
      }

      // this decides in which direction we should descend by evaluating
      // the corresponding bit
      // the bits are coded from left to right starting with level 1
      // being in position max_level
      const bool right = (src_index & (1 << (max_level - work_level))) > 0;
      work_level++;

      if (right) {
        working.rightChild(current_dim);
      } else {
        working.leftChild(current_dim);
      }
    }
  }
};

}  // namespace base
}  // namespace sgpp

#endif /* ALGORITHMEVALUATIONTRANSPOSED_HPP */