#ifndef _DG_SHU_CUH
#define _DG_SHU_CUH

#include <exception>
#include <cusp/ell_matrix.h>

#include "dg/blas.h"
#include "dg/arakawa.h"
#include "dg/elliptic.h"
#include "dg/cg.h"

namespace dg
{
template< class Matrix, class container>
struct Diffusion
{
    Diffusion( const dg::Grid2d& g, double nu): nu_(nu),
        w2d( dg::create::weights( g) ), v2d( dg::create::inv_weights(g) ) ,
        LaplacianM( g, dg::normed)
    { 
    }
    void operator()( const container& x, container& y)
    {
        dg::blas2::gemv( LaplacianM, x, y);
        dg::blas1::scal( y, -nu_);
    }
    const container& weights(){return w2d;}
    const container& precond(){return v2d;}
  private:
    double nu_;
    const container w2d, v2d;
    dg::Elliptic<dg::CartesianGrid2d, Matrix,container> LaplacianM;
};

template< class Matrix, class container >
struct Shu 
{
    typedef typename container::value_type value_type;
    typedef container Vector;

    Shu( const Grid2d& grid, double eps);

    const Elliptic<Matrix, container, container>& lap() const { return laplaceM;}
    ArakawaX<CartesianGrid2d, Matrix, container>& arakawa() {return arakawa_;}
    /**
     * @brief Returns psi that belong to the last y in operator()
     *
     * In a multistep scheme this belongs to the point HEAD-1
     * @return psi is the potential
     */
    const container& potential( ) {return psi;}
    void operator()( Vector& y, Vector& yp);
  private:
    //typedef typename VectorTraits< Vector>::value_type value_type;
    container psi, w2d, v2d;
    Elliptic<CartesianGrid2d, Matrix, container> laplaceM;
    ArakawaX<CartesianGrid2d, Matrix, container> arakawa_; 
    Invert<container> invert;
};

template<class Matrix, class container>
Shu< Matrix, container>::Shu( const Grid2d& g, double eps): 
    psi( g.size()),
    w2d( create::weights( g)), v2d( create::inv_weights(g)),  
    laplaceM( g, not_normed),
    arakawa_( g), 
    invert( psi, g.size(), eps)
{
}

template< class Matrix, class container>
void Shu<Matrix, container>::operator()( Vector& y, Vector& yp)
{
    invert( laplaceM, psi, y, w2d, v2d);
    arakawa_( y, psi, yp); //A(y,psi)-> yp
}

}//namespace dg

#endif //_DG_SHU_CUH
