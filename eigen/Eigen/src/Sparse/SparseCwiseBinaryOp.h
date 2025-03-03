// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
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

#ifndef EIGEN_SPARSE_CWISE_BINARY_OP_H
#define EIGEN_SPARSE_CWISE_BINARY_OP_H

// Here we have to handle 3 cases:
//  1 - sparse op dense
//  2 - dense op sparse
//  3 - sparse op sparse
// We also need to implement a 4th iterator for:
//  4 - dense op dense
// Finally, we also need to distinguish between the product and other operations
// :
//                configuration      returned mode
//  1 - sparse op dense    product      sparse
//                         generic      dense
//  2 - dense op sparse    product      sparse
//                         generic      dense
//  3 - sparse op sparse   product      sparse
//                         generic      sparse
//  4 - dense op dense     product      dense
//                         generic      dense

namespace internal
{

    template <> struct promote_storage_type<Dense, Sparse>
    {
        typedef Sparse ret;
    };

    template <> struct promote_storage_type<Sparse, Dense>
    {
        typedef Sparse ret;
    };

    template <typename BinaryOp, typename Lhs, typename Rhs, typename Derived,
              typename _LhsStorageMode = typename traits<Lhs>::StorageKind,
              typename _RhsStorageMode = typename traits<Rhs>::StorageKind>
    class sparse_cwise_binary_op_inner_iterator_selector;

} // end namespace internal

template <typename BinaryOp, typename Lhs, typename Rhs>
class CwiseBinaryOpImpl<BinaryOp, Lhs, Rhs, Sparse>
    : public SparseMatrixBase<CwiseBinaryOp<BinaryOp, Lhs, Rhs>>
{
public:
    class InnerIterator;
    typedef CwiseBinaryOp<BinaryOp, Lhs, Rhs> Derived;
    EIGEN_SPARSE_PUBLIC_INTERFACE(Derived)
};

template <typename BinaryOp, typename Lhs, typename Rhs>
class CwiseBinaryOpImpl<BinaryOp, Lhs, Rhs, Sparse>::InnerIterator
    : public internal::sparse_cwise_binary_op_inner_iterator_selector<
          BinaryOp, Lhs, Rhs,
          typename CwiseBinaryOpImpl<BinaryOp, Lhs, Rhs, Sparse>::InnerIterator>
{
public:
    typedef typename Lhs::Index Index;
    typedef internal::sparse_cwise_binary_op_inner_iterator_selector<
        BinaryOp, Lhs, Rhs, InnerIterator>
        Base;

    EIGEN_STRONG_INLINE InnerIterator(const CwiseBinaryOpImpl& binOp,
                                      Index outer)
        : Base(binOp.derived(), outer)
    {
    }
};

/***************************************************************************
 * Implementation of inner-iterators
 ***************************************************************************/

// template<typename T> struct internal::func_is_conjunction { enum { ret =
// false }; }; template<typename T> struct
// internal::func_is_conjunction<internal::scalar_product_op<T> > { enum { ret =
// true }; };

// TODO generalize the internal::scalar_product_op specialization to all
// conjunctions if any !

namespace internal
{

