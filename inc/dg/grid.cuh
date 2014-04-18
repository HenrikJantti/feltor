#ifndef _DG_GRID_CUH_
#define _DG_GRID_CUH_

#include <cassert>
#include "dlt.cuh"

/*! @file Grid objects
  */


namespace dg{


    //future plans: grid object should be the one which computes node coordinates (as vec2d?)

/**
 * @brief Switch between boundary conditions
 * 
 * @ingroup creation
 */
enum bc{ 
    PER = 0, //!< periodic boundaries
    DIR = 1, //!< homogeneous dirichlet boundaries
    DIR_NEU = 2, //!< Dirichlet on left, Neumann on right boundary
    NEU_DIR = 3, //!< Neumann on left, Dirichlet on right boundary
    NEU = 4 //!< Neumann on both boundaries
};

///@addtogroup grid
///@{
/**
* @brief 1D grid
*
* @tparam T value type
*/
template <class T>
struct Grid1d
{
    /**
     * @brief 1D grid
     * 
     @param x0 left boundary
     @param x1 right boundary
     @param n # of polynomial coefficients
     @param N # of cells
     @param bcx boundary conditions
     */
    Grid1d( T x0, T x1, unsigned n, unsigned N, bc bcx = PER):
        x0_(x0), x1_(x1),
        n_(n), Nx_(N), bcx_(bcx), dlt_(n)
    {
        assert( x1 > x0 );
        assert( N > 0  );
        assert( n != 0 );
        lx_ = (x1-x0);
        hx_ = lx_/(double)Nx_;
    }
    T x0() const {return x0_;}
    T x1() const {return x1_;}
    T lx() const {return lx_;}
    T h() const {return hx_;}
    unsigned N() const {return Nx_;}
    unsigned n() const {return n_;}
    bc bcx() const {return bcx_;}
    /**
     * @brief The total number of points
     *
     * @return n*Nx
     */
    unsigned size() const { return n_*Nx_;}
    const DLT<T>& dlt() const {return dlt_;}
  private:
    T x0_, x1_;
    T lx_;
    unsigned n_, Nx_;
    T hx_;
    bc bcx_;
    DLT<T> dlt_;
};

/**
 * @brief A 2D grid class 
 *
 * @tparam T scalar value type 
 */
template< class T>
struct Grid
{
    /**
     * @brief Construct a 2D grid
     *
     * @param x0 left boundary in x
     * @param x1 right boundary in x 
     * @param y0 lower boundary in y
     * @param y1 upper boundary in y 
     * @param n  # of polynomial coefficients per dimension
     * @param Nx # of points in x 
     * @param Ny # of points in y
     * @param bcx boundary condition in x
     * @param bcy boundary condition in y
     */
    Grid( T x0, T x1, T y0, T y1, unsigned n, unsigned Nx, unsigned Ny, bc bcx = PER, bc bcy = PER):
        x0_(x0), x1_(x1), y0_(y0), y1_(y1), 
        n_(n), Nx_(Nx), Ny_(Ny), bcx_(bcx), bcy_( bcy), dlt_(n)
    {
        assert( n != 0);
        assert( x1 > x0 && y1 > y0);
        assert( Nx > 0  && Ny > 0);
        lx_ = (x1-x0), ly_ = (y1-y0);
        hx_ = lx_/(double)Nx_, hy_ = ly_/(double)Ny_;
    }
    T x0() const {return x0_;}
    T x1() const {return x1_;}
    T y0() const {return y0_;}
    T y1() const {return y1_;}
    T lx() const {return lx_;}
    T ly() const {return ly_;}
    T hx() const {return hx_;}
    T hy() const {return hy_;}
    unsigned n() const {return n_;}
    unsigned Nx() const {return Nx_;}
    unsigned Ny() const {return Ny_;}
    bc bcx() const {return bcx_;}
    bc bcy() const {return bcy_;}
    const DLT<T>& dlt() const{return dlt_;}
    /**
     * @brief The total number of points
     *
     * @return n*n*Nx*Ny
     */
    unsigned size() const { return n_*n_*Nx_*Ny_;}
    void display( std::ostream& os = std::cout) const
    {
        os << "Grid parameters are: \n"
            <<"    n  = "<<n_<<"\n"
            <<"    Nx = "<<Nx_<<"\n"
            <<"    Ny = "<<Ny_<<"\n"
            <<"    hx = "<<hx_<<"\n"
            <<"    hy = "<<hy_<<"\n"
            <<"    lx = "<<lx_<<"\n"
            <<"    ly = "<<ly_<<"\n"
            <<"Boundary conditions in x are: \n";
        switch(bcx_)
        {
            case(dg::PER): os << "    PERIODIC \n"; break;
            case(dg::DIR): os << "    DIRICHLET\n"; break;
            default: os << "    Not specified!!\n"; 
        }
        os <<"Boundary conditions in y are: \n";
        switch(bcy_)
        {
            case(dg::PER): os << "    PERIODIC \n"; break;
            case(dg::DIR): os << "    DIRICHLET\n"; break;
            default: os << "    Not specified!!\n"; 
        }
    }
  private:
    T x0_, x1_, y0_, y1_;
    T lx_, ly_;
    unsigned n_, Nx_, Ny_;
    T hx_, hy_;
    bc bcx_, bcy_;
    DLT<T> dlt_;
};
/**
 * @brief A 3D grid class 
 *
 * @tparam T scalar value type 
 */
template< class T>
struct Grid3d
{
    /**
     * @brief Construct a 3D grid
     *
     * 
     * @param x0 left boundary in x
     * @param x1 right boundary in x 
     * @param y0 lower boundary in y
     * @param y1 upper boundary in y 
     * @param z0 lower boundary in z
     * @param z1 upper boundary in z 
     * @param n  # of polynomial coefficients per (x,y-) dimension
     * @param Nx # of points in x 
     * @param Ny # of points in y
     * @param Nz # of points in z
     * @param bcx boundary condition in x
     * @param bcy boundary condition in y
     * @attention # of coefficients in z direction is always 1
     */
    Grid3d( T x0, T x1, T y0, T y1, T z0, T z1, unsigned n, unsigned Nx, unsigned Ny, unsigned Nz, bc bcx = PER, bc bcy = PER, bc bcz = PER):
        x0_(x0), x1_(x1), y0_(y0), y1_(y1), z0_(z0), z1_(z1),
        n_(n), Nx_(Nx), Ny_(Ny), Nz_(Nz), bcx_(bcx), bcy_( bcy), bcz_( bcz), dlt_(n)
    {
        assert( n != 0);
        assert( x1 > x0 && y1 > y0 ); assert( z1 > z0 );         
        assert( Nx > 0  && Ny > 0); assert( Nz > 0);

        lx_ = (x1-x0), ly_ = (y1-y0), lz_ = (z1-z0);
        hx_ = lx_/(double)Nx_, hy_ = ly_/(double)Ny_, hz_ = lz_/(double)Nz_;
    }
    T x0() const {return x0_;}
    T x1() const {return x1_;}

