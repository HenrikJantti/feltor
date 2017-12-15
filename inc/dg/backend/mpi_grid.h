#pragma once

#include <cmath>
#include "../enums.h"
#include "grid.h"
#include "mpi_vector.h"

/*! @file 
  @brief MPI Grid objects
  */

namespace dg
{

/*! @class hide_comm_parameters2d
 * @param comm a two-dimensional Cartesian communicator
 * @note the paramateres given in the constructor are global parameters 
 */
/*! @class hide_comm_parameters3d
 * @param comm a three-dimensional Cartesian communicator
 * @note the paramateres given in the constructor are global parameters 
 */




/**
 * @brief 2D MPI abstract grid class 
 *
 * Represents the global grid coordinates and the process topology. 
 * It just divides the given (global) box into nonoverlapping (local) subboxes that are attributed to each process
 * @note a single cell is never divided across processes.
 * @note although it is abstract objects, are not meant to be hold on the heap via a base class pointer ( we protected the destructor)
 * @attention
 * The access functions \c n() \c Nx() ,... all return the global parameters. If you want to have the local ones call the \c local() function.
 * @ingroup basictopology
 */
struct aMPITopology2d
{
    typedef MPITag memory_category;
    typedef TwoDimensionalTag dimensionality;

    /**
     * @brief Return global x0
     *
     * @return global left boundary
     */
    double x0() const { return g.x0();}
    /**
     * @brief Return global x1
     *
     * @return global right boundary
     */
    double x1() const { return g.x1(); }
    /**
     * @brief Return global y0
     *
     * @return global left boundary
     */
    double y0() const { return g.y0();}
    /**
     * @brief Return global y1
     *
     * @return global right boundary
     */
    double y1() const { return g.y1();}
    /**
     * @brief Return global lx
     *
     * @return global length
     */
    double lx() const {return g.lx();}
    /**
     * @brief Return global ly
     *
     * @return global length
     */
    double ly() const {return g.ly();}
    /**
     * @brief Return global hx
     *
     * @return global grid constant
     */
    double hx() const {return g.hx();}
    /**
     * @brief Return global hy
     *
     * @return global grid constant
     */
    double hy() const {return g.hy();}
    /**
     * @brief Return n
     *
     * @return number of polynomial coefficients
     */
    unsigned n() const {return g.n();}
    /**
     * @brief Return the global number of cells 
     *
     * @return number of cells
     */
    unsigned Nx() const { return g.Nx();}
    /**
     * @brief Return the global number of cells 
     *
     * Not the one given in the constructor
     * @return number of cells
     */
    unsigned Ny() const { return g.Ny();}
    /**
     * @brief global x boundary
     *
     * @return boundary condition
     */
    bc bcx() const {return g.bcx();}
    /**
     * @brief global y boundary
     *
     * @return boundary condition
     */
    bc bcy() const {return g.bcy();}
    /**
     * @brief Return mpi cartesian communicator that is used in this grid
     *
     * @return Communicator
     */
    MPI_Comm communicator() const{return comm;}
    MPI_Comm get_poloidal_comm() const{
        int remain[] = {false, true};
        MPI_Comm comm1d;
        MPI_Cart_sub( comm, remain, &comm1d);
        return comm1d;
    }
    /**
     * @brief The Discrete Legendre Transformation 
     *
     * @return DLT corresponding to n given in the constructor
     */
    const DLT<double>& dlt() const{return g.dlt();}
    /**
     * @brief The total global number of points
     * @return equivalent of \c n()*n()*Nx()*Ny()
     */
    unsigned size() const { return g.size();}
    /**
     * @brief The total local number of points
     * @return equivalent of \c local.size()
     */
    unsigned local_size() const { return l.size();}
    /**
     * @brief Display global and local grid
     *
     * @param os output stream
     */
    void display( std::ostream& os = std::cout) const
    {
        os << "GLOBAL GRID \n";
        g.display();
        os << "LOCAL GRID \n";
        l.display();
    }