    // sparse - sparse  (generic)
    template <typename BinaryOp, typename Lhs, typename Rhs, typename Derived>
    class sparse_cwise_binary_op_inner_iterator_selector<
        BinaryOp, Lhs, Rhs, Derived, Sparse, Sparse>
    {
        typedef CwiseBinaryOp<BinaryOp, Lhs, Rhs> CwiseBinaryXpr;
        typedef typename traits<CwiseBinaryXpr>::Scalar Scalar;
        typedef typename traits<CwiseBinaryXpr>::_LhsNested _LhsNested;
        typedef typename traits<CwiseBinaryXpr>::_RhsNested _RhsNested;
        typedef typename _LhsNested::InnerIterator LhsIterator;
        typedef typename _RhsNested::InnerIterator RhsIterator;
        typedef typename Lhs::Index Index;

    public:
        EIGEN_STRONG_INLINE sparse_cwise_binary_op_inner_iterator_selector(
            const CwiseBinaryXpr& xpr, Index outer)
            : m_lhsIter(xpr.lhs(), outer)
            , m_rhsIter(xpr.rhs(), outer)
            , m_functor(xpr.functor())
        {
            this->operator++();
        }

        EIGEN_STRONG_INLINE Derived& operator++()
        {
            if (m_lhsIter && m_rhsIter
                && (m_lhsIter.index() == m_rhsIter.index()))
            {
                m_id = m_lhsIter.index();
                m_value = m_functor(m_lhsIter.value(), m_rhsIter.value());
                ++m_lhsIter;
                ++m_rhsIter;
            }
            else if (m_lhsIter
                     && (!m_rhsIter || (m_lhsIter.index() < m_rhsIter.index())))
            {
                m_id = m_lhsIter.index();
                m_value = m_functor(m_lhsIter.value(), Scalar(0));
                ++m_lhsIter;
            }
            else if (m_rhsIter
                     && (!m_lhsIter || (m_lhsIter.index() > m_rhsIter.index())))
            {
                m_id = m_rhsIter.index();
                m_value = m_functor(Scalar(0), m_rhsIter.value());
                ++m_rhsIter;
            }
            else
            {
                m_value = 0; // this is to avoid a compilation warning
                m_id = -1;
            }
            return *static_cast<Derived*>(this);
        }

        EIGEN_STRONG_INLINE Scalar value() const { return m_value; }

        EIGEN_STRONG_INLINE Index index() const { return m_id; }

        EIGEN_STRONG_INLINE Index row() const
        {
            return Lhs::IsRowMajor ? m_lhsIter.row() : index();
        }

        EIGEN_STRONG_INLINE Index col() const
        {
            return Lhs::IsRowMajor ? index() : m_lhsIter.col();
        }

        EIGEN_STRONG_INLINE operator bool() const { return m_id >= 0; }

    protected:
        LhsIterator m_lhsIter;
        RhsIterator m_rhsIter;
        const BinaryOp& m_functor;
        Scalar m_value;
        Index m_id;
    };

    // sparse - sparse  (product)
    template <typename T, typename Lhs, typename Rhs, typename Derived>
    class sparse_cwise_binary_op_inner_iterator_selector<
        scalar_product_op<T>, Lhs, Rhs, Derived, Sparse, Sparse>
    {
        typedef scalar_product_op<T> BinaryFunc;
        typedef CwiseBinaryOp<BinaryFunc, Lhs, Rhs> CwiseBinaryXpr;
        typedef typename CwiseBinaryXpr::Scalar Scalar;
        typedef typename traits<CwiseBinaryXpr>::_LhsNested _LhsNested;
        typedef typename _LhsNested::InnerIterator LhsIterator;
        typedef typename traits<CwiseBinaryXpr>::_RhsNested _RhsNested;
        typedef typename _RhsNested::InnerIterator RhsIterator;
        typedef typename Lhs::Index Index;

    public:
        EIGEN_STRONG_INLINE sparse_cwise_binary_op_inner_iterator_selector(
            const CwiseBinaryXpr& xpr, Index outer)
            : m_lhsIter(xpr.lhs(), outer)
            , m_rhsIter(xpr.rhs(), outer)
            , m_functor(xpr.functor())
        {
            while (m_lhsIter && m_rhsIter
                   && (m_lhsIter.index() != m_rhsIter.index()))
            {
                if (m_lhsIter.index() < m_rhsIter.index())
                    ++m_lhsIter;
                else
                    ++m_rhsIter;
            }
        }

        EIGEN_STRONG_INLINE Derived& operator++()
        {
            ++m_lhsIter;
            ++m_rhsIter;
            while (m_lhsIter && m_rhsIter
                   && (m_lhsIter.index() != m_rhsIter.index()))
            {
                if (m_lhsIter.index() < m_rhsIter.index())
                    ++m_lhsIter;
                else
                    ++m_rhsIter;
            }
            return *static_cast<Derived*>(this);
        }

        EIGEN_STRONG_INLINE Scalar value() const
        {
            return m_functor(m_lhsIter.value(), m_rhsIter.value());
        }

        EIGEN_STRONG_INLINE Index index() const { return m_lhsIter.index(); }

        EIGEN_STRONG_INLINE Index row() const { return m_lhsIter.row(); }

        EIGEN_STRONG_INLINE Index col() const { return m_lhsIter.col(); }

        EIGEN_STRONG_INLINE operator bool() const
        {
            return (m_lhsIter && m_rhsIter);
        }

    protected:
        LhsIterator m_lhsIter;
        RhsIterator m_rhsIter;
        const BinaryFunc& m_functor;
    };

