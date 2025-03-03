// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
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

// no include guard, we'll include this twice from All.h from Eigen2Support, and
// it's internal anyway

/** \geometry_module \ingroup Geometry_Module
 *
 * \class Rotation2D
 *
 * \brief Represents a rotation/orientation in a 2 dimensional space.
 *
 * \param _Scalar the scalar type, i.e., the type of the coefficients
 *
 * This class is equivalent to a single scalar representing a counter clock wise
 * rotation as a single angle in radian. It provides some additional features
 * such as the automatic conversion from/to a 2x2 rotation matrix. Moreover this
 * class aims to provide a similar interface to Quaternion in order to
 * facilitate the writing of generic algorithms dealing with rotations.
 *
 * \sa class Quaternion, class Transform
 */
template <typename _Scalar> struct ei_traits<Rotation2D<_Scalar>>
{
    typedef _Scalar Scalar;
};

template <typename _Scalar>
class Rotation2D : public RotationBase<Rotation2D<_Scalar>, 2>
{
    typedef RotationBase<Rotation2D<_Scalar>, 2> Base;

public:
    using Base::operator*;

    enum
    {
        Dim = 2
    };

    /** the scalar type of the coefficients */
    typedef _Scalar Scalar;
    typedef Matrix<Scalar, 2, 1> Vector2;
    typedef Matrix<Scalar, 2, 2> Matrix2;

protected:
    Scalar m_angle;

public:
    /** Construct a 2D counter clock wise rotation from the angle \a a in
     * radian. */
    inline Rotation2D(Scalar a)
        : m_angle(a)
    {
    }

    /** \returns the rotation angle */
    inline Scalar angle() const { return m_angle; }

    /** \returns a read-write reference to the rotation angle */
    inline Scalar& angle() { return m_angle; }

    /** \returns the inverse rotation */
    inline Rotation2D inverse() const { return -m_angle; }

    /** Concatenates two rotations */
    inline Rotation2D operator*(const Rotation2D& other) const
    {
        return m_angle + other.m_angle;
    }

    /** Concatenates two rotations */
    inline Rotation2D& operator*=(const Rotation2D& other)
    {
        return m_angle += other.m_angle;
        return *this;
    }

    /** Applies the rotation to a 2D vector */
    Vector2 operator*(const Vector2& vec) const
    {
        return toRotationMatrix() * vec;
    }

    template <typename Derived>
    Rotation2D& fromRotationMatrix(const MatrixBase<Derived>& m);
    Matrix2 toRotationMatrix(void) const;

    /** \returns the spherical interpolation between \c *this and \a other using
     * parameter \a t. It is in fact equivalent to a linear interpolation.
     */
    inline Rotation2D slerp(Scalar t, const Rotation2D& other) const
    {
        return m_angle * (1 - t) + other.angle() * t;
    }

    /** \returns \c *this with scalar type casted to \a NewScalarType
     *
     * Note that if \a NewScalarType is equal to the current scalar type of \c
     * *this then this function smartly returns a const reference to \c *this.
     */
    template <typename NewScalarType>
    inline typename internal::cast_return_type<Rotation2D,
                                               Rotation2D<NewScalarType>>::type
    cast() const
    {
        return typename internal::cast_return_type<
            Rotation2D, Rotation2D<NewScalarType>>::type(*this);
    }

    /** Copy constructor with scalar type conversion */
    template <typename OtherScalarType>
    inline explicit Rotation2D(const Rotation2D<OtherScalarType>& other)
    {
        m_angle = Scalar(other.angle());
    }

    /** \returns \c true if \c *this is approximately equal to \a other, within
     * the precision determined by \a prec.
     *
     * \sa MatrixBase::isApprox() */
    bool
    isApprox(const Rotation2D& other,
             typename NumTraits<Scalar>::Real prec = precision<Scalar>()) const
    {
        return ei_isApprox(m_angle, other.m_angle, prec);
    }
};

/** \ingroup Geometry_Module
 * single precision 2D rotation type */
typedef Rotation2D<float> Rotation2Df;
/** \ingroup Geometry_Module
 * double precision 2D rotation type */
typedef Rotation2D<double> Rotation2Dd;

/** Set \c *this from a 2x2 rotation matrix \a mat.
 * In other words, this function extract the rotation angle
 * from the rotation matrix.
 */
template <typename Scalar>
template <typename Derived>
Rotation2D<Scalar>&
Rotation2D<Scalar>::fromRotationMatrix(const MatrixBase<Derived>& mat)
{
    EIGEN_STATIC_ASSERT(Derived::RowsAtCompileTime == 2
                            && Derived::ColsAtCompileTime == 2,
                        YOU_MADE_A_PROGRAMMING_MISTAKE)
    m_angle = ei_atan2(mat.coeff(1, 0), mat.coeff(0, 0));
    return *this;
}

/** Constructs and \returns an equivalent 2x2 rotation matrix.
 */
template <typename Scalar>
typename Rotation2D<Scalar>::Matrix2
Rotation2D<Scalar>::toRotationMatrix(void) const
{
    Scalar sinA = ei_sin(m_angle);
    Scalar cosA = ei_cos(m_angle);
    return (Matrix2() << cosA, -sinA, sinA, cosA).finished();
}
