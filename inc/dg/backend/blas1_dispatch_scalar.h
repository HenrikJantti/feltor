#ifndef _DG_BLAS_SCALAR_
#define _DG_BLAS_SCALAR_

#include "scalar_categories.h"
#include "tensor_traits.h"
#include "predicate.h"

#include "blas1_serial.h"

///@cond
namespace dg
{
namespace blas1
{
namespace detail
{

template< class Vector1, class Vector2>
std::vector<int64_t> doDot_superacc( const Vector1& x, const Vector2& y, AnyScalarTag)
{
    //both Vectors are scalars
    static_assert( std::is_convertible<get_value_type<Vector1>, double>::value, "We only support double precision dot products at the moment!");
    static_assert( std::is_convertible<get_value_type<Vector2>, double>::value, "We only support double precision dot products at the moment!");
    const get_value_type<Vector1>* x_ptr = &x;
    const get_value_type<Vector2>* y_ptr = &y;
    //since we only accumulate up to two values (multiplication and rest) reduce the size of the FPE
    std::vector<int64_t> h_superacc(exblas::BIN_COUNT);
    exblas::exdot_cpu<const get_value_type<Vector1>*, const get_value_type<Vector2>*, 2>( 1, x_ptr,y_ptr, &h_superacc[0]) ;
    return h_superacc;
}

template< class Subroutine, class ContainerType, class ...ContainerTypes>
inline void doEvaluate( AnyScalarTag, Subroutine f, ContainerType&& x, ContainerTypes&&... xs)
{
    f(x,xs...);
}

} //namespace detail
} //namespace blas1
} //namespace dg
///@endcond

#endif //_DG_BLAS_SCALAR_
