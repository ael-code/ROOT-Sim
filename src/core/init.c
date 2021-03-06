/**
*			Copyright (C) 2008-2018 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; only version 3 of the License applies.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file init.c
* @brief This module implements the simulator initialization routines
* @author Francesco Quaglia
* @author Roberto Vitali
* @author Alessandro Pellegrini
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <limits.h>
#include <getopt.h>

#include <ROOT-Sim.h>
#include <arch/os.h>
#include <arch/thread.h>
#include <communication/communication.h>
#include <core/core.h>
#include <core/init.h>
#include <scheduler/process.h>
#include <gvt/gvt.h>
#include <gvt/ccgs.h>
#include <scheduler/scheduler.h>
#include <mm/state.h>
#include <mm/dymelor.h>
#include <statistics/statistics.h>
#include <lib/numerical.h>
#include <serial/serial.h>
#ifdef HAVE_MPI
#include <communication/mpi.h>
#endif

/// The initial number of application-level argument the simulator reserves space for. If a greater number is found, the array is realloc'd
#define APPLICATION_ARGUMENTS 32

// This is the list of mnemonics for arguments
#define OPT_NP			1
#define OPT_NPRC		2
#define OPT_OUTPUT_DIR		3
#define OPT_SCHEDULER		4
#define OPT_NPWD		5
#define OPT_P			6
#define OPT_FULL		7
#define OPT_INC			8
#define OPT_A			9
#define OPT_GVT			10
#define OPT_CKTRM_MODE		11
#define OPT_BLOCKING_GVT	12
#define OPT_GVT_SNAPSHOT_CYCLES	13
#define OPT_SIMULATION_TIME	14
#define OPT_LPS_DISTRIBUTION	15
#define OPT_DETERMINISTIC_SEED	16
#define OPT_VERBOSE		17
#define OPT_STATS		18
#define OPT_SEED		19
#define OPT_SERIAL		20
#define OPT_NO_CORE_BINDING	21

#ifdef HAVE_PREEMPTION
#define OPT_PREEMPTION		22
#endif

#ifdef HAVE_PARALLEL_ALLOCATOR
#define OPT_ALLOCATOR		23
#endif

/// This variable keeps the executable's name
char *program_name;

/// To let the parallel initialization access the user-level command line arguments
struct app_arguments model_parameters;


// TODO: a vector of vector with text name of numerical options, which should be used for parsing options and for displaying names
// static char *opt_opt[][] = { ... }
static char *opt_desc[] = {
	"",
	"Number of total cores being used by the simulation",
	"Total number of Logical Processes being lunched at simulation startup",
	"Path to a folder where execution statistics are stored. If not present, it is created",
	"LP Scheduling algorithm. Supported values are: stf",
	"Non Piece-Wise-Deterministic simulation model. See manpage for accurate description",
	"Checkpointing interval",
	"Take only full logs",
	"Take only incremental logs (still to be released)",
	"Autonomic subsystem: set checkpointing interval and log mode automatically at runtime (still to be released)",
	"Time between two GVT reductions (in milliseconds)",
	"Termination Detection mode. Supported values: standard, incremental",
	"Blocking GVT. All distributed nodes block until a consensus is agreed",
	"Termination detection is invoked after this number of GVT reductions",
	"Halt the simulation when all LPs reach this logical time. 0 means infinite",
	"LPs distributions over simulation kernels policies. Supported values: block, circular",
	"Do not change the initial random seed for LPs. Enforces different deterministic simulation runs",
	"Verbose execution",
	"Level of detail in the output statistics",
	"Manually specify the initial random seed",
	"Run a serial simulation (using Calendar Queues)",
	"Disable the binding of threads to specific phisical processing cores",

#ifdef HAVE_PREEMPTION
	"Disable Preemptive Time Warp",
#endif

#ifdef HAVE_PARALLEL_ALLOCATOR
	"Disable parallel allocator",
#endif

	""
};


static struct option long_options[] = {
	{"np",			required_argument,	0, OPT_NP},
	{"output-dir",		required_argument,	0, OPT_OUTPUT_DIR},
	{"scheduler",		required_argument,	0, OPT_SCHEDULER},
	{"npwd",		no_argument,		0, OPT_NPWD},
	{"p",			required_argument,	0, OPT_P},
	{"full",		no_argument,		0, OPT_FULL},
	{"inc",			no_argument,		0, OPT_INC},
	{"A",			no_argument,		0, OPT_A},
	{"gvt",			required_argument,	0, OPT_GVT},
	{"cktrm_mode",		required_argument,	0, OPT_CKTRM_MODE},
	{"nprc",		required_argument,	0, OPT_NPRC},
	{"blocking_gvt",	no_argument,		0, OPT_BLOCKING_GVT},
	{"gvt_snapshot_cycles",	required_argument,	0, OPT_GVT_SNAPSHOT_CYCLES},
	{"simulation_time",	required_argument,	0, OPT_SIMULATION_TIME},
	{"lps_distribution",	required_argument,	0, OPT_LPS_DISTRIBUTION},
	{"deterministic_seed",	no_argument,		0, OPT_DETERMINISTIC_SEED},
	{"verbose",		required_argument,	0, OPT_VERBOSE},
	{"stats",		required_argument,	0, OPT_STATS},
	{"seed",		required_argument,	0, OPT_SEED},
	{"serial",		no_argument,		0, OPT_SERIAL},
	{"sequential",		no_argument,		0, OPT_SERIAL},
	{"no-core-binding",	no_argument,		0, OPT_NO_CORE_BINDING},

#ifdef HAVE_PREEMPTION
	{"no-preemption",	no_argument,		0, OPT_PREEMPTION},
#endif

#ifdef HAVE_PARALLEL_ALLOCATOR
	{"no-allocator",	no_argument,		0, OPT_ALLOCATOR},
#endif

	{0,			0,			0, 0}
};

static void usage(char **argv) {
	unsigned int i = 0;

	fprintf(stderr, "This is a ROOT-Sim executable. At least the number of cores and LPs must be specified.\n");
	fprintf(stderr, "Usage: %s [OPTIONS]\n\n", argv[0]);

	fprintf(stderr, "Available options:\n");
	while(long_options[i].name != NULL) {
		fprintf(stderr, "\t%s:\t%s\n", long_options[i].name,  opt_desc[long_options[i].val]);
		i++;
	}
	fflush(stderr);

	exit(EXIT_FAILURE);
}



/**
* This function reads the configuration passed at command line and sets the internal values accordingly
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param argc number of parameters passed at command line
* @param argv array of parameters passed at command line
*
* @return the index of the first application-level parameter
*/
static int parse_cmd_line(int argc, char **argv) {
	int length;
	int c;
	int option_index;

	if(argc < 2){
		usage(argv);
	}

	// Keep track of the program name
	program_name = argv[0];

	// Store the predefined values, before reading any overriding one
	rootsim_config.output_dir = DEFAULT_OUTPUT_DIR;
	rootsim_config.gvt_time_period = 1000;
	rootsim_config.scheduler = SMALLEST_TIMESTAMP_FIRST;
	rootsim_config.checkpointing = INVALID_STATE_SAVING;
	rootsim_config.ckpt_period = 10;
	rootsim_config.gvt_snapshot_cycles = 2;
	rootsim_config.simulation_time = 0;
	rootsim_config.lps_distribution = LP_DISTRIBUTION_BLOCK;
	rootsim_config.check_termination_mode = NORM_CKTRM;
	rootsim_config.blocking_gvt = false;
	rootsim_config.snapshot = FULL_SNAPSHOT;
	rootsim_config.deterministic_seed = false;
	rootsim_config.set_seed = 0;
	rootsim_config.verbose = VERBOSE_INFO;
	rootsim_config.stats = STATS_ALL;
	rootsim_config.serial = false;
	rootsim_config.core_binding = true;

	#ifdef HAVE_PREEMPTION
	rootsim_config.disable_preemption = false;
	#endif

	#ifdef HAVE_PARALLEL_ALLOCATOR
	rootsim_config.disable_allocator = false;
	#endif


	// Parse command-line options
	while(true) {

		c = getopt_long(argc, argv, "", long_options, &option_index);

		if (c == -1)
			break;

		#define parseIntLimits(s, low, high) ({\
					int period;\
					char *endptr;\
					period = strtol(s, &endptr, 10);\
					if(!(*s != '\0' && *endptr == '\0' && period >= low && period <= high)) {\
						rootsim_error(true, "Invalid option value: %s\n", s);\
					}\
					period;\
				     })

		switch (c) {

			case OPT_NP:
				n_cores = parseInt(optarg);
				// TODO: Qui si dovra' a un certo punto spostare questo controllo nel mapping LP <-> Kernel <-> Thread!!!
				if(n_cores > get_cores()) {
					rootsim_error(true, "Demanding %d cores, which are more than available (%d)\n", n_cores, get_cores());
					return -1;
				}
				if(n_cores <= 0) {
					rootsim_error(true, "Demanding a non-positive number of cores\n");
					return -1;
				}

				if(n_cores > MAX_THREADS_PER_KERNEL){
					rootsim_error(true, "Too many threads, maximum supported number is %u\n", MAX_THREADS_PER_KERNEL);
				}
				break;

			case OPT_OUTPUT_DIR:
				length = strlen(optarg);
				rootsim_config.output_dir = (char *)rsalloc(length + 1);
				strcpy(rootsim_config.output_dir, optarg);
				break;

			case OPT_SCHEDULER:
				if(strcmp(optarg, "stf") == 0) {
					rootsim_config.scheduler = SMALLEST_TIMESTAMP_FIRST;
				} else {
					rootsim_error(true, "Invalid argument for scheduler parameter\n");
					return -1;
				}
				break;

			case OPT_NPWD:
				if (rootsim_config.checkpointing == INVALID_STATE_SAVING) {
					rootsim_config.checkpointing = COPY_STATE_SAVING;
				} else {
					rootsim_error(false, "Some options are conflicting: I'm requested to run non piece-wise deterministically, but a checkpointing interval is set. Skipping the -npwd option.\n");
				}
				break;

			case OPT_P:
				if(rootsim_config.checkpointing == COPY_STATE_SAVING) {
					rootsim_error(false, "Some options are conflicting: Copy State Saving is selected, but I'm requested to set a checkpointing interval. Skipping the -p option.\n");
				} else {
					rootsim_config.checkpointing = PERIODIC_STATE_SAVING;
					rootsim_config.ckpt_period = parseIntLimits(optarg, 1, 40);
					// This is a micro optimization that makes the LogState function to avoid checking the checkpointing interval and keeping track of the logs taken
					if(rootsim_config.ckpt_period == 1) {
						rootsim_config.checkpointing = COPY_STATE_SAVING;
					}
				}
				break;

			case OPT_FULL:
				if (rootsim_config.snapshot == INVALID_SNAPSHOT) {
					rootsim_config.snapshot = FULL_SNAPSHOT;
				}
				break;

			case OPT_INC:
				rootsim_error(false, "Incremental state saving is not supported in stable version yet...\n");
				break;

			case OPT_A:
				rootsim_error(false, "Autonomic state saving is not supported in stable version yet...\n");
				break;

			case OPT_GVT:
				rootsim_config.gvt_time_period = parseIntLimits(optarg, 1, INT_MAX);
				break;

			case OPT_CKTRM_MODE:
				if(strcmp(optarg, "standard") == 0) {
					rootsim_config.check_termination_mode = NORM_CKTRM;
				} else if(strcmp(optarg, "incremental") == 0) {
					rootsim_config.check_termination_mode = INCR_CKTRM;
				} else {
					rootsim_error(true, "Invalid argument for cktrm_mode\n");
					return -1;
				}
				break;

			case OPT_NPRC:
				n_prc_tot = parseIntLimits(optarg, 1, MAX_LPs); // In this way, a change in MAX_LPs is reflected here
				break;

			case OPT_BLOCKING_GVT:
				rootsim_config.blocking_gvt = true;
				break;

			case OPT_GVT_SNAPSHOT_CYCLES:
				rootsim_config.gvt_snapshot_cycles = parseIntLimits(optarg, 1, INT_MAX);
				break;

			case OPT_SIMULATION_TIME:
				rootsim_config.simulation_time = parseIntLimits(optarg, 0, INT_MAX);
				break;

			case OPT_LPS_DISTRIBUTION:
				if(strcmp(optarg, "block") == 0) {
					rootsim_config.lps_distribution = LP_DISTRIBUTION_BLOCK;
				} else if(strcmp(optarg, "circular") == 0) {
					rootsim_config.lps_distribution = LP_DISTRIBUTION_CIRCULAR;
				} else {
					rootsim_error(true, "Invalid argument for lps_distribution\n");
					return -1;
				}
				break;

			case OPT_DETERMINISTIC_SEED:
				rootsim_config.deterministic_seed = true;
				break;

			case OPT_VERBOSE:
				if(strcmp(optarg, "info") == 0) {
					rootsim_config.verbose = VERBOSE_INFO;
				} else if(strcmp(optarg, "debug") == 0) {
					rootsim_config.verbose = VERBOSE_DEBUG;
				} else if(strcmp(optarg, "no") == 0) {
					rootsim_config.verbose = VERBOSE_NO;
				} else {
					rootsim_error(true, "Invalid argument for verbose\n");
					return -1;
				}
				break;

			case OPT_STATS:
				if(strcmp(optarg, "all") == 0) {
					rootsim_config.stats = STATS_ALL;
				} else if(strcmp(optarg, "performance") == 0) {
					rootsim_config.stats = STATS_PERF;
				} else if(strcmp(optarg, "lp") == 0) {
					rootsim_config.stats = STATS_LP;
				} else if(strcmp(optarg, "global") == 0) {
					rootsim_config.stats = STATS_GLOBAL;
				} else {
					rootsim_error(true, "Invalid argument for stats\n");
					return -1;
				}
				break;

			case OPT_SEED:
				rootsim_config.set_seed = parseInt(optarg);
				break;

			case OPT_SERIAL:
				rootsim_config.serial = true;
				break;

			case OPT_NO_CORE_BINDING:
				rootsim_config.core_binding = false;
				break;

			#ifdef HAVE_PREEMPTION
			case OPT_PREEMPTION:
				rootsim_config.disable_preemption = true;
				break;
			#endif

			#ifdef HAVE_PARALLEL_ALLOCATOR
			case OPT_ALLOCATOR:
				rootsim_config.disable_allocator = true;
				break;
			#endif

			case -1:
			case '?':
			default:
				break;
		}

		#undef parseIntLimits
	}

	if(!rootsim_config.serial && n_cores == 0){
		rootsim_error(true, "Number of cores was not provided \"--np\"\n");
	}

	if(!rootsim_config.serial && n_prc_tot < n_cores) {
		rootsim_error(true, "Requested a simulation run with %u LPs and %u worker threads: the mapping is not possible. Aborting...\n", n_prc_tot, n_cores);
	}

	if (!rootsim_config.serial && rootsim_config.snapshot == INVALID_SNAPSHOT)
		rootsim_config.snapshot = FULL_SNAPSHOT; // TODO: in the future, default to AUTONOMIC_

	if (!rootsim_config.serial && rootsim_config.checkpointing == INVALID_STATE_SAVING)
		rootsim_config.checkpointing = PERIODIC_STATE_SAVING;


	// Return the first argv element where to find app args
	return optind;

}



