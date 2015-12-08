#pragma once

#include "dg/backend/grid.h"
#include "dg/backend/functions.h"
#include "dg/backend/interpolation.cuh"
#include "dg/backend/operator.h"
#include "dg/functors.h"
#include "dg/runge_kutta.h"
#include "dg/nullstelle.h"
#include "geometry.h"



namespace solovev
{

namespace detail
{


/**
 * @brief Find R such that \f$ \psi_p(R,0) = psi_0\f$
 *
 * @param gp
 * @param psi_0
 *
 * @return 
 */
double find_initial_R( const GeomParameters& gp, double psi_0)
{
    solovev::Psip psip( gp);
    double min = gp.R_0, max = gp.R_0+2*gp.a, middle;
    double value_middle, value_max=psip(gp.R_0+2*gp.a, 0)-psi_0, value_min=psip(gp.R_0, 0) - psi_0;
    if( value_max*value_min>=0)
        throw dg::KeineNST_1D( min, max);
    double eps=max-min, eps_old = 2*eps;
    unsigned number =0;
    while( eps<eps_old)
    {
        eps_old = eps;
        value_middle = psip( middle = (min+max)/2., 0) - psi_0;
        if( value_middle == 0)              {max = min = middle; break;}
        else if( value_middle*value_max >0) max = middle;
        else                                min = middle;
        eps = max-min; number++;
    }
    //std::cout << eps<<" with "<<number<<" steps\n";
    return (min+max)/2;
}

struct Fpsi
{

    Fpsi( const GeomParameters& gp, double psi_0): 
        fieldRZYT_(gp), fieldRZtau_(gp), 
        R_init( detail::find_initial_R( gp, psi_0)), psi_0(psi_0) 
    { }

    //compute f for a given psi between psi0 and psi1
    double construct_f( double psi, double& R_0, double& Z_0) const
    {
        unsigned N = 50;
        thrust::host_vector<double> begin2d( 2, 0), end2d( begin2d), end2d_old(begin2d); 
        begin2d[0] = end2d[0] = end2d_old[0] = R_init;
        //std::cout << "In init function\n";
        double eps = 1e10, eps_old = 2e10;
        while( eps < eps_old && N<1e6 && eps > 1e-15)
        {
            //remember old values
            eps_old = eps;
            end2d_old = end2d;
            //compute new values
            N*=2;
            dg::stepperRK17( fieldRZtau_, begin2d, end2d, psi_0, psi, N);
            eps = sqrt( (end2d[0]-end2d_old[0])*(end2d[0]-end2d_old[0]) + (end2d[1]-end2d_old[1])*(end2d[1]-end2d_old[1]));
        }
        //std::cout << "Begin error "<<eps_old<<" with "<<N<<" steps\n";
        //std::cout << "In Stepper function:\n";
        //double y_old=0;
        thrust::host_vector<double> begin( 3, 0), end(begin), end_old(begin);
        R_0 = begin[0] = end2d_old[0], Z_0 = begin[1] = end2d_old[1];
        //std::cout << begin[0]<<" "<<begin[1]<<" "<<begin[2]<<"\n";
        eps = 1e10, eps_old = 2e10;
        N = 50;
        //double y_eps;
        while( eps < eps_old && N < 1e6)
        {
            //remember old values
            eps_old = eps, end_old = end; //y_old = end[2];
            //compute new values
            N*=2;
            dg::stepperRK17( fieldRZYT_, begin, end, 0., 2*M_PI, N);
            eps = sqrt( (end[0]-begin[0])*(end[0]-begin[0]) + (end[1]-begin[1])*(end[1]-begin[1]));
            //y_eps = sqrt( (y_old - end[2])*(y_old-end[2]));
            //std::cout << "\t error "<<eps<<" with "<<N<<" steps\t";
            //std::cout <<"error in y is "<<y_eps<<"\n";
        }
        double f_psi = 2.*M_PI/end_old[2];

        return f_psi;
    }
    double operator()( double psi)const{double R_0, Z_0; return construct_f( psi, R_0, Z_0);}

