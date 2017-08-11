#pragma once

#include "cusp/transpose.h"
#include "dg/backend/grid.h"
#include "dg/backend/weights.cuh"
#include "dg/backend/interpolation.cuh"
#include "dg/blas.h"

#include "base_geometry.h"


namespace dg
{
    //actually there shouldn't be a reason to manually refine a grid when we can use our generators instead

///@cond
namespace detail
{
    ///@cond

/**
 * @brief Divide cells in an equally distributed number of new cells
 *
 * @param add_x the number of cells to add 
 * @param node the node around which to refine
 * @param n polynomials
 * @param N # of cells
 * @param bcx boundary condition
 * @param howmany number of cells to refine
 *
 * @return Weights
 */
thrust::host_vector<double> equidist_ref( unsigned add_x, unsigned node, unsigned n, unsigned N, dg::bc bcx, unsigned howmany)
{
    assert( howmany <= N);
    assert( node <= N);
    if( node != 0 && node != N) assert( howmany <= node && howmany <= N-node);
    if( add_x == 0 || howmany == 0)
    {
        thrust::host_vector<double> w_( n*N, 1);
        return w_;
    }
    assert( node <= N);
    //there are add_x+1 finer cells per refined cell ...
    thrust::host_vector< double> left( n*N+n*add_x*howmany, 1), right(left);
    for( unsigned i=0; i<(add_x+1)*howmany; i++)//the original cell and the additional ones
        for( unsigned k=0; k<n; k++)
            left[i*n+k] = add_x + 1;
    //mirror left into right
    for( unsigned i=0; i<right.size(); i++)
        right[i] = left[ (left.size()-1)-i];
    thrust::host_vector< double> both( n*N+2*n*add_x*howmany, 1);
    for( unsigned i=0; i<left.size(); i++)
        both[i] *= left[i];
    for( unsigned i=0; i<right.size(); i++)
        both[i+n*add_x*howmany] *= right[i];
    if(      node == 0     && bcx != dg::PER) { return left; }
    else if( node == N && bcx != dg::PER) { return right; }
    else if((node == N || node == 0) && bcx == dg::PER) { return both; }
    else 
    {
        thrust::host_vector<double> w_ = both;
        //now shift indices so that refinement is around nodes
        for( unsigned i=0; i<both.size(); i++)
            w_[((howmany*add_x+node)*n+i)%both.size()] = both[i];
        return w_;
    }
}

/**
 * @brief Divide cells in an equally distributed number of new cells
 *
 * @param add_x the number of cells to add 
 * @param node the node around which to refine
 * @param n polynomials
 * @param N # of cells
 * @param bcx boundary condition
 *
 * @return Weights
 */
thrust::host_vector<double> equidist_ref( unsigned add_x, unsigned node, unsigned n, unsigned N, dg::bc bcx)
{
    return equidist_ref(add_x, node, n, N, bcx, 1);
}

thrust::host_vector<double> exponential_ref( unsigned add_x, unsigned node, unsigned n, unsigned N, dg::bc bcx)
{
    if( add_x == 0)
    {
        thrust::host_vector<double> w_( n*N, 1);
        return w_;
    }
    assert( node <= N);
    //there are add_x+1 finer cells per refined cell ...
    thrust::host_vector< double> left( n*N+n*add_x, 1), right(left);
    for( unsigned k=0; k<n; k++)//the original cell and the additional ones
        left[k] = pow( 2, add_x);
    for( unsigned i=0; i<add_x; i++) 
        for( unsigned k=0; k<n; k++)
            left[(i+1)*n+k] = pow( 2, add_x-i);
    //mirror left into right
    for( unsigned i=0; i<right.size(); i++)
        right[i] = left[ (left.size()-1)-i];
    thrust::host_vector< double> both( n*N+2*n*add_x, 1);
    for( unsigned i=0; i<left.size(); i++)
        both[i] *= left[i];
    for( unsigned i=0; i<right.size(); i++)
        both[i+n*add_x] *= right[i];
    if(      node == 0     && bcx != dg::PER) { return left; }
    else if( node == N && bcx != dg::PER) { return right; }
    else if((node == N || node == 0) && bcx == dg::PER) { return both; }
    else 
    {
        thrust::host_vector<double> w_ = both;
        //now shift indices so that refinement is around nodes
        for( unsigned i=0; i<both.size(); i++)
            w_[((add_x+node)*n+i)%both.size()] = both[i];
        return w_;
    }
}

/**
 * @brief Refine every cell in the grid by an integer number of new cells
 *
 * @param multiple_x
 * @param n
 * @param N
 * @param bcx
 *
 * @return 
 */
thrust::host_vector<double> linear_ref( unsigned multiple_x, unsigned n, unsigned N, dg::bc bcx)
{
    assert( multiple_x >= 1);
    //there are add_x+1 finer cells per refined cell ...
    thrust::host_vector< double> left( n*N*multiple_x, 1);
    for( unsigned k=0; k<left.size(); k++)
        left[k] = (double)multiple_x;
    return left;
}

/**
 * @brief Normalize the given weights and compute the abscissas of the grid
 *
 * @param g The grid to be refined
 * @param weights the unnormalized weights
 *
 * @return The abscissas of the new grid
 */
thrust::host_vector<double> normalize_weights_and_compute_abscissas( const Grid1d& g, thrust::host_vector<double>& weights)
{
    //normalize weights
    unsigned Nx_new = weights.size()/g.n();
    for( unsigned i=0;i<weights.size(); i++)
        weights[i] *= (double)g.N()/(double)Nx_new;

    thrust::host_vector<double> boundaries(Nx_new+1), abs(g.n()*Nx_new);
    boundaries[0] = g.x0();
    for( unsigned i=0; i<Nx_new; i++)
    {
        boundaries[i+1] = boundaries[i] + g.lx()/(double)Nx_new/weights[g.n()*i];
        for( unsigned j=0; j<g.n(); j++)
        {
            abs[i*g.n()+j] =  (boundaries[i+1]+boundaries[i])/2. + 
                (boundaries[i+1]-boundaries[i])/2.*g.dlt().abscissas()[j];
        }
    }
    return abs;
}

/**
 * @brief Create 1d refinement weights and abscissas for the exponential refinement around a node 
 *
 * There will be two refined cells at the end except if a corner node is 
 * given and the boundary condition is not periodic. We count nodes from
 * 0 (left corner) to N (right corner). 
 * @param add_x number of additional cells in the cells idx-1 and idx
 * @param node The cells node-1 and node will be refined
 * @param g The 1d grid to refine
 *
 * @param weights A 1d vector of size n*(Nx+add_x) for one-sided refinement and n*(Nx+2*add_x)) for two-sided refinement
 * @param abscissas A 1d vector of size n*(Nx+add_x) for one-sided refinement and n*(Nx+2*add_x)) for two-sided refinement
 * @return the new number of cells
 */
int exponential_ref( unsigned add_x, unsigned node, const Grid1d& g, thrust::host_vector<double>& weights, thrust::host_vector<double>& abscissas)
{
    if( add_x == 0)
    {
        thrust::host_vector<double> w_( g.size(), 1);
        thrust::host_vector<double> abs_= dg::create::abscissas(g);
        weights = w_; abscissas = abs_; 
        return g.N();
    }
    weights = exponential_ref( add_x, node, g.n(), g.N(), g.bcx());
    unsigned Nx_new = weights.size()/g.n();
    abscissas = normalize_weights_and_compute_abscissas( g, weights);
    return Nx_new;
}

/**
 * @brief Create 1d refinement weights and abscissas for the equidist refinement around a node 
 *
 * There will be two*howmany refined cells at the end except if a corner node is 
 * given and the boundary condition is not periodic. We count nodes from
 * 0 (left corner) to N (right corner). 
 * @param add_x number of additional cells in the cells idx-1 and idx
 * @param node The cells node-1 and node will be refined
 * @param g The 1d grid to refine
 *
 * @param weights A 1d vector of size n*(Nx+add_x*howmany) for one-sided refinement and n*(Nx+2*add_x*howmany)) for two-sided refinement
 * @param abscissas A 1d vector of size n*(Nx+add_x*howmany) for one-sided refinement and n*(Nx+2*add_x*howmany)) for two-sided refinement
 * @param howmany  number of cells around a node to refine
 * @return the new number of cells
 */
int equidist_ref( unsigned add_x, unsigned node, const Grid1d& g, thrust::host_vector<double>& weights, thrust::host_vector<double>& abscissas, unsigned howmany)
{
    if( add_x == 0 || howmany == 0)
    {
        thrust::host_vector<double> w_( g.size(), 1);
        thrust::host_vector<double> abs_= dg::create::abscissas(g);
        weights = w_; abscissas = abs_; 
        return g.N();
    }
    weights = equidist_ref( add_x, node, g.n(), g.N(), g.bcx(), howmany);
    unsigned Nx_new = weights.size()/g.n();
    abscissas = normalize_weights_and_compute_abscissas( g, weights);
    return Nx_new;
}
int equidist_ref( unsigned add_x, unsigned node, const Grid1d& g, thrust::host_vector<double>& weights, thrust::host_vector<double>& abscissas)
{
    return equidist_ref( add_x, node, g, weights, abscissas, 1);
}
/**
 * @brief Create 1d refinement weights and abscissas for the linear refinement 
 *
 * @param multiple_x number of additional cells in the cells 
 * @param g The 1d grid to refine
 *
 * @param weights A 1d vector of size n*(Nx+add_x) for one-sided refinement and n*(Nx+2*add_x)) for two-sided refinement
 * @param abscissas A 1d vector of size n*(Nx+add_x) for one-sided refinement and n*(Nx+2*add_x)) for two-sided refinement
 * @return the new number of cells
 */
int linear_ref( unsigned multiple_x, const Grid1d& g, thrust::host_vector<double>& weights, thrust::host_vector<double>& abscissas)
{
    weights = linear_ref( multiple_x, g.n(), g.N(), g.bcx());
    unsigned Nx_new = weights.size()/g.n();
    abscissas = normalize_weights_and_compute_abscissas( g, weights);
    return Nx_new;
}

///@endcond
}//namespace detail

///@endcond
/**
 * @brief aRefined grid 
 * @deprecated
 * @ingroup grid
 */
struct aRefinedGrid2d : public dg::aGeometry2d
{

