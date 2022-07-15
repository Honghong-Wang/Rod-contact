#include "timeStepper.h"

//using namespace std::chrono;


timeStepper::timeStepper(std::vector<elasticRod*> m_rod_vec)
{
	rod_vec = m_rod_vec;
	// define freeDOF
	freeDOF = 0;
	for (int i = 0 ; i < rod_vec.size(); i++)
	{
		rod = rod_vec[i];
		freeDOF = freeDOF + rod->uncons;
	}
	totalForce = new double[freeDOF];

	Jacobian = MatrixXd::Zero(freeDOF, freeDOF);
	Force = VectorXd::Zero(freeDOF);
	dx = new double[freeDOF];
	DX = VectorXd::Zero(freeDOF);
}

timeStepper::~timeStepper()
{
	;
}

double* timeStepper::getForce()
{
	return totalForce;
}

double* timeStepper::getJacobian()
{
	return jacobian;
}

void timeStepper::addForce(int ind, double p, int idx)
{
	rod = rod_vec[idx];
	int offset = idx * rod->uncons;
	if (rod->getIfConstrained(ind) == 0) // free dof
	{
		mappedInd = rod->fullToUnconsMap[ind];
		// totalForce[mappedInd] = totalForce[mappedInd] + p; // subtracting elastic force

		Force[mappedInd + offset] = Force[mappedInd + offset] + p;

	}
	force[idx*rod->ndof+ind] = force[idx*rod->ndof+ind] + p;
}

void timeStepper::addJacobian(int ind1, int ind2, double p, int idx)
{
	rod = rod_vec[idx];
	int offset = idx * rod->uncons;
	mappedInd1 = rod->fullToUnconsMap[ind1];
	mappedInd2 = rod->fullToUnconsMap[ind2];
	if (rod->getIfConstrained(ind1) == 0 && rod->getIfConstrained(ind2) == 0) // both are free
	{
		Jacobian(mappedInd2 + offset, mappedInd1 + offset) = Jacobian(mappedInd2 + offset, mappedInd1 + offset) + p;
	}
}

void timeStepper::addJacobian(int ind1, int ind2, double p, int idx1, int idx2) // col, row, value, col_offset, row_offset
{
	rod = rod_vec[idx1];
	rod1 = rod_vec[idx2];
	int offset_col = idx1 * rod->uncons;
	int offset_row = idx2 * rod->uncons;
	mappedInd1 = rod->fullToUnconsMap[ind1];
	mappedInd2 = rod1->fullToUnconsMap[ind2];
	if (rod->getIfConstrained(ind1) == 0 && rod->getIfConstrained(ind2) == 0) // both are free
	{
		Jacobian(mappedInd2 + offset_row, mappedInd1 + offset_col) = Jacobian(mappedInd2 + offset_row, mappedInd1 + offset_col) + p;
	}
}

void timeStepper::setZero()
{
	Force = VectorXd::Zero(freeDOF);
	Jacobian = MatrixXd::Zero(freeDOF, freeDOF);
	force = VectorXd::Zero(rod_vec.size() * rod_vec[0]->ndof);
}