    // sparse - dense  (product)
    template <typename T, typename Lhs, typename Rhs, typename Derived>
    class sparse_cwise_binary_op_inner_iterator_selector<
        scalar_product_op<T>, Lhs, Rhs, Derived, Sparse, Dense>
    {
        typedef scalar_product_op<T> BinaryFunc;
        typedef CwiseBinaryOp<BinaryFunc, Lhs, Rhs> CwiseBinaryXpr;
        typedef typename CwiseBinaryXpr::Scalar Scalar;
        typedef typename traits<CwiseBinaryXpr>::_LhsNested _LhsNested;
        typedef typename traits<CwiseBinaryXpr>::RhsNested RhsNested;
        typedef typename _LhsNested::InnerIterator LhsIterator;
        typedef typename Lhs::Index Index;

        enum
        {
            IsRowMajor = (int(Lhs::Flags) & RowMajorBit) == RowMajorBit
        };

    public:
        EIGEN_STRONG_INLINE sparse_cwise_binary_op_inner_iterator_selector(
            const CwiseBinaryXpr& xpr, Index outer)
            : m_rhs(xpr.rhs())
            , m_lhsIter(xpr.lhs(), outer)
            , m_functor(xpr.functor())
            , m_outer(outer)
        {
        }

        EIGEN_STRONG_INLINE Derived& operator++()
        {
            ++m_lhsIter;
            return *static_cast<Derived*>(this);
        }

        EIGEN_STRONG_INLINE Scalar value() const
        {
            return m_functor(
                m_lhsIter.value(),
                m_rhs.coeff(IsRowMajor ? m_outer : m_lhsIter.index(),
                            IsRowMajor ? m_lhsIter.index() : m_outer));
        }

        EIGEN_STRONG_INLINE Index index() const { return m_lhsIter.index(); }

        EIGEN_STRONG_INLINE Index row() const { return m_lhsIter.row(); }

        EIGEN_STRONG_INLINE Index col() const { return m_lhsIter.col(); }

        EIGEN_STRONG_INLINE operator bool() const { return m_lhsIter; }

    protected:
        const RhsNested m_rhs;
        LhsIterator m_lhsIter;
        const BinaryFunc m_functor;
        const Index m_outer;
    };

    // sparse - dense  (product)
    template <typename T, typename Lhs, typename Rhs, typename Derived>
    class sparse_cwise_binary_op_inner_iterator_selector<
        scalar_product_op<T>, Lhs, Rhs, Derived, Dense, Sparse>
    {
        typedef scalar_product_op<T> BinaryFunc;
        typedef CwiseBinaryOp<BinaryFunc, Lhs, Rhs> CwiseBinaryXpr;
        typedef typename CwiseBinaryXpr::Scalar Scalar;
        typedef typename traits<CwiseBinaryXpr>::_RhsNested _RhsNested;
        typedef typename _RhsNested::InnerIterator RhsIterator;
        typedef typename Lhs::Index Index;

        enum
        {
            IsRowMajor = (int(Rhs::Flags) & RowMajorBit) == RowMajorBit
        };

    public:
        EIGEN_STRONG_INLINE sparse_cwise_binary_op_inner_iterator_selector(
            const CwiseBinaryXpr& xpr, Index outer)
            : m_xpr(xpr)
            , m_rhsIter(xpr.rhs(), outer)
            , m_functor(xpr.functor())
            , m_outer(outer)
        {
        }

        EIGEN_STRONG_INLINE Derived& operator++()
        {
            ++m_rhsIter;
            return *static_cast<Derived*>(this);
        }

        EIGEN_STRONG_INLINE Scalar value() const
        {
            return m_functor(
                m_xpr.lhs().coeff(IsRowMajor ? m_outer : m_rhsIter.index(),
                                  IsRowMajor ? m_rhsIter.index() : m_outer),
                m_rhsIter.value());
        }

        EIGEN_STRONG_INLINE Index index() const { return m_rhsIter.index(); }

        EIGEN_STRONG_INLINE Index row() const { return m_rhsIter.row(); }

        EIGEN_STRONG_INLINE Index col() const { return m_rhsIter.col(); }

        EIGEN_STRONG_INLINE operator bool() const { return m_rhsIter; }

    protected:
        const CwiseBinaryXpr& m_xpr;
        RhsIterator m_rhsIter;
        const BinaryFunc& m_functor;
        const Index m_outer;
    };

} // end namespace internal