    /**
     * @brief Returns the pid of the process that holds the local grid surrounding the given point
     *
     * @param x X-coord
     * @param y Y-coord
     *
     * @return pid of a process, or -1 if non of the grids matches
     */
    int pidOf( double x, double y) const;
    /**
    * @brief Multiply the number of cells with a given factor
    *
    * With this function you can resize the grid ignorantly of its current size
    * @param fx new global number of cells is fx*global().Nx()
    * @param fy new global number of cells is fy*global().Ny()
    */
    void multiplyCellNumbers( double fx, double fy){
        set(g.n(), floor(fx*(double)g.Nx()+0.5), floor(fy*(double)g.Ny()+0.5));
    }
    /**
    * @copydoc Grid2d::set(unsigned,unsigned,unsigned)
    */
    void set( unsigned new_n, unsigned new_Nx, unsigned new_Ny) {
        check_division( new_Nx, new_Ny, g.bcx(), g.bcy());
        if( new_n == g.n() && new_Nx == g.Nx() && new_Ny == g.Ny()) return;
        do_set( new_n,new_Nx,new_Ny);
    }
    /**
    * @brief Map a local index plus the PID to a global vector index
    *
    * @param localIdx a local vector index
    * @param PID a PID in the communicator
    * @param globalIdx the corresponding global vector Index (contains result on output)
    * @return true if successful, false if localIdx or PID is not part of the grid
    */
    bool local2globalIdx( int localIdx, int PID, int& globalIdx)const
    {
        if( localIdx < 0 || localIdx >= (int)size()) return -1;
        int coords[2];
        if( MPI_Cart_coords( comm, PID, 2, coords) != MPI_SUCCESS)
            return false;
        int lIdx0 = localIdx %(l.n()*l.Nx());
        int lIdx1 = localIdx /(l.n()*l.Nx());
        int gIdx0 = coords[0]*l.n()*l.Nx()+lIdx0;
        int gIdx1 = coords[1]*l.n()*l.Ny()+lIdx1;
        globalIdx = gIdx1*g.n()*g.Nx() + gIdx0;
        return true;
    }
    /**
    * @brief Map a global vector index to a local vector Index and the corresponding PID
    *
    * @param globalIdx a global vector Index
    * @param localIdx contains local vector index on output
    * @param PID contains corresponding PID in the communicator on output
    * @return true if successful, false if globalIdx is not part of the grid
    */
    bool global2localIdx( int globalIdx, int& localIdx, int& PID)const
    {
        if( globalIdx < 0 || globalIdx >= (int)g.size()) return -1;
        int coords[2];
        int gIdx0 = globalIdx%(g.n()*g.Nx());
        int gIdx1 = globalIdx/(g.n()*g.Nx());
        coords[0] = gIdx0/(l.n()*l.Nx());
        coords[1] = gIdx1/(l.n()*l.Ny());
        int lIdx0 = gIdx0%(l.n()*l.Nx());
        int lIdx1 = gIdx1%(l.n()*l.Ny());
        localIdx = lIdx1*l.n()*l.Nx() + lIdx0;
        if( MPI_Cart_rank( comm, coords, &PID) == MPI_SUCCESS ) 
            return true;
        else
        {
            std::cout<<"Failed "<<PID<<"\n";
            return false;
        }
    }
    /**
     * @brief Return a non-MPI grid local for the calling process
     *
     * The local grid contains the boundaries and cell numbers the calling process sees and is in charge of. 
     * @return Grid object
     * @note the boundary conditions in the local grid are not well defined since there might not actually be any boundaries
     */
    const Grid2d& local() const {return l;}
    /**
     * @brief Return the global non-MPI grid 
     *
     * The global grid contains the global boundaries and cell numbers. 
     * This is the grid that we would have to use in a non-MPI implementation.
     * The global grid returns the same values for x0(), x1(), ..., Nx(), Ny(), ... as the grid
     * class itself 
     * @return non-MPI Grid object
     */
    const Grid2d& global() const {return g;}
    protected:
    ///disallow deletion through base class pointer
    ~aMPITopology2d(){}