    /**
     * @brief The grid that this object refines
     *
     * This function is vitual so that derived classes can also construct the associated grid 
     * @return  2d grid
     */
    virtual const dg::Grid2d& associated()const {return g_assoc_;}
    /**
     * @brief Return the abscissas in X-direction 
     *
     * @return A 2d vector
     */
    const thrust::host_vector<double>& abscissasX() const {return absX_;} 
    /**
     * @brief Return the abscissas in Y-direction 
     *
     * @return A 2d vector
     */
    const thrust::host_vector<double>& abscissasY() const {return absY_;} 
    /**
     * @brief Return the weights in X-direction 
     *
     * @return A 2d vector
     */
    const thrust::host_vector<double>& weightsX() const {return wx_;} 
    /**
     * @brief Return the weights in Y-direction 
     *
     * @return A 2d vector
     */
    const thrust::host_vector<double>& weightsY() const {return wy_;} 

    protected:
    void init_X_boundaries( double x0, double x1)
    {
        double alpha = (x1-x0)/this->lx();
        double beta = (x0*this->x1()-x1*this->x0())/this->lx();
        //weights are invariant
        for( unsigned i=0; i<absX_.size(); i++)
            absX_[i]=alpha*absX_[i]+beta;
        dg::Grid2d::init_X_boundaries( x0, x1);
    }
    /**
     * @brief Refine a corner of a grid
     *
     * @param node_x
     * @param node_y
     * @param add_x Add number of cells to the existing one
     * @param add_y Add number of cells to the existing one
     * @param howmanyX Add number of cells to the existing one
     * @param howmanyY Add number of cells to the existing one
     * @copydetails Grid2d::Grid2d()
     */
    aRefinedGrid2d( unsigned node_x, unsigned node_y, unsigned add_x, unsigned add_y, 
            unsigned howmanyX, unsigned howmanyY,
            double x0, double x1, double y0, double y1, 
            unsigned n, unsigned Nx, unsigned Ny, bc bcx = dg::PER, bc bcy = dg::PER) : dg::aGeometry2d( x0, x1, y0, y1, n, n_new(Nx, add_x*howmanyX, bcx), n_new(Ny, add_y*howmanyY, bcy), bcx, bcy), 
        wx_(size()), wy_(size()), absX_(size()), absY_(size()),
        g_assoc_( x0, x1, y0, y1, n, Nx, Ny, bcx, bcy)
    {
        //assert( howmanyX <= node_x && howmanyX <= Nx - node_x);
        //assert( howmanyY <= node_y && howmanyY <= Ny - node_y);
        Grid1d gx( x0, x1, n, Nx, bcx);
        Grid1d gy( y0, y1, n, Ny, bcy);
        thrust::host_vector<double> wx, ax, wy, ay;
        detail::equidist_ref( add_x, node_x, gx, wx, ax, howmanyX);
        detail::equidist_ref( add_y, node_y, gy, wy, ay, howmanyY);
        //now make product space
        for( unsigned i=0; i<wy.size(); i++)
            for( unsigned j=0; j<wx.size(); j++)
            {
                wx_[i*wx.size()+j] = wx[j];
                wy_[i*wx.size()+j] = wy[i];
                absX_[i*wx.size()+j] = ax[j];
                absY_[i*wx.size()+j] = ay[i];
            }
    }