    T y0() const {return y0_;}
    T y1() const {return y1_;}

    T z0() const {return z0_;}
    T z1() const {return z1_;}

    T lx() const {return lx_;}
    T ly() const {return ly_;}
    T lz() const {return lz_;}
    
    T hx() const {return hx_;}
    T hy() const {return hy_;}
    T hz() const {return hz_;}
    unsigned n() const {return n_;}
    unsigned Nx() const {return Nx_;}
    unsigned Ny() const {return Ny_;}
    unsigned Nz() const {return Nz_;}
    bc bcx() const {return bcx_;}
    bc bcy() const {return bcy_;}
    bc bcz() const {return bcz_;}
    const DLT<T>& dlt() const{return dlt_;}
    /**
     * @brief The total number of points
     *
     * @return n*n*Nx*Ny
     */
    unsigned size() const { return n_*n_*Nx_*Ny_*Nz_;}
    void display( std::ostream& os = std::cout) const
    {
        os << "Grid parameters are: \n"
            <<"    n  = "<<n_<<"\n"
            <<"    Nx = "<<Nx_<<"\n"
            <<"    Ny = "<<Ny_<<"\n"
            <<"    Nz = "<<Nz_<<"\n"
            <<"    hx = "<<hx_<<"\n"
            <<"    hy = "<<hy_<<"\n"
            <<"    hz = "<<hz_<<"\n"
            <<"    lx = "<<lx_<<"\n"
            <<"    ly = "<<ly_<<"\n"
            <<"    lz = "<<lz_<<"\n"
            <<"Boundary conditions in x are: \n";
        switch(bcx_)
        {
            case(dg::PER): os << "    PERIODIC \n"; break;
            case(dg::DIR): os << "    DIRICHLET\n"; break;
            default: os << "    Not specified!!\n"; 
        }
        os <<"Boundary conditions in y are: \n";
        switch(bcy_)
        {
            case(dg::PER): os << "    PERIODIC \n"; break;
            case(dg::DIR): os << "    DIRICHLET\n"; break;
            default: os << "    Not specified!!\n"; 
        }
        os <<"Boundary conditions in z are: \n";
        switch(bcz_)
        {
            case(dg::PER): os << "    PERIODIC \n"; break;
            case(dg::DIR): os << "    DIRICHLET\n"; break;
            default: os << "    Not specified!!\n"; 
        }
    }
  private:
    T x0_, x1_, y0_, y1_, z0_, z1_;
    T lx_, ly_, lz_;
    unsigned n_, Nx_, Ny_, Nz_;
    T hx_, hy_, hz_;
    bc bcx_, bcy_, bcz_;
    DLT<T> dlt_;
};

///@}
}// namespace dg
#endif // _DG_GRID_CUH_