    /**
     * @copydoc hide_grid_parameters2d
     * @copydoc hide_bc_parameters2d
     * @copydoc hide_comm_parameters2d
     */
    aMPITopology2d( double x0, double x1, double y0, double y1, unsigned n, unsigned Nx, unsigned Ny, bc bcx, bc bcy, MPI_Comm comm):
        g( x0, x1, y0, y1, n, Nx, Ny, bcx, bcy), l(g), comm( comm)
    {
        update_local();
        check_division( Nx, Ny, bcx, bcy);
    }
    ///copydoc aTopology2d::aTopology2d(const aTopology2d&)
    aMPITopology2d(const aMPITopology2d& src):g(src.g),l(src.l),comm(src.comm){ }
    ///copydoc aTopology2d::operator()(const aTopology2d&)
    aMPITopology2d& operator=(const aMPITopology2d& src){
        g = src.g; l = src.l; comm = src.comm;
        return *this;
    }
    ///This function has an implementation 
    virtual void do_set( unsigned new_n, unsigned new_Nx, unsigned new_Ny)=0;
    private:
    void check_division( unsigned Nx, unsigned Ny, bc bcx, bc bcy)
    {
        int rank, dims[2], periods[2], coords[2];
        MPI_Cart_get( comm, 2, dims, periods, coords);
        MPI_Comm_rank( comm, &rank);
        if( rank == 0)
        {
            if(Nx%dims[0]!=0)
                std::cerr << "Nx "<<Nx<<" npx "<<dims[0]<<std::endl;
            assert( Nx%dims[0] == 0);
            if(Ny%dims[1]!=0)
                std::cerr << "Ny "<<Ny<<" npy "<<dims[1]<<std::endl;
            assert( Ny%dims[1] == 0);
            if( bcx == dg::PER) assert( periods[0] == true);
            else assert( periods[0] == false);
            if( bcy == dg::PER) assert( periods[1] == true);
            else assert( periods[1] == false);
        }
    }
    void update_local(){
        int dims[2], periods[2], coords[2];
        MPI_Cart_get( comm, 2, dims, periods, coords);
        double x0 = g.x0() + g.lx()/(double)dims[0]*(double)coords[0]; 
        double x1 = g.x0() + g.lx()/(double)dims[0]*(double)(coords[0]+1); 
        if( coords[0] == dims[0]-1) 
            x1 = g.x1();
        double y0 = g.y0() + g.ly()/(double)dims[1]*(double)coords[1]; 
        double y1 = g.y0() + g.ly()/(double)dims[1]*(double)(coords[1]+1); 
        if( coords[1] == dims[1]-1) 
            y1 = g.y1();
        unsigned Nx = g.Nx()/dims[0];
        unsigned Ny = g.Ny()/dims[1];
        l = Grid2d(x0, x1, y0, y1, g.n(), Nx, Ny, g.bcx(), g.bcy());
    }
    Grid2d g, l; //global and local grid
    MPI_Comm comm; //just an integer...
};


/**
 * @brief 3D MPI Grid class 
 *
 * @copydetails aMPITopology2d
 * @ingroup basictopology
 */
struct aMPITopology3d
{
    typedef MPITag memory_category;
    typedef ThreeDimensionalTag dimensionality;