    /**
     * @brief Refine a all cells of a grid
     *
     * @param multiple_x refine all cells in x 
     * @param multiple_y refine all cells in y
     * @param n_old the polynomials in the old grid
     * @copydetails Grid2d::Grid2d()
     */
    aRefinedGrid2d( unsigned multiple_x, unsigned multiple_y,
            double x0, double x1, double y0, double y1, unsigned n,
            unsigned n_old, unsigned Nx, unsigned Ny, bc bcx = dg::PER, bc bcy = dg::PER) : dg::aGeometry2d( x0, x1, y0, y1, n, multiple_x*Nx, multiple_y*Ny, bcx, bcy), 
        wx_(size()), wy_(size()), absX_(size()), absY_(size()),
        g_assoc_( x0, x1, y0, y1, n_old, Nx, Ny, bcx, bcy)
    {
        Grid1d gx( x0, x1, n, Nx, bcx);
        Grid1d gy( y0, y1, n, Ny, bcy);
        thrust::host_vector<double> wx, ax, wy, ay;
        detail::linear_ref( multiple_x, gx, wx, ax);
        detail::linear_ref( multiple_y, gy, wy, ay);
        //now make product space
        for( unsigned i=0; i<wy.size(); i++)
            for( unsigned j=0; j<wx.size(); j++)
            {
                wx_[i*wx.size()+j] = wx[j];
                wy_[i*wx.size()+j] = wy[i];
                absX_[i*wx.size()+j] = ax[j];
                absY_[i*wx.size()+j] = ay[i];
            }
    }

