#pragma once

#include <thrust/host_vector.h>
#include "grid.h"
#include "operator.h"
#include "../enums.h"

/*! @file

  * @brief Creation functions for integration weights and their inverse
  */

namespace dg{
namespace create{

///@addtogroup highlevel
///@{

/*!@class hide_weights_doc
* @brief create host vector containing X-space weight coefficients
* @param g The grid
* @return Host Vector
* @sa <a href="./dg_introduction.pdf" target="_blank">Introduction to dg methods</a>
*/
/*!@class hide_inv_weights_doc
* @brief create host_vector containing inverse X-space weight coefficients
* @param g The grid
* @return Host Vector
* @sa <a href="./dg_introduction.pdf" target="_blank">Introduction to dg methods</a>
*/
/*!@class hide_weights_coo_doc
* @brief create host vector containing X-space weight coefficients
* @param g The grid
* @param coo The coordinate for which to generate the weights (in 2d only \c dg::x and \c dg::y are allowed)
* @return Host Vector with full grid size
* @sa <a href="./dg_introduction.pdf" target="_blank">Introduction to dg methods</a>
*/

///@copydoc hide_weights_doc
///@copydoc hide_code_evaluate1d
template<class real_type>
thrust::host_vector<real_type> weights( const RealGrid1d<real_type>& g)
{
    thrust::host_vector<real_type> v( g.size());
    for( unsigned i=0; i<g.N(); i++)
        for( unsigned j=0; j<g.n(); j++)
            v[i*g.n() + j] = g.h()/2.*g.dlt().weights()[j];
    return v;
}
///@copydoc hide_inv_weights_doc
template<class real_type>
thrust::host_vector<real_type> inv_weights( const RealGrid1d<real_type>& g)
{
    thrust::host_vector<real_type> v = weights( g);
    for( unsigned i=0; i<g.size(); i++)
        v[i] = 1./v[i];
    return v;
}

/*!@brief Create the integral of a function on a grid
 * \f[ F_h(x) = \int_a^x f_h(x') dx' \f]
 *
 * This function computes the indefinite integral of a given input
 * @param in Host vector discretized on g
 * @param g The grid
 * @return integral of in on the grid g
 * @sa <a href="./dg_introduction.pdf" target="_blank">Introduction to dg methods</a>
 */
template<class real_type>
thrust::host_vector<real_type> integral( const thrust::host_vector<real_type>& in, const RealGrid1d<real_type>& g)
{
    double h = g.h();
    unsigned n = g.n();
    thrust::host_vector<real_type> out(in.size(), 0);
    dg::Operator<real_type> forward = g.dlt().forward();
    dg::Operator<real_type> backward = g.dlt().backward();
    dg::Operator<real_type> ninj = create::ninj<real_type>( n );
    Operator<real_type> t = create::pipj_inv<real_type>(n);
    t *= h/2.;
    ninj = backward*t*ninj*forward;
    real_type constant = 0.;

    for( unsigned i=0; i<g.N(); i++)
    {
        for( unsigned k=0; k<n; k++)
        {
            for( unsigned l=0; l<n; l++)
            {
                out[ i*n + k] += ninj(k,l)*in[ i*n + l];
            }
            out[ i*n + k] += constant;
        }
        for( unsigned l=0; l<n; l++)
            constant += h*forward(0,l)*in[i*n+l];
    }
    return out;


}

///@cond
namespace detail{

static inline int get_i( unsigned n, int idx) { return idx%(n*n)/n;}
static inline int get_j( unsigned n, int idx) { return idx%(n*n)%n;}
static inline int get_i( unsigned n, unsigned Nx, int idx) { return (idx/(n*Nx))%n;}
static inline int get_j( unsigned n, unsigned Nx, int idx) { return idx%n;}
}//namespace detail
///@endcond

///@copydoc hide_weights_doc
///@copydoc hide_code_evaluate2d
template<class real_type>
thrust::host_vector<real_type> weights( const aRealTopology2d<real_type>& g)
{
    thrust::host_vector<real_type> v( g.size());
    for( unsigned i=0; i<g.size(); i++)
        v[i] = g.hx()*g.hy()/4.*
                g.dlt().weights()[detail::get_i(g.n(),g.Nx(), i)]*
                g.dlt().weights()[detail::get_j(g.n(),g.Nx(), i)];
    return v;
}
///@copydoc hide_inv_weights_doc
template<class real_type>
thrust::host_vector<real_type> inv_weights( const aRealTopology2d<real_type>& g)
{
    thrust::host_vector<real_type> v = weights( g);
    for( unsigned i=0; i<g.size(); i++)
        v[i] = 1./v[i];
    return v;
}

///@copydoc hide_weights_coo_doc
template<class real_type>
thrust::host_vector<real_type> weights( const aRealTopology2d<real_type>& g, enum coo2d coo)
{
    thrust::host_vector<real_type> w( g.size());
    if( coo == coo2d::x) {
        for( unsigned i=0; i<g.size(); i++)
            w[i] = g.hx()/2.* g.dlt().weights()[i%g.n()];
    }
    else if( coo == coo2d::y) {
        for( unsigned i=0; i<g.size(); i++)
            w[i] = g.hy()/2.* g.dlt().weights()[(i/(g.n()*g.Nx()))%g.n()];
    }
    return w;
}


///@copydoc hide_weights_doc
///@copydoc hide_code_evaluate3d
template<class real_type>
thrust::host_vector<real_type> weights( const aRealTopology3d<real_type>& g)
{
    thrust::host_vector<real_type> v( g.size());
    for( unsigned i=0; i<g.size(); i++)
        v[i] = g.hx()*g.hy()*g.hz()/4.*
               g.dlt().weights()[detail::get_i(g.n(), g.Nx(), i)]*
               g.dlt().weights()[detail::get_j(g.n(), g.Nx(), i)];
    return v;
}

///@copydoc hide_inv_weights_doc
template<class real_type>
thrust::host_vector<real_type> inv_weights( const aRealTopology3d<real_type>& g)
{
    thrust::host_vector<real_type> v = weights( g);
    for( unsigned i=0; i<g.size(); i++)
        v[i] = 1./v[i];
    return v;
}

///@copydoc hide_weights_coo_doc
template<class real_type>
thrust::host_vector<real_type> weights( const aRealTopology3d<real_type>& g, enum coo3d coo)
{
    thrust::host_vector<real_type> w( g.size());
    if( coo == coo3d::x) {
        for( unsigned i=0; i<g.size(); i++)
            w[i] = g.hx()/2.* g.dlt().weights()[i%g.n()];
    }
    else if( coo == coo3d::y) {
        for( unsigned i=0; i<g.size(); i++)
            w[i] = g.hy()/2.* g.dlt().weights()[(i/(g.n()*g.Nx()))%g.n()];
    }
    else if( coo == coo3d::z) {
        for( unsigned i=0; i<g.size(); i++)
            w[i] = g.hz();
    }
    else if( coo == coo3d::xy) {
        for( unsigned i=0; i<g.size(); i++)
            w[i] = g.hx()*g.hy()/4.* g.dlt().weights()[i%g.n()]*g.dlt().weights()[(i/(g.n()*g.Nx()))%g.n()];
    }
    else if( coo == coo3d::yz) {
        for( unsigned i=0; i<g.size(); i++)
            w[i] = g.hy()*g.hz()/2.* g.dlt().weights()[(i/(g.n()*g.Nx()))%g.n()];
    }
    else if( coo == coo3d::xz) {
        for( unsigned i=0; i<g.size(); i++)
            w[i] = g.hx()*g.hz()/2.* g.dlt().weights()[i%g.n()];
    }
    return w;
}

///@}
}//namespace create
}//namespace dg