    private:
    const FieldRZYT fieldRZYT_;
    const FieldRZtau fieldRZtau_;
    const double R_init;
    const double psi_0;

};

struct FieldFinv
{
    FieldFinv( const GeomParameters& gp, double psi_0, double psi_1): psi_0(psi_0), psi_1(psi_1), R_init( detail::find_initial_R(gp, psi_0)), fpsi_(gp, psi_0), fieldRZYT_(gp), fieldRZtau_(gp) 
            {
    }
    inline void operator()(const thrust::host_vector<double>& psi, thrust::host_vector<double>& fpsiM) const { 
        unsigned N = 50;
        thrust::host_vector<double> begin2d( 2, 0), end2d( begin2d), end2d_old(begin2d); 
        begin2d[0] = end2d[0] = end2d_old[0] = R_init;
        //std::cout << "In init function\n";
        double eps = 1e10, eps_old = 2e10;
        while( eps < eps_old && N<1e6)
        {
            //remember old values
            eps_old = eps;
            end2d_old = end2d;
            //compute new values
            N*=2;
            dg::stepperRK17( fieldRZtau_, begin2d, end2d, psi_0, psi[0], N);
            eps = sqrt( (end2d[0]-end2d_old[0])*(end2d[0]-end2d_old[0]) + (end2d[1]-end2d_old[1])*(end2d[1]-end2d_old[1]));
        }
        thrust::host_vector<double> begin( 3, 0), end(begin), end_old(begin);
        begin[0] = end2d_old[0], begin[1] = end2d_old[1];

        //eps = 1e10, eps_old = 2e10;
        //N = 10;
        //double y_old;
        //while( eps < eps_old && N < 1e6)
        //{
        //    //remember old values
        //    eps_old = eps, end_old = end, y_old = end[2];
        //    //compute new values
        //    N*=2;
        //    dg::stepperRK17( fieldRZYT_, begin, end, 0., 2*M_PI, N);
        //    //eps = sqrt( (end[0]-begin[0])*(end[0]-begin[0]) + (end[1]-begin[1])*(end[1]-begin[1]));
        //    eps = fabs( (y_old - end[2]));
        //    //std::cout << "F error "<<eps<<" with "<<N<<" steps\n";
        //    //std::cout <<"error in y is "<<y_eps<<"\n";
        //}

        //std::cout << begin[0]<<" "<<begin[1]<<" "<<begin[2]<<"\n";
        dg::stepperRK17( fieldRZYT_, begin, end, 0., 2*M_PI, 500);
        //eps = sqrt( (end[0]-begin[0])*(end[0]-begin[0]) + (end[1]-begin[1])*(end[1]-begin[1]));
        fpsiM[0] = - end[2]/2./M_PI;
        //std::cout <<"fpsiMinverse is "<<fpsiM[0]<<" "<<-1./fpsi_(psi[0])<<" "<<eps<<"\n";
    }
    private:
    double psi_0, psi_1, R_init;
    Fpsi fpsi_;
    FieldRZYT fieldRZYT_;
    FieldRZtau fieldRZtau_;

    thrust::host_vector<double> fpsi_neg_inv;
    unsigned P_;
};

double find_x1( const GeomParameters& gp, double psi_0, double psi_1 )
{
    Fpsi fpsi(gp, psi_0);
    unsigned P=3;
    double x1 = 0, x1_old = 0;
    double eps=1e10, eps_old=2e10;
    std::cout << "In x1 function\n";
    while(eps < eps_old && P < 20 && eps > 1e-15)
    {
        eps_old = eps; 
        x1_old = x1;

        P+=1;
        dg::Grid1d<double> grid( psi_0, psi_1, P, 1);
        thrust::host_vector<double> psi_vec = dg::evaluate( dg::coo1, grid);
        thrust::host_vector<double> f_vec(grid.size(), 0);
        thrust::host_vector<double> w1d = dg::create::weights(grid);
        for( unsigned i=0; i<psi_vec.size(); i++)
        {
            f_vec[i] = fpsi( psi_vec[i]);
        }
        x1 = dg::blas1::dot( f_vec, w1d);

        eps = fabs(x1 - x1_old);
        std::cout << "X1 = "<<-x1<<" error "<<eps<<" with "<<P<<" polynomials\n";
    }
    return -x1_old;

}


struct Naive
{
    Naive( const dg::Grid2d<double>& g2d): dx_(dg::create::pidxpj(g2d.n())), dy_(dx_)
    {
        dg::Operator<double> tx( dg::create::pipj_inv(g2d.n())), ty(tx);
        dg::Operator<double> forward( g2d.dlt().forward()); 
        dg::Operator<double> backward( g2d.dlt().backward());
        tx*= 2./g2d.hx();
        ty*= 2./g2d.hy();
        dx_ = backward*tx*dx_*forward;
        dy_ = backward*ty*dy_*forward;
        Nx = g2d.Nx();
        Ny = g2d.Ny();
        n = g2d.n();
    }
    void dx( const thrust::host_vector<double>& in, thrust::host_vector<double>& out)
    {
        for( unsigned i=0; i<Ny*n; i++)
            for( unsigned j=0; j<Nx; j++)
                for( unsigned k=0; k<n; k++)
                {
                    out[i*Nx*n + j*n +k] = 0;
                    for( unsigned l=0; l<n; l++)
                        out[i*Nx*n + j*n +k] += dx_(k,l)*in[i*Nx*n+j*n+l];
                }
    }
    void dy( const thrust::host_vector<double>& in, thrust::host_vector<double>& out)
    {
        for( unsigned i=0; i<Ny; i++)
            for( unsigned k=0; k<n; k++)
                for( unsigned j=0; j<Nx*n; j++)
                {
                    out[i*Nx*n*n + k*Nx*n +j] = 0;
                    for( unsigned l=0; l<n; l++)
                        out[i*Nx*n*n + k*Nx*n +j] += dy_(k,l)*in[i*Nx*n*n+l*Nx*n+j];
                }
    }