    /**
     * @brief Return global x0
     *
     * @return global left boundary
     */
    double x0() const { return g.x0();}
    /**
     * @brief Return global x1
     *
     * @return global right boundary
     */
    double x1() const { return g.x1();}
    /**
     * @brief Return global y0
     *
     * @return global left boundary
     */
    double y0() const { return g.y0();}
    /**
     * @brief Return global y1
     *
     * @return global right boundary
     */
    double y1() const { return g.y1();}
    /**
     * @brief Return global z0
     *
     * @return global left boundary
     */
    double z0() const { return g.z0();}
    /**
     * @brief Return global z1
     *
     * @return global right boundary
     */
    double z1() const { return g.z1();}
    /**
     * @brief Return global lx
     *
     * @return global length
     */
    double lx() const {return g.lx();}
    /**
     * @brief Return global ly
     *
     * @return global length
     */
    double ly() const {return g.ly();}
    /**
     * @brief Return global lz
     *
     * @return global length
     */
    double lz() const {return g.lz();}
    /**
     * @brief Return global hx
     *
     * @return global grid constant
     */
    double hx() const {return g.hx();}
    /**
     * @brief Return global hy
     *
     * @return global grid constant
     */
    double hy() const {return g.hy();}
    /**
     * @brief Return global hz
     *
     * @return global grid constant
     */
    double hz() const {return g.hz();}
    /**
     * @brief Return n
     *
     * @return number of polynomial coefficients
     */
    unsigned n() const {return g.n();}
    /**
     * @brief Return the global number of cells 
     *
     * Not the one given in the constructor
     * @return number of cells
     */
    unsigned Nx() const { return g.Nx();}
    /**
     * @brief Return the global number of cells 
     *
     * Not the one given in the constructor
     * @return number of cells
     */
    unsigned Ny() const { return g.Ny();}
    /**
     * @brief Return the global number of cells 
     *
     * Not the one given in the constructor
     * @return number of cells
     */
    unsigned Nz() const { return g.Nz();}
    /**
     * @brief global x boundary
     *
     * @return boundary condition
     */
    bc bcx() const {return g.bcx();}
    /**
     * @brief global y boundary
     *
     * @return boundary condition
     */
    bc bcy() const {return g.bcy();}
    /**
     * @brief global z boundary
     *
     * @return boundary condition
     */
    bc bcz() const {return g.bcz();}
    /**
     * @brief Return mpi cartesian communicator that is used in this grid
     * @return Communicator
     */
    MPI_Comm communicator() const{return comm;}
    /**
     * @brief MPI Cartesian communicator in the first two dimensions (x and y)
     * @return 2d Cartesian Communicator
     */
    MPI_Comm get_perp_comm() const {return planeComm;}
    /**
     * @brief The Discrete Legendre Transformation 
     *
     * @return DLT corresponding to n given in the constructor
     */
    const DLT<double>& dlt() const{return g.dlt();}
    /**
     * @brief The total global number of points
     * @return equivalent of \c n()*n()*Nx()*Ny()*Nz()
     */
    unsigned size() const { return g.size();}
    /**
     * @brief The total local number of points
     * @return equivalent of \c local.size()
     */
    unsigned local_size() const { return l.size();}
    /**
     * @brief Display global and local grid paramters 
     *
     * @param os output stream
     */
    void display( std::ostream& os = std::cout) const
    {
        os << "GLOBAL GRID \n";
        g.display();
        os << "LOCAL GRID \n";
        l.display();
    }
    /**
     * @brief Returns the pid of the process that holds the local grid surrounding the given point
     *
     * @param x X-coord
     * @param y Y-coord
     * @param z Z-coord
     *
     * @return pid of a process, or -1 if non of the grids matches
     */
    int pidOf( double x, double y, double z) const;
    ///@copydoc aMPITopology2d::multiplyCellNumbers()
    void multiplyCellNumbers( double fx, double fy){
        set(g.n(), round(fx*(double)g.Nx()), round(fy*(double)g.Ny()), g.Nz());
    }
    /**
     * @copydoc Grid3d::set(unsigned,unsigned,unsigned,unsigned)
     */
    void set( unsigned new_n, unsigned new_Nx, unsigned new_Ny, unsigned new_Nz) {
        check_division( new_Nx,new_Ny,new_Nz,g.bcx(),g.bcy(),g.bcz());
        if( new_n == g.n() && new_Nx == g.Nx() && new_Ny == g.Ny() && new_Nz == g.Nz()) return;
        do_set(new_n,new_Nx,new_Ny,new_Nz);
    }
    ///@copydoc aMPITopology2d::local2globalIdx(int,int,int&)const
    bool local2globalIdx( int localIdx, int PID, int& globalIdx)const
    {
        if( localIdx < 0 || localIdx >= (int)size()) return false;
        int coords[3];
        if( MPI_Cart_coords( comm, PID, 3, coords) != MPI_SUCCESS)
            return false;
        int lIdx0 = localIdx %(l.n()*l.Nx());
        int lIdx1 = (localIdx /(l.n()*l.Nx())) % (l.n()*l.Ny());
        int lIdx2 = localIdx / (l.n()*l.n()*l.Nx()*l.Ny());
        int gIdx0 = coords[0]*l.n()*l.Nx()+lIdx0;
        int gIdx1 = coords[1]*l.n()*l.Ny()+lIdx1;
        int gIdx2 = coords[2]*l.Nz()  + lIdx2;
        globalIdx = (gIdx2*g.n()*g.Ny() + gIdx1)*g.n()*g.Nx() + gIdx0;
        return true;
    }
    ///@copydoc aMPITopology2d::global2localIdx(int,int&,int&)const
    bool global2localIdx( int globalIdx, int& localIdx, int& PID)const
    {
        if( globalIdx < 0 || globalIdx >= (int)g.size()) return false;
        int coords[3];
        int gIdx0 = globalIdx%(g.n()*g.Nx());
        int gIdx1 = (globalIdx/(g.n()*g.Nx())) % (g.n()*g.Ny());
        int gIdx2 = globalIdx/(g.n()*g.n()*g.Nx()*g.Ny());
        coords[0] = gIdx0/(l.n()*l.Nx());
        coords[1] = gIdx1/(l.n()*l.Ny());
        coords[2] = gIdx2/l.Nz();
        int lIdx0 = gIdx0%(l.n()*l.Nx());
        int lIdx1 = gIdx1%(l.n()*l.Ny());
        int lIdx2 = gIdx2%l.Nz();
        localIdx = (lIdx2*l.n()*l.Ny() + lIdx1)*l.n()*l.Nx() + lIdx0;
        if( MPI_Cart_rank( comm, coords, &PID) == MPI_SUCCESS ) 
            return true;
        else
            return false;
    }
    ///@copydoc aMPITopology2d::local()const
    const Grid3d& local() const {return l;}
     ///@copydoc aMPITopology2d::global()const
    const Grid3d& global() const {return g;}
    protected:
    ///disallow deletion through base class pointer
    ~aMPITopology3d(){}