void timeStepper::pardisoSolver()
{
	int n = freeDOF;
	int ia[n+1];
	ia[0] = 1;

	int temp = 0;
	for (int i =0; i < n; i++)
	{
		for (int j = 0; j < n; j++)
		{
			if (Jacobian(i,j) != 0)
			{
				temp = temp + 1;
			}
		}
		ia[i+1] = temp+1;
	}

	int ja[ia[n]];
	double a[ia[n]];
	temp = 0;

	for (int i = 0; i < n; i++)
	{
		for (int j = 0; j < n; j++)
		{
			if (Jacobian(i,j) != 0)
			{
				ja[temp] = j + 1;
				a[temp] = Jacobian(i,j);
				temp = temp + 1;
			}
		}
	}
	MKL_INT mtype = 11;       /* Real unsymmetric matrix */
  // Descriptor of main sparse matrix properties
	double b[n], x[n], bs[n], res, res0;
	MKL_INT nrhs = 1;     /* Number of right hand sides. */
	/* Internal solver memory pointer pt, */
	/* 32-bit: int pt[64]; 64-bit: long int pt[64] */
	/* or void *pt[64] should be OK on both architectures */
	void *pt[64];
	/* Pardiso control parameters. */
	MKL_INT iparm[64];
	MKL_INT maxfct, mnum, phase, error, msglvl;
	/* Auxiliary variables. */
	MKL_INT i, j;
	double ddum;          /* Double dummy */
	MKL_INT idum;         /* Integer dummy. */
/* -------------------------------------------------------------------- */
/* .. Setup Pardiso control parameters. */
/* -------------------------------------------------------------------- */
	for ( i = 0; i < 64; i++ )
	{
			iparm[i] = 0;
	}
	iparm[0] = 0;         /* No solver default */
	iparm[1] = 2;         /* Fill-in reordering from METIS */
	iparm[3] = 0;         /* No iterative-direct algorithm */
	iparm[4] = 0;         /* No user fill-in reducing permutation */
	iparm[5] = 0;         /* Write solution into x */
	iparm[6] = 0;         /* Not in use */
	iparm[7] = 2;         /* Max numbers of iterative refinement steps */
	iparm[8] = 0;         /* Not in use */
	iparm[9] = 13;        /* Perturb the pivot elements with 1E-13 */
	iparm[10] = 1;        /* Use nonsymmetric permutation and scaling MPS */
	iparm[11] = 0;        /* Conjugate transposed/transpose solve */
	iparm[12] = 1;        /* Maximum weighted matching algorithm is switched-on (default for non-symmetric) */
	iparm[13] = 0;        /* Output: Number of perturbed pivots */
	iparm[14] = 0;        /* Not in use */
	iparm[15] = 0;        /* Not in use */
	iparm[16] = 0;        /* Not in use */
	iparm[17] = -1;       /* Output: Number of nonzeros in the factor LU */
	iparm[18] = -1;       /* Output: Mflops for LU factorization */
	iparm[19] = 0;        /* Output: Numbers of CG Iterations */


	maxfct = 1;           /* Maximum number of numerical factorizations. */
	mnum = 1;         /* Which factorization to use. */
	msglvl = 0;           /* Print statistical information  */
	error = 0;            /* Initialize error flag */
/* -------------------------------------------------------------------- */
/* .. Initialize the internal solver memory pointer. This is only */
/* necessary for the FIRST call of the PARDISO solver. */
/* -------------------------------------------------------------------- */
	for ( i = 0; i < 64; i++ )
	{
			pt[i] = 0;
	}
/* -------------------------------------------------------------------- */
/* .. Reordering and Symbolic Factorization. This step also allocates */
/* all memory that is necessary for the factorization. */
/* -------------------------------------------------------------------- */
	phase = 11;
	PARDISO (pt, &maxfct, &mnum, &mtype, &phase,
					 &n, a, ia, ja, &idum, &nrhs, iparm, &msglvl, &ddum, &ddum, &error);
	if ( error != 0 )
	{
			printf ("\nERROR during symbolic factorization: " IFORMAT, error);
			exit (1);
	}
	// printf ("\nReordering completed ... ");
	// printf ("\nNumber of nonzeros in factors = " IFORMAT, iparm[17]);
	// printf ("\nNumber of factorization MFLOPS = " IFORMAT, iparm[18]);
/* -------------------------------------------------------------------- */
/* .. Numerical factorization. */
/* -------------------------------------------------------------------- */
	phase = 22;
	PARDISO (pt, &maxfct, &mnum, &mtype, &phase,
					 &n, a, ia, ja, &idum, &nrhs, iparm, &msglvl, &ddum, &ddum, &error);
	if ( error != 0 )
	{
			printf ("\nERROR during numerical factorization: " IFORMAT, error);
			exit (2);
	}
	// printf ("\nFactorization completed ... ");
/* -------------------------------------------------------------------- */
/* .. Back substitution and iterative refinement. */
/* -------------------------------------------------------------------- */
	phase = 33;

// descrA.type = SPARSE_MATRIX_TYPE_GENERAL;
// descrA.mode = SPARSE_FILL_MODE_UPPER;
// descrA.diag = SPARSE_DIAG_NON_UNIT;
// mkl_sparse_d_create_csr ( &csrA, SPARSE_INDEX_BASE_ONE, n, n, ia, ia+1, ja, a );

	/* Set right hand side to one. */
	for ( i = 0; i < n; i++ )
	{
			b[i] = Force[i];
	}
//  Loop over 3 solving steps: Ax=b, AHx=b and ATx=b
			PARDISO (pt, &maxfct, &mnum, &mtype, &phase,
							 &n, a, ia, ja, &idum, &nrhs, iparm, &msglvl, b, x, &error);
			if ( error != 0 )
			{
					printf ("\nERROR during solution: " IFORMAT, error);
					exit (3);
			}

			// printf ("\nThe solution of the system is: ");
			// for ( j = 0; j < n; j++ )
			// {
			//     printf ("\n x [" IFORMAT "] = % f", j, x[j]);
			// }
			// printf ("\n");

/* -------------------------------------------------------------------- */
/* .. Termination and release of memory. */
/* -------------------------------------------------------------------- */
	phase = -1;           /* Release internal memory. */
	PARDISO (pt, &maxfct, &mnum, &mtype, &phase,
					 &n, &ddum, ia, ja, &idum, &nrhs,
					 iparm, &msglvl, &ddum, &ddum, &error);

	for (int i = 0; i < n; i++)
	{
		dx[i] = x[i];
		DX[i] = x[i];
	}

	// auto stop = high_resolution_clock::now();
	// auto duration = duration_cast<microseconds>(stop - start);
	//
	// cout << "Time taken by function: "
	// 		 << duration.count() << " microseconds" << endl;

}

void timeStepper::integrator()
{
	pardisoSolver();
}