    private:
    dg::Operator<double> dx_, dy_;
    unsigned Nx, Ny, n;

};


} //namespace detail


struct ConformalRingGrid
{

    ConformalRingGrid( GeomParameters gp, double psi_0, double psi_1, unsigned n, unsigned Nx, unsigned Ny, dg::bc bcx): 
        g2d_( 0, detail::find_x1(gp, psi_0, psi_1), 0, 2*M_PI, n, Nx, Ny, bcx, dg::PER),
        psi_0(psi_0), psi_1(psi_1),
        gp_(gp), 
        psi_x(g2d_.n()*g2d_.Nx(),0){ }

    //compute psi for a grid on x 
    void construct_psi( ) 
    {
        detail::FieldFinv fpsiM_(gp_, psi_0, psi_1);
        //assert( psi.size() == g2d_.n()*g2d_.Nx());
        dg::Grid1d<double> g1d_( g2d_.x0(), g2d_.x1(), g2d_.n(), g2d_.Nx(), g2d_.bcx());
        thrust::host_vector<double> x_vec = dg::evaluate( dg::coo1, g1d_);
        thrust::host_vector<double> psi_old(g2d_.n()*g2d_.Nx(), 0), psi_diff( psi_old);
        thrust::host_vector<double> w1d = dg::create::weights( g1d_);
        thrust::host_vector<double> begin(1,psi_0), end(begin), temp(begin);
        unsigned N = 1;
        double eps = 1e10; //eps_old=2e10;
        std::cout << "In psi function:\n";
        double x0=g2d_.x0(), x1 = x_vec[1];
        //while( eps <  eps_old && N < 1e6)
        while( eps >  1e-10 && N < 1e6)
        {
            //eps_old = eps;
            psi_old = psi_x; 
            x0 = 0, x1 = x_vec[0];

            dg::stepperRK17( fpsiM_, begin, end, x0, x1, N);
            psi_x[0] = end[0]; 
            for( unsigned i=1; i<g1d_.size(); i++)
            {
                temp = end;
                x0 = x_vec[i-1], x1 = x_vec[i];
                dg::stepperRK17( fpsiM_, temp, end, x0, x1, N);
                psi_x[i] = end[0];
            }
            dg::blas1::axpby( 1., psi_x, -1., psi_old, psi_diff);
            double epsi = dg::blas2::dot( psi_diff, w1d, psi_diff);
            eps =  sqrt( epsi);
            std::cout << "Psi error is "<<eps<<" with "<<N<<" steps\n";
            temp = end;
            dg::stepperRK17(fpsiM_, temp, end, x1, g2d_.x1(),N);
            eps = fabs( end[0]-psi_1); 
            std::cout << "Effective Psi error is "<<eps<<" with "<<N<<" steps\n";
            N*=2;
        }

        //psi_x = psi_old;
    }
    void construct_rz( thrust::host_vector<double>& r, thrust::host_vector<double>& z) 
    {
        assert( r.size() == g2d_.size() && z.size() == g2d_.size());
        const unsigned Nx = g2d_.n()*g2d_.Nx();
        thrust::host_vector<double> y_vec = dg::evaluate( dg::coo2, g2d_);
        thrust::host_vector<double> r_old(g2d_.size(), 0), r_diff( r_old);
        thrust::host_vector<double> z_old(g2d_.size(), 0), z_diff( z_old);
        const thrust::host_vector<double> w2d = dg::create::weights( g2d_);
        unsigned N = 1;
        double eps = 1e10, eps_old=2e10;
        f_x.resize( g2d_.n()*g2d_.Nx());
        FieldRZY fieldRZY(gp_);
        detail::Fpsi fpsi( gp_, psi_0);
        for( unsigned i=0; i<Nx; i++)
            f_x[i] = fpsi( psi_x[i]);
        std::cout << "In grid function:\n";
        while( eps <  eps_old && N < 1e6)
        {
            eps_old = eps;
            r_old = r; z_old = z;
            N*=2;
            for( unsigned j=0; j<Nx; j++)
            {
                thrust::host_vector<double> begin( 2, 0), end(begin), temp(begin);
                double f_psi = fpsi.construct_f( psi_x[j], begin[0], begin[1]);
                //std::cout <<f_psi<<" "<< psi_x[j] <<" "<< begin[0] << " "<<begin[1]<<"\t";
                fieldRZY.set_f(f_psi);
                double y0 = 0, y1 = y_vec[0]; 
                dg::stepperRK17( fieldRZY, begin, end, y0, y1, N);
                r[0+j] = end[0]; z[0+j] = end[1];
                //std::cout <<end[0]<<" "<< end[1] <<"\n";
                for( unsigned i=1; i<g2d_.n()*g2d_.Ny(); i++)
                {
                    temp = end;
                    y0 = y_vec[(i-1)*Nx+j], y1 = y_vec[i*Nx+j];
                    dg::stepperRK17( fieldRZY, temp, end, y0, y1, N);
                    r[i*Nx+j] = end[0]; z[i*Nx+j] = end[1];
                    //std::cout << y0<<" "<<y1<<" "<<temp[0]<<" "<<temp[1]<<" "<<end[0]<<" "<<end[1]<<"\n";
                }
                //std::cout << r[j] <<" "<< z[j] << " "<<r[Nx + j]<<" "<<z[Nx + j]<<"\n";
            }
            dg::blas1::axpby( 1., r, -1., r_old, r_diff);
            dg::blas1::axpby( 1., z, -1., z_old, z_diff);
            double er = dg::blas2::dot( r_diff, w2d, r_diff);
            double ez = dg::blas2::dot( z_diff, w2d, z_diff);
            eps =  sqrt( er + ez);
            std::cout << "error is "<<eps<<" with "<<N<<" steps\n";
        }

        r = r_old, z = z_old;
    }
    void construct_metric()
    {
        r_.resize(g2d_.size()), z_.resize( g2d_.size());
        g_xx.resize(g2d_.size()), g_xy.resize( g2d_.size()), g_yy.resize( g2d_.size()), g_pp.resize( g2d_.size()), vol.resize( g2d_.size());
        construct_rz( r_, z_);
        std::cout << "Construction successful!\n";
        thrust::host_vector<double> w2d = dg::create::weights( g2d_);
        thrust::host_vector<double> r_x( r_), r_y(r_), z_x(r_), z_y(r_);
        thrust::host_vector<double> temp0( r_), temp1(r_);
        detail::Naive naive( g2d_);
        naive.dx( r_, r_x), naive.dx( z_, z_x);
        naive.dy( r_, r_y), naive.dy( z_, z_y);
        dg::blas1::pointwiseDot( r_x, r_x, temp0);
        dg::blas1::pointwiseDot( z_x, z_x, temp1);
        dg::blas1::axpby( 1., temp0, 1., temp1, g_xx);
        dg::blas1::pointwiseDot( r_x, r_y, temp0);
        dg::blas1::pointwiseDot( z_x, z_y, temp1);
        dg::blas1::axpby( 1., temp0, 1., temp1, g_xy);
        dg::blas1::pointwiseDot( r_y, r_y, temp0);
        dg::blas1::pointwiseDot( z_y, z_y, temp1);
        dg::blas1::axpby( 1., temp0, 1., temp1, g_yy);
        dg::blas1::pointwiseDot( g_xx, g_yy, temp0);
        dg::blas1::pointwiseDot( g_xy, g_xy, temp1);
        dg::blas1::axpby( 1., temp0, -1., temp1, vol); //determinant
        //now invert to get contravariant elements
        dg::blas1::pointwiseDivide( g_xx, vol, g_xx);
        dg::blas1::pointwiseDivide( g_xy, vol, g_xy);
        dg::blas1::pointwiseDivide( g_yy, vol, g_yy);
        g_xx.swap( g_yy);
        dg::blas1::scal( g_xy, -1.);
        //compute real volume form
        dg::blas1::transform( vol, vol, dg::SQRT<double>());
        dg::blas1::pointwiseDot( r_, vol, vol);
        thrust::host_vector<double> ones = dg::evaluate( dg::one, g2d_);
        dg::blas1::pointwiseDivide( ones, r_, temp0);
        dg::blas1::pointwiseDivide( temp0, r_, g_pp); //1/R^2

        //compute error in volume element
        dg::blas1::pointwiseDot( g_xx, g_yy, temp0);
        dg::blas1::pointwiseDot( g_xy, g_xy, temp1);
        dg::blas1::axpby( 1., temp0, -1., temp1, temp0);
        //dg::blas1::transform( temp0, temp0, dg::SQRT<double>());
        dg::blas1::pointwiseDot( g_xx, g_xx, temp1);

        dg::blas1::axpby( 1., temp1, -1., temp0, temp0);
        std::cout<< "Rel Error in Determinant is "<<sqrt( dg::blas2::dot( temp0, w2d, temp0)/dg::blas2::dot( temp1, w2d, temp1))<<"\n";
        dg::blas1::pointwiseDot( g_xx, g_yy, temp0);
        dg::blas1::pointwiseDot( g_xy, g_xy, temp1);
        dg::blas1::axpby( 1., temp0, -1., temp1, temp0);
        dg::blas1::pointwiseDot( temp0, g_pp, temp0);
        dg::blas1::transform( temp0, temp0, dg::SQRT<double>());
        dg::blas1::pointwiseDivide( ones, temp0, temp0);
        dg::blas1::axpby( 1., temp0, -1., vol, temp0);
        std::cout << "Rel Consistency  of volume is "<<sqrt(dg::blas2::dot( temp0, w2d, temp0)/dg::blas2::dot( vol, w2d, vol))<<"\n";

        dg::blas1::pointwiseDivide( r_, g_xx, temp0);
        dg::blas1::axpby( 1., temp0, -1., vol, temp0);
        std::cout << "Rel Error of volume form is "<<sqrt(dg::blas2::dot( temp0, w2d, temp0))/sqrt( dg::blas2::dot(vol, w2d, vol))<<"\n";

        FieldY fieldY(gp_);
        thrust::host_vector<double> by = pull_back( fieldY);
        for( unsigned i=0; i<g2d_.n()*g2d_.Ny(); i++)
            for( unsigned j=0; j<g2d_.n()*g2d_.Nx(); j++)
                by[i*g2d_.n()*g2d_.Nx() + j] *= f_x[j];
        dg::blas1::scal( by, 1./gp_.R_0);
        dg::blas1::pointwiseDivide( g_xx, r_, temp0);
        std::cout << "Magnitude by " << dg::blas2::dot( by, w2d, by)<<"\n";
        std::cout << "Magnitude g_xx " << dg::blas2::dot( temp0, w2d, temp0)<<"\n";
        dg::blas1::axpby( 1., temp0, -1., by, temp1);
        double err= dg::blas2::dot( temp1, w2d, temp1);
        std::cout << "Rel Error of g_xx is "<<sqrt(err/dg::blas2::dot( by, w2d, by))<<"\n";


        

        

    }
    template< class BinaryOp>
    thrust::host_vector<double> pull_back( BinaryOp f)
    {
        thrust::host_vector<double> vec( g2d_.size());
        for( unsigned i=0; i<vec.size(); i++)
            vec[i] = f( r_[i], z_[i]);
        return vec;
    }
    thrust::host_vector<double> pull_back( double (f)(double,double))
    {
        thrust::host_vector<double> vec( g2d_.size());
        for( unsigned i=0; i<vec.size(); i++)
            vec[i] = f( r_[i], z_[i]);
        return vec;
    }

    const dg::Grid2d<double>& grid() const{return g2d_;}
    private:
    const dg::Grid2d<double> g2d_;
    const double psi_0, psi_1;
    const GeomParameters gp_;
    thrust::host_vector<double> psi_x, f_x;
    thrust::host_vector<double> r_, z_;
    thrust::host_vector<double> g_xx, g_xy, g_yy, g_pp, vol;

};

}//namespace solovev
