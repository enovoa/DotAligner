/*
 * nwdp.cpp
 *
 *  Created on: Oct 28, 2013
 *      Author: Stefan Seemann, seemann@rth.dk
 */

#include "nwdp.h"
#include <map>

using namespace std;


void usage_dotaligner(char *program)
{
	cerr << "\nDotAligner v0.2";
	cerr << "\n===============";
	cerr << "\n   by Stefan E Seemann (seemann@rth.dk)\n";
	cerr << "\n   Usage:   " << program << " -d <file> -d <file> [ options ]";
    cerr << "\n   Semi-local pairwise alignment of two base pair probability matrices (dotplots).\n";
    cerr << "\n   -d --dotplot <file>   ... dotplot file (matrix of base pair probabilities)";
	cerr << "\n   --seq                 ... calculate sequence alignment\n";
    cerr << "\n   Debug modes:";
	cerr << "\n   --printdp             ... print dynamic programming matrices";
    cerr << "\n   --logodds             ... replace probabilities by expectation normalized log odds";
    cerr << "\n   --local1              ... local alignment in step 1 (default is global in step 1)";
    cerr << "\n   --global2             ... global alignment in step 2 (default is local alignment in step 2)";
    cerr << "\n   -k --kappa <float>    ... weight of sequence similarity (0..1; default = 0.5); 1 - kappa is weight of dotplots";
    cerr << "\n   -a --alpha <float>    ... affine gap costs = alpha + k * beta (k gaps; alpha default = -4)";
    cerr << "\n                             set <alpha> equal 0 to make the score becomes larger as a linear function of gap length";
    cerr << "\n   -b --beta <float>     ... affine gap costs = alpha + k * beta (k gaps; beta default = -1)";
    cerr << "\n                             set <beta> equal 0 to keep the score similar regardless of gap length";
    cerr << "\n   -p --precision <int>  ... number of digits considered of log-odds of base pair reliabilities (default = 4)";
    cerr << "\n   -m --maxshift <int>   ... maximal shift of two input sequences in the final alignment (default 50% of longer sequence)";
    cerr << "\n                             speeds up the calculation by ignoring pairwise comparisons of distant nucleotides";
    cerr << "\n                             local alignments may be missed for long sequences if <maxshift> is set too low";
    cerr << "\n   -s --seednr <int>     ... number of seed alignments (default = 3)";
    cerr << "\n   -l --seedlen <int>    ... length of seed alignments (default = 5 nucleotides)\n";
    cerr << "\n   -n --pnull <prob>     ... Minimal probability (default = 0.0005 )\n";
	cerr << "\n   --help                ... this output\n";
    cerr << "\n   Output:   similarity and alignment\n\n";

    exit( 1 ) ;
}


int readinput ( istream & is, string & name, string & seq, vector<float> & prob )
{
	// read probability matrix
	string line;
	int lnr = 0;
	int i = -1;

	while( !is.eof() ) {
		getline(is, line);

		if( i == -1 ) {
			name = line.replace(0,1,"");
			//prob = (float) realloc (prob, lnr * lnr * sizeof(float));
		}
		else {
			if( i == 0 ) {
				seq = line;
				lnr = line.length();
				prob.reserve( lnr * lnr );
			}
			else {
				vector<string> probv = split(line, ' ');
				//int offset = (i-1)*lnr;
				for( unsigned int k = 0; k < probv.size(); k++ )
				{
					//prob[offset + k] = atof( probv[k].c_str() );
					prob.push_back( atof( probv[k].c_str() ) );
					//cout << offset+k << " " << prob[offset + k] << endl;
				}
			}
		}

		if( i == lnr )
			break;
		i++;
	}

	return lnr;
}


void getunpaired( vector<float> & prob, int len, vector<float> & probSgl )
{
	float psingle;

	probSgl.reserve( len );

	for( int i = 0; i < len; i++ )
	{
		psingle = 1;
		for( int j = 0; j < len; j++ )
		{
			psingle -= prob.at( i*len + j );
		}
		probSgl.push_back( psingle );
	}
}


vector<string> &split(const string &s, char delim, vector<string> &elems)
{
    stringstream ss(s);
    string item;
    while(getline(ss, item, delim))
    {
        elems.push_back(item);
    }
    return elems;
}


vector<string> split(const string &s, char delim)
{
    vector<string> elems;
    return split(s, delim, elems);
}


void reducematrix(vector<float> & prob, int len, int precision)
{
	 char p[100];
     for( int i = 0; i < len; i++ ) {
    	 	sprintf(p, "%.*lf", precision, prob[i]);
    	 	prob.at(i) = atof(p);
     }
}