/***************************************************************************
 * Implementation of SparseMatrixBase and SparseCwise functions/operators
 ***************************************************************************/

// template<typename Derived>
// template<typename OtherDerived>
// EIGEN_STRONG_INLINE const
// CwiseBinaryOp<internal::scalar_difference_op<typename
// internal::traits<Derived>::Scalar>,
//                                  Derived, OtherDerived>
// SparseMatrixBase<Derived>::operator-(const SparseMatrixBase<OtherDerived>
// &other) const
// {
//   return CwiseBinaryOp<internal::scalar_difference_op<Scalar>,
//                        Derived, OtherDerived>(derived(), other.derived());
// }

template <typename Derived>
template <typename OtherDerived>
EIGEN_STRONG_INLINE Derived& SparseMatrixBase<Derived>::operator-=(
    const SparseMatrixBase<OtherDerived>& other)
{
    return *this = derived() - other.derived();
}

// template<typename Derived>
// template<typename OtherDerived>
// EIGEN_STRONG_INLINE const CwiseBinaryOp<internal::scalar_sum_op<typename
// internal::traits<Derived>::Scalar>, Derived, OtherDerived>
// SparseMatrixBase<Derived>::operator+(const SparseMatrixBase<OtherDerived>
// &other) const
// {
//   return CwiseBinaryOp<internal::scalar_sum_op<Scalar>, Derived,
//   OtherDerived>(derived(), other.derived());
// }

template <typename Derived>
template <typename OtherDerived>
EIGEN_STRONG_INLINE Derived& SparseMatrixBase<Derived>::operator+=(
    const SparseMatrixBase<OtherDerived>& other)
{
    return *this = derived() + other.derived();
}

// template<typename ExpressionType>
// template<typename OtherDerived>
// EIGEN_STRONG_INLINE const EIGEN_SPARSE_CWISE_PRODUCT_RETURN_TYPE
// SparseCwise<ExpressionType>::operator*(const SparseMatrixBase<OtherDerived>
// &other) const
// {
//   return EIGEN_SPARSE_CWISE_PRODUCT_RETURN_TYPE(_expression(),
//   other.derived());
// }

template <typename Derived>
template <typename OtherDerived>
EIGEN_STRONG_INLINE const EIGEN_SPARSE_CWISE_PRODUCT_RETURN_TYPE
SparseMatrixBase<Derived>::cwiseProduct(
    const MatrixBase<OtherDerived>& other) const
{
    return EIGEN_SPARSE_CWISE_PRODUCT_RETURN_TYPE(derived(), other.derived());
}

// template<typename ExpressionType>
// template<typename OtherDerived>
// EIGEN_STRONG_INLINE const
// EIGEN_SPARSE_CWISE_BINOP_RETURN_TYPE(internal::scalar_quotient_op)
// SparseCwise<ExpressionType>::operator/(const SparseMatrixBase<OtherDerived>
// &other) const
// {
//   return
//   EIGEN_SPARSE_CWISE_BINOP_RETURN_TYPE(internal::scalar_quotient_op)(_expression(),
//   other.derived());
// }
//
// template<typename ExpressionType>
// template<typename OtherDerived>
// EIGEN_STRONG_INLINE const
// EIGEN_SPARSE_CWISE_BINOP_RETURN_TYPE(internal::scalar_quotient_op)
// SparseCwise<ExpressionType>::operator/(const MatrixBase<OtherDerived> &other)
// const
// {
//   return
//   EIGEN_SPARSE_CWISE_BINOP_RETURN_TYPE(internal::scalar_quotient_op)(_expression(),
//   other.derived());
// }

// template<typename ExpressionType>
// template<typename OtherDerived>
// inline ExpressionType& SparseCwise<ExpressionType>::operator*=(const
// SparseMatrixBase<OtherDerived> &other)
// {
//   return m_matrix.const_cast_derived() = _expression() * other.derived();
// }

#endif // EIGEN_SPARSE_CWISE_BINARY_OP_H
