/*
* Main function for tissue modeling
* Updated from work of Jonathan D. Stott <jonathan.stott@gmail.com> (Manchester)
  Haibo Ni <haibo.ni02@gmail.com>
 *
 */
#include <memory>
#include <algorithm>
#include <vector>
#include <zlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "util.h"
// #include "conduction.h"
#include "tissue.h"
// #include "cell.h"
#include "stimulus.h"
#include "input_output.h"
#include <string.h>
#include "simulation_config.h"
#include <iomanip>
#include <exception>
#include <iostream>
#include <string>

#include "conduction_2d.h"

// #include "input_output.h"


// #include "Tent_Vent.h"
// #include "CNZ.h"
// #include "Updated_CNZ.h"

// #include "NCZ_ODE.h"

// #include "rudy.h"
// #include "EnumSimulationCtrl.hpp"

// #include "TNNP_MarkovIKr_SP.hpp"
// #include "TNNP_MarkovIKr_function.hpp"
// #include "TNNP_ISO.hpp"
// #include "TNNP_MarkovIKr.h"
// #include "one_d_conduction.h"
#include "APInfo.hpp"
#include "topology.h"
// #include "Spatial_Cell.h"
#include "CVOde_Cell.hpp"

// if we're compiling with openMP, include the file, else define some stub
#include <omp.h>
#include <mpi.h>
// #define _OPENMP 1`
// #define OUT_TO_BIN_THREADED

#ifndef OUT_TO_BIN_THREADED
#define OUT_TO_BIN_SINGLE
#endif
// functions
#ifdef _OPENMP
#include <omp.h>
#else
// #define omp_get_num_threads() (48)
// #define omp_get_thread_num() (0)
#endif
// #define NX (235)
// #define NY (269)
// #define NZ (298)


// #define BREAK_IF_CVM // break if CV measured values reach steady-state values.
// #define OUT_INICOND
/* define the num of neighboorhood ( related to the model dimension */
// #define CV_COM (1)
#define NUM_NBH (9)  // 2D -> 9 3D -> 19
#define DIFF_COEF (0.15*1.525*0.63*1.45)  // wild type, CV = 0.752688


// #define DIFF_COEF (0)
#define FIRST_ACT (100)
#define SECOND_ACT (NX-20)

/* Downsampled IS ventricle dimension */

#define CELL_WARP 382

/* Downsampled IS ventricle dimension */
#define NX 125 //203//;(462)
#define NY 120 //203//;(325)
#define NZ 1 //3//;(358)


// #define PROFILE_TIME
/*#define NX (100)
#define NY (100)
#define NZ (100)*/


// #define D2 (0.21)
// #define DD (8*0.21)
/* to be determined later */
/* The diffusion parameter */
// #define D2 (0.18/10.0)
// #define DD (0.18)
// #define D1 (0.18)
// #define D2 (0.20*0.33/3.0)

#define D1 (0.18*1.1)
// #define D2 (0.20*0.33)  // 0.398 m/s
// #define D2 (0.20*0.33*2.25)   // 0.6380952380952382   m/s
// #define D2 (0.20*0.33*2.25 * 0.25)   // D reduce by 4-fold
#define D2 (0.20*0.33*2.25)   // control Diffusion coeff

#define DD (0) //(D1-D2)
// typedef TTCell TNNP_MarkovIKr;

#define STATE_LEN (40)//cnz_con_length())  /* the state length of Tent model is 21,crn_con_length() = 39*/


