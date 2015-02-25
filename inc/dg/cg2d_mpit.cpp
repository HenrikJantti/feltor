#include <iostream>
#include <iomanip>
#include "mpi.h"

#include <thrust/host_vector.h>

#include "backend/timer.cuh"
#include "backend/mpi_evaluation.h"
#include "backend/mpi_derivatives.h"
#include "backend/mpi_init.h"
#include "elliptic.h"

#include "cg.h"

const double ly = 2.*M_PI;
const double lx = 2.*M_PI;

const double eps = 1e-6; //# of pcg iterations increases very much if 
 // eps << relativer Abstand der exakten Lösung zur Diskretisierung vom Sinus

double fct(double x, double y){ return sin(y)*sin(x);}
double derivative( double x, double y){return cos(x)*sin(y);}
double laplace_fct( double x, double y) { return 2*sin(y)*sin(x);}
double initial( double x, double y) {return sin(0);}

dg::bc bcx = dg::PER;

int main( int argc, char* argv[])
{
    MPI_Init(&argc, &argv);
    unsigned n, Nx, Ny; 
    MPI_Comm comm;
    mpi_init2d( bcx, dg::PER, n, Nx, Ny, comm);

    dg::MPI_Grid2d grid( 0., lx, 0, ly, n, Nx, Ny, bcx, dg::PER, comm);
    const dg::MPrecon w2d = dg::create::weights( grid);
    const dg::MPrecon v2d = dg::create::inv_weights( grid);
    int rank;
    MPI_Comm_rank( MPI_COMM_WORLD, &rank);
    if( rank == 0) std::cout<<"Expand initial condition\n";
    dg::MVec x = dg::evaluate( initial, grid);

    if( rank == 0) std::cout << "Create symmetric Laplacian\n";
    dg::Elliptic<dg::MMatrix, dg::MVec, dg::MPrecon> A ( grid, dg::not_normed); 

    dg::CG< dg::MVec > pcg( x, n*n*Nx*Ny);
    if( rank == 0) std::cout<<"Expand right hand side\n";
    const dg::MVec solution = dg::evaluate ( fct, grid);
    const dg::MVec deriv = dg::evaluate( derivative, grid);
    dg::MVec b = dg::evaluate ( laplace_fct, grid);
    //compute W b
    dg::blas2::symv( w2d, b, b);
    //////////////////////////////////////////////////////////////////////
    
    int number = pcg( A, x, b, v2d, eps);
    if( rank == 0)
    {
        std::cout << "# of pcg itersations   "<<number<<std::endl;
        std::cout << "... for a precision of "<< eps<<std::endl;
    }

    dg::MVec  error(  solution);
    dg::blas1::axpby( 1., x,-1., error);

    double normerr = dg::blas2::dot( w2d, error);
    double norm = dg::blas2::dot( w2d, solution);
    if( rank == 0) std::cout << "L2 Norm of relative error is:               " <<sqrt( normerr/norm)<<std::endl;
    dg::MMatrix DX = dg::create::dx( grid);
    dg::blas2::gemv( DX, x, error);
    dg::blas1::axpby( 1., deriv, -1., error);
    normerr = dg::blas2::dot( w2d, error); 
    norm = dg::blas2::dot( w2d, deriv);
    if( rank == 0) std::cout << "L2 Norm of relative error in derivative is: " <<sqrt( normerr/norm)<<std::endl;

    MPI_Finalize();
    return 0;
}