    private:
    unsigned n_new( unsigned N, unsigned factor, dg::bc bc)
    {
        if( bc == dg::PER) return N + 2*factor; 
        return N + factor;
    }
    thrust::host_vector<double> wx_, wy_; //weights
    thrust::host_vector<double> absX_, absY_; //abscissas 
    dg::Grid2d g_assoc_;
};

/**
 * @brief aRefined grid 
 * @deprecated
 * @ingroup grid
 */
struct aRefinedGrid3d : public dg::aGeometry3d
{
    /**
     * @brief The grid that this object refines
     *
     * This function is vitual so that derived classes can also construct the associated grid 
     * @return  2d grid
     */
    virtual const dg::Grid3d& associated()const {return g_assoc_;}
    /**
     * @brief Return the abscissas in X-direction 
     *
     * @return A 2d vector
     */
    const thrust::host_vector<double>& abscissasX() const {return absX_;} 
    /**
     * @brief Return the abscissas in Y-direction 
     *
     * @return A 2d vector
     */
    const thrust::host_vector<double>& abscissasY() const {return absY_;} 
    /**
     * @brief Return the weights in X-direction 
     *
     * @return A 2d vector
     */
    const thrust::host_vector<double>& weightsX() const {return wx_;} 
    /**
     * @brief Return the weights in Y-direction 
     *
     * @return A 2d vector
     */
    const thrust::host_vector<double>& weightsY() const {return wy_;} 
    protected:
    void init_X_boundaries( double x0, double x1)
    {
        double alpha = (x1-x0)/this->lx();
        double beta = (x0*this->x1()-x1*this->x0())/this->lx();
        //weights are invariant under linear transformation
        for( unsigned i=0; i<absX_.size(); i++)
            absX_[i]=alpha*absX_[i]+beta;
        dg::Grid3d::init_X_boundaries( x0, x1);
    }
    /**
     * @brief Refine a corner of a grid
     *
     * @param node_x index of X-point
     * @param node_y index of X-point
     * @param add_x Add number of cells to the existing one
     * @param add_y Add number of cells to the existing one
     * @param howmanyX howmany cells should be refined in x
     * @param howmanyY howmany cells should be refined in y
     * @copydetails Grid3d::Grid3d()
     */
    aRefinedGrid3d( unsigned node_x, unsigned node_y, unsigned add_x, unsigned add_y, 
            unsigned howmanyX, unsigned howmanyY,
            double x0, double x1, double y0, double y1, double z0, double z1, 
            unsigned n, unsigned Nx, unsigned Ny, unsigned Nz, bc bcx = dg::PER, bc bcy = dg::PER, bc bcz = dg::PER) : dg::aGeometry3d( x0, x1, y0, y1, z0, z1, n, n_new(Nx, add_x*howmanyX, bcx), n_new(Ny, add_y*howmanyY, bcy), Nz, bcx, bcy, bcz), 
        wx_(size()), wy_(size()), absX_(size()), absY_(size()),
        g_assoc_( x0, x1, y0, y1, z0, z1, n, Nx, Ny, Nz, bcx, bcy, bcz)
    {
        //assert( howmanyX <= node_x && howmanyX <= Nx - node_x);
        //assert( howmanyY <= node_y && howmanyY <= Ny - node_y);
        Grid1d gx( x0, x1, n, Nx, bcx);
        Grid1d gy( y0, y1, n, Ny, bcy);
        thrust::host_vector<double> wx, ax, wy, ay;
        detail::equidist_ref( add_x, node_x, gx, wx, ax, howmanyX);
        detail::equidist_ref( add_y, node_y, gy, wy, ay, howmanyY);
        //now make product space
        for( unsigned s=0; s<Nz; s++)
            for( unsigned i=0; i<wy.size(); i++)
                for( unsigned j=0; j<wx.size(); j++)
                {
                    wx_[(s*wy.size()+i)*wx.size()+j] = wx[j];
                    wy_[(s*wy.size()+i)*wx.size()+j] = wy[i];
                    absX_[(s*wy.size()+i)*wx.size()+j] = ax[j];
                    absY_[(s*wy.size()+i)*wx.size()+j] = ay[i];
                }
    }