/**
   alignment of the pairing probabilities of two reference nucleotides (one line per dot plot)

   \param[seq_1]  sequence 1 ( S_a )
   \param[probDbl_1]  pairing probabilities of S_a (dot plot)
   \param[probSgl_1]  unpaired probabilities of S_a
   \param[idx_1]  index of reference nucleotide of S_a
   \param[idx_1_aln]  indices of nucleotides in the subsequence of S_a that is considered for alignment ( SS_a )
   \param[len_1] length of considered subsequence SS_a
   \param[seq_2]  sequence 2 ( S_b )
   \param[probDbl_2]  pairing probabilities of S_b (dot plot)
   \param[probSgl_2]  unpaired probabilities of S_b
   \param[idx_2]  index of reference nucleotide of S_b
   \param[idx_2_aln]  indices of nucleotides in the subsequence of S_b that is considered for alignment ( SS_b )
   \param[len_2] length of considered subsequence SS_b
   \param[local]  boolean if local alignment (set to 1) or global alignment (set to 0); if local it holds length of local alignment
   \param[prm]  boolean if dynamic programming matrices are printed

   \returns  Similarity score ( 0 .. 1 )
*/
float nwdp( string seq_1, vector<float> & probDbl_1, vector<float> & probSgl_1, int idx_1, int * idx_1_aln, int len_1, string seq_2, vector<float> & probDbl_2, vector<float> & probSgl_2, int idx_2, int * idx_2_aln, int len_2, int & local, bool prm )
{
    float sim;
    float* subprob_1 = new float[len_1];
    float* subprob_2 = new float[len_2];
    string subseq_1, subseq_2;

    int sidx_1 = idx_1 * seq_1.length();
    int sidx_2 = idx_2 * seq_2.length();

    // get basepair probabilities that should be aligned
    for( int i = 0; i < len_1; i++ )
    	subprob_1[ i ] = probDbl_1.at( sidx_1 + idx_1_aln[ i ] );
    for( int j = 0; j < len_2; j++ )
    	subprob_2[ j ] = probDbl_2.at( sidx_2 + idx_2_aln[ j ] );
    // get subsequence that should be aligned
    for( int i = 0; i < len_1; i++ )
    	subseq_1 += seq_1.at( idx_1_aln[ i ] );
    for( int j = 0; j < len_2; j++ )
    	subseq_2 += seq_2.at( idx_2_aln[ j ] );

    /* create global alignment */
    if( !local )
    {
    	//sim = nwdb_global_align_affinegaps( seq_1.c_str()[idx_1], probSgl_1.at(idx_1), subseq_1, subprob_1, len_1, seq_2.c_str()[idx_2], probSgl_2.at(idx_2), subseq_2, subprob_2, len_2 );
    	float tau = 0;
    	/* alignment upstream of the constraint pair ( idx_1, idx_2 ) */
    	if( idx_1>0 && idx_2>0 )
    	{
    		float* subprob_1u = new float[idx_1];
    		float* subprob_2u = new float[idx_2];
    		//cerr << "SP1U " << endl;
    		for( int i = 0; i < idx_1; i++ ) {
    			subprob_1u[ i ] = probDbl_1.at( sidx_1 + idx_1_aln[ i ] );
    			//cerr << subprob_1u[ i ] << " ";
    		}
    		//cerr << endl;
    		//cerr << "SP2U " << endl;
    		for( int j = 0; j < idx_2; j++ ) {
    			subprob_2u[ j ] = probDbl_2.at( sidx_2 + idx_2_aln[ j ] );
    			//cerr << subprob_2u[ j ] << " ";
    		}
    		//cerr << endl;
    		/*string subseq_1u;
    		for( int i = 0; i < idx_1; i++ )
    			subseq_1u += seq_1.at( idx_1_aln[ i ] );
    		string subseq_2u;
    		for( int j = 0; j < idx_2; j++ )
    			subseq_2u += seq_2.at( idx_2_aln[ j ] );*/
    		tau = nwdb_global_align_affinegaps( subprob_1u, idx_1, subprob_2u, idx_2 );
    		//cerr << "TAU1 " << tau << endl;
    		//tau = nwdb_global_align_affinegaps( seq_1.c_str()[idx_1], probSgl_1.at(idx_1), subseq_1u, subprob_1u, idx_1, seq_2.c_str()[idx_2], probSgl_2.at(idx_2), subseq_2u, subprob_2u, idx_2 );
           	delete[] subprob_1u;
           	delete[] subprob_2u;
    	}
    	/* alignment downstream of the constraint pair ( idx_1, idx_2 ) */
    	if( idx_1<len_1-1 && idx_2<len_2-1 )
    	{
			float* subprob_1d = new float[len_1-idx_1-1];
			float* subprob_2d = new float[len_2-idx_2-1];
    		//cerr << "SP1D " << endl;
			for( int i = 0; i < len_1-idx_1-1; i++ ) {
				subprob_1d[ i ] = probDbl_1.at( sidx_1 + idx_1_aln[ idx_1+i+1 ] );
				//cerr << subprob_1d[ i ] << " ";
			}
			//cerr << endl;
    		//cerr << "SP2D " << endl;
			for( int j = 0; j < len_2-idx_2-1; j++ ) {
				subprob_2d[ j ] = probDbl_2.at( sidx_2 + idx_2_aln[ idx_2+j+1 ] );
    			//cerr << subprob_2d[ j ] << " ";
    		}
    		//cerr << endl;
			/*string subseq_1d;
			for( int i = 0; i < len_1-idx_1-1; i++ )
				subseq_1d += seq_1.at( idx_1_aln[ idx_1+i+1 ] );
			string subseq_2d;
			for( int j = 0; j < len_2-idx_2-1; j++ )
				subseq_2d += seq_2.at( idx_2_aln[ idx_2+j+1 ] );*/
			//cerr << "TAU2 " << nwdb_global_align_affinegaps( subprob_1d, len_1-idx_1-1, subprob_2d, len_2-idx_2-1 ) << endl;
			tau += nwdb_global_align_affinegaps( subprob_1d, len_1-idx_1-1, subprob_2d, len_2-idx_2-1 );
			//tau += nwdb_global_align_affinegaps( seq_1.c_str()[idx_1], probSgl_1.at(idx_1), subseq_1d, subprob_1d, len_1-idx_1-1, seq_2.c_str()[idx_2], probSgl_2.at(idx_2), subseq_2d, subprob_2d, len_2-idx_2-1 );
	       	delete[] subprob_1d;
	       	delete[] subprob_2d;
    	}
        // length of shorter sequence
        int minlen = ( len_2 > len_1 ) ? len_1 : len_2;
    	float sigma = nwdb_align_seq_sim( seq_1.c_str()[idx_1], probSgl_1.at(idx_1), seq_2.c_str()[idx_2], probSgl_2.at(idx_2) );
    	sim = kappa * sigma + ( 1 - kappa ) * tau / ( minlen - 1 );
    	cerr << idx_1 << " " << idx_2 << " " << sigma << " " << tau / ( minlen - 1 ) << " " << sim << endl;
    }
    /* create local alignment */
    else
    {
    	/* initialize arrays of indices */
    	int len_aln;
    	int *idx_1_aln_local = new int[len_1];
    	int *idx_2_aln_local = new int[len_2];
    	sim = nwdb_local_align_affinegaps( seq_1.c_str()[idx_1], probSgl_1.at(idx_1), subseq_1, subprob_1, len_1, seq_2.c_str()[idx_2], probSgl_2.at(idx_2), subseq_2, subprob_2, len_2, idx_1_aln_local, idx_2_aln_local, len_aln );
    	/* convert idx_[12]_aln_local to idx_[12]_aln */
    	local = len_aln;
    	for( int i=0; i<len_aln; i++ ) idx_1_aln[ i ] = idx_1_aln_local[ i ];
    	for( int i=0; i<len_aln; i++ ) idx_2_aln[ i ] = idx_2_aln_local[ i ];

    	delete[] idx_1_aln_local;
    	delete[] idx_2_aln_local;
    }
    //cerr << seq_1.c_str()[idx_1] << " " << subseq_1 << " " << seq_2.c_str()[idx_2] << " " << subseq_2 << " " << sim << endl;

   	delete[] subprob_1;
   	delete[] subprob_2;

    return ( sim > 0 ) ? sim : 0;
}


/*
 * initialize matrix of costs for alignment of prefixes (a_1 ... a_i; b_1 ... b_j)
 */
void nwdp_initF( float ** F, int L1, int L2 )
{
	F[ 0 ][ 0 ] =  0. ;

	for( int j = 1; j <= L2; j++ )
		for( int i = 1; i <= L1; i++ )
			F[ j ][ i ] = INFINITE;

    for( int i = 1; i <= L1; i++ )
     	F[ 0 ][ i ] =  i * alpha;
    for( int j = 1; j <= L2; j++ )
      	F[ j ][ 0 ] =  j * alpha;
}


/*
 * initialize matrix of costs for alignment of prefixes (a_1 ... a_i; b_1 ... b_j)
 * for affine gap costs
 */
