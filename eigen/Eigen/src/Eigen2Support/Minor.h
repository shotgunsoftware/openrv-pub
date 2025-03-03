// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2006-2009 Benoit Jacob <jacob.benoit.1@gmail.com>
//
// Eigen is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// Alternatively, you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Eigen is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License or the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License and a copy of the GNU General Public License along with
// Eigen. If not, see <http://www.gnu.org/licenses/>.

#ifndef EIGEN_MINOR_H
#define EIGEN_MINOR_H

/**
 * \class Minor
 *
 * \brief Expression of a minor
 *
 * \param MatrixType the type of the object in which we are taking a minor
 *
 * This class represents an expression of a minor. It is the return
 * type of MatrixBase::minor() and most of the time this is the only way it
 * is used.
 *
 * \sa MatrixBase::minor()
 */

namespace internal
{
    template <typename MatrixType>
    struct traits<Minor<MatrixType>> : traits<MatrixType>
    {
        typedef typename nested<MatrixType>::type MatrixTypeNested;
        typedef
            typename remove_reference<MatrixTypeNested>::type _MatrixTypeNested;
        typedef typename MatrixType::StorageKind StorageKind;

        enum
        {
            RowsAtCompileTime = (MatrixType::RowsAtCompileTime != Dynamic)
                                    ? int(MatrixType::RowsAtCompileTime) - 1
                                    : Dynamic,
            ColsAtCompileTime = (MatrixType::ColsAtCompileTime != Dynamic)
                                    ? int(MatrixType::ColsAtCompileTime) - 1
                                    : Dynamic,
            MaxRowsAtCompileTime =
                (MatrixType::MaxRowsAtCompileTime != Dynamic)
                    ? int(MatrixType::MaxRowsAtCompileTime) - 1
                    : Dynamic,
            MaxColsAtCompileTime =
                (MatrixType::MaxColsAtCompileTime != Dynamic)
                    ? int(MatrixType::MaxColsAtCompileTime) - 1
                    : Dynamic,
            Flags = _MatrixTypeNested::Flags & (HereditaryBits | LvalueBit),
            CoeffReadCost =
                _MatrixTypeNested::CoeffReadCost // minor is used typically on
                                                 // tiny matrices, where loops
                                                 // are unrolled and the 'if'
                                                 // evaluates at compile time
        };
    };
} // namespace internal

template <typename MatrixType>
class Minor : public MatrixBase<Minor<MatrixType>>
{
public:
    typedef MatrixBase<Minor> Base;
    EIGEN_DENSE_PUBLIC_INTERFACE(Minor)

    inline Minor(const MatrixType& matrix, Index row, Index col)
        : m_matrix(matrix)
        , m_row(row)
        , m_col(col)
    {
        eigen_assert(row >= 0 && row < matrix.rows() && col >= 0
                     && col < matrix.cols());
    }

    EIGEN_INHERIT_ASSIGNMENT_OPERATORS(Minor)

    inline Index rows() const { return m_matrix.rows() - 1; }

    inline Index cols() const { return m_matrix.cols() - 1; }

    inline Scalar& coeffRef(Index row, Index col)
    {
        return m_matrix.const_cast_derived().coeffRef(row + (row >= m_row),
                                                      col + (col >= m_col));
    }

    inline const Scalar coeff(Index row, Index col) const
    {
        return m_matrix.coeff(row + (row >= m_row), col + (col >= m_col));
    }

protected:
    const typename MatrixType::Nested m_matrix;
    const Index m_row, m_col;
};

/**
 * \return an expression of the (\a row, \a col)-minor of *this,
 * i.e. an expression constructed from *this by removing the specified
 * row and column.
 *
 * Example: \include MatrixBase_minor.cpp
 * Output: \verbinclude MatrixBase_minor.out
 *
 * \sa class Minor
 */
template <typename Derived>
inline Minor<Derived> MatrixBase<Derived>::minor(Index row, Index col)
{
    return Minor<Derived>(derived(), row, col);
}

/**
 * This is the const version of minor(). */
template <typename Derived>
inline const Minor<Derived> MatrixBase<Derived>::minor(Index row,
                                                       Index col) const
{
    return Minor<Derived>(derived(), row, col);
}

#endif // EIGEN_MINOR_H