    ///@copydoc hide_grid_parameters3d
    ///@copydoc hide_bc_parameters3d
    ///@copydoc hide_comm_parameters3d
    aMPITopology3d( double x0, double x1, double y0, double y1, double z0, double z1, unsigned n, unsigned Nx, unsigned Ny, unsigned Nz, bc bcx, bc bcy, bc bcz, MPI_Comm comm):
        g( x0, x1, y0, y1, z0, z1, n, Nx, Ny, Nz, bcx, bcy, bcz), l(g), comm( comm)
    {
        update_local();
        check_division( Nx, Ny, Nz, bcx, bcy, bcz);
        int remain_dims[] = {true,true,false}; //true true false
        MPI_Cart_sub( comm, remain_dims, &planeComm);
    }
    ///explicit copy constructor (default)
    ///@param src source
    aMPITopology3d(const aMPITopology3d& src):g(src.g),l(src.l),comm(src.comm),planeComm(src.planeComm){ }
    ///explicit assignment operator (default)
    ///@param src source
    aMPITopology3d& operator=(const aMPITopology3d& src){
        g = src.g; l = src.l; comm = src.comm; planeComm = src.planeComm;
        return *this;
    }
    virtual void do_set( unsigned new_n, unsigned new_Nx, unsigned new_Ny, unsigned new_Nz)=0; 
    private:
    void check_division( unsigned Nx, unsigned Ny, unsigned Nz, bc bcx, bc bcy, bc bcz)
    {
        int rank, dims[3], periods[3], coords[3];
        MPI_Cart_get( comm, 3, dims, periods, coords);
        MPI_Comm_rank( comm, &rank);
        if( rank == 0)
        {
            if(!(Nx%dims[0]==0))
                std::cerr << "Nx "<<Nx<<" npx "<<dims[0]<<std::endl;
            assert( Nx%dims[0] == 0);
            if( !(Ny%dims[1]==0))
                std::cerr << "Ny "<<Ny<<" npy "<<dims[1]<<std::endl;
            assert( Ny%dims[1] == 0);
            if( !(Nz%dims[2]==0))
                std::cerr << "Nz "<<Nz<<" npz "<<dims[2]<<std::endl;
            assert( Nz%dims[2] == 0);
            if( bcx == dg::PER) assert( periods[0] == true);
            else assert( periods[0] == false);
            if( bcy == dg::PER) assert( periods[1] == true);
            else assert( periods[1] == false);
            if( bcz == dg::PER) assert( periods[2] == true);
            else assert( periods[2] == false);
        }
    }
    void update_local(){
        int dims[3], periods[3], coords[3];
        MPI_Cart_get( comm, 3, dims, periods, coords);
        double x0 = g.x0() + g.lx()/(double)dims[0]*(double)coords[0]; 
        double x1 = g.x0() + g.lx()/(double)dims[0]*(double)(coords[0]+1); 
        if( coords[0] == dims[0]-1) 
            x1 = g.x1();

        double y0 = g.y0() + g.ly()/(double)dims[1]*(double)coords[1]; 
        double y1 = g.y0() + g.ly()/(double)dims[1]*(double)(coords[1]+1); 
        if( coords[1] == dims[1]-1) 
            y1 = g.y1();

        double z0 = g.z0() + g.lz()/(double)dims[2]*(double)coords[2]; 
        double z1 = g.z0() + g.lz()/(double)dims[2]*(double)(coords[2]+1); 
        if( coords[2] == dims[2]-1) 
            z1 = g.z1();
        unsigned Nx = g.Nx()/dims[0];
        unsigned Ny = g.Ny()/dims[1];
        unsigned Nz = g.Nz()/dims[2];
        
        l = Grid3d(x0, x1, y0, y1, z0, z1, g.n(), Nx, Ny, Nz, g.bcx(), g.bcy(), g.bcz());
    }
    Grid3d g, l; //global grid
    MPI_Comm comm, planeComm; //just an integer...
};
///@cond
int aMPITopology2d::pidOf( double x, double y) const
{
    int dims[2], periods[2], coords[2];
    MPI_Cart_get( comm, 2, dims, periods, coords);
    coords[0] = (unsigned)floor( (x-g.x0())/g.lx()*(double)dims[0] );
    coords[1] = (unsigned)floor( (y-g.y0())/g.ly()*(double)dims[1] );
    //if point lies on or over boundary of last cell shift into current cell (not so good for periodic boundaries)
    coords[0]=(coords[0]==dims[0]) ? coords[0]-1 :coords[0];
    coords[1]=(coords[1]==dims[1]) ? coords[1]-1 :coords[1];
    int rank;
    if( MPI_Cart_rank( comm, coords, &rank) == MPI_SUCCESS ) 
        return rank;
    else
        return -1;
}
int aMPITopology3d::pidOf( double x, double y, double z) const
{
    int dims[3], periods[3], coords[3];
    MPI_Cart_get( comm, 3, dims, periods, coords);
    coords[0] = (unsigned)floor( (x-g.x0())/g.lx()*(double)dims[0] );
    coords[1] = (unsigned)floor( (y-g.y0())/g.ly()*(double)dims[1] );
    coords[2] = (unsigned)floor( (z-g.z0())/g.lz()*(double)dims[2] );
    //if point lies on or over boundary of last cell shift into current cell (not so good for periodic boundaries)
    coords[0]=(coords[0]==dims[0]) ? coords[0]-1 :coords[0];
    coords[1]=(coords[1]==dims[1]) ? coords[1]-1 :coords[1];
    coords[2]=(coords[2]==dims[2]) ? coords[2]-1 :coords[2];
    int rank;
    if( MPI_Cart_rank( comm, coords, &rank) == MPI_SUCCESS ) 
        return rank;
    else
        return -1;
}
void aMPITopology2d::do_set( unsigned new_n, unsigned new_Nx, unsigned new_Ny) {
    g.set(new_n,new_Nx,new_Ny);
    update_local();
}
void aMPITopology3d::do_set( unsigned new_n, unsigned new_Nx, unsigned new_Ny, unsigned new_Nz) {
    g.set(new_n,new_Nx,new_Ny,new_Nz);
    update_local();
}

///@endcond

/**
 * @brief The simplest implementation of aMPITopology2d
 * @ingroup grid
 * @copydoc hide_code_mpi_evaluate2d
 */
struct MPIGrid2d: public aMPITopology2d
{
    /**
     * @copydoc hide_grid_parameters2d
     * @copydoc hide_comm_parameters2d
     */
    MPIGrid2d( double x0, double x1, double y0, double y1, unsigned n, unsigned Nx, unsigned Ny, MPI_Comm comm):
        aMPITopology2d( x0,x1,y0,y1,n,Nx,Ny,dg::PER,dg::PER,comm)
    { }