void nwdp_initF_affinegaps( float ** F, int L1, int L2, bool local )
{
	F[ 0 ][ 0 ] =  0. ;

	for( int j = 1; j <= L2; j++ )
		for( int i = 1; i <= L1; i++ )
			F[ j ][ i ] = INFINITE;

	for( int i = 1; i <= L1; i++ )
		F[ 0 ][ i ] = ( !local ) ? alpha + i * beta : 0;
		//F[ 0 ][ i ] = alpha + i * beta;
	for( int j = 1; j <= L2; j++ )
		F[ j ][ 0 ] = ( !local ) ? alpha + j * beta : 0;
		//F[ j ][ 0 ] = alpha + j * beta;
}


/*
 * initialize matrix of costs for alignment of prefixes (a_1 ... a_i; b_1 ... b_j)
 * that ends with a gap
 */
void nwdp_initGap( float ** Q, int L1, int L2 )
{
	Q[ 0 ][ 0 ] =  0. ;

    for( int i = 1; i <= L1; i++ )
     	Q[ 0 ][ i ] =  INFINITE;
    for( int j = 1; j <= L2; j++ )
      	Q[ j ][ 0 ] =  INFINITE;
}


void nwdp_initTB( char ** traceback, int L1, int L2 )
//void nwdp_initTB( vector< vector<char> > traceback, int L1, int L2 )
{
    traceback[ 0 ][ 0 ] = 'n' ;

    for( int i = 1; i <= L1; i++ )
    	traceback[ 0 ][ i ] =  '-' ;
    for( int j = 1; j <= L2 ; j++ )
    	traceback[ j ][ 0 ] =  '|' ;
}


float nwdb_align( char nuc_1, float probSgl_1, string seq_1, float* probDbl_1, int L1, char nuc_2, float probSgl_2, string seq_2, float* probDbl_2, int L2 )
{
    float fU, fD, fL;
    float pnullij, psi_1, psi_2, delta, tau, sigma, omega_1, omega_2;
    char ptr;

    // Initialize dynamic programming matrix F (fill in first row and column)
    float ** F = new float * [ L2 + 1  ];
    for( int j = 0; j <= L2; j++ )
    	F[ j ] = new float [ L1 + 1 ];
    nwdp_initF( F, L1, L2 );

    // base pair similarity of bases pairing with nuc_1 and bases pairing with nuc_2
    for( int j = 1; j <= L2; j++ )
    {
            for( int i = 1; i <= L1; i++ )
            {
            		// normalized log odds of base pair probabilities weighted by paired probabilities
            		pnullij = pnullDbl[ 4*nucIdx.at(nuc_1) + nucIdx.at(seq_1.at(i-1)) ];
            		//psi_1 = log( probDbl_1[ i-1 ] / pnullij ) / log( 1 / pnullij );
                    psi_1 = ( probDbl_1[ i-1 ] == 0 ) ? INFINITE : log( probDbl_1[ i-1 ] / pnullij ) / log( 1 / pnullij );
               		//psi_1 = probDbl_1[ i-1 ] / pnullij;
            		pnullij = pnullDbl[ 4*nucIdx.at(nuc_2) + nucIdx.at(seq_2.at(j-1)) ];
            		//psi_2 = log( probDbl_2[ j-1 ] / pnullij ) / log( 1 / pnullij );
            		psi_2 = ( probDbl_2[ j-1 ] == 0 ) ? INFINITE : log( probDbl_2[ j-1 ] / pnullij ) / log( 1 / pnullij );
            		//psi_2 = probDbl_2[ j-1 ] / pnullij;
            		// tau = 1 - 2 * | P_a - P_b | -> match=1, mismatch=-1
            		delta = abs( psi_1 - psi_2 );
                    tau = delta / 2;

                    fU = F[ j-1 ][ i ] + alpha;
                    fD = F[ j-1 ][ i-1 ] + tau;
                    fL = F[ j ][ i-1 ] + alpha;

                    F[ j ][ i ] = max3( fU, fD, fL, &ptr );
             }
    }
    // sequence similarity of nuc_1 and nuc_2
    omega_1 = log( probSgl_1 / pnullSgl[ nucIdx.at(nuc_1) ] ) / log( 1 / pnullSgl[ nucIdx.at(nuc_1) ] );
    //omega_1 = probSgl_1 / pnullSgl[ nucIdx.at(nuc_1) ];
    omega_2 = log( probSgl_2 / pnullSgl[ nucIdx.at(nuc_2) ] ) / log( 1 / pnullSgl[ nucIdx.at(nuc_2) ] );
    //omega_2 = probSgl_2 / pnullSgl[ nucIdx.at(nuc_2) ];
    delta = abs( omega_1 - omega_2 );
    sigma = ( nuc_1 == nuc_2 ) ? delta / 2 : 0;

    float similarity = kappa * sigma + ( 1 - kappa ) * F[ L2 ][ L1 ];

    // free memory
    for( int j = 0; j <= L2; j++ )  delete[] F[ j ];
  	delete[] F;

    return similarity;
}


float nwdb_global_align_affinegaps( float* probDbl_1, int L1, float* probDbl_2, int L2 )
{
    float tau;
    char ptr;
    float ** F, ** Q, ** P;

    // opening gap penalty
    float gapopen = alpha + beta;

    // Initialize dynamic programming matrix F (fill in first row and column)
    F = new float * [ L2 + 1  ];
    for( int j = 0; j <= L2; j++ )
    	F[ j ] = new float [ L1 + 1 ];
    nwdp_initF_affinegaps( F, L1, L2, 0 );

    // Initialize Q and P matrices for cost of alignments that end with a gap
    Q = new float * [ L2 + 1  ];
    for( int j = 0; j <= L2; j++ )
    	Q[ j ] = new float [ L1 + 1 ];
    nwdp_initGap( Q, L1, L2 );
    P = new float * [ L2 + 1  ];
    for( int j = 0; j <= L2; j++ )
      	P[ j ] = new float [ L1 + 1 ];
   	nwdp_initGap( P, L1, L2 );

   	// base pair similarity of bases pairing with nuc_1 and bases pairing with nuc_2
    for( int j = 1; j <= L2; j++ )
    {
    	for( int i = 1; i <= L1; i++ )
        {
    		// calculate P
    		P[ j ][ i ] = max3( P[ j-1 ][ i ] + beta, F[ j-1 ][ i ] + gapopen, INFINITE, &ptr );

    		// calculate Q
    		Q[ j ][ i ] = max3( INFINITE, F[ j ][ i-1 ] + gapopen, Q[ j ][ i-1 ] + beta, &ptr );

            // normalized log odds of base pair probabilities weighted by paired probabilities
            //delta = abs( probDbl_1[ i-1 ] - probDbl_2[ j-1 ] );
            ////cerr << i << " " << j << " " << nuc_1 << " " << nuc_2 << " " << probDbl_1[ i-1 ] << " " << probDbl_2[ j-1 ] << endl;
            //tau = ( delta < 1 ) ? 1 - delta : 0;
            tau = ( probDbl_1[ i-1 ]==0 || probDbl_2[ j-1 ]==0 ) ? 0 : 1 - abs( probDbl_1[ i-1 ] - probDbl_2[ j-1 ] );
            //cerr << i << " " << j << " " << probDbl_1[ i-1 ] << " " << probDbl_2[ j-1 ] << " " << tau << endl;
            //cerr << "DELTA " << delta << endl;
            //cerr << "TAU " << probDbl_1[ i-1 ] << " " << psi_1 << " " << probDbl_2[ j-1 ] << " " << psi_2 << " " << tau << endl;

            // calculate F
            F[ j ][ i ] = max3( P[ j ][ i ], F[ j-1 ][ i-1 ] + tau, Q[ j ][ i ], &ptr );
        }
    }
	float score = F[ L2 ][ L1 ];

    // free memory
    for( int j = 0; j <= L2; j++ )  delete[] F[ j ];
  	delete[] F;
    for( int j = 0; j <= L2; j++ )  delete[] Q[ j ];
    delete[] Q;
    for( int j = 0; j <= L2; j++ )  delete[] P[ j ];
    delete[] P;

    return score;
}