    /**
     * @brief Refine all cells of a grid
     *
     * @param multiple_x Multiply all cells in x - direction
     * @param multiple_y Multiply all cells in y - direction
     * @copydetails Grid3d::Grid3d()
     */
    aRefinedGrid3d( unsigned multiple_x, unsigned multiple_y,
            double x0, double x1, double y0, double y1, double z0, double z1, 
            unsigned n,
            unsigned n_old, unsigned Nx, unsigned Ny, unsigned Nz, 
            bc bcx = dg::PER, bc bcy = dg::PER, bc bcz = dg::PER) : 
        dg::aGeometry3d( x0, x1, y0, y1, z0, z1, n, multiple_x*Nx, multiple_y*Ny, Nz, bcx, bcy, bcz), 
        wx_(size()), wy_(size()), absX_(size()), absY_(size()),
        g_assoc_( x0, x1, y0, y1, z0, z1, n_old, Nx, Ny, Nz, bcx, bcy, bcz)
    {
        Grid1d gx( x0, x1, n, Nx, bcx);
        Grid1d gy( y0, y1, n, Ny, bcy);
        thrust::host_vector<double> wx, ax, wy, ay;
        detail::linear_ref( multiple_x, gx, wx, ax);
        detail::linear_ref( multiple_y, gy, wy, ay);
        //now make product space
        for( unsigned s=0; s<Nz; s++)
            for( unsigned i=0; i<wy.size(); i++)
                for( unsigned j=0; j<wx.size(); j++)
                {
                    wx_[(s*wy.size()+i)*wx.size()+j] = wx[j];
                    wy_[(s*wy.size()+i)*wx.size()+j] = wy[i];
                    absX_[(s*wy.size()+i)*wx.size()+j] = ax[j];
                    absY_[(s*wy.size()+i)*wx.size()+j] = ay[i];
                }
    }