int main (int argc, char **argv) {

	// omp_set_num_threads(6);
	// MPI stuff
	int numprocs, rank, namelen;
	char processor_name[MPI_MAX_PROCESSOR_NAME];
	int iam = 0, np = 1;
	int provided = 1;

	MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
	// MPI_Init(&argc, &argv);


	MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Get_processor_name(processor_name, &namelen);

	// omp_set_dynamic(1);
	// counters
	int i, c;

	// tissue variables
	unsigned char           ** atrium;
	unsigned char            *tissue;

	// propagation variables
	const double            dx = 0.25;
	const double dt      = 1 / 20.0; // large Dt here, addpative dt for single cell computations.
	int                      *nbd;
	int                      **neighbourhood;
	double                   *lap;
	double                   **laplacian;
	double                   *dv;
	double                   *v_new;
	double                   *v_old;
	double 					 *dv_diff_temp;
	double 					 *RK_K1;
	double 					 *RK_K2;
	double 					 *RK_K3;
	double 					 *v_temp;
	double 					 *v_temp_2;
	double *v_global;
	double *v_global_temp_1;
	double *v_global_temp_2;

	const int S1_number = 10;

	// time variables
	double  t_max        = 2000.0;
	// double  t;

	Simulation_Config Config_para(argc, argv);
	Config_para.dt = dt;

	Config_para.Geometry_file =  "Geometry/TWO_D_Plat_125X120.geo.gz";
	Config_para.Pacemap =  "Geometry/atria.stim.gz";
	Config_para.Fibre_theta =  "Geometry/TWO_D_Plat_125X120.theta.gz";
	Config_para.Fibre_phi =  "Geometry/TWO_D_Plat_125X120.phi.gz";
	Config_para.Stim_amp_file =  "Geometry/atria.S1.STIM.AMP.bin";
	Config_para.S2_StimLocFile =  "Geometry/atria.S2.STIM.AMP.bin";
	// if (rank == 0)
	Config_para.Report_Config();
	// #ifdef OUT_INICOND
	// 	const double t_start = 0.0;//(Config_para.S1_number-1)*Config_para.BCL - 10;
	// #else
	// 	const double t_start = - 10 + dt;//(Config_para.S1_number - 1) * Config_para.BCL - 10 + dt;
	// #endif
	// cell stuff
	double *state;
	// cell_params              *params;
	// lookup_table_t           *table;
	// heterogeneous_params_t   *het;
	double                  results[1];
	int conduction = 1;
	// std::vector<TNNP_MarkovIKr> cell_vec;

	// stim stuff
	const double stim   = 20.0;
	char         *san   = NULL;
	char         *sanin = NULL;
	float        *stims = NULL;
	gzFile       gz     = NULL;
	int          iStim  = 0;


	float     *v_out;

	float *Fcell;
	FILE *in;
	int *FB_number;
	int FB_type;

	float ISO, ACH;
	int AF;

	int BCL, S2;
	double stimint;
	int intBCL, intstim, stimcount, timeint, durationint;
	double stimduration;
	int stimflag;
	double Dscale    = 1.0;

	double Ggap;

	int IKur_switch  = 0;

	// #ifdef _OPENMP
	double start     = omp_get_wtime();
	double end       = 0;
	double end_setup = 0;
	// #endif
	float *Stim_amp, * Stim_time, *Stim_amp_S2;

	/*3D*/
	/*atrium = read_and_embed_geometry(Config_para.Geometry_file, NZ, NY, NX);
	neighbourhood = generate_neighbours_map(atrium, NZ, NY, NX, &c);
	laplacian = generate_laplacian(atrium, NZ, NY, NX, c,
	Config_para.Fibre_theta, Config_para.Fibre_phi, dx, DD, D2);
	tissue = generate_tissue_map(atrium, NZ, NY, NX, c);*/
	/*laplacian = generate_laplacian_test_heterogeneity(atrium, NZ, NY, NX, c,
	Fibre_theta, Fibre_phi, dx, DD, D2);*/

	/*laplacian = generate_laplacian_using_fibre_components(atrium, NZ, NY, NX, c,
	"Last_Atria_Geo_Full_Fibre_X.geo.gz", "Last_Atria_Geo_Full_Fibre_Y.geo.gz", "Last_Atria_Geo_Full_Fibre_Z.geo.gz",
	dx, DD, D2);*/
	/* for One Dimensional */

	/*laplacian = generate_one_D_laplacian_test(NX, tissue, DIFF_COEF, dx);
	neighbourhood = generate_one_D_neighboors(NX);*/



	atrium = read_and_embed_geometry_2d(Config_para.Geometry_file.c_str(), NY, NX);

	neighbourhood = generate_neighbours_map_2d(atrium,  NY, NX, &c);
	/*laplacian = generate_laplacian(atrium, NZ, NY, NX, c,
	                               Config_para.Fibre_theta.c_str(),
	                               Config_para.Fibre_phi.c_str(), dx, DD, D2);*/

	laplacian = generate_laplacian_heterogeneity_2d(atrium,
	            NY, NX, c,
	            Config_para.Fibre_phi.c_str(),
	            dx, dx, DD, D2);
	// tissue = generate_tissue_map_2d(atrium, NY, NX, c);
	// unsigned char * stim_flag = create_stimulation_map_vec_2D_geo(atrium, Config_para.Pacemap.c_str(), NY, NX, c);
	unsigned char * stim_flag = create_stimulation_map_vec_band(atrium, NY, NX, c, "LEFT", 10);

	int * cell_block_type = generate_patch_pattern_2d(atrium, NY, NX, c, 5);

	for (int i = 0; i < c; ++i)
	{
		std::cout<< cell_block_type[i] << std::endl;
	}

	deallocate_and_zero_2d_matrix(atrium, NY + 2, NX + 2);

	const int total_cell_num = c;
	std::cerr << ">>Succesfully read in Geometry and Fibre Files<<\n";
	std::cerr << ">>Total Number of Cells: " << c << " <<\n";

	tissue = new unsigned char [c];
	for (int id = 0; id < c; id++) {
		tissue[id] = 14;
	}
	int cell_para_num = 18;
	int cell_pop_num = 600;
	double * cell_para_vec = new double [cell_pop_num * cell_para_num];

	memset(cell_para_vec, 0, cell_pop_num * cell_para_num * sizeof(double));
	read_num_data_file("para_log.dat.New", cell_para_vec, cell_pop_num * cell_para_num);

	nbd = new int [c * NUM_NBH];
	lap = new double [c * NUM_NBH];


	for (int n = 0; n < c; n++) {
		for (int i = 0; i < NUM_NBH; i++) {
			nbd[(n * NUM_NBH) + i] = neighbourhood[n][i];
			lap[(n * NUM_NBH) + i] = laplacian[n][i];
		}
	}

	deallocate_and_zero_2d_matrix(neighbourhood, c, NUM_NBH);
	deallocate_and_zero_2d_matrix(laplacian, c, NUM_NBH);

	Topology top(MPI_COMM_WORLD, c);
	top.setup_dependecies(nbd, NUM_NBH);
	int local_cell_num = top.local_cell_num;
	int starting_index = top.starting_index;
	int * local_cell_num_vec = top.local_cell_num_vec;
	int * offset_cell_num_vec = top.offset_cell_num_vec;



	const int count = local_cell_num;
	Stim_time = new float [total_cell_num];
	Stim_amp  = new float [total_cell_num];
	Stim_amp_S2 = new float[total_cell_num];
	v_global = new double [ total_cell_num];
	v_global_temp_1 = new double [ total_cell_num];
	v_global_temp_2 = new double [ total_cell_num];

	float *v_global_out = new float[total_cell_num];
	float *Cai_global_out = new float [total_cell_num];


	float* Cai_out;// = new float[count];

	state = new double [ STATE_LEN * count];
	// het       = new heterogeneous_params_t [count];
	v_new     = new double [count];
	v_old     = new double [count];
	dv_diff_temp = new double [count];
	v_temp = new double [count];
	v_temp_2 = new double [count];
	RK_K1 = new double [count];
	RK_K2 = new double [count];
	RK_K3 = new double [count];
	san       = new  char [count];
	stims     = new float [count];
	FB_number = new int [count];
	Fcell     = new float [count];

	// SingleCellPara *cell_para;
	// cell_para = new SingleCellPara [count];
	std::vector<bool> read_file_flag;
	// cell_vec  = new TTCell* [count];
	// read_file_flag.push_back(read_float_from_bin(Config_para.Stim_time_file.c_str(), Stim_time, count));
	/*read_file_flag.push_back(read_float_from_bin(Config_para.Stim_amp_file.c_str(), Stim_amp, count));
	// read_file_flag.push_back(read_float_from_bin(Config_para.S2_StimLocFile.c_str(), S2_Stim_amp, count));

	for (int i = 0; i < NY; ++i)
		for (int j = 0; j < NX; ++j)
		{
			if ( (i * i + j * j < 25))
				Stim_amp[i * NX + j] = 20;
			else
				Stim_amp[i * NX + j] = 0;
		}
	*/
	std::vector<int> AP_Number(c);
	// Dealing with Config_paraments etc
	// Default values

	// Config_para.Total_time = (Config_para.S1_number) * Config_para.BCL + Config_para.S2 * 2 + 100.0;
	std::vector<double> PrePotential(c);
	std::vector<double> TemPotential(c);
	std::vector<double> ActTime(c);
	ISO     = ACH = AF = 0;
	BCL     = 1000;
	S2      = 0;
	FB_type = 0;
	Ggap    = 3.0;
	double timeelasped;

	std::ofstream OneD_output;
	std::ofstream cv_out_file("cv_out_file.dat", std::ios::out);
	// std::vector<TypeCell> cell_type_vec;
	std::vector<double> cv; // for recording cv
	std::vector<std::shared_ptr<APInfor> > APInfor_vec;

	for (auto& x : Config_para.Popul_scalingF)
		std::cout << x << ' ';


	std::setprecision(10);



	std::string filename = "TwoD_output.dat." + std::to_string(rank);


	double t_first_act, t_second_act;


	// the spatial cell model here.
	CVOde_Cell **cell_vec = new CVOde_Cell* [top.local_cell_num];


	// omp_set_num_threads(2);

	{

		v_out = new float [total_cell_num];

		Cai_out = new float[count];

		const int count = top.local_cell_num;
		for (int i = 0; i < count; ++i)
		{
			cell_vec[i] = new CVOde_Cell(42, 0.2, fnew_vm_as_para, false,
			                             i + top.starting_index, cell_block_type[i + top.starting_index]);


			cell_vec[i]->cell.assign_cell_pop_para(&cell_para_vec[cell_vec[i]->m_cell_type_ID * cell_para_num]); //(i + top.starting_index)*17]);

			cell_vec[i]->cell.BCL = 500;

			if (Config_para.PLB_Sim != "Default") {

				std::string filename = "Initial_Conditions/" + Config_para.PLB_Sim + "/Ini.bin." + std::to_string(cell_vec[i]->m_cell_type_ID);
				// std::cerr << filename << std::endl; // debug;
				cell_vec[i]->read_initial_condition(filename.c_str());
				// "kmf.1.0.ISO.0.0", "kmf.0.25.ISO.0.0","kmf.1.0.ISO.1.0","kmf.0.25.ISO.1.0"
				if (Config_para.PLB_Sim == "kmf.1.0.ISO.0.0" ) {
					cell_vec[i]->cell.kmf_scale = 1.0;
					cell_vec[i]->cell.ISO = 0.0;
				} else if (Config_para.PLB_Sim == "kmf.1.0.ISO.1.0" ) {
					cell_vec[i]->cell.kmf_scale = 1.0;
					cell_vec[i]->cell.ISO = 1.0;
				} else if (Config_para.PLB_Sim == "kmf.0.25.ISO.0.0" ) {
					cell_vec[i]->cell.kmf_scale = 0.25;
					cell_vec[i]->cell.ISO = 0.0;
				}  else if (Config_para.PLB_Sim == "kmf.0.25.ISO.1.0" ) {
					cell_vec[i]->cell.kmf_scale = 0.25;
					cell_vec[i]->cell.ISO = 1.0;
				} else {
					std::cerr << "Wrong PLB_Sim parameter! Please check! Aborting ..." << std::endl;
					std::exit(0);
				}
				// cell_vec[i]->cell.kmf_scale = atof(argv[2]);
				// cell_vec[i]->cell.ISO = atof(argv[3]);
			}

			/*if (i + top.starting_index > 4000) {
				cell_vec[i] = new CVOde_Cell(43, 0.2, fnew_vm_as_para, false,
				                             i + top.starting_index, cell_block_type[i + top.starting_index]); //Spatial_Cell(0.05, 0, i + top.starting_index,"tub_input_ver_0.txt");  // allocate memory for each cell here.
				std::cout <<  i + top.starting_index << " tub_input_ver_0.txt" << std::endl;
			}
			else {
				cell_vec[i] = new CVOde_Cell(43, 0.2, fnew_vm_as_para, false,
				                             i + top.starting_index, cell_block_type[i + top.starting_index]); //Spatial_Cell(0.05, 0, i + top.starting_index, "tub_input_ver_19.txt");  // allocate memory for each cell here.
				std::cout <<  i + top.starting_index << " tub_input_ver_19.txt" << std::endl;
			}*/
		}

		// dv    = new double [count];
		// MALLOC(dv, count * sizeof(double));


		// allocate voltage arrays


		// cell_vec  = new TNNP_MarkovIKr* [count];
		/*for (int i = 0; i < count; ++i)
		{
		    TypeCell celltype = EPI;
		    double epi_mid_ratio = 1.5;
		    TNNP_MarkovIKr cell( celltype, epi_mid_ratio, dt );
		    cell_vec.push_back(cell);
		}*/

		/*read_float_from_bin(Config_para.Stim_time_file, Stim_time, count);
		read_float_from_bin(Config_para.Stim_amp_file, Stim_amp, count);*/

		/*for (int i = 0; i < total_cell_num; ++i) {
			Stim_time[i] = 0.0;
			Stim_amp[i]  = 20.0;
			Stim_amp_S2[i] = 0.0;
			if (i > 15)
				Stim_amp[i] = 0.0;

			if (i > 100 and i < 106)
				Stim_amp_S2[i] = 30.0;
		}*/


		/*if (rank == 0) {
			if (!Config_para.SAN_type.c_str())
				perror("SAN Setup wrong");

			std::cerr << "SAN_type = " << Config_para.SAN_type << std::endl;
			std::cerr << "Stim_time_File = " << Config_para.Stim_time_file << std::endl;
			// printf(">>Assigned Fcell according to parameters<<\n");
			std::cerr << ">>Assigned Fcell according to parameters<<" << std::endl;
		}*/


		for (int i = 0; i < count; ++i)
		{
			AP_Number[i] = 0; // initial beats numbers.

			if (i == SECOND_ACT)
			{
				std::string filename = "APD2.txt" + std::to_string(i + top.starting_index);
				APInfor_vec.push_back(std::shared_ptr<APInfor>(new APInfor(filename.c_str(), false, true) ));
			} else if (i == FIRST_ACT)
			{
				std::string filename = "APD2.txt" + std::to_string(i + top.starting_index);

				APInfor_vec.push_back(std::shared_ptr<APInfor>(new APInfor(filename.c_str(), false, true) ));
			}

			else {
				// APInfor temp;
				APInfor_vec.push_back(std::shared_ptr<APInfor>(new APInfor() ));
			}
		}


		// assign voltages
		#pragma omp for schedule(static)
		for (int n = 0; n < count; n++) {

			// initialise v_new and v_old arrays with voltage of each cell.
			v_new[n] = v_old[n] = cell_vec[n]->cell.V;

#ifdef CV_COM
			PrePotential[n] = v_new[n];
			TemPotential[n] = v_new[n];
			ActTime[n] = -1000.0;
#endif
			/*regional cell models parameter assignment */
			/* to be continued.... */
			/* this part was updated in the latest wholeheart main function */
			/* if you want to complete this part, please refer to the new file system Paramters.c*/
		}


		// #ifdef _OPENMP
		#pragma omp master
		{
			end_setup = omp_get_wtime();
			// fprintf(stderr, "Setup time:   %6g\n", end_setup - start);
			std::cerr << "Setup time:  " << end_setup - start << std::endl;
		}
		// #endif
		// #pragma omp barrier
		#pragma omp master
		{
			// fprintf(stderr, ">>Assigned regional and modulation parameters<<\n>>Starting time loop now<<\n");
			std::cerr << ">>Assigned regional and modulation parameters<<\n>>Starting time loop now<<" << std::endl;
		}
		#pragma omp barrier

		double output_time = 0.0;
		timeelasped = omp_get_wtime();
		int Thread_ID = omp_get_thread_num();
		int threadnum = omp_get_thread_num();
		int    numthreads = omp_get_num_threads();
		int low = count * threadnum / numthreads;
		int    high = count * (threadnum + 1) / numthreads;


		int outindex = 0;
		int outcount = 0;
		int filecount = 0;


		int outcounter = 0;
		// loop over time.
		#pragma omp master
		{
			MPI_Barrier(MPI_COMM_WORLD);
		}
		#pragma omp barrier


		std::cerr << "Total  time: " << Config_para.Total_time <<  " ms" << std::endl;

		for (double t = 0; t <= Config_para.Total_time + dt / 2; t += dt) {

#ifdef OUT_INICOND
			#pragma omp for schedule(static)   // part 1, y_bar_j+1 = y_i + h*f(t, y(t))
			for (int n = 0; n < count; ++n) {


				if (t >= Config_para.Total_time - 2 * Config_para.BCL - 10 - dt / 2.0 and t <= Config_para.Total_time - 2 * Config_para.BCL - 10 + dt / 2.0) // output initial values
				{
					char filename [100];
					sprintf(filename, "IniCond/Cell_%03d_BCL_%04d.bin", n, (int) Config_para.BCL);
					output_double_array_bin(filename, &state[n * STATE_LEN], STATE_LEN);
					outcount = 0;
					/*if (omp_get_thread_num() == 0)
						std::cerr << std::setprecision(20) << " current time is t = " << t <<  " ID is = " << omp_get_thread_num() << std::endl;*/
				}
			}
#endif

			#pragma omp master
			{
				MPI_Barrier(MPI_COMM_WORLD);
				// if (rank == 0)

			}

			
#ifdef OUT_TO_BIN_SINGLE

			// #pragma omp master
			// {
			// if (t > Config_para.Total_time - 2 * Config_para.BCL - 10)
			if (outcounter % 20 == 0 )
			{

				if (rank == 0)
					std::cerr << "in time loop now, t = " << t << " <<" << std::endl;
				if( t > 0) {
					int error = MPI_Gatherv(v_old, count, MPI_DOUBLE, v_global_temp_2, local_cell_num_vec, offset_cell_num_vec, MPI_DOUBLE, 0, MPI_COMM_WORLD);
					if (rank == 0) {
						#pragma omp parallel for
						for (int i = 0; i < total_cell_num; ++i)
						{
							v_global_out[i] = (float) v_global_temp_2[i];
						}
						output_voltage_array(total_cell_num, v_global_out, "v_%04d.bin", (int)std::round(t));
					}

				}

			}



			outcounter++;

#endif






#ifdef PROFILE_TIME
			#pragma omp single
			{
				std::cerr << "Setup time:  " <<  omp_get_wtime() - end_setup << std::endl;
				timeelasped = omp_get_wtime();
			}
#endif

			/*operator spliting part 1*/

			/*RK4 method*/
			/* using RK4 here*/
			#pragma omp barrier
			// MPI_Barrier( MPI_COMM_WORLD ); // add a barier here to ensure Synchronization

			#pragma omp master
			{

				MPI_Barrier(MPI_COMM_WORLD);
			}


			// v_global
			#pragma omp master
			{
				top.set_ghost_cells(v_old, v_global);
			}
			#pragma omp barrier




			#pragma omp for schedule(static)   // part 1, y_bar_j+1 = y_i + h*f(t, y(t))
			for (int n = 0; n < count; n++) {

				v_new[n] = v_old[n];  // keep a copy of v_old[n], which is y_i;
				//dv[n] = 0.0;
				double sum = 0.0;
				int ii = 0;

				// #pragma omp simd reduction // not available on icpc 12.0
				for (ii = 0; ii < NUM_NBH; ii++) {
					sum +=  (lap[((n + starting_index) * NUM_NBH) + ii] * v_global[nbd[((n + starting_index) * NUM_NBH) + ii]]); // this is f(t, y_i)
				}
				// dv[n] = sum;
				//y_bar_i+1
				RK_K1[n] = sum; // clear to 0 first, otherwise its going to accumulate ...
				v_temp[n] = v_old[n] + sum * dt / 2.0 / 2.0; // devide by two because of operator splitting. // y_n_K1/2
			}

			#pragma omp master
			{
				top.set_ghost_cells(v_temp, v_global_temp_1);
			}
			#pragma omp barrier

			#pragma omp for schedule(static)   // part 1, y_bar_j+1 = y_i + h*f(t, y(t))
			for (int n = 0; n < count; n++) {

				// v_new[n] = v_old[n];  // keep a copy of v_old[n], which is y_i;
				//dv[n] = 0.0;
				double sum = 0.0;
				int ii = 0;

				// #pragma omp simd reduction // not available on icpc 12.0
				for (ii = 0; ii < NUM_NBH; ii++) {
					sum +=  (lap[((n + starting_index)  * NUM_NBH) + ii] * v_global_temp_1[nbd[((n + starting_index)  * NUM_NBH) + ii]]); // this is f(t, y_i)
				}
				// dv[n] = sum;
				//y_bar_i+1
				RK_K2[n] = sum; // clear to 0 first, otherwise its going to accumulate ...
				v_temp_2[n] = v_old[n] + sum * dt / 2.0 / 2.0; // devide by two because of operator splitting. // y_n_K2/2
			}


			#pragma omp master
			{
				top.set_ghost_cells(v_temp_2, v_global_temp_2);

			}
			#pragma omp barrier

			#pragma omp for schedule(static)   // part 1, y_bar_j+1 = y_i + h*f(t, y(t))
			for (int n = 0; n < count; n++) {

				// v_new[n] = v_old[n];  // keep a copy of v_old[n], which is y_i;
				//dv[n] = 0.0;
				double sum = 0.0;
				int ii = 0;

				// #pragma omp simd reduction // not available on icpc 12.0
				for (ii = 0; ii < NUM_NBH; ii++) {
					sum +=  (lap[((n + starting_index)  * NUM_NBH) + ii] * v_global_temp_2[nbd[((n + starting_index)  * NUM_NBH) + ii]]); // this is f(t, y_i)
				}
				// dv[n] = sum;
				//y_bar_i+1
				RK_K3[n] = sum; // clear to 0 first, otherwise its going to accumulate ...
				v_temp[n] = v_old[n] + sum * dt / 2.0; // devide by two because of operator splitting. // y_n_K3/2
			}

			#pragma omp master
			{

				top.set_ghost_cells(v_temp, v_global_temp_1);
			}
			#pragma omp barrier


			#pragma omp for schedule(static)   // part 1, y_bar_j+1 = y_i + h*f(t, y(t))
			for (int n = 0; n < count; n++) {

				// v_new[n] = v_old[n];  // keep a copy of v_old[n], which is y_i;
				//dv[n] = 0.0;
				double sum = 0.0;
				int ii = 0;

				// #pragma omp simd reduction // not available on icpc 12.0
				for (ii = 0; ii < NUM_NBH; ii++) {
					sum +=  (lap[((n + starting_index)  * NUM_NBH) + ii] * v_global_temp_1[nbd[((n + starting_index)  * NUM_NBH) + ii]]); // this is f(t, y_i)
				}
				v_new[n] = v_new[n] + (sum + RK_K1[n] + 2 * RK_K2[n] + 2 * RK_K3[n]) * dt / 2.0 / 6.0; // devide by two because of operator splitting. // y_n_K3/2
				// APInfor_vec[n]->MeasureAPD90Using_dVdtMax(t, 0, Config_para.BCL, dt, v_new[n]);  // measure APD at the end of each time step.
			}



			#pragma omp parallel for schedule(auto)
			for (int n = 0; n < count; n++) {



				// double stims = 0;
				/*if (Stim_amp[n + starting_index] != 0)
					stims  = S1(0, 12.5, Config_para.BCL, t, 5.0);*/

				double stims = S1(0, 12.5, cell_vec[n]->cell.BCL, t, 5.0);


				if (!stim_flag[n + starting_index])
					// stims  = S1(0, 20.0, Config_para.BCL, t, 2.0);
					cell_vec[n]->cell.allow_stimulation_flag = false;

				if (t > 10030)
					cell_vec[n]->cell.allow_stimulation_flag = false;


				cell_vec[n]->cell.i_Stim = stims * cell_vec[n]->cell.allow_stimulation_flag;

				// std::cerr << t << " working on " << n << " " << cell_vec[n]->m_cell_type_ID << std::endl;
				cell_vec[n]->cell.V = v_new[n];
				cell_vec[n]->solve_single_time_step_vm_para(t + dt, dt);

				// if (n < 5)
				// for (int i = 0; i < 2; ++i)  // due to difference in time steps.
				// {
				// cell_vec[n] -> pace(stims);  // - stim;
				// cell_vec[n] -> pace(stims);  // - stim;
				// }

				v_new[n] = cell_vec[n]->cell.V;

				if (v_new[n] != v_new[n]) {
					std::cerr << " NaNs Encountered, Exiting Programe... ... \n\n\n";
					std::exit(0);
				}

				// state[(n * STATE_LEN)] = v_new[n];
			}



#ifdef PROFILE_TIME
			#pragma omp single
			{
				// end_setup = omp_get_wtime();
				// fprintf(stderr, "Setup time:   %6g\n", end_setup - start);
				std::cerr << "Setup time:  " <<  omp_get_wtime() - end_setup << std::endl;
				timeelasped = omp_get_wtime();
			}
#endif





			/*operator spliting part 2*/

			/* using RK4 here*/

			#pragma omp master
			{

				MPI_Barrier(MPI_COMM_WORLD);
				top.set_ghost_cells(v_new, v_global);
			}
			#pragma omp barrier



			#pragma omp for schedule(static)   // part 1, y_bar_j+1 = y_i + h*f(t, y(t))
			for (int n = 0; n < count; n++) {

				v_old[n] = v_new[n];  // keep a copy of v_new[n], which is y_i;
				//dv[n] = 0.0;
				double sum = 0.0;
				int ii = 0;

				// #pragma omp simd reduction // not available on icpc 12.0
				for (ii = 0; ii < NUM_NBH; ii++) {
					sum +=  (lap[((n + starting_index) * NUM_NBH) + ii] * v_global[nbd[((n + starting_index) * NUM_NBH) + ii]]); // this is f(t, y_i)
				}
				// dv[n] = sum;
				//y_bar_i+1
				RK_K1[n] = sum; // clear to 0 first, otherwise its going to accumulate ...
				v_temp[n] = v_new[n] + sum * dt / 2.0 / 2.0; // devide by two because of operator splitting. // y_n_K1/2
			}

			#pragma omp single
			{

				top.set_ghost_cells(v_temp, v_global_temp_1);
			}
			// #pragma omp barrier


			#pragma omp for schedule(static)   // part 1, y_bar_j+1 = y_i + h*f(t, y(t))
			for (int n = 0; n < count; n++) {

				// v_old[n] = v_new[n];  // keep a copy of v_new[n], which is y_i;
				//dv[n] = 0.0;
				double sum = 0.0;
				int ii = 0;

				// #pragma omp simd reduction // not available on icpc 12.0
				for (ii = 0; ii < NUM_NBH; ii++) {
					sum +=  (lap[((n + starting_index) * NUM_NBH) + ii] * v_global_temp_1[nbd[((n + starting_index) * NUM_NBH) + ii]]); // this is f(t, y_i)
				}
				// dv[n] = sum;
				//y_bar_i+1
				RK_K2[n] = sum; // clear to 0 first, otherwise its going to accumulate ...
				v_temp_2[n] = v_new[n] + sum * dt / 2.0 / 2.0; // devide by two because of operator splitting. // y_n_K2/2
			}



			#pragma omp master
			{

				top.set_ghost_cells(v_temp_2, v_global_temp_2);

			}
			#pragma omp barrier

			#pragma omp for schedule(static)   // part 1, y_bar_j+1 = y_i + h*f(t, y(t))
			for (int n = 0; n < count; n++) {

				// v_old[n] = v_new[n];  // keep a copy of v_new[n], which is y_i;
				//dv[n] = 0.0;
				double sum = 0.0;
				int ii = 0;

				// #pragma omp simd reduction // not available on icpc 12.0
				for (ii = 0; ii < NUM_NBH; ii++) {
					sum +=  (lap[((n + starting_index) * NUM_NBH) + ii] * v_global_temp_2[nbd[((n + starting_index) * NUM_NBH) + ii]]); // this is f(t, y_i)
				}
				// dv[n] = sum;
				//y_bar_i+1
				RK_K3[n] = sum; // clear to 0 first, otherwise its going to accumulate ...
				v_temp[n] = v_new[n] + sum * dt / 2.0; // devide by two because of operator splitting. // y_n_K3/2
			}



			#pragma omp master
			{

				top.set_ghost_cells(v_temp, v_global_temp_1);

			}
			#pragma omp barrier

			#pragma omp for schedule(static)   // part 1, y_bar_j+1 = y_i + h*f(t, y(t))
			for (int n = 0; n < count; n++) {

				// v_old[n] = v_new[n];  // keep a copy of v_new[n], which is y_i;
				//dv[n] = 0.0;
				double sum = 0.0;
				int ii = 0;

				// #pragma omp simd reduction // not available on icpc 12.0
				for (ii = 0; ii < NUM_NBH; ii++) {
					sum +=  (lap[((n + starting_index)  * NUM_NBH) + ii] * v_global_temp_1[nbd[((n + starting_index)  * NUM_NBH) + ii]]); // this is f(t, y_i)
				}
				// dv[n] = sum;
				//y_bar_i+1
				// RK_K3[n] = sum; // clear to 0 first, otherwise its going to accumulate ...
				v_old[n] = v_old[n] + (sum + RK_K1[n] + 2 * RK_K2[n] + 2 * RK_K3[n]) * dt / 2.0 / 6.0; // devide by two because of operator splitting. // y_n_K3/2
				APInfor_vec[n]->MeasureAPD90Using_dVdtMax(t, 0, Config_para.BCL, dt, v_old[n]);  // measure APD at the end of each time step.
			}




			/* Euler method applied 4 times */
			/*#pragma omp for schedule(static)   // part 1, y_bar_j+1 = y_i + h*f(t, y(t))
			for (int n = 0; n < count; n++) {

				v_old[n] = v_new[n];  // keep a copy of v_new[n], which is y_i;
				//dv[n] = 0.0;
				double sum = 0.0;
				int ii = 0;

				// #pragma omp simd reduction // not available on icpc 12.0
				for (ii = 0; ii < NUM_NBH; ii++) {
					sum +=  (lap[(n * NUM_NBH) + ii] * v_new[nbd[(n * NUM_NBH) + ii]]); // this is f(t, y_i)
				}
				// dv[n] = sum;
				//y_bar_i+1
				RK_K1[n] = sum; // clear to 0 first, otherwise its going to accumulate ...
				v_temp[n] = v_new[n] + sum * dt / 2.0 / 4.0; // devide by two because of operator splitting. // y_n_K1/2
			}
			#pragma omp for schedule(static)   // part 1, y_bar_j+1 = y_i + h*f(t, y(t))
			for (int n = 0; n < count; n++) {

				// v_old[n] = v_new[n];  // keep a copy of v_new[n], which is y_i;
				//dv[n] = 0.0;
				double sum = 0.0;
				int ii = 0;

				// #pragma omp simd reduction // not available on icpc 12.0
				for (ii = 0; ii < NUM_NBH; ii++) {
					sum +=  (lap[(n * NUM_NBH) + ii] * v_temp[nbd[(n * NUM_NBH) + ii]]); // this is f(t, y_i)
				}
				// dv[n] = sum;
				//y_bar_i+1
				RK_K2[n] = sum; // clear to 0 first, otherwise its going to accumulate ...
				v_temp_2[n] = v_temp[n] + sum * dt / 2.0 / 4.0; // devide by two because of operator splitting. // y_n_K2/2
			}

			#pragma omp for schedule(static)   // part 1, y_bar_j+1 = y_i + h*f(t, y(t))
			for (int n = 0; n < count; n++) {

				// v_old[n] = v_new[n];  // keep a copy of v_new[n], which is y_i;
				//dv[n] = 0.0;
				double sum = 0.0;
				int ii = 0;

				// #pragma omp simd reduction // not available on icpc 12.0
				for (ii = 0; ii < NUM_NBH; ii++) {
					sum +=  (lap[(n * NUM_NBH) + ii] * v_temp_2[nbd[(n * NUM_NBH) + ii]]); // this is f(t, y_i)
				}
				// dv[n] = sum;
				//y_bar_i+1
				RK_K3[n] = sum; // clear to 0 first, otherwise its going to accumulate ...
				v_temp[n] = v_temp_2[n] + sum * dt / 2.0/4.0; // devide by two because of operator splitting. // y_n_K3/2
			}

			#pragma omp for schedule(static)   // part 1, y_bar_j+1 = y_i + h*f(t, y(t))
			for (int n = 0; n < count; n++) {

				// v_old[n] = v_new[n];  // keep a copy of v_new[n], which is y_i;
				//dv[n] = 0.0;
				double sum = 0.0;
				int ii = 0;

				// #pragma omp simd reduction // not available on icpc 12.0
				for (ii = 0; ii < NUM_NBH; ii++) {
					sum +=  (lap[(n * NUM_NBH) + ii] * v_temp[nbd[(n * NUM_NBH) + ii]]); // this is f(t, y_i)
				}
				// dv[n] = sum;
				//y_bar_i+1
				// RK_K3[n] = sum; // clear to 0 first, otherwise its going to accumulate ...
				v_old[n] = v_temp[n] + sum * dt/2.0/4.0; // + (sum + RK_K1[n] + 2 * RK_K2[n] + 2 * RK_K3[n]) * dt / 2.0 / 6.0; // devide by two because of operator splitting. // y_n_K3/2
				APInfor_vec[n]->MeasureAPD90Using_dVdtMax(t, 0, Config_para.BCL, dt, v_old[n]);  // measure APD at the end of each time step.
			}
			*/


			/*Forward Euler */

			/*#pragma omp for schedule(static)   // part 1, y_bar_j+1 = y_i + h*f(t, y(t))
			for (int n = 0; n < count; n++) {

				v_old[n] = v_new[n];  // keep a copy of v_new[n], which is y_i;
				//dv[n] = 0.0;
				double sum = 0.0;
				int ii = 0;

				// #pragma omp simd reduction // not available on icpc 12.0
				for (ii = 0; ii < NUM_NBH; ii++) {
					sum +=  (lap[(n * NUM_NBH) + ii] * v_new[nbd[(n * NUM_NBH) + ii]]); // this is f(t, y_i)
				}
				// dv[n] = sum;
				//y_bar_i+1
				dv_diff_temp[n] = sum; // clear to 0 first, otherwise its going to accumulate ...
				v_old[n] = v_new[n] + sum * dt / 2.0;  // devide by two because of operator splitting.
				APInfor_vec[n]->MeasureAPD90Using_dVdtMax(t, 0, Config_para.BCL, dt, v_old[n]);  // measure APD at the end of each time step.
			}*/





#ifdef CV_COM
			#pragma omp for schedule(static)
			for (int n = 0; n < count; ++n) {


				PrePotential[n] = TemPotential[n];
				TemPotential[n] = v_old[n];
				// v_new[n] = v_new[n] -  dt * (Itot - stims[n]);
				if ((TemPotential[n] > -20.0) and (PrePotential[n] <= -20.0)) {

					if (t - ActTime[n] >= Config_para.S2 / 5.0)
					{
						ActTime[n] = t;
						AP_Number[n] += 1; // add An AP count
					}

				}



			}

			#pragma omp single
			{
				// printf("a\n");
				if ((TemPotential[FIRST_ACT] > -20.0) && (PrePotential[FIRST_ACT] <= -20.0)) {
					t_first_act =  t;
				}
				if ((TemPotential[SECOND_ACT] > -20.0) && (PrePotential[SECOND_ACT] <= -20.0)) {
					t_second_act =  t;
					double cvvalue = (SECOND_ACT - FIRST_ACT) * dx / (ActTime[SECOND_ACT] - ActTime[FIRST_ACT]);
					std::cerr << "t_first_act = " << t_first_act << " , t_second_act = " << t_second_act << std::endl;
					std::cerr << "CV = " << cvvalue << std::endl;
					cv.push_back(cvvalue);
#ifdef BREAK_IF_CVM

					if (cv.size() > 20)
					{
						if (fabs(cv[cv.size() - 1] - cv[cv.size() - 2]) < 0.0005)
						{
							// break;
							Config_para.Total_time = t;
						}
					}
#endif
				}

				/*std::ofstream out35("35.dat", std::ios::out | std::ios::app);
				out35 << t << " " << TemPotential[9] << std::endl;
				out35.close();*/
			}
#endif






#ifdef PROFILE_TIME
			#pragma omp single
			{
				// end_setup = omp_get_wtime();
				// fprintf(stderr, "Setup time:   %6g\n", end_setup - start);
				std::cerr << "Setup time:  " <<  omp_get_wtime() - end_setup << std::endl;
				timeelasped = omp_get_wtime();
			}
#endif
			timeint = t * intstim;
		} // end of time loop
#ifdef OUT_TO_BIN_THREADED
		if (outindex != 0)
			if (omp_get_thread_num() < outindex) {
				if (rank == 0)
					output_voltage_array(total_cell_num, v_out, "v%04d.bin", (filecount * omp_get_num_threads()) + omp_get_thread_num());
			}

#endif

		if (v_out)
			delete [] v_out;
	} // end parallel loops


	end = omp_get_wtime();
	std::cerr << "Elapsed Time : " << end - start << std::endl;
	std::cerr << "Time per ms : " << (end - end_setup) / (t_max - Config_para.t_start) << std::endl;


	delete [] Stim_time       ;//= new float [total_cell_num];
	delete [] Stim_amp        ;//= new float [total_cell_num];
	delete [] Stim_amp_S2     ;//= new float[total_cell_num];


	delete [] v_global        ;//= new double [ total_cell_num];
	delete [] v_global_temp_1 ;//= new double [ total_cell_num];
	delete [] v_global_temp_2 ;//= new double [ total_cell_num];

	delete [] state           ;//= new double [ STATE_LEN * count];
// delete [] // het          ;//= new heterogeneous_params_t [count];
	delete [] v_new           ;//= new double [count];
	delete [] v_old           ;//= new double [count];
	delete [] dv_diff_temp    ;//= new double [count];
	delete [] v_temp          ;//= new double [count];
	delete [] v_temp_2        ;//= new double [count];
	delete [] RK_K1           ;//= new double [count];
	delete [] RK_K2           ;//= new double [count];
	delete [] RK_K3           ;//= new double [count];
	delete [] san             ;//= new  char [count];
	delete [] stims           ;//= new float [count];
	delete [] FB_number       ;//= new int [count];
	delete [] Fcell           ;//= new float [count];

	// delete [] cell_para       ;//= new SingleCellPara [count];

	delete [] nbd;
	delete [] lap;
	if (v_global_out)
		delete [] v_global_out;
	if (tissue)
		delete [] tissue;


	if (Cai_global_out)
		delete [] Cai_global_out;
	if (Cai_out)
		delete [] Cai_out;

	if (stim_flag) delete [] stim_flag;

	if (cell_block_type != nullptr) delete [] cell_block_type;
	if (cell_para_vec != nullptr) delete [] cell_para_vec;

	for (int i = 0; i < top.local_cell_num; ++i)
	{
		if(cell_vec[i] != nullptr) {
			delete cell_vec[i];  // ask to delete each cell first;
		}
	}

	delete [] cell_vec;


	MPI_Finalize();

	return 0;
}
// end of main()