float nwdb_align_seq_sim( char nuc_1, float probSgl_1, char nuc_2, float probSgl_2 )
{
	// sequence similarity of nuc_1 and nuc_2
	float sigma = 0;
	if( nuc_1 == nuc_2 )
	{
		//delta = abs( probSgl_1 - probSgl_2 );
		//cerr << nuc_1 << " " << probSgl_1 << " " << nuc_2 << " " << probSgl_2 << endl;
		//sigma = ( delta < 1 ) ? 1 - delta : 0;
		sigma = ( probSgl_1==0 || probSgl_2==0 ) ? 0 : 1 - abs( probSgl_1 - probSgl_2 );
	}
	return sigma;
}


float nwdb_local_align_affinegaps( char nuc_1, float probSgl_1, string seq_1, float* probDbl_1, int L1, char nuc_2, float probSgl_2, string seq_2, float* probDbl_2, int L2, int * idx_1_local_aln, int * idx_2_local_aln, int & len_aln  )
{
    float tau;
    char ptr;
    float ** F, ** Q, ** P;
    int i = 0, j = 0;

    // opening gap penalty
    float gapopen = alpha + beta;

    /* Initialize dynamic programming matrix F (fill in first row and column) */
    F = new float * [ L2 + 1  ];
    for( j = 0; j <= L2; j++ )
    	F[ j ] = new float [ L1 + 1 ];
    nwdp_initF_affinegaps( F, L1, L2, 1 );

    /* Initialize Q and P matrices for cost of alignments that end with a gap */
    Q = new float * [ L2 + 1  ];
    for( j = 0; j <= L2; j++ )
    	Q[ j ] = new float [ L1 + 1 ];
    nwdp_initGap( Q, L1, L2 );
    P = new float * [ L2 + 1  ];
    for( j = 0; j <= L2; j++ )
      	P[ j ] = new float [ L1 + 1 ];
   	nwdp_initGap( P, L1, L2 );

    /* Traceback matrices */
    char ** trF = new char * [ L2+1 ];
    for( j = 0; j <= L2; j++ )
    	trF[ j ] = new char [ L1+1 ];
    nwdp_initTB( trF, L1, L2 );
    char ** trQ = new char * [ L2+1 ];
    for( j = 0; j <= L2; j++ )
    	trQ[ j ] = new char [ L1+1 ];
    nwdp_initTB( trQ, L1, L2 );
    char ** trP = new char * [ L2+1 ];
    for( j = 0; j <= L2; j++ )
    	trP[ j ] = new char [ L1+1 ];
    nwdp_initTB( trP, L1, L2 );

    /* base pair similarity of bases pairing with nuc_1 and bases pairing with nuc_2 */
    for( int j = 1; j <= L2; j++ )
    {
    	for( int i = 1; i <= L1; i++ )
        {
    		// calculate P
    		P[ j ][ i ] = max3( P[ j-1 ][ i ] + beta, F[ j-1 ][ i ] + gapopen, INFINITE, &ptr );
            trP[ j ][ i ] =  ptr;

    		// calculate Q
    		Q[ j ][ i ] = max3( INFINITE, F[ j ][ i-1 ] + gapopen, Q[ j ][ i-1 ] + beta, &ptr );
            trQ[ j ][ i ] =  ptr;

            // normalized log odds of base pair probabilities weighted by paired probabilities
            //delta = abs( probDbl_1[ i-1 ] - probDbl_2[ j-1 ] );
            //tau = ( delta < 1 ) ? 1 - delta : 0;
            tau = ( !probDbl_1[ i-1 ] && !probDbl_2[ j-1 ] ) ? 0 : 0.5 - abs( probDbl_1[ i-1 ] - probDbl_2[ j-1 ] );
            //cerr << "DELTA " << delta << endl;
            //cerr << "TAU " << probDbl_1[ i-1 ] << " " << nuc_1 << " " << probDbl_2[ j-1 ] << " " << nuc_2 << " " << tau << endl;

            // calculate F
            F[ j ][ i ] = max3( P[ j ][ i ], F[ j-1 ][ i-1 ] + tau, Q[ j ][ i ], &ptr );
            if( F[ j ][ i ] < 0 )
            	F[ j ][ i ] = 0;
            trF[ j ][ i ] =  ptr;
        }
    }
    float similarity = F[ L2 ][ L1 ];
    i--; j--;

    /* find maximal entry of similarity in matrix F, P or Q for local alignments */
    int maxmat = 0;  // 0 .. F; 1 .. P; 2 .. Q
    float lmax = 0;
    int li = 0;
    int lj = 0;
    for( j = 1; j <= L2; j++ )
       	for( i = 1; i <= L1; i++ ) {
       		if( F[ j ][ i ] >= lmax ) {
       			lmax = F[ j ][ i ];
       			li = i;
       			lj = j;
       			maxmat = 0;
       		}
       		if( P[ j ][ i ] >= lmax ) {
       			lmax = P[ j ][ i ];
       			li = i;
       			lj = j;
       			maxmat = 1;
       		}
       		if( Q[ j ][ i ] >= lmax ) {
       			lmax = Q[ j ][ i ];
       			li = i;
       			lj = j;
       			maxmat = 2;
       		}
      	}
    i = li;
    j = lj;
    similarity = lmax;
    //cerr << "LSCORE = " << similarity << endl;
    //cerr << i << " " << j << " " << maxmat << endl;

    /* backtracking */
    int k = 0;
    while( i > 0 || j > 0 )
    {
    	if( (maxmat == 0 && F[ j ][ i ] == 0) || (maxmat == 1 && P[ j ][ i ] == 0) || (maxmat == 2 && Q[ j ][ i ] == 0) )
    		break;

    	switch( maxmat )
    	{
    		case 0 : //cerr << "BT = " << i << " " << j << " " << maxmat << " " << trF[ j ][ i ] << " " << F[ j ][ i ] << endl;
    				 switch( trF[ j ][ i ] )
    				 {
    					case '|' : maxmat = 1;
        			 	 	 	 	break;
        			 	 case '\\': idx_1_local_aln[ k ] = i-1;
        			 	 	 	 	idx_2_local_aln[ k++ ] = j-1;
        			 	 	 	 	i--; j--;
        			 	 	 	 	break ;
        			 	 case '-' : maxmat = 2;
        			 	 	 	 	break;
    				 }
    				 break;
    		 case 1: //cerr << "BT = " << i << " " << j << " " << maxmat << " " << trP[ j ][ i ] << " " << P[ j ][ i ] << endl;
    			 	 switch( trP[ j ][ i ] )
    				 {
    		 	 	 	 case '|' : j--;
    		 	 	 	 	 	 	break;
    		 	 	 	 case '\\': maxmat = 0;
    		 	 	 	 	 	 	j--;
    		 	 	 	 	 	 	break;
    				 }
    				 break;
    		 case 2: //cerr << "BT = " << i << " " << j << " " << maxmat << " " << trQ[ j ][ i ] << " " << Q[ j ][ i ] << endl;
    		 	 	 switch( trQ[ j ][ i ] )
    		 	 	 {
 	 	 	 	 	 	 case '\\': maxmat = 0;
 	 	 	 	 	 	 	 	 	i--;
 	 	 	 	 	 	 	 	 	break;
 	 	 	 	 	 	 case '-' : i--;
 	 	 	 	 	 	 	 	 	break;
    		 	 	 }
    		 	 	 break;
    	}
    }

    len_aln = k;
    reverse( idx_1_local_aln, len_aln );
    reverse( idx_2_local_aln, len_aln );

    /* sequence similarity of nuc_1 and nuc_2 */
    float sigma = 0;
    //if( kappa && nuc_1 == nuc_2 )
    if( kappa )
    {
    	//delta = abs( probSgl_1 - probSgl_2 );
    	//sigma = ( delta < 1 ) ? 1 - delta : 0;
    	//sigma -= 0.5;
        sigma = ( !probSgl_1 && !probSgl_2 ) ? 0 : 0.5 - abs( probSgl_1 - probSgl_2 );
    }
    similarity = kappa * sigma + ( 1 - kappa ) * similarity / len_aln;
    //cerr << "SIGMA " << sigma << " " << probSgl_1 << " " << omega_1 << " " << probSgl_2 << " " << omega_2 << " F/minL " << F[ L2 ][ L1 ] / minlen << " SIM " << similarity << endl;

//    if( prm ) {
/*        cout << "\nDynamic programming matrix: " << endl;
        float* tempidx_1 = new float[L1+1];
        float* tempidx_2 = new float[L2+1];
        for( int k = 0; k <= L1; k++ ) tempidx_1[ k ] = (float) k;
        for( int k = 0; k <= L2; k++ ) tempidx_2[ k ] = (float) k;
        cout << "F:" << endl;
    	print_matrixdp( F, tempidx_1, L1, tempidx_2, L2);
    	print_matrixdp( trF, tempidx_1, L1, tempidx_2, L2);
        cout << "P:" << endl;
    	print_matrixdp( P, tempidx_1, L1, tempidx_2, L2);
    	print_matrixdp( trP, tempidx_1, L1, tempidx_2, L2);
        cout << "Q:" << endl;
    	print_matrixdp( Q, tempidx_1, L1, tempidx_2, L2);
    	print_matrixdp( trQ, tempidx_1, L1, tempidx_2, L2);
        free(tempidx_1);
        free(tempidx_2);*/
//    }


    /* free memory */
    for( int j = 0; j <= L2; j++ )  delete[] F[ j ];
  	delete[] F;
    for( int j = 0; j <= L2; j++ )  delete[] Q[ j ];
    delete[] Q;
    for( int j = 0; j <= L2; j++ )  delete[] P[ j ];
    delete[] P;
    for( int j = 0; j <= L2; j++ )  delete trF[ j ];
    delete[] trF;
    for( int j = 0; j <= L2; j++ )  delete trP[ j ];
    delete[] trP;
    for( int j = 0; j <= L2; j++ )  delete trQ[ j ];
    delete[] trQ;

    return similarity;
}


