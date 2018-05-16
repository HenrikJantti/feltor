#pragma once
#include <vector>
#include "blas1_dispatch_mpi.h"
#include "thrust_matrix_blas.cuh"

namespace dg
{

///@cond
namespace blas2
{
namespace detail
{
template< class Vector1, class Matrix, class Vector2 >
inline std::vector<int64_t> doDot_superacc( const Vector1& x, const Matrix& P, const Vector2& y, MPIVectorTag, MPIVectorTag)
{
    static_assert( all_true<
            std::is_base_of<MPIVectorTag, get_data_layout<Vector1>>::value,
            std::is_base_of<MPIVectorTag, get_data_layout<Vector2>>::value
            >::value,
        "All data layouts must derive from the same vector category (MPIVectorTag in this case)!");
#ifdef DG_DEBUG
    int compare;
    MPI_Comm_compare( x.communicator(), y.communicator(), &compare);
    assert( compare == MPI_CONGRUENT || compare == MPI_IDENT);
    MPI_Comm_compare( x.communicator(), P.communicator(), &compare);
    assert( compare == MPI_CONGRUENT || compare == MPI_IDENT);
#endif //DG_DEBUG
    using inner_container = typename std::decay<Matrix>::type::container_type;
    //local computation
    std::vector<int64_t> acc = doDot_superacc(x.data(), P.data(), y.data(),
        get_data_layout<inner_container>());
    std::vector<int64_t> receive(exblas::BIN_COUNT, (int64_t)0);
    exblas::reduce_mpi_cpu( 1, acc.data(), receive.data(), x.communicator(), x.communicator_mod(), x.communicator_mod_reduce());

    return receive;
}
template< class Vector1, class Matrix, class Vector2>
inline std::vector<int64_t> doDot_superacc( const Vector1& x, const Matrix& m, const Vector2& y, MPIVectorTag, VectorVectorTag)
{
    static_assert( std::is_base_of<VectorVectorTag,
        get_data_layout<Vector2>>::value,
        "All data layouts must derive from the same vector category (VectorVectorTag in this case)!");
#ifdef DG_DEBUG
    assert( x.size() == y.size() );
#endif //DG_DEBUG
    using inner_container1 = typename std::decay<Vector1>::type::value_type;
    std::vector<std::vector<int64_t>> acc( x.size());
    for( unsigned i=0; i<x.size(); i++)
        acc[i] = doDot_superacc( x[i], m, y[i],
                       get_data_layout<Matrix>(),
                       get_data_layout<inner_container1>() );
    for( unsigned i=1; i<x.size(); i++)
    {
        int imin = exblas::IMIN, imax = exblas::IMAX;
        exblas::cpu::Normalize( &(acc[0][0]), imin, imax);
        imin = exblas::IMIN, imax = exblas::IMAX;
        exblas::cpu::Normalize( &(acc[i][0]), imin, imax);
        for( int k=exblas::IMIN; k<exblas::IMAX; k++)
            acc[0][k] += acc[i][k];
    }
    return acc[0];
}

template< class Vector1, class Matrix, class Vector2 >
inline get_value_type<Matrix> doDot( const Vector1& x, const Matrix& P, const Vector2& y, MPIVectorTag)
{
    std::vector<int64_t> acc = doDot_superacc( x,P,y, MPIVectorTag(), get_data_layout<Vector1>());
    return exblas::cpu::Round(acc.data());
}

template< class Matrix, class Vector>
inline get_value_type<Matrix> doDot( const Matrix& m, const Vector& x, dg::MPIVectorTag)
{
    std::vector<int64_t> acc = doDot_superacc( x,m,x, MPIVectorTag(), get_data_layout<Vector1>());
    return exblas::cpu::Round(acc.data());
}

template< class Matrix1, class Matrix2>
inline void doTransfer( const Matrix1& m1, Matrix2& m2, AnyMatrixTag, MPIMatrixTag)
{
    Matrix2 m(m1);
    m2 = m;
}

//Matrix = MPI Vector
template< class Matrix, class Vector1, class Vector2>
inline void doSymv( Matrix& m, const Vector1& x, Vector2& y, MPIVectorTag, MPIVectorTag )
{
    dg::blas2::symv( m.data(), x.data(), y.data());
}

template< class Matrix, class Vector1, class Vector2>
inline void doSymv( get_value_type<Vector1> alpha,
                Matrix& m,
                const Vector1& x,
                get_value_type<Vector1> beta,
                Vector2& y,
                MPIVectorTag,
                MPIVectorTag
                )
{
    dg::blas2::symv( alpha, m.data(), x.data(), beta, y.data());
}
template< class Matrix, class Vector1, class Vector2>
inline void doSymv( Matrix& m, const Vector1& x, Vector2& y, MPIVectorTag, VectorVectorTag )
{
    for( unsigned i=0; i<x.size(); i++)
        dg::blas2::symv( m, x[i], y[i]);
}

template< class Matrix, class Vector1, class Vector2>
inline void doSymv( get_value_type<Vector1> alpha,
                Matrix& m,
                const Vector1& x,
                get_value_type<Vector1> beta,
                Vector2& y,
                MPIVectorTag,
                VectorVectorTag
                )
{
    for( unsigned i=0; i<x.size(); i++)
        dg::blas2::symv( alpha, m, x[i], beta, y[i]);
}
//Matrix is an MPI matrix
template< class Matrix, class Vector1, class Vector2>
inline void doSymv( Matrix& m, const Vector1& x, Vector2& y, MPIMatrixTag, MPIVectorTag )
{
    m.symv( x, y);
}

template< class Matrix, class Vector1, class Vector2>
inline void doSymv( get_value_type<Vector1> alpha,
                Matrix& m,
                const Vector1& x,
                get_value_type<Vector1> beta,
                Vector2& y,
                MPIMatrixTag,
                MPIVectorTag
                )
{
    m.symv( alpha, x, beta, y);
}

template< class Matrix, class Vector1, class Vector2>
inline void doSymv( Matrix& m, const Vector1& x, Vector2& y, MPIMatrixTag, VectorVectorTag )
{
    for( unsigned i=0; i<x.size(); i++)
        dg::blas2::symv( m, x[i], y[i]);
}

template< class Matrix, class Vector1, class Vector2>
inline void doSymv( get_value_type<Vector1> alpha,
                Matrix& m,
                const Vector1& x,
                get_value_type<Vector1> beta,
                Vector2& y,
                MPIMatrixTag,
                VectorVectorTag
                )
{
    for( unsigned i=0; i<x.size(); i++)
        dg::blas2::symv( alpha, m, x[i], beta, y[i]);
}


} //namespace detail
} //namespace blas2
///@endcond
} //namespace dg