    private:
    unsigned n_new( unsigned N, unsigned factor, dg::bc bc)
    {
        if( bc == dg::PER) return N + 2*factor; 
        return N + factor;
    }
    thrust::host_vector<double> wx_, wy_; //weights
    thrust::host_vector<double> absX_, absY_; //abscissas 
    dg::Grid3d g_assoc_;
    virtual SparseTensor<thrust::host_vector<double> > do_compute_metric()const {

        return SparseTensor<thrust::host_vector<double> >();
    }
    virtual SparseTensor<thrust::host_vector<double> > do_compute_jacobian()const {
        return SparseTensor<thrust::host_vector<double> >();
    }
    virtual std::vector<thrust::host_vector<double> > do_compute_map()const{
        std::vector<thrust::host_vector<double> > map(2);
        map[0] = dg::evaluate(dg::cooX2d, *this);
        map[1] = dg::evaluate(dg::cooY2d, *this);
        return map;
    }

};

/**
 * @brief A refined cartesian grid
 *
 * @ingroup basicgrids
 * @deprecated
 */
struct CartesianRefinedGrid2d : public dg::aRefinedGrid2d
{
    typedef CurvilinearPerpTag metric_category; 
    CartesianaRefinedGrid2d( unsigned multiple_x, unsigned multiple_y, double x0, double x1, double y0, double y1, unsigned n, unsigned n_old, unsigned Nx, unsigned Ny, bc bcx = PER, bc bcy = PER): dg::aRefinedGrid2d(multiple_x, multiple_y,x0,x1,y0,y1,n,n_old,Nx,Ny,bcx,bcy), g_assoc_(x0,x1,y0,y1,n_old,Nx,Ny,bcx,bcy){ 
        dg::blas1::transfer( weightsX(), g_xx_);
        dg::blas1::transfer( weightsY(), g_yy_);
        dg::blas1::transfer( weightsX(), vol2d_);
        dg::blas1::pointwiseDot( g_xx_,g_yy_,vol2d_);
        dg::blas1::pointwiseDot( g_xx_,g_xx_,g_xx_);
        dg::blas1::pointwiseDot( g_yy_,g_yy_,g_yy_);
        dg::blas1::pointwiseDivide( vol2d_, g_xx_, vol2d_);
        dg::blas1::pointwiseDivide( vol2d_, g_yy_, vol2d_);
    }