float  max3( float f1, float f2, float f3, char* ptr )
{
        float  max = 0 ;

        if( f1 >= f2 && f1 >= f3 )
        {
                max = f1 ;
                *ptr = '|' ;
        }
        else if( f2 > f3 )
        {
                max = f2 ;
                *ptr = '\\' ;
        }
        else
        {
                max = f3 ;
                *ptr = '-' ;
        }

        return  max ;
}



float  min3( float f1, float f2, float f3, char* ptr )
{
        float  min = 0 ;

        if( f1 <= f2 && f1 <= f3 )
        {
                min = f1 ;
                *ptr = '|' ;
        }
        else if( f2 < f3 )
        {
                min = f2 ;
                *ptr = '\\' ;
        }
        else
        {
                min = f3 ;
                *ptr = '-' ;
        }

        return  min ;
}


float max( float f1, float f2 )
{
    return ( f1 >= f2 ) ? f1 : f2;
}


float min( float f1, float f2 )
{
    return ( f1 <= f2 ) ? f1 : f2;
}


template <typename T>
void print_matrixdp( T ** F, float * prob_1, int L1, float * prob_2, int L2 )
{
    cout << "        ";
    for( int i = 0; i < L1; i++ )
    {
            cout << prob_1[ i ] << "   ";
    }
    cout << "\n  ";

    for( int j = 0; j <= L2; j++ )
    {
            if( j > 0 )
            {
                    cout << prob_2[ j-1 ] << " ";
            }
            for( int i = 0; i <= L1; i++ )
            {
                    cout.width( 3 );
                    cout << F[ j ][ i ] << " ";
            }
            cout << endl;
    }
}