    /**
     * @copydoc hide_grid_parameters2d
     * @copydoc hide_bc_parameters2d
     * @copydoc hide_comm_parameters2d
     */
    MPIGrid2d( double x0, double x1, double y0, double y1, unsigned n, unsigned Nx, unsigned Ny, bc bcx, bc bcy, MPI_Comm comm):
        aMPITopology2d( x0,x1,y0,y1,n,Nx,Ny,bcx,bcy,comm)
    { }
    ///allow explicit type conversion from any other topology
    explicit MPIGrid2d( const aMPITopology2d& src): aMPITopology2d(src){}
    private:
    virtual void do_set( unsigned new_n, unsigned new_Nx, unsigned new_Ny){
        aMPITopology2d::do_set(new_n,new_Nx,new_Ny);
    }
};

/**
 * @brief The simplest implementation of aMPITopology3d
 * @ingroup grid
 * @copydoc hide_code_mpi_evaluate3d
 */
struct MPIGrid3d : public aMPITopology3d
{
    ///@copydoc hide_grid_parameters3d
    ///@copydoc hide_comm_parameters3d
    MPIGrid3d( double x0, double x1, double y0, double y1, double z0, double z1, unsigned n, unsigned Nx, unsigned Ny, unsigned Nz, MPI_Comm comm):
        aMPITopology3d( x0, x1, y0, y1, z0, z1, n, Nx, Ny, Nz, dg::PER, dg::PER, dg::PER,comm )
    { }