/**
* This function initializes the simulator
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param argc number of parameters passed at command line
* @param argv array of parameters passed at command line
*/
void SystemInit(int argc, char **argv) {
	register int w;

	#ifdef HAVE_MPI
	mpi_init(&argc, &argv);

	if(n_ker > MAX_KERNELS){
		rootsim_error(true, "Too many kernels, maximum supported number is %u\n", MAX_KERNELS);
	}
	#else
	n_ker = 1;
	#endif

	// Early initialization of ECS subsystem if needed
	#ifdef HAVE_CROSS_STATE
	ecs_init();
	#endif

	// Parse the argument passed at command line, to initialize the internal configuration
	w = parse_cmd_line(argc, argv);
	if(w == -1) {
		return;
	}

	// Create a pointer to parameters which are needed by the user-level code
	// Skip all the NULL args (if any)
	// TODO: è ancora necessario questo? Era una patch per la shell che ora è stata eliminata
	while (argv[w] != NULL && (argv[w][0] == '\0' || argv[w][0] == ' ')) {
		w++;
	}
	model_parameters.size = argc - w + sizeof(char *);
	model_parameters.arguments = &argv[w];

	// If we're going to run a serial simulation, configure the simulation to support it
	if(rootsim_config.serial) {
		SetState = SerialSetState;
		ScheduleNewEvent = SerialScheduleNewEvent;
		numerical_init();
		//dymelor_init();
		statistics_init();
		serial_init(argc, model_parameters.arguments, model_parameters.size);
		return;
	} else {
		SetState = ParallelSetState;
		ScheduleNewEvent = ParallelScheduleNewEvent;
	}


	if (master_kernel()) {

		 printf("****************************\n"
			"*  ROOT-Sim Configuration  *\n"
			"****************************\n"
			"Kernels: %u\n"
			"Cores: %ld available, %d used\n"
			"Number of Logical Processes: %u\n"
			"Output Statistics Directory: %s\n"
			"Scheduler: %d\n"
			#ifdef HAVE_MPI
			"MPI multithread support: %s\n"
			#endif
			"GVT Time Period: %.2f seconds\n"
			"Checkpointing Type: %d\n"
			"Checkpointing Period: %d\n"
			"Snapshot Reconstruction Type: %d\n"
			"Halt Simulation After: %d\n"
			"LPs Distribution Mode across Kernels: %d\n"
			"Check Termination Mode: %d\n"
			"Blocking GVT: %d\n"
			"Set Seed: %ld\n",
			n_ker,
			get_cores(),
			n_cores,
			n_prc_tot,
			rootsim_config.output_dir,
			rootsim_config.scheduler,
			#ifdef HAVE_MPI
			((mpi_support_multithread)? "yes":"no"),
			#endif
			rootsim_config.gvt_time_period / 1000.0,
			rootsim_config.checkpointing,
			rootsim_config.ckpt_period,
			rootsim_config.snapshot,
			rootsim_config.simulation_time,
			rootsim_config.lps_distribution,
			rootsim_config.check_termination_mode,
			rootsim_config.blocking_gvt,
			rootsim_config.set_seed);
	}

	distribute_lps_on_kernels();

	// Initialize ROOT-Sim subsystems.
	// All init routines are executed serially (there is no notion of threads in there)
	// and the order of invocation can matter!

	base_init();
	scheduler_init();
	statistics_init();
	dymelor_init();
	communication_init();
	gvt_init();
	numerical_init();

	// This call tells the simulation engine that the sequential initial simulation is complete
	initialization_complete();
}