float simalign( float ** Z, int L1, int L2, int * idx_1_aln, int * idx_2_aln, int & L1_aln, int & L2_aln, int precision, bool global, bool prm)
{
    int i = 0, j = 0;
    float sim;

    /* get gap penalty as mean of all similarities
    float gap = 0;
    for( j = 0; j < L2; j++ )
        for( i = 0; i < L1; i++ )
        	gap += D[ j ][ i ];
    gap = alpha * gap / ( L1 * L2 );
    char p[10];
    sprintf(p, "%.*lf", precision, gap);
    gap = ( atof(p) < 0. ) ? atof(p) : -0.01;
    if( prm )
    	cout << "Step 2 gap penalty: " << gap << endl; */

    /* Dynamic programming matrix */
    float ** F = new float * [ L2+1 ];
    for( j = 0; j <= L2; j++ )
    	F[ j ] = new float [ L1+1 ];
    nwdp_initF( F, L1, L2 );

    /* Traceback matrix */
    char ** traceback = new char * [ L2+1 ];
    for( j = 0; j <= L2; j++ )
    	traceback[ j ] = new char [ L1+1 ];
    nwdp_initTB( traceback, L1, L2 );

    /* create alignment */
    float fU, fD, fL;
    char ptr;
    for( j = 1; j <= L2; j++ )
    {
    	for( i = 1; i <= L1; i++ )
        {
    		fU = F[ j-1 ][ i ] + alpha;
            fD = F[ j-1 ][ i-1 ] + Z[ j-1 ][ i-1 ];
            fL = F[ j ][ i-1 ] + alpha;

            F[ j ][ i ] = max3( fU, fD, fL, &ptr );
            traceback[ j ][ i ] =  ptr;
        }
    }
    sim = F[ L2 ][ L1 ];
    i--; j--;

    /* find maximal entry of similarity matrix F for local alignments */
    if( !global ) {
    	float lmax = INFINITE;
    	int li = 0;
    	int lj = 0;
        for( j = 1; j <= L2; j++ )
        	for( i = 1; i <= L1; i++ )
        		if( F[ j ][ i ] >= lmax ) {
        			lmax = F[ j ][ i ];
        			li = i;
        			lj = j;
        		}
        i = li;
        j = lj;
        sim = lmax;
    }
    //cerr << i << " " << j << endl;

    /* backtracking */
    int k = 0;
    while( i > 0 || j > 0 )
    {
    	if( !global )
    		if( F[ j ][ i ] <= 0 )
    			break;
    	//cout << j << " " << i << " " << traceback[ j ][ i ] << endl;
    	switch( traceback[ j ][ i ] )
        {
        	case '|' : j--;
                       break;
            case '\\': idx_1_aln[ k ] = i-1;
                       idx_2_aln[ k++ ] = j-1;
                       i--; j--;
                       break ;
            case '-' : i--;
                       break;
        }
    }

    L1_aln = L2_aln = k;
    reverse( idx_1_aln, L1_aln );
    reverse( idx_2_aln, L2_aln );

    if( prm ) {
        cout << "\nDynamic programming matrix: " << endl;
        float* tempidx_1 = new float[L1+1];
        float* tempidx_2 = new float[L2+1];
        for( int k = 0; k <= L1; k++ ) tempidx_1[ k ] = (float) k;
        for( int k = 0; k <= L2; k++ ) tempidx_2[ k ] = (float) k;
    	print_matrixdp( F, tempidx_1, L1, tempidx_2, L2);
    	print_matrixdp( traceback, tempidx_1, L1, tempidx_2, L2);
        free(tempidx_1);
        free(tempidx_2);
    }

    /* free memory */
    for( int j = 0; j <= L2; j++ )  delete F[ j ];
    delete[] F;
    for( int j = 0; j <= L2; j++ )  delete traceback[ j ];
    delete[] traceback;

    return sim;
}


float simalign_affinegaps( float ** Z, int L1, int L2, int * idx_1_aln, int * idx_2_aln, int & Laln, int precision, bool global, bool prm)
{
    int i = 0, j = 0;
    float sim;

    // opening gap penalty
    float gapopen = alpha + beta;

    /* Dynamic programming matrix */
    float ** F = new float * [ L2+1 ];
    for( j = 0; j <= L2; j++ )
    	F[ j ] = new float [ L1+1 ];
    nwdp_initF_affinegaps( F, L1, L2, !global );

    // Initialize Q and P matrices for cost of alignments that end with a gap
    float ** Q = new float * [ L2 + 1  ];
    for( int j = 0; j <= L2; j++ )
    	Q[ j ] = new float [ L1 + 1 ];
    nwdp_initGap( Q, L1, L2 );
    float ** P = new float * [ L2 + 1  ];
    for( int j = 0; j <= L2; j++ )
      	P[ j ] = new float [ L1 + 1 ];
   	nwdp_initGap( P, L1, L2 );

    /* Traceback matrices */
    char ** trF = new char * [ L2+1 ];
    for( j = 0; j <= L2; j++ )
    	trF[ j ] = new char [ L1+1 ];
    nwdp_initTB( trF, L1, L2 );
    char ** trQ = new char * [ L2+1 ];
    for( j = 0; j <= L2; j++ )
    	trQ[ j ] = new char [ L1+1 ];
    nwdp_initTB( trQ, L1, L2 );
    char ** trP = new char * [ L2+1 ];
    for( j = 0; j <= L2; j++ )
    	trP[ j ] = new char [ L1+1 ];
    nwdp_initTB( trP, L1, L2 );

    // base pair similarity of bases pairing with nuc_1 and bases pairing with nuc_2
    /* create alignment */
    char ptr;
    for( j = 1; j <= L2; j++ )
    {
    	for( i = 1; i <= L1; i++ )
        {
    	    // calculate P
   			P[ j ][ i ] = max3( P[ j-1 ][ i ] + beta, F[ j-1 ][ i ] + gapopen, INFINITE, &ptr );
            trP[ j ][ i ] =  ptr;

    		// calculate Q
           	Q[ j ][ i ] = max3( INFINITE, F[ j ][ i-1 ] + gapopen, Q[ j ][ i-1 ] + beta, &ptr );
            trQ[ j ][ i ] =  ptr;

            // calculate F
            F[ j ][ i ] = max3( P[ j ][ i ], F[ j-1 ][ i-1 ] + Z[ j-1 ][ i-1 ], Q[ j ][ i ], &ptr );
            if( !global && F[ j ][ i ] < 0 )
            	F[ j ][ i ] = 0;
            trF[ j ][ i ] =  ptr;
        }
    }
    sim = F[ L2 ][ L1 ];
    i--; j--;

    /* find maximal entry of similarity in matrix F, P or Q for local alignments */
    int maxmat = 0;  // 0 .. F; 1 .. P; 2 .. Q
    if( !global ) {
    	float lmax = 0;
    	int li = 0;
    	int lj = 0;
        for( j = 1; j <= L2; j++ )
        	for( i = 1; i <= L1; i++ ) {
        		if( F[ j ][ i ] >= lmax ) {
        			lmax = F[ j ][ i ];
        			li = i;
        			lj = j;
        			maxmat = 0;
        		}
        		if( P[ j ][ i ] >= lmax ) {
        			lmax = P[ j ][ i ];
        			li = i;
        			lj = j;
        			maxmat = 1;
        		}
        		if( Q[ j ][ i ] >= lmax ) {
        			lmax = Q[ j ][ i ];
        			li = i;
        			lj = j;
        			maxmat = 2;
        		}
        	}
        i = li;
        j = lj;
        sim = lmax;
    }
    //cerr << i << " " << j << " " << maxmat << endl;

    /* backtracking */
    int k = 0;
    while( i > 0 || j > 0 )
    {
    	if( !global )
    		if( (maxmat == 0 && F[ j ][ i ] == 0) || (maxmat == 1 && P[ j ][ i ] == 0) || (maxmat == 2 && Q[ j ][ i ] == 0) )
    			break;

    	switch( maxmat )
    	{
    		case 0 : switch( trF[ j ][ i ] )
    				 {
        			 	 case '|' : maxmat = 1;
        			 	 	 	 	break;
        			 	 case '\\': idx_1_aln[ k ] = i-1;
        			 	 	 	 	idx_2_aln[ k++ ] = j-1;
        			 	 	 	 	i--; j--;
        			 	 	 	 	break ;
        			 	 case '-' : maxmat = 2;
        			 	 	 	 	break;
    				 }
    				 break;
    		 case 1: switch( trP[ j ][ i ] )
    				 {
    		 	 	 	 case '|' : j--;
    		 	 	 	 	 	 	break;
    		 	 	 	 case '\\': maxmat = 0;
    		 	 	 	 	 	 	j--;
    		 	 	 	 	 	 	break;
    				 }
    				 break;
    		 case 2: switch( trQ[ j ][ i ] )
    		 	 	 {
 	 	 	 	 	 	 case '\\': maxmat = 0;
 	 	 	 	 	 	 	 	 	i--;
 	 	 	 	 	 	 	 	 	break;
 	 	 	 	 	 	 case '-' : i--;
 	 	 	 	 	 	 	 	 	break;
    		 	 	 }
    		 	 	 break;
    	}
    }

    Laln = k;
    reverse( idx_1_aln, Laln );
    reverse( idx_2_aln, Laln );

    if( prm ) {
        cout << "\nDynamic programming matrix: " << endl;
        float* tempidx_1 = new float[L1+1];
        float* tempidx_2 = new float[L2+1];
        for( int k = 0; k <= L1; k++ ) tempidx_1[ k ] = (float) k;
        for( int k = 0; k <= L2; k++ ) tempidx_2[ k ] = (float) k;
        cout << "F:" << endl;
    	print_matrixdp( F, tempidx_1, L1, tempidx_2, L2);
    	print_matrixdp( trF, tempidx_1, L1, tempidx_2, L2);
        cout << "P:" << endl;
    	print_matrixdp( P, tempidx_1, L1, tempidx_2, L2);
    	print_matrixdp( trP, tempidx_1, L1, tempidx_2, L2);
        cout << "Q:" << endl;
    	print_matrixdp( Q, tempidx_1, L1, tempidx_2, L2);
    	print_matrixdp( trQ, tempidx_1, L1, tempidx_2, L2);
        free(tempidx_1);
        free(tempidx_2);
    }

    /* free memory */
    for( int j = 0; j <= L2; j++ )  delete F[ j ];
    delete[] F;
    for( int j = 0; j <= L2; j++ )  delete P[ j ];
    delete[] P;
    for( int j = 0; j <= L2; j++ )  delete Q[ j ];
    delete[] Q;
    for( int j = 0; j <= L2; j++ )  delete trF[ j ];
    delete[] trF;
    for( int j = 0; j <= L2; j++ )  delete trP[ j ];
    delete[] trP;
    for( int j = 0; j <= L2; j++ )  delete trQ[ j ];
    delete[] trQ;

 	return sim;
}