    const dg::CartesianGrid2d& associated() const {return g_assoc_;}
    virtual CartesianRefinedGrid2d* clone()const{return new CartesianRefinedGrid2d(*this);}
    private:
    dg::CartesianGrid2d g_assoc_;
};

namespace create{

///@deprecated
cusp::coo_matrix<int, double, cusp::host_memory> interpolation( const dg::aRefinedGrid2d& g_fine)
{
    dg::Grid2d g = g_fine.associated();
    thrust::host_vector<double> x = g_fine.abscissasX();
    thrust::host_vector<double> y = g_fine.abscissasY();
    return dg::create::interpolation( x,y, g);

}
///@deprecated
cusp::coo_matrix<int, double, cusp::host_memory> interpolationT( const dg::aRefinedGrid2d& g_fine)
{
    cusp::coo_matrix<int, double, cusp::host_memory> temp = interpolation( g_fine), A;
    cusp::transpose( temp, A);
    return A;
}

///@deprecated
cusp::coo_matrix<int, double, cusp::host_memory> projection( const dg::aRefinedGrid2d& g_fine)
{
    //form the adjoint
    thrust::host_vector<double> w_f = dg::create::weights( g_fine);
    thrust::host_vector<double> v_c = dg::create::inv_weights( g_fine.associated() );
    cusp::coo_matrix<int, double, cusp::host_memory> Wf( w_f.size(), w_f.size(), w_f.size());
    cusp::coo_matrix<int, double, cusp::host_memory> Vc( v_c.size(), v_c.size(), v_c.size());
    for( int i =0; i<(int)w_f.size(); i++)
    {
        Wf.row_indices[i] = Wf.column_indices[i] = i;
        Wf.values[i] = w_f[i]/g_fine.weightsX()[i]/g_fine.weightsY()[i];
    }
    for( int i =0; i<(int)v_c.size(); i++)
    {
        Vc.row_indices[i] = Vc.column_indices[i] = i;
        Vc.values[i] = v_c[i];
    }
    cusp::coo_matrix<int, double, cusp::host_memory> temp = interpolation( g_fine), A;
    cusp::transpose( temp, A);
    cusp::multiply( A, Wf, temp);
    cusp::multiply( Vc, temp, A);
    A.sort_by_row_and_column();
    return A;
}

///@deprecated
cusp::coo_matrix<int, double, cusp::host_memory> smoothing( const dg::aRefinedGrid2d& g)
{
    cusp::coo_matrix<int, double, cusp::host_memory> A = interpolation(g);
    cusp::coo_matrix<int, double, cusp::host_memory> B = projection(g);
    cusp::coo_matrix<int, double, cusp::host_memory> C;
    cusp::multiply( A, B, C);
    C.sort_by_row_and_column();
    return C; 
}

///@deprecated
cusp::coo_matrix<int, double, cusp::host_memory> interpolation( const dg::aRefinedGrid3d& g_fine)
{
    dg::Grid3d g = g_fine.associated();
    thrust::host_vector<double> x = g_fine.abscissasX();
    thrust::host_vector<double> y = g_fine.abscissasY();
    thrust::host_vector<double> z = dg::evaluate( dg::cooZ3d, g_fine);
    return dg::create::interpolation( x,y,z, g);
}

///@deprecated
cusp::coo_matrix<int, double, cusp::host_memory> interpolationT( const dg::aRefinedGrid3d& g_fine)
{
    cusp::coo_matrix<int, double, cusp::host_memory> temp = interpolation( g_fine), A;
    cusp::transpose( temp, A);
    return A;
}

///@deprecated
cusp::coo_matrix<int, double, cusp::host_memory> projection( const dg::aRefinedGrid3d& g_fine)
{
    //form the adjoint
    thrust::host_vector<double> w_f = dg::create::weights( g_fine);
    thrust::host_vector<double> v_c = dg::create::inv_weights( g_fine.associated() );
    cusp::coo_matrix<int, double, cusp::host_memory> Wf( w_f.size(), w_f.size(), w_f.size());
    cusp::coo_matrix<int, double, cusp::host_memory> Vc( v_c.size(), v_c.size(), v_c.size());
    for( int i =0; i<(int)w_f.size(); i++)
    {
        Wf.row_indices[i] = Wf.column_indices[i] = i;
        Wf.values[i] = w_f[i]/g_fine.weightsX()[i]/g_fine.weightsY()[i];
    }
    for( int i =0; i<(int)v_c.size(); i++)
    {
        Vc.row_indices[i] = Vc.column_indices[i] = i;
        Vc.values[i] = v_c[i];
    }
    cusp::coo_matrix<int, double, cusp::host_memory> temp = interpolation( g_fine), A;
    cusp::transpose( temp, A);
    cusp::multiply( A, Wf, temp);
    cusp::multiply( Vc, temp, A);
    A.sort_by_row_and_column();
    return A;
}

///@deprecated
cusp::coo_matrix<int, double, cusp::host_memory> smoothing( const dg::aRefinedGrid3d& g)
{
    cusp::coo_matrix<int, double, cusp::host_memory> A = interpolation(g);
    cusp::coo_matrix<int, double, cusp::host_memory> B = projection(g);
    cusp::coo_matrix<int, double, cusp::host_memory> C;
    cusp::multiply( A, B, C);
    C.sort_by_row_and_column();
    return C; 
}

}//namespace create

}//namespace dg