    ///@copydoc hide_grid_parameters3d
    ///@copydoc hide_bc_parameters3d
    ///@copydoc hide_comm_parameters3d
    MPIGrid3d( double x0, double x1, double y0, double y1, double z0, double z1, unsigned n, unsigned Nx, unsigned Ny, unsigned Nz, bc bcx, bc bcy, bc bcz, MPI_Comm comm):
        aMPITopology3d( x0, x1, y0, y1, z0, z1, n, Nx, Ny, Nz, bcx, bcy, bcz, comm)
    { }
    ///allow explicit type conversion from any other topology
    ///@param src source
    explicit MPIGrid3d( const aMPITopology3d& src): aMPITopology3d(src){ }
    private:
    virtual void do_set( unsigned new_n, unsigned new_Nx, unsigned new_Ny, unsigned new_Nz){
        aMPITopology3d::do_set(new_n,new_Nx,new_Ny,new_Nz);
    }
};

///@cond
template<>
struct MemoryTraits< MPITag, TwoDimensionalTag> {
    using host_vector = MPI_Vector<thrust::host_vector<double>>;
    using host_grid   = MPIGrid2d;
};
template<>
struct MemoryTraits< MPITag, ThreeDimensionalTag> {
    using host_vector = MPI_Vector<thrust::host_vector<double>>;
    using host_grid   = MPIGrid3d;
};
///@endcond

}//namespace dg