void affinegapcosts( int * idx_1_aln, int * idx_2_aln, int & len_aln, int & open, int & extended )
{
	int gaplen;

	for( int i=1; i<len_aln; i++ ) {
		gaplen = idx_1_aln[ i ] - idx_1_aln[ i-1 ] - 1;
		if( gaplen ) {
			open++;
			extended += gaplen;
		}

		gaplen = idx_2_aln[ i ] - idx_2_aln[ i-1 ] - 1;
		if( gaplen ) {
			open++;
			extended += gaplen;
		}
	}
}


void freeMatrix(float ** matrix, int leny)
{
	for(int i = 0 ; i < leny ; i++) {
		delete[] matrix[i];
	}
	delete[] matrix;
}


void reverse( int * list, int len )
{
	int temp;
	for( int i = 0; i < len/2; i++ )
	{
		temp = list[ i ];
	    list[ i ] = list[ len-i-1 ];
	    list[ len-i-1 ] = temp;
	}
}


bool compareBySimilarity(const LocalHit& a, const LocalHit& b)
{
	// smallest comes first
	return a.similarity < b.similarity;
}


/*
 * return overlap of the second interval by the first interval
 */
float getOverlap2ndInterval( int start_1, int end_1, int start_2, int end_2 )
{
	int len_2 = end_2 - start_2 + 1;

	if( end_1 >= start_2 )
		if( start_1 <= start_2 )
			if( end_1 <= end_2 )
				return (float) (end_1 - start_2 + 1) / len_2;
			else
				return 1.;
		else
			if( end_1 <= end_2)
				return (float) (end_1 - start_2 + 1 - start_1 + start_2) / len_2;
			else
				return (float) (end_2 - start_1 + 1) / len_2;
	else
		return 0.;
}


/*
 * return normalized log odds of base pair probabilities weighted by paired probabilities
 */
void getlogoddsDbl( vector<float> & probDbl, string seq, int len, float pnull )
{
    for( int i = 0; i < len; i++ )
    {
    	int offset = i * len;
    	for( int j = 0; j < len; j++ )
    	{
    		//pnullij = pnullDbl[ 4*nucIdx.at( seq.c_str()[ i ] ) + nucIdx.at( seq.c_str()[ j ] ) ];
    		//probDbl[ offset + j ] = ( probDbl[ offset + j ] <= nullprob ) ? log( nullprob / pnullij ) / log( 1 / pnullij ) : log( probDbl[ offset + j ] / pnullij ) / log( 1 / pnullij );
    		probDbl[ offset + j ] = max( 0, log( probDbl[ offset + j ] / pnull ) / log( 1 / pnull ) );
    		//cerr << i << " " << " " << j << " " << probDbl[ offset + j ] << endl;
    	}
    }
}


/*
 * return normalized log odds of unpaired probabilities weighted by probability of random occurrences of nucleotides
 */
void getlogoddsSgl( vector<float> & probSgl, string seq, int len, float pnull )
{
    for( int i = 0; i < len; i++ )
    {
    	//pnulli = pnullSgl[ nucIdx.at( seq.c_str()[ i ] ) ];
    	//probSgl[ i ] = ( probSgl[ i ] <= nullprob ) ? log( nullprob / pnulli ) / log( 1 / pnulli ) : log( probSgl[ i ] / pnulli ) / log( 1 / pnulli );
    	probSgl[ i ] = max( 0, log( probSgl[ i ] / pnull ) / log( 1 / pnull ) );
    }
}


void printalign( string & seq_1, int * idx_1_aln, string & seq_2, int * idx_2_aln, int len_aln )
{
	int len_1 = seq_1.length();
	int len_2 = seq_2.length();

	char *seqar_1 = new char[len_1+1];
	strcpy(seqar_1, seq_1.c_str());
	char *seqar_2 = new char[len_2+1];
	strcpy(seqar_2, seq_2.c_str());
	string seq_1_aln, seq_2_aln;

	int offset_1 = 0, offset_2 = 0;
	for (int k=0; k < len_aln; k++) {
		while( idx_1_aln[ k ] != offset_1 ) { seq_1_aln.push_back( seqar_1[ offset_1 ] ); seq_2_aln.push_back( '-' ); offset_1++; }
		while( idx_2_aln[ k ] != offset_2 ) { seq_2_aln.push_back( seqar_2[ offset_2 ] ); seq_1_aln.push_back( '-' ); offset_2++; }
		seq_1_aln.push_back( seqar_1[ idx_1_aln[ k ] ] ); offset_1++;
		seq_2_aln.push_back( seqar_2[ idx_2_aln[ k ] ] ); offset_2++;
	}
	while( offset_1 < len_1 ) { seq_1_aln.push_back( seqar_1[ offset_1 ] ); seq_2_aln.push_back( '-' ); offset_1++; }
	while( offset_2 < len_2 ) { seq_2_aln.push_back( seqar_2[ offset_2 ] ); seq_1_aln.push_back( '-' ); offset_2++; }

	cout << seq_1_aln << endl;
	cout << seq_2_aln << endl;

    delete[] seqar_1;
    delete[] seqar_2;
}


/*
 * Needleman-Wunsch algorithm for global alignment of nt sequence
 */
int nw( string seq_1, string seq_2, string& seq_1_al, string& seq_2_al, bool prm)
{
        int  d = 5;                 /* gap penalty like in blastn existence gap costs */

        int  L1 = seq_1.length();
        int  L2 = seq_2.length();

        // Dynamic programming matrix
        int ** F = new int * [ L2+1 ];
        for( int i = 0; i <= L2; i++ )  F[ i ] = new int [ L1+1 ];

        // Traceback matrix
        char ** traceback = new char * [ L2+1 ];
        for( int i = 0; i <= L2; i++ )  traceback[ i ] = new char [ L1+1 ];

        // Initialize traceback and F matrix (fill in first row and column)
        dpm_init( F, traceback, L1, L2, d );

        // Create alignment
        int dist = nw_align( F, traceback, seq_1, seq_2, seq_1_al, seq_2_al, d );

        #if DEBUG
            int  L_al = seq_1_al.length();
            cout << "Length after alignment: " << L_al << endl;
        #endif

        if( prm )
        {
                cout << "\nDynamic programming matrix: " << "\n\n";
                print_matrix( F, seq_1, seq_2 );

                cout << "\nTraceback matrix: " << "\n\n";
                print_traceback( traceback, seq_1, seq_2 );

                cout << endl;
        }

        for( int i = 0; i <= L2; i++ )  delete F[ i ];
        delete[] F;
        for( int i = 0; i <= L2; i++ )  delete traceback[ i ];
        delete[] traceback;

        return dist;
}


void  dpm_init( int ** F, char ** traceback, int L1, int L2, int d )
{
        F[ 0 ][ 0 ] =  0 ;
        traceback[ 0 ][ 0 ] = 'n' ;

        int i=0, j=0;

        for( j = 1; j <= L1; j++ )
        {
                F[ 0 ][ j ] =  -j * d ;
                traceback[ 0 ][ j ] =  '-' ;
        }
        for( i = 1; i <= L2; i++ )
        {
                F[ i ][ 0 ] =  -i * d ;
                traceback[ i ][ 0 ] =  '|' ;
        }
}


/*
 * Needleman-Wunsch algorithm
 */
int nw_align( int **F, char **traceback, string seq_1, string seq_2, string& seq_1_al, string& seq_2_al, int d )
{
        int        k = 0, x = 0, y = 0;
        int        fU, fD, fL ;
        char       ptr, nuc ;
        int        i = 0, j = 0;

        const int a = 2;   // Match like in blastn (optimized for sequence identity of 90%)
        const int b = -3;   // Mismatch like in blastn

        const int  s[ 4 ][ 4 ] = { { a, b, b, b },    /* substitution matrix */
                                   { b, a, b, b },
                                   { b, b, a, b },
                                   { b, b, b, a } } ;

        int  L1 = seq_1.length();
        int  L2 = seq_2.length();

		for( i = 1; i <= L2; i++ )
        {
                for( j = 1; j <= L1; j++ )
                {
                        nuc = seq_1[ j-1 ] ;

                        switch( nuc )
                        {
                                case 'A':  x = 0 ;  break ;
                                case 'C':  x = 1 ;  break ;
                                case 'G':  x = 2 ;  break ;
                                case 'T':  x = 3 ;  break ;
                        }

                        nuc = seq_2[ i-1 ] ;

                        switch( nuc )
                        {
                                case 'A':  y = 0 ;  break ;
                                case 'C':  y = 1 ;  break ;
                                case 'G':  y = 2 ;  break ;
                                case 'T':  y = 3 ;  break;
                        }

                        fU = F[ i-1 ][ j ] - d ;
                        fD = F[ i-1 ][ j-1 ] + s[ x ][ y ] ;
                        fL = F[ i ][ j-1 ] - d ;

                        F[ i ][ j ] = (int) max3( (float) fU, (float) fD, (float) fL, &ptr ) ;

                        traceback[ i ][ j ] =  ptr ;
                }
        }
        i-- ; j-- ;

        while( i > 0 || j > 0 )
        {
                switch( traceback[ i ][ j ] )
                {
                        case '|' :      seq_1_al += '-' ;
                                        seq_2_al += seq_2[ i-1 ] ;
                                        i-- ;
                                        break ;

                        case '\\':     	seq_1_al += seq_1[ j-1 ] ;
                                        seq_2_al += seq_2[ i-1 ] ;
                                        i-- ;  j-- ;
                                        break ;

                        case '-' :      seq_1_al += seq_1[ j-1 ] ;
                                        seq_2_al += '-' ;
                                        j-- ;
                                        break;
                }
                k++ ;
        }

        reverse( seq_1_al.begin(), seq_1_al.end() );
        reverse( seq_2_al.begin(), seq_2_al.end() );

        return  F[ L2 ][ L1 ] ;
}


void  print_matrix( int ** F, string seq_1, string seq_2 )
{
        int  L1 = seq_1.length();
        int  L2 = seq_2.length();

        cout << "        ";
        for( int j = 0; j < L1; j++ )
        {
                cout << seq_1[ j ] << "   ";
        }
        cout << "\n  ";

        for( int i = 0; i <= L2; i++ )
        {
                if( i > 0 )
                {
                        cout << seq_2[ i-1 ] << " ";
                }
                for( int j = 0; j <= L1; j++ )
                {
                        cout.width( 3 );
                        cout << F[ i ][ j ] << " ";
                }
                cout << endl;
        }
}


void  print_traceback( char ** traceback, string seq_1, string seq_2 )
{
        int  L1 = seq_1.length();
        int  L2 = seq_2.length();

        cout << "    ";
        for( int j = 0; j < L1; j++ )
        {
                cout << seq_1[ j ] << " ";
        }
        cout << "\n  ";

        for( int i = 0; i <= L2; i++ )
        {
                if( i > 0 )
                {
                        cout << seq_2[ i-1 ] << " ";
                }
                for( int j = 0; j <= L1; j++ )
                {
                        cout << traceback[ i ][ j ] << " ";
                }
                cout << endl;
        }
}


void  print_al( string& seq_1_al, string& seq_2_al )
{
        cout << seq_1_al << endl;
        cout << seq_2_al << endl;
}




