/*  
	TRACE: fasT and Robust Ancestry Coordinate Estimation
    Copyright (C) 2013-2016  Chaolong Wang

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Version.h"
#include "aux.h"
#include "TableReader.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <map>
#include <vector>

#include <openblas/cblas.h>
#define  __GSL_CBLAS_H__
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_statistics_double.h>
#include <cmath>
#include <algorithm>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#define  ARMA_DONT_USE_WRAPPER
#include "armadillo"
using namespace arma;
using namespace std;

const string ARG_PARAM_FILE = "-p";
const string ARG_STUDY_FILE = "-s";
const string ARG_GENO_FILE = "-g";
const string ARG_COORD_FILE = "-c";
const string ARG_OUT_PREFIX = "-o";
const string ARG_DIM = "-k";
const string ARG_DIM_HIGH = "-K";
const string ARG_MIN_LOCI = "-l";
const string ARG_REF_SIZE = "-N";
const string ARG_FIRST_IND = "-x";
const string ARG_LAST_IND = "-y";
const string ARG_ALPHA = "-a";
const string ARG_THRESHOLD = "-t";
const string ARG_MASK_PROP = "-m";
const string ARG_TRIM_PROP = "-M";
const string ARG_EXCLUDE_LIST = "-ex";
const string ARG_PROCRUSTES_SCALE = "-rho";
const string ARG_RANDOM_SEED= "-seed";
const string ARG_KNN_ZSCORE= "-knn";
const string ARG_NUM_THREADS = "-nt";

const string default_str = "---this-is-a-default-string---";
const int default_int = -999999998;
const double default_double = -9.99999999;
char tmpstr[2000];

string LOG_FILE;
string TMP_LOG_FILE = "Temporary.";

string PARAM_FILE = default_str;    // parameter file name
string STUDY_FILE = default_str;     // Study genotype datafile name
string GENO_FILE = default_str;      // Reference genotype datafile name
string COORD_FILE = default_str;    // Reference coordinates datafile name
string OUT_PREFIX = default_str;    // Prefix for output files
int DIM = default_int;              // Number of PCs in the reference to match;
int DIM_HIGH = default_int;         // Number of PCs in from sample-specific PCA;
int MIN_LOCI = default_int;         // Minimum number of non-missing loci required
int REF_SIZE = default_int;         // Number of individuals in the reference set
int FIRST_IND = default_int;	    // First individual in the list sample to be tested; 
int LAST_IND = default_int;	        // Last individual in the list of sample to be tested;
double MASK_PROP = default_double;  // Proportion of loci that will be masked a study sample
double ALPHA = default_double;      // Significance level of the TW statistic when setting DIM_HIGH
double THRESHOLD = default_double;  // Convergence criterion for the projection Procrustes analysis
double TRIM_PROP = default_double;  // Proportion of random loci to be removed from analyses (same set across all samples)
string EXCLUDE_LIST = default_str;  // File name of a list of SNPs to exclude
int PROCRUSTES_SCALE = default_int;  // 0: Fit the scaling parameter to maximize similarity   
									 // 1: Fix the scaling to match the variance between X and Y
int RANDOM_SEED = default_int;       // Random seed used in the program  
int NUM_THREADS = default_int;        // Number of CPU cores for multi-threading parallel analysis 
int KNN_ZSCORE = default_int;       // Number of nearest neigbors used to calculate the Z score for each study individual. 
									
// The following parameters will be determined from the input data files					 
int REF_INDS = default_int;      // Number of reference individuals
int INDS = default_int;       	 // Number of study samples in the STUDY_FILE

int LOCI_G = default_int;      // Number of loci in GENO_FILE;
int LOCI_S = default_int;      // Number of loci in STUDY_FILE;
int LOCI = default_int;        // Number of shared loci


int NUM_PCS = default_int;       // Number of PCs in the COORD_FILE;
int STUDY_NON_DATA_ROWS = 0;    // Number of non-data rows in the STUDY_FILE;
int STUDY_NON_DATA_COLS = 2;    // Number of non-data columns in the STUDY_FILE;
int GENO_NON_DATA_ROWS = 0;     // Number of non-data rows in the GENO_FILE;
int GENO_NON_DATA_COLS = 2;     // Number of non-data columns in the GENO_FILE;
int COORD_NON_DATA_ROWS = 1;   // Number of non-data rows in the COORD_FILE;
int COORD_NON_DATA_COLS = 2;   // Number of non-data columns in the COORD_FILE;

bool AUTO_MODE = false; // If the program will determine DIM_HIGH automatically;
int MAX_ITER = 10000;    // Maximum iterations for the projection Procrustes analysis
double TW = default_double;    // Threshold to determine significant Tracy-Widom statistic

string STUDY_SITE_FILE = default_str;     // Sitefile of the study data
string GENO_SITE_FILE = default_str;      // Sitefile of the reference data

//=======================================================================================
bool is_int(string str);
bool is_numeric(string str);
bool parse_cmd_line(int argc, char* argv[], map<string,string> &args, map<string,int> &argi, map<string,double> &argd);
int read_paramfile(string filename);
int create_paramfile(string filename);
int check_parameters();
void print_configuration();

int normalize(fmat &G, fmat &Gm, fmat &Gsd);
int pca_cov(mat &M, int nPCs, mat &PC, rowvec &PCvar);
int procrustes(mat &X, mat &Y, mat &Xnew, double &t, double &rho, mat &A, rowvec &b, int ps);
double pprocrustes(mat &X, mat &Y, mat &Xnew, double &t, double &rho, mat &A, rowvec &b, int iter, double eps, int ps);

ofstream foutLog;

//=========================================================================================================
int main(int argc, char* argv[]){
	int i=0;
	int j=0;
	int k=0;
	int tmp=0;
	ifstream fin;
	ofstream fout;	
	string str;
	string outfile;	
	time_t t1,t2;
	float runningtime;
	t1 = clock();   // Program starting time	
	time_t rawtime;
  	struct tm * timeinfo;
 	time ( &rawtime );
  	timeinfo = localtime ( &rawtime );	
	stringstream ss;
	ss << getpid();
	string strpid = ss.str();
	TMP_LOG_FILE.append(strpid);
	TMP_LOG_FILE.append(".log");
	foutLog.open(TMP_LOG_FILE.c_str());
	if(foutLog.fail()){
		cerr << "Error: cannot create a temporary log file." << endl;
		return 1;
	}

    cout << endl;
    cout << setfill('=') << setw(70) << "=" << endl;
    cout << "==== " << setfill(' ') << setw(60) << left << "TRACE: fasT and Robust Ancestry Coordinate Estimation" << " ====" << endl;
    cout << "==== " << "Version " << setfill(' ') << setw(52) << LASER_VERSION << " ====" << endl;
    cout << "==== " << "Last modified on " << setfill(' ') << setw(43) << LAST_MODIFIED << " ====" << endl;
    cout << "==== " << setfill(' ') << setw(60) << "(C) 2013-2017 Chaolong Wang, GNU GPL v3.0" << " ====" << endl;
    cout << "==== " << setfill(' ') << setw(60) << "Maintained by Daniel Taliun" << " ====" << endl;
    cout << setfill('=') << setw(70) << "=" << endl;
    cout << setfill(' ');
    cout << "Started at: " << asctime (timeinfo) << endl;

    foutLog << endl;
    foutLog << setfill('=') << setw(70) << "=" << endl;
    foutLog << "==== " << setfill(' ') << setw(60) << left << "TRACE: fasT and Robust Ancestry Coordinate Estimation" << " ====" << endl;
    foutLog << "==== " << "Version " << setfill(' ') << setw(52) << LASER_VERSION << " ====" << endl;
    foutLog << "==== " << "Last modified on " << setfill(' ') << setw(43) << LAST_MODIFIED << " ====" << endl;
    foutLog << "==== " << setfill(' ') << setw(60) << "(C) 2013-2017 Chaolong Wang, GNU GPL v3.0" << " ====" << endl;
    foutLog << "==== " << setfill(' ') << setw(60) << "Maintained by Daniel Taliun" << " ====" << endl;
    foutLog << setfill('=') << setw(70) << "=" << endl;
    foutLog << setfill(' ');
    foutLog << "Started at: " << asctime (timeinfo) << endl;

	// ################ Read in command line ##########################
	map<string,string> args;
	map<string,int> argi;
	map<string,double> argd;	
	bool cmd_flag = parse_cmd_line(argc, argv, args, argi, argd);	
	if(args[ARG_PARAM_FILE].compare(default_str)!=0){PARAM_FILE = args[ARG_PARAM_FILE];}
	if(args[ARG_STUDY_FILE].compare(default_str)!=0){STUDY_FILE = args[ARG_STUDY_FILE];}
	if(args[ARG_GENO_FILE].compare(default_str)!=0){GENO_FILE = args[ARG_GENO_FILE];}
	if(args[ARG_COORD_FILE].compare(default_str)!=0){COORD_FILE = args[ARG_COORD_FILE];}
	if(args[ARG_OUT_PREFIX].compare(default_str)!=0){OUT_PREFIX = args[ARG_OUT_PREFIX];}
	if(argi[ARG_DIM]!=default_int){DIM = argi[ARG_DIM];}
	if(argi[ARG_DIM_HIGH]!=default_int){DIM_HIGH = argi[ARG_DIM_HIGH];}
	if(argi[ARG_MIN_LOCI]!=default_int){MIN_LOCI = argi[ARG_MIN_LOCI];}
	if(argi[ARG_REF_SIZE]!=default_int){REF_SIZE = argi[ARG_REF_SIZE];}
	if(argi[ARG_FIRST_IND]!=default_int){FIRST_IND = argi[ARG_FIRST_IND];}
	if(argi[ARG_LAST_IND]!=default_int){LAST_IND = argi[ARG_LAST_IND];}
	if(argd[ARG_MASK_PROP]!=default_double){MASK_PROP = argd[ARG_MASK_PROP];}
	if(argd[ARG_ALPHA]!=default_double){ALPHA = argd[ARG_ALPHA];}
	if(argd[ARG_THRESHOLD]!=default_double){THRESHOLD = argd[ARG_THRESHOLD];}
	if(argd[ARG_TRIM_PROP]!=default_double){TRIM_PROP = argd[ARG_TRIM_PROP];}
	if(args[ARG_EXCLUDE_LIST]!=default_str){EXCLUDE_LIST = args[ARG_EXCLUDE_LIST];}
	if(argi[ARG_PROCRUSTES_SCALE]!=default_int){PROCRUSTES_SCALE = argi[ARG_PROCRUSTES_SCALE];}
	if(argi[ARG_RANDOM_SEED]!=default_int){RANDOM_SEED = argi[ARG_RANDOM_SEED];}
	if(argi[ARG_KNN_ZSCORE]!=default_int){KNN_ZSCORE = argi[ARG_KNN_ZSCORE];}
	if(argi[ARG_NUM_THREADS]!=default_int){NUM_THREADS = argi[ARG_NUM_THREADS];}
	//##################  Read in and check parameter values  #######################
	if(PARAM_FILE.compare(default_str)==0){ PARAM_FILE = "trace.conf"; }
	int flag = read_paramfile(PARAM_FILE);
	if(flag==0 || cmd_flag==0){
		foutLog.close();
		if(OUT_PREFIX.compare(default_str)==0){
			LOG_FILE = "trace.log";
		}else{
			LOG_FILE = OUT_PREFIX;
			LOG_FILE.append(".log");
		}
		sprintf(tmpstr, "%s%s%s%s", "mv ", TMP_LOG_FILE.c_str()," ", LOG_FILE.c_str());
		int sys_msg = system(tmpstr);
		return 1;
	}
	//################  Set default values to some parameters #######################
	if(MIN_LOCI==default_int){ MIN_LOCI = 100; }
	if(MASK_PROP==default_double){ MASK_PROP = 0; }
	if(TRIM_PROP==default_double){ TRIM_PROP = 0; }
	if(DIM == default_int) { DIM = 2; }
	if(DIM_HIGH == default_int) { DIM_HIGH = 20; }  // 0 means DIM_HIGH will be automatically set by the program
	if(ALPHA == default_double) { ALPHA = 0.1; }
	if(THRESHOLD == default_double) { THRESHOLD = 0.000001; }
	if(PROCRUSTES_SCALE==default_int){ PROCRUSTES_SCALE = 0; }
	if(RANDOM_SEED==default_int){ RANDOM_SEED = 0; }
	if(KNN_ZSCORE==default_int){ KNN_ZSCORE = 10; }
	if(NUM_THREADS==default_int){ NUM_THREADS = 8; }
	//###############################################################################
	if(OUT_PREFIX.compare(default_str)==0){ OUT_PREFIX = "trace"; }
	foutLog.close();
	LOG_FILE = OUT_PREFIX;
	LOG_FILE.append(".log");
	sprintf(tmpstr, "%s%s%s%s", "mv ", TMP_LOG_FILE.c_str()," ", LOG_FILE.c_str());
	int sys_msg = system(tmpstr);
	foutLog.open(LOG_FILE.c_str(), ios::app);
	if(foutLog.fail()){
		cerr << "Error: cannot create the log file." << endl;
		return 1;
	}
	
	//############## Get values for REF_INDS, LOCI, INDS, NUM_PCs, and Check data format ################
	int nrow = 0;
	int ncol = 0;
	flag = 1;
	map<string,int> idxS;
	map<string,int> idxG;
	map<string,string> alleleS;
	map<string,string> alleleG;	
	vector<string> cmnsnp;
	uvec cmnS;
	uvec cmnG;
	int unmatchSite = 0;
	int LOCI_trim = 0;
	int Lex = 0;
	map<string,int> exSNP;	
	
	gsl_rng *rng;
	rng = gsl_rng_alloc(gsl_rng_taus);
	// long seed = time(NULL)*getpid();
	gsl_rng_set(rng, RANDOM_SEED);
	
	//================================================================================
	if (EXCLUDE_LIST.compare(default_str) != 0) {
		fin.open(EXCLUDE_LIST.c_str());
		if (fin.fail()) {
			cerr << "Error: cannot open the file '" << EXCLUDE_LIST << "'." << endl;    
			foutLog << "Error: cannot open the file '" << EXCLUDE_LIST << "'." << endl;   
			foutLog.close();
			return 1;
		} else {
			while (!fin.eof()) {
				fin >> str;
				if(str.length()>0 && str!=" "){
					exSNP[str] = 1;
				}				
			}
			fin.close();		
		}
	}

	if ((STUDY_FILE.compare(default_str) != 0) && (flag == 1)) {
        TableReader geno_reader;
        TableReader sites_reader;
        LOCI_S = 0;
        STUDY_SITE_FILE = build_sites_filename(STUDY_FILE);
        vector<string> tokens;
        string message("");

        geno_reader.set_file_name(STUDY_FILE);
        geno_reader.open();
        geno_reader.get_dim(nrow, ncol, '\t');
        INDS = nrow - STUDY_NON_DATA_ROWS;
		int tmpLOCI = ncol - STUDY_NON_DATA_COLS;
		cout << INDS << " individuals are detected in the STUDY_FILE." << endl;  
		foutLog << INDS << " individuals are detected in the STUDY_FILE." << endl; 
		if (INDS < 0) {
			cerr << "Error: Invalid number of rows in '" << STUDY_FILE << "'." << endl;
			foutLog << "Error: Invalid number of rows in '" << STUDY_FILE << "'." << endl;
			flag = 0;
		}
		if (tmpLOCI < 0) {
			cerr << "Error: Invalid number of columns in the STUDY_FILE '" << STUDY_FILE << "'." << endl;
			foutLog << "Error: Invalid number of columns in the STUDY_FILE '" << STUDY_FILE << "'." << endl;
			flag = 0;
		}

        sites_reader.set_file_name(STUDY_SITE_FILE);
        sites_reader.open();
        sites_reader.read_row(tokens, '\t'); //skip header
		while (sites_reader.read_row(tokens, '\t') >= 0) {
		    if (tokens.size() != 5) {
                cerr << "Error: incorrect number of columns in '" << STUDY_SITE_FILE << "'." << endl;
                foutLog << "Error: incorrect number of columns in '" << STUDY_SITE_FILE << "'." << endl;
                foutLog.close();
                return 1;
		    }
		    string variant_name = tokens.at(0) + ":" + tokens.at(1);
		    string variant_alleles = tokens.at(3) + "," + tokens.at(4);
            idxS[variant_name] = LOCI_S;
            alleleS[variant_name] = variant_alleles;
            LOCI_S++;
		}
        sites_reader.close();

		cout << LOCI_S << " loci are detected in the STUDY_FILE." << endl; 
		foutLog << LOCI_S << " loci are detected in the STUDY_FILE." << endl;
		if ((tmpLOCI < 0) || (tmpLOCI != LOCI_S)) {
			cerr << "Error: Number of loci doesn't match in '" << STUDY_SITE_FILE << "' and '" << STUDY_FILE << "'." << endl;
			foutLog << "Error: Number of loci doesn't match in '" << STUDY_SITE_FILE << "' and '" << STUDY_FILE << "'." << endl;
			flag = 0;
		}
		if (flag == 1) {
            flag = geno_reader.check_format(GENO_NON_DATA_ROWS, GENO_NON_DATA_COLS, INDS, LOCI_S, TableReader::Format::NPLOID_GT, message);
            if (message.length() > 0) {
                cout << message << endl;
                foutLog << message << endl;
            }
		}
        geno_reader.close();
	} else {
		cerr << "Error: STUDY_FILE (-s) is not specified." << endl;
		foutLog << "Error: STUDY_FILE (-s) is not specified." << endl;
		foutLog.close();
		return 1;
	}
	
	if(GENO_FILE.compare(default_str) != 0 && flag == 1){
	    TableReader geno_reader;
        TableReader sites_reader;
        LOCI_G = 0;
        LOCI = 0;
        GENO_SITE_FILE = build_sites_filename(GENO_FILE);
        vector<string> tokens;
        string message("");

        geno_reader.set_file_name(GENO_FILE);
        geno_reader.open();
        geno_reader.get_dim(nrow, ncol, '\t');
		REF_INDS = nrow - GENO_NON_DATA_ROWS;
		int tmpLOCI = ncol - GENO_NON_DATA_COLS;
		cout << REF_INDS << " individuals are detected in the GENO_FILE." << endl; 
		foutLog << REF_INDS << " individuals are detected in the GENO_FILE." << endl; 		 		
		if(REF_INDS < 0){
			cerr << "Error: Invalid number of rows in '" << GENO_FILE << "'." << endl;
			foutLog << "Error: Invalid number of rows in '" << GENO_FILE << "'." << endl;
			flag = 0;
		}
		if(tmpLOCI < 0){
			cerr << "Error: Invalid number of columns in the GENO_FILE '" << GENO_FILE << "'." << endl;
			foutLog << "Error: Invalid number of columns in the GENO_FILE '" << GENO_FILE << "'." << endl;
			flag = 0;
		}

        sites_reader.set_file_name(GENO_SITE_FILE);
        sites_reader.open();
        sites_reader.read_row(tokens, '\t'); //skip header TODO: check if header = CHR\tPOS\tID\tREF\tALT
		while (sites_reader.read_row(tokens, '\t') >= 0) {
		    if (tokens.size() != 5) {
                cerr << "Error: incorrect number of columns in '" << GENO_SITE_FILE << "'." << endl;
                foutLog << "Error: incorrect number of columns in '" << GENO_SITE_FILE << "'." << endl;
                foutLog.close();
                return 1;
		    }
		    string variant_name = tokens.at(0) + ":" + tokens.at(1);
            string variant_alleles = tokens.at(3) + "," + tokens.at(4);
            idxG[variant_name] = LOCI_G;
            alleleG[variant_name] = variant_alleles;
            ++LOCI_G;
            if (idxS.count(variant_name) > 0) {
                if (alleleS[variant_name].compare(variant_alleles) != 0 ) {
                    cerr << "Warning: Two datasets have different alleles at locus [" << variant_name << "]: " << "[" << variant_alleles<< "] vs [" << alleleS[variant_name] << "]." << endl;
                    foutLog << "Warning: Two datasets have different alleles at locus [" << variant_name << "]: " << "[" << variant_alleles << "] vs [" << alleleS[variant_name] << "]." << endl;
                    ++unmatchSite;
                } else if (exSNP.count(tokens.at(2)) > 0) { // use ID column
                    ++Lex;
                } else if (gsl_rng_uniform(rng) < TRIM_PROP) {
                    ++LOCI_trim;
                } else {
                    cmnsnp.push_back(variant_name);
                    ++LOCI;
                }
            }
		}
        sites_reader.close();

		cout << LOCI_G << " loci are detected in the GENO_FILE." << endl; 
		foutLog << LOCI_G << " loci are detected in the GENO_FILE." << endl;

		if(tmpLOCI < 0 || tmpLOCI != LOCI_G){
			cerr << "Error: Number of loci doesn't match in '" << GENO_SITE_FILE << "' and '" << GENO_FILE << "'." << endl;
			foutLog << "Error: Number of loci doesn't match in '" << GENO_SITE_FILE << "' and '" << GENO_FILE << "'." << endl;	
			flag = 0;
		}else if(LOCI>0){
			cmnS.set_size(LOCI);
			cmnG.set_size(LOCI);
			for(i=0; i<LOCI; i++){
				cmnG(i)=idxG[cmnsnp[i]];
				cmnS(i)=idxS[cmnsnp[i]];
			}
			cmnsnp.clear();
			idxG.clear();
			idxS.clear();
			alleleG.clear();
			alleleS.clear();
		}
		if (flag == 1) {
            flag = geno_reader.check_format(GENO_NON_DATA_ROWS, GENO_NON_DATA_COLS, REF_INDS, LOCI_G, TableReader::Format::NPLOID_GT, message);
            if (message.length() > 0) {
                cout << message << endl;
                foutLog << message << endl;
            }
		}
        geno_reader.close();
	} else {
		cerr << "Error: GENO_FILE (-g) is not specified." << endl;
		foutLog << "Error: GENO_FILE (-g) is not specified." << endl;
		foutLog.close();
		gsl_rng_free(rng);
		return 1;
	}

	if(COORD_FILE.compare(default_str) != 0 && GENO_FILE.compare(default_str) != 0 && flag == 1){
	    TableReader coord_reader;
        string message("");

        coord_reader.set_file_name(COORD_FILE);
        coord_reader.open();
        coord_reader.get_dim(nrow, ncol, '\t');
		int tmpINDS = nrow - COORD_NON_DATA_ROWS;
		NUM_PCS = ncol - COORD_NON_DATA_COLS;
		cout << tmpINDS << " individuals are detected in the COORD_FILE." << endl;
		cout << NUM_PCS << " PCs are detected in the COORD_FILE." << endl;
		foutLog << tmpINDS << " individuals are detected in the COORD_FILE." << endl;
		foutLog << NUM_PCS << " PCs are detected in the COORD_FILE." << endl;
		if (tmpINDS < 0) {
			cerr << "Error: Invalid number of rows in the COORD_FILE " << COORD_FILE << "." << endl;
			foutLog << "Error: Invalid number of rows in the COORD_FILE " << COORD_FILE << "." << endl;
			flag = 0;
		} else if (tmpINDS != REF_INDS && REF_INDS >= 0) {
			cerr << "Error: Number of individuals in the COORD_FILE is not the same as in the GENO_FILE." << endl;
			foutLog << "Error: Number of individuals in the COORD_FILE is not the same as in the GENO_FILE." << endl;
			flag = 0;
		}
		if (NUM_PCS < 0) {
			cerr << "Error: Invqalid number of columns in the COORD_FILE " << COORD_FILE << "." << endl;
			foutLog << "Error: Invalid number of columns in the COORD_FILE " << COORD_FILE << "." << endl;
			flag = 0;
		}
		if (flag == 1) {
            flag = coord_reader.check_format(COORD_NON_DATA_ROWS, COORD_NON_DATA_COLS, REF_INDS, NUM_PCS, TableReader::Format::FLOAT, message);
            if (message.length() > 0) {
                cout << message << endl;
                foutLog << message << endl;
            }
		}
        coord_reader.close();
	}	
	if(flag == 0){
		foutLog.close();
		gsl_rng_free(rng);
		return 1;
	}	
	//################  Set default values to some parameters #######################		
	if(FIRST_IND==default_int){ FIRST_IND = 1; }
	if(LAST_IND==default_int){ LAST_IND = INDS; }
	if(REF_SIZE==default_int){ REF_SIZE = REF_INDS; }
	if(GENO_FILE.compare(default_str)==0){
		FIRST_IND = 1;
		LAST_IND = INDS;	
	}

	// ################### Check Parameters #############################
	flag = check_parameters();
	if(MIN_LOCI <= DIM && flag!=0){
		cerr << "Warning: DIM>=MIN_LOCI is found; DIM=" << DIM << ", MIN_LOCI=" << MIN_LOCI << "." << endl; 
		cerr << "Reset MIN_LOCI to DIM+1: MIN_LOCI=" << DIM+1 << "." << endl;
		foutLog << "Warning: DIM>=MIN_LOCI is found; DIM=" << DIM << ", MIN_LOCI=" << MIN_LOCI << "." << endl; 
		foutLog << "Reset MIN_LOCI to DIM+1: MIN_LOCI=" << DIM+1 << "." << endl;
		MIN_LOCI = DIM+1;
	}
	if(REF_SIZE > REF_INDS){
		cerr << "Warning: REF_SIZE>REF_INDS is found; REF_SIZE=" << REF_SIZE << ", REF_INDS=" << REF_INDS << "." << endl; 
		cerr << "Reset REF_SIZE to REF_INDS: REF_SIZE=" << REF_INDS << "." << endl;
		foutLog << "Warning: REF_SIZE>REF_INDS is found; REF_SIZE=" << REF_SIZE << ", REF_INDS=" << REF_INDS << "." << endl; 
		foutLog << "Reset REF_SIZE to REF_INDS: REF_SIZE=" << REF_INDS << "." << endl;
		REF_SIZE = REF_INDS;
	}	
	if(flag==0){
		foutLog.close();
		gsl_rng_free(rng);
		return 1;
	}else{
		print_configuration();
	}
	// Setting significance cutoff for the Tracy-Widom statistic
	if(ALPHA==0.2){
		TW = -0.1653;
	}else if(ALPHA==0.15){
		TW = 0.1038;
	}else if(ALPHA==0.1){
		TW = 0.4501;
	}else if(ALPHA==0.05){
		TW = 0.9793;
	}else if(ALPHA==0.01){
		TW = 2.0233;
	}else if(ALPHA==0.005){
		TW = 2.4221;
	}else if(ALPHA==0.001){
		TW = 3.2712;
	}
	// Set number of threads 
	openblas_set_num_threads(NUM_THREADS);
	
	// ##################################################################
	// === get the index of the reference individuals ===
	uvec Refset(REF_SIZE);
	int Index[REF_INDS], subset[REF_SIZE];
	for(i=0; i<REF_INDS; i++){
		Index[i] = i;
	}	
	if(REF_SIZE >0){
		if(REF_SIZE < REF_INDS){
			gsl_ran_choose(rng, subset, REF_SIZE, Index, REF_INDS, sizeof (int));
			for(i=0; i<REF_SIZE; i++){
				Refset(i) = subset[i];
			}
			Refset = sort(Refset);
			time ( &rawtime );
			timeinfo = localtime ( &rawtime );
			cout << endl << asctime (timeinfo);
			cout << "Randomly select " << REF_SIZE << " reference individuals (-N)." << endl;
			foutLog << "Randomly select " << REF_SIZE << " reference individuals (-N)." << endl;
		}else{
			for(i=0; i<REF_INDS; i++){
				Refset(i) = i;
			}
		}
	}

	
	// == Variables to be saved for reused ==
	string *RefInfo1 = new string [REF_SIZE];
	string *RefInfo2 = new string [REF_SIZE];
	fmat RefD(REF_SIZE, LOCI);
	mat refPC = zeros<mat>(REF_SIZE,DIM);
	//========================= Read the reference data ==========================
	if(GENO_FILE.compare(default_str)!=0){
 		time ( &rawtime );
  		timeinfo = localtime ( &rawtime );
  		cout << endl << asctime (timeinfo);	
  		foutLog << endl << asctime (timeinfo);
		
		cout << "Identified " << LOCI + LOCI_trim + unmatchSite + Lex << " loci shared by STUDY_FILE and GENO_FILE." << endl;
		foutLog << "Identified " << LOCI + LOCI_trim + unmatchSite + Lex << " loci shared by STUDY_FILE and GENO_FILE." << endl;
		if (unmatchSite > 0){
			cout << "Exclude "<< unmatchSite << " loci that have different alleles in two datasets." << endl;
			foutLog << "Exclude "<< unmatchSite << " loci that have different alleles in two datasets." << endl;
		}
		if (Lex > 0) {
			cout << "Exclude " << Lex << " loci given by the EXCLUDE_LIST (-ex)." << endl;
			foutLog << "Exclude " << Lex << " loci given by the EXCLUDE_LIST (-ex)." << endl;
		}
		if (TRIM_PROP > 0) {
			cout << "Exclude " << LOCI_trim << " random loci by the TRIM_PROP (-M) option." << endl;
			foutLog << "Exclude " << LOCI_trim << " random loci by the TRIM_PROP (-M) option." << endl;
		}	
		cout << "The analysis will based on the remaining " << LOCI << " shared loci." << endl;
		foutLog << "The analysis will based on the remaining " << LOCI << " shared loci." << endl;
		if(LOCI==0){
			cout << "Error: No data for the analysis. Program exit!" << endl;
			foutLog << "Error: No data for the analysis. Program exit!" << endl;
			foutLog.close();
			gsl_rng_free(rng);
			return 1;
		}		
		//=====================================================================================

		TableReader reader;
		vector<string> tokens;
		int row = 0;
		int ref_ind = 0;
		int ref_subset_ind = 0;

		reader.set_file_name(GENO_FILE);

        time ( &rawtime );
        timeinfo = localtime ( &rawtime );
        cout << endl << asctime (timeinfo);
        cout << "Reading reference genotype data ..." << endl;
        foutLog << endl << asctime (timeinfo);
        foutLog << "Reading reference genotype data ..." << endl;

        reader.open();
        while(reader.read_row(tokens, '\t') >= 0) {
            ++row;
            if (row <= GENO_NON_DATA_ROWS) { // skip any headers
                continue;
            }
            if (ref_ind == Refset[ref_subset_ind]) {
                RefInfo1[ref_subset_ind] = tokens[0];
                RefInfo2[ref_subset_ind] = tokens[1];
                for(unsigned int i = 0; i < LOCI; ++i) {
                    RefD(ref_subset_ind, i) = stof(tokens.at(GENO_NON_DATA_COLS + cmnG(i)));
                }
                ++ref_subset_ind;
                if (ref_subset_ind >= REF_SIZE) {
                    break;
                }
            }
            ++ref_ind;
        }
		reader.close();
	}

	//===============================================================================
	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	cout << endl << asctime (timeinfo);
	cout << "Calculating reference covariance matrix ..." << endl;
	foutLog << endl << asctime (timeinfo);
	foutLog << "Calculating reference covariance matrix ..." << endl;	
	fmat RefMean(LOCI,1);
	fmat RefSD(LOCI,1);
	normalize(RefD, RefMean, RefSD);		
	mat RefM = conv_to<mat>::from(RefD*RefD.t());

	//========================= Get reference coordinates  ==========================
	if(COORD_FILE.compare(default_str)!=0 && GENO_FILE.compare(default_str)!=0){		
		fin.open(COORD_FILE.c_str());
		if(fin.fail()){
			cerr << "Error: cannot find the COORD_FILE '" << COORD_FILE << "'." << endl;
			foutLog << "Error: cannot find the COORD_FILE '" << COORD_FILE << "'." << endl;  
			foutLog.close();
			gsl_rng_free(rng);	
			return 1;
		}
		time ( &rawtime );
		timeinfo = localtime ( &rawtime );
		cout << endl << asctime (timeinfo);
		cout << "Reading reference PCA coordinates ..." << endl;
		foutLog << endl << asctime (timeinfo);
		foutLog << "Reading reference PCA coordinates ..." << endl;
		for(i=0; i<COORD_NON_DATA_ROWS; i++){
			getline(fin, str);          // Read non-data rows
		}
		int ii = 0;
		for(i=0; i<REF_INDS; i++){
			if(i==Refset[ii]){
				string popstr;
				string indstr;
				fin >> popstr >> indstr;
				if(popstr.compare(RefInfo1[ii])!=0 || indstr.compare(RefInfo2[ii])!=0){
					cerr << "Error: ID of individual " << i+1 << " in the COORD_FILE differs from that in the GENO_FILE." << endl;
					foutLog << "Error: ID of individual " << i+1 << " in the COORD_FILE differs from that in the GENO_FILE." << endl;
					fin.close();
					foutLog.close();
					gsl_rng_free(rng);	
					return 1;
				}
				for(j=2; j<COORD_NON_DATA_COLS; j++){
					fin >> str;
				}
				for(j=0; j<DIM; j++){
					fin >> refPC(ii,j);    // Read reference coordiantes
				}
				getline(fin, str);  // Read the rest of the line
				ii++;
			}else{
				getline(fin, str);  // Read the rest of the line
			}
		}
		if(!fin.good()){
			fin.close();
			cerr << "Error: ifstream error occurs when reading the COORD_FILE." << endl;
			foutLog << "Error: ifstream error occurs when reading the COORD_FILE." << endl;
			foutLog.close();
			gsl_rng_free(rng);	
			return 1;
		}
		fin.close();
	}else{
		time ( &rawtime );
		timeinfo = localtime ( &rawtime );
		cout << endl << asctime (timeinfo);
		cout << "Performing PCA on reference individuals ..." << endl;
		foutLog << endl << asctime (timeinfo);
		foutLog << "Performing PCA on reference individuals ..." << endl;	
		rowvec PCvar = zeros<rowvec>(DIM);
		pca_cov(RefM, DIM, refPC, PCvar);   // Perform PCA
		//==================== Output reference PCs ==========================
		outfile = OUT_PREFIX;
		outfile.append(".RefPC.coord");
		fout.open(outfile.c_str());
		if(fout.fail()){
			cerr << "Error: cannot create a file named " << outfile << "." << endl;
			foutLog << "Error: cannot create a file named " << outfile << "." << endl;  
			foutLog.close();
			return 1;
		}		
		fout << "popID" << "\t" << "indivID" << "\t";
		for(j=0; j<DIM-1; j++){ fout << "PC" << j+1 << "\t"; }
		fout << "PC" << DIM << endl; 
		for(i=0; i<REF_SIZE; i++){
			fout << RefInfo1[i] << "\t" << RefInfo2[i] << "\t";
			for(j=0; j<DIM-1; j++){
				fout << refPC(i,j) << "\t";
			}
			fout << refPC(i,DIM-1) << endl;	
		}		
		fout.close();
		cout << "Reference PCA coordinates are output to '" << outfile << "'." << endl;
		foutLog << "Reference PCA coordinates are output to '" << outfile << "'." << endl;
		//==================================================================
		outfile = OUT_PREFIX;
		outfile.append(".RefPC.var");
		fout.open(outfile.c_str());
		if(fout.fail()){
			cerr << "Error: cannot create a file named " << outfile << "." << endl;
			foutLog << "Error: cannot create a file named " << outfile << "." << endl;  
			foutLog.close();
			return 1;
		}
		fout << "PC" << "\t" << "Variance(%)" << endl;
		for(j=0; j<DIM; j++){ 
			fout << j+1 << "\t" << PCvar(j) << endl;
		}	
		fout.close();
		PCvar.clear();
		cout << "Variances explained by PCs are output to '" << outfile << "'." << endl;
		foutLog << "Variances explained by PCs are output to '" << outfile << "'." << endl;		
	}
			
	//========================= Read genotype data of the study sample ==========================

    TableReader reader;
    vector<string> tokens;
    int row = 0;
    i = 0;

    reader.set_file_name(STUDY_FILE.c_str());
    reader.open();

	//==== Open output file ====
	outfile = OUT_PREFIX;
	outfile.append(".ProPC.coord");
	fout.open(outfile.c_str());
	if(fout.fail()) {
		delete [] RefInfo1;
		delete [] RefInfo2;
		cerr << "Error: cannot create a file named " << outfile << "." << endl;
		foutLog << "Error: cannot create a file named " << outfile << "." << endl;
		foutLog.close();
		gsl_rng_free(rng);
		return 1;
	}
	fout << "popID\t" << "indivID\t" << "L\t" << "K\t" << "t\t" << "Z\t";
	for(j=0; j<DIM-1; j++){ fout << "PC" << j+1 << "\t"; }
	fout << "PC" << DIM << endl;

	//==========================
 	time ( &rawtime );
  	timeinfo = localtime ( &rawtime );
  	cout << endl << asctime (timeinfo);
	cout << "Analyzing study individuals ..." << endl;
  	foutLog << endl << asctime (timeinfo);	
	foutLog << "Analyzing study individuals ..." << endl;

	if (DIM_HIGH == 0) {
		AUTO_MODE = true;
	}

	while (reader.read_row(tokens, '\t') >= 0) {
	    ++row;
	    if (row <= STUDY_NON_DATA_ROWS) { // skip header lines;
	        continue;
	    }
	    ++i;
        if (i < FIRST_IND) {
            continue;
        }
        if (i > LAST_IND) {
            break;
        }
        string Info1 = tokens[0];
		string Info2 = tokens[1];
		frowvec tmpG(LOCI_S);
		frowvec G_one(LOCI);
		int Lm = 0;           // Number of loci that are missing data

        for (j = 0; j < LOCI; ++j) {
            G_one(j) =  stof(tokens.at(STUDY_NON_DATA_COLS + cmnS(j)));
            if (G_one(j) == -9) {
                Lm++;
            } else if (MASK_PROP > 0) {
                if(gsl_rng_uniform(rng) < MASK_PROP) {
                    G_one(j) = -9;
                    Lm++;
                }
            }
        }

        if((LOCI-Lm) >= MIN_LOCI){
            //=================== Calculate covariance matrix ======================
            uvec mSites(Lm);
            uvec nSites(LOCI-Lm);
            int sm=0;
            int sn=0;
            frowvec D_one = zeros<frowvec>(LOCI);
            for(j=0; j<LOCI; j++){
                if(G_one(j)!=-9 && RefSD(j)!=0){
                    D_one(j) = (G_one(j)-RefMean(j))/RefSD(j);
                }
                if(G_one(j)==-9){
                    mSites(sm) = j;
                    sm++;
                }else{
                    nSites(sn) = j;
                    sn++;
                }
            }
            mat M;
            if(sn>sm){
                if(sm>0){
                    fmat subD = RefD.cols(mSites);
                    M = RefM-conv_to<mat>::from(subD*subD.t());
                }else{
                    M = RefM;
                }
            }else{
                fmat subD = RefD.cols(nSites);
                M = conv_to<mat>::from(subD*subD.t());
            }
            mSites.clear();
            nSites.clear();
            mat tmprow = conv_to<mat>::from(D_one*RefD.t());
            M.insert_rows(REF_SIZE, tmprow);
            mat tmpmat = conv_to<mat>::from(D_one*D_one.t());
            tmprow.insert_cols(REF_SIZE, tmpmat);
            M.insert_cols(REF_SIZE, tmprow.t());
            G_one.clear();
            D_one.clear();
            tmprow.clear();
            tmpmat.clear();
            // =============================== Eigen Decomposition ==============================
            vec eigval;
            mat eigvec;
            eig_sym(eigval, eigvec, M, "dc");

            //ofstream foutM;
            //foutM.open("testM.txt");
            //foutM << M << endl;
            //foutM.close();
            // M.clear();

            // ####    Calculate Tracy-Widom Statistics and determine DIM_HIGH  ####
            // Calculation of TW statistic follows Patterson et al 2006 PLoS Genetics
            if(AUTO_MODE){
                DIM_HIGH = 0;
                double eigsum = 0;
                double eig2sum = 0;
                double eigsum2 = 0;
                for(j=0; j<REF_SIZE; j++){     // The length of eigval is REF_SIZE+1;
                    eigsum += eigval(j+1);
                    eig2sum += pow(eigval(j+1), 2);
                }
                for(j=0; j<REF_SIZE; j++){
                    int m = REF_SIZE-j;
                    if(j>0){
                        eigsum -= eigval(m+1);
                        eig2sum -= pow(eigval(m+1),2);
                    }
                    eigsum2 = eigsum*eigsum;
                    double n = (m+1)*eigsum2/((m-1)*eig2sum-eigsum2);
                    double nsqrt = sqrt(n-1);
                    double msqrt = sqrt(m);
                    double mu = pow(nsqrt+msqrt, 2)/n;
                    double sigma = (nsqrt+msqrt)/n*pow(1/nsqrt+1/msqrt, 1.0/3);
                    double x = (m*eigval(m)/eigsum-mu)/sigma;  // Tracy-Widom statistic
                    if(x>TW){           // TW is the threshold for the Tracy-Widom statisic
                        DIM_HIGH++;
                    }else{
                        break;
                    }
                }
                if(DIM_HIGH<DIM){
                    DIM_HIGH = DIM;
                    cout << "Warning: DIM is greater than the number of significant PCs for study sample " << i << "." << endl;
                    foutLog << "Warning: DIM is greater than the number of significant PCs for study sample " << i << "." << endl;
                }
            }
            //#################################################################################

            rowvec PC_one = zeros<rowvec>(DIM_HIGH);
            mat refPC_new = zeros<mat>(REF_SIZE,DIM_HIGH);
            for(j=0; j<DIM_HIGH; j++){
                for(k=0; k<REF_SIZE; k++){
                    refPC_new(k,j) = eigvec(k, REF_SIZE-j)*sqrt(eigval(REF_SIZE-j));
                }
                PC_one(j) = eigvec(REF_SIZE, REF_SIZE-j)*sqrt(eigval(REF_SIZE-j));
            }
            eigval.clear();
            eigvec.clear();
            //=================  Procrustes Analysis =======================
            mat refPC_rot(REF_SIZE, DIM_HIGH);
            double t;
            double rho;
            mat A(DIM_HIGH, DIM_HIGH);
            rowvec b(DIM_HIGH);
            double epsilon = pprocrustes(refPC_new, refPC, refPC_rot, t, rho, A, b, MAX_ITER, THRESHOLD, PROCRUSTES_SCALE);
            if(epsilon>THRESHOLD){
                cout << "Warning: Projection Procrustes analysis doesn't converge in " << MAX_ITER << " iterations for " << Info2 <<", THRESHOLD=" << THRESHOLD << "." << endl;
                foutLog << "Warning: Projection Procrustes analysis doesn't converge in " << MAX_ITER << " iterations for " << Info2 <<", THRESHOLD=" << THRESHOLD << "." << endl;
            }
            refPC_new.clear();
            refPC_rot.clear();
            rowvec rotPC_one = rho*PC_one*A+b;
            if(DIM_HIGH > DIM){
                rotPC_one.shed_cols(DIM, DIM_HIGH-1);
            }

            //== Calculating Z score to indicate if an individual's ancestry is represented in the reference ==
            vec d1 = zeros<vec>(REF_SIZE);
            for(j=0; j<REF_SIZE; j++){
                rowvec v = rotPC_one-refPC.row(j);
                for(k=0; k<DIM; k++) d1(j) += v(k)*v(k);
            }
            uvec idx = sort_index(d1);
            vec Mk = zeros<vec>(KNN_ZSCORE);
            for(j=0; j<KNN_ZSCORE; j++) Mk(j) = M(idx(j),idx(j));
            double Z = (M(REF_SIZE,REF_SIZE)-mean(Mk))/stddev(Mk);

            //================= Output Procrustes Results ===================
            fout << Info1 << "\t" << Info2 << "\t" << (LOCI-Lm) << "\t" << DIM_HIGH << "\t" << t << "\t" << Z << "\t";
            for(j=0; j<DIM-1; j++){
                fout << rotPC_one(j) << "\t";
            }
            fout << rotPC_one(DIM-1) << endl;
        }else{
            fout << Info1 << "\t" << Info2 << "\t" << (LOCI-Lm) << "\t" << "NA" << "\t" << "NA" << "\t" << "NA" << "\t";
            for(j=0; j<DIM-1; j++){
                fout << "NA" << "\t";
            }
            fout << "NA" << endl;
        }
        if(i%100==0){
            cout << "Progress: finish analysis of individual " << i << "." << endl;
            foutLog << "Progress: finish analysis of individual " << i << "." << endl;
        }
	}

	gsl_rng_free(rng);
	delete [] RefInfo1;
	delete [] RefInfo2;

	reader.close();

	fout.close();
	cout << "Procrustean PCA coordinates are output to '" << outfile << "'." << endl;
	foutLog << "Procrustean PCA coordinates are output to '" << outfile << "'." << endl;

	//#########################################################################
	time ( &rawtime );
  	timeinfo = localtime ( &rawtime );
	t2 = clock();
	runningtime = (t2-t1)/CLOCKS_PER_SEC;
	cout << endl << "Finished at: " << asctime (timeinfo);
	//cout << "Total CPU time: " << runningtime << " seconds." << endl;
	cout << "=====================================================================" <<endl;
	foutLog << endl << "Finished at: " << asctime (timeinfo);
	//foutLog << "Total CPU time: " << runningtime << " seconds." << endl;
	foutLog << "=====================================================================" <<endl;
	foutLog.close();	
	return 0;
}
//##########################################################################################################
bool parse_cmd_line(int argc, char* argv[], map<string,string> &args, map<string,int> &argi, map<string,double> &argd){
	bool flag=1;
	//Populate with default values
	args[ARG_PARAM_FILE] = default_str;
	args[ARG_STUDY_FILE] = default_str;
	args[ARG_GENO_FILE] = default_str;
	args[ARG_COORD_FILE] = default_str;
	args[ARG_OUT_PREFIX] = default_str;
 	argi[ARG_DIM] = default_int;
	argi[ARG_DIM_HIGH] = default_int;
 	argi[ARG_MIN_LOCI] = default_int;
	argi[ARG_REF_SIZE] = default_int;	
	argi[ARG_FIRST_IND] = default_int;	
	argi[ARG_LAST_IND] = default_int;
	argd[ARG_MASK_PROP] = default_double;
	argd[ARG_ALPHA] = default_double;
	argd[ARG_THRESHOLD] = default_double;	
	argd[ARG_TRIM_PROP] = default_double;
	args[ARG_EXCLUDE_LIST] = default_str;
	argi[ARG_PROCRUSTES_SCALE] = default_int;
	argi[ARG_RANDOM_SEED] = default_int;
	argi[ARG_KNN_ZSCORE] = default_int;
	argi[ARG_NUM_THREADS] = default_int;
	
	for(int i = 1; i < argc-1; i++){
		if(args.count(argv[i]) > 0){
	  		args[argv[i]] = argv[i+1];
			i++;
		}else if(argi.count(argv[i]) > 0){
			if(is_int(argv[i+1])){
	  			argi[argv[i]] = atoi(argv[i+1]);
				i++;
			}else{
				cerr <<"Error: "<<"invalid value for "<<argv[i]<<"." << endl;
				foutLog <<"Error: "<<"invalid value for "<<argv[i]<<"." << endl;
				flag=0;
			}
		}else if(argd.count(argv[i]) > 0){
			if(is_numeric(argv[i+1])){
	  			argd[argv[i]] = atof(argv[i+1]);
				i++;
			}else{
				cerr <<"Error: "<<"invalid value for "<<argv[i]<<"." << endl;
				foutLog <<"Error: "<<"invalid value for "<<argv[i]<<"." << endl;
				flag=0;
			}
		}else{
			cerr << "Error: " << argv[i] << " is not recognized as a valid argument." << endl;
			foutLog << "Error: " << argv[i] << " is not recognized as a valid argument." << endl;
			flag=0;
		}
	}
	return flag;
}
//############## Check if a string is an integer #######################
bool is_int(string str){
	bool flag=1;
	for(int i=0; i<str.length(); i++){
		if( str[i] < '0' || str[i] > '9' && !(i==0 && str[i]=='-')){
			flag=0;         // not an integer number
		}
	}
	return flag;
}
//############## Check if a string is a number #######################
bool is_numeric(string str){
	bool flag=1;
	bool dot_flag=0;

	for(int i=0; i<str.length(); i++){
		if( str[i] < '0' || str[i] > '9' ){
			if(str[i] == '.' && dot_flag==0){
				dot_flag = 1;
			}else if(!(i==0 && str[i]=='-')){
				flag = 0;
			}
		}
	}
	return flag;
}
//#########################     Normalization      ##########################
int normalize(fmat &G, fmat &Gm, fmat &Gsd){
	int i=0;
	int j=0;
	int N = G.n_rows;
	int L = G.n_cols;
	Gm = mean(G,0);
	for (j = 0; j < L; j++) {
		fvec Gj = G.col(j);
		uvec mis = find(Gj == -9);  //find missing elements
		int M = mis.n_elem;
		if (M > 0) { // TODO: Should we do a sanity check and report if M is very high. Average imputation will not work if there are a lot of missing values.
            Gm(j) = (Gm(j) * N + 9 * M) / (N - M);
			for (i = 0; i < M; i++) {
				G(mis(i),j) = Gm(j);
			}
		}
	}
	Gsd = stddev(G, 0);
 	for (int j = 0; j < L; j++){
		if (Gsd(j) == 0) {    // Monophmorphic sites are set to 0
			G.col(j) = zeros<fvec>(N);
		} else {
			G.col(j) = (G.col(j) - Gm(j)) / Gsd(j);
		}
	}
	return 1;
}
// #########################     PCA      ##########################
int pca_cov(mat &M, int nPCs, mat &PC, rowvec &PCvar){
	int i=0;
	int j=0;
	int N = M.n_rows;
	//===================== Perform eigen decomposition =======================
	vec eigval;
	mat eigvec;	
	eig_sym(eigval, eigvec, M, "dc");	
	double eigsum = sum(eigval);
	vec propvar = eigval/eigsum*100;		
	PCvar = zeros<rowvec>(nPCs);
	for(j=0; j<nPCs; j++){
		PCvar(j) = propvar(N-1-j);
		for(i=0; i<N; i++){
			PC(i,j) = eigvec(i, N-1-j)*sqrt(eigval(N-1-j));
		}	
	}
	return 1;
}
//######################### Standard Procrustes Analysis ##########################
int procrustes(mat &X, mat &Y, mat &Xnew, double &t, double &rho, mat &A, rowvec &b, int ps){
	int NUM = X.n_rows;
	//======================= Center to mean =======================
	mat Xm = mean(X);
	mat Ym = mean(Y);
	mat Xc = X-repmat(Xm, NUM, 1);
	mat Yc = Y-repmat(Ym, NUM, 1);
	//======================  SVD =====================
	mat C = Yc.t()*Xc;
	mat U;
	vec s;
	mat V;
	bool bflag = svd(U, s, V, C, "dc");	// use "divide & conquer" algorithm
	//bool bflag = svd(U, s, V, C);
	if(!bflag){
		cout << "Error: singular value decomposition in procrustes() fails." << endl;
		return 0;
	}
	//===================== Transformation ===================
	double trXX = trace(Xc.t()*Xc);
	double trYY = trace(Yc.t()*Yc);
	double trS = sum(s);
	A = V*U.t(); 
	if(ps==1){     // Orthogonal Procrustes analysis, match variance between X and Y
		rho = sqrt(trYY/trXX);
	}else{ 
		rho = trS/trXX;
	}
	b = Ym-rho*Xm*A;
	//============= New coordinates and similarity score ========
	Xnew = rho*X*A+repmat(b, NUM, 1);	
	mat Z = Y-Xnew;
	double d = trace(Z.t()*Z);
	double D = d/trYY;
	t = sqrt(1-D);
	return 1;
}
//######################### Projection Procrustes Analysis ##########################
double pprocrustes(mat &X, mat &Y, mat &Xnew, double &t, double &rho, mat &A, rowvec &b, int iter, double eps, int ps){
	double epsilon = 0;
	int NUM = X.n_rows;
	int DimX = X.n_cols;
	int DimY = Y.n_cols;
	int i = 0;
	if(DimX<DimY){
		cout << "Error: dimension of Y cannot be higher than dimension of X." <<endl;
		return 0;
	}else if(DimX==DimY){
		procrustes(X, Y, Xnew, t, rho, A, b, ps);
		return 0;
	}else{
		mat Z = zeros<mat>(NUM, DimX-DimY);
		for(i=0; i<iter; i++){
			mat W = Y;
			W.insert_cols(DimY, Z);
			double tt;
			procrustes(X, W, Xnew, tt, rho, A, b, ps);
			mat Znew = Xnew;
			Znew.shed_cols(0,(DimY-1));
			mat Zm = mean(Znew);
			mat Zc = Znew-repmat(Zm, NUM, 1);
			mat Zd = Znew-Z;
			epsilon = trace(Zd.t()*Zd)/trace(Zc.t()*Zc);
			if(epsilon<=eps){
				break;
			}else{
				Z = Znew;
			}
		}	
		mat Xnew2;
		mat A2;
		rowvec b2;
		double rho2;
		mat X2 = Xnew;
		X2.shed_cols(DimY, (DimX-1));
		procrustes(X2, Y, Xnew2, t, rho2, A2, b2, ps);
		return epsilon; 
	}
}

//################# Function to create an empty paramfile  ##################
int create_paramfile(string filename){
	ofstream fout;
	fout.open(filename.c_str());
	if(fout.fail()){
		cerr << "Error: cannot create a file named '" << filename << "'." << endl;
		return 0;
	}	
	fout << "# This is a parameter file for TRACE v1.03." << endl;
	fout << "# The entire line after a '#' will be ignored." <<endl;
	fout << "\n" << "###----Main Parameters----###" <<endl;
	fout << endl << "STUDY_FILE         # File name of the study genotype data (include path if in a different directory)" <<endl;
	fout << endl << "GENO_FILE          # File name of the reference genotype data (include path if in a different directory)" <<endl;
	fout << endl << "COORD_FILE         # File name of the reference coordinates (include path if in a different directory)" <<endl;
	fout << endl << "OUT_PREFIX         # Prefix of output files (include path if output to a different directory, default \"trace\")" <<endl;
	fout << endl << "DIM                # Number of PCs to compute (must be a positive integer; default 2)" <<endl;
	fout << endl << "DIM_HIGH           # Number of informative PCs for projection (must be a positive integer >= DIM; default 20)" <<endl;
	fout << endl << "MIN_LOCI           # Minimum number of non-missing loci in a sample (must be a positive integer; default 100)" <<endl;	
	
	fout << "\n\n" << "###----Advanced Parameters----###" <<endl;
	fout << endl << "ALPHA              # Significance level to determine informative PCs (must be a number between 0 and 1; default 0.1)" <<endl;	
	fout <<         "                   # This parameter is effective only if DIM_HIGH is undefined or set to 0." <<endl;
	fout << endl << "THRESHOLD          # Convergence criterion of the projection Procrustes analysis (must be a positive number; default 0.000001)" <<endl;		
	fout << endl << "FIRST_IND          # Index of the first sample to analyze (must be a positive integer; default 1)" <<endl;
	fout <<         "                   # This parameter cannot be modified if GENO_FILE is undefined." <<endl;
	fout << endl << "LAST_IND           # Index of the last sample to analyze (must be a positive integer; default [last sample in the STUDY_FILE])" <<endl;
	fout <<         "                   # This parameter cannot be modified if GENO_FILE is undefined." <<endl;
	fout << endl << "REF_SIZE           # Number of individuals randomly selected as the reference (must be a positive integer; default [sample size in the GENO_FILE])" <<endl;
	fout << endl << "TRIM_PROP          # Proportion of shared loci to be trimmed off for all samples (must be a number between 0 and 1; default 0)" <<endl;
	fout << endl << "MASK_PROP          # Proportion of loci to be randomly masked in each study sample (must be a number between 0 and 1; default 0)" <<endl;
	fout << endl << "EXCLUDE_LIST       # File name of a list of SNPs to exclude from the analysis (include path if in a different directory)" <<endl;
	fout << endl << "PROCRUSTES_SCALE   # Methods to calculate the scaling parameter in Procrustes analysis (must be 0 or 1; default 0)" <<endl; 
	fout <<	        "                   # 0: Calculate the scaling parameter to maximize the Procrustes similarity" <<endl; 
	fout <<         "                   # 1: Fix the scaling parameter to match the variance of two sets of coordinates in Procrustes analysis" <<endl;
	fout << endl << "KNN_ZSCORE         # Number of nearest neigbors used to calculate the Z score for each study individual (must be an integer >2; default 10)" <<endl;
	fout << endl << "RANDOM_SEED        # Seed for the random number generator in the program (must be a non-negative integer; default 0)" <<endl;
	fout << endl << "NUM_THREADS        # Number of CPU cores for multi-threading parallel analysis (must be a positive integer; default 8)" <<endl; 
	
 	fout << "\n\n" << "###----Command line arguments----###" <<endl <<endl;
	fout << "# -p     parameterfile (this file)" <<endl;
	fout << "# -s     STUDY_FILE" <<endl;
	fout << "# -g     GENO_FILE" <<endl;
	fout << "# -c     COORD_FILE" <<endl;
	fout << "# -o     OUT_PREFIX" <<endl;
	fout << "# -k     DIM" <<endl;
	fout << "# -K     DIM_HIGH" <<endl;
	fout << "# -l     MIN_LOCI" <<endl;
	fout << "# -a     ALPHA" << endl;
	fout << "# -t     THRESHOLD" <<endl;	
	fout << "# -x     FIRST_IND" <<endl;
	fout << "# -y     LAST_IND" <<endl;
	fout << "# -N     REF_SIZE" <<endl;	
	fout << "# -M     TRIM_PROP" <<endl;
	fout << "# -m     MASK_PROP" <<endl;	
	fout << "# -ex    EXCLUDE_LIST" <<endl;
	fout << "# -rho   PROCRUSTES_SCALE" << endl;
	fout << "# -knn   KNN_ZSCORE" << endl;
	fout << "# -seed  RANDOM_SEED" << endl;
	fout << "# -nt    NUM_THREADS" << endl;
	
	fout << "\n" << "###----End of file----###";
	fout.close();
	cout << "An empty template parameter file named '"<< filename << "' has been created." << endl;
	foutLog << "An empty template parameter file named '"<< filename << "' has been created." << endl;
	return 1;
}		
//################# Function to read and check the paramfile  ##################
int read_paramfile(string filename){	
	int flag = 1;
	ifstream fin;
	fin.open(filename.c_str());
	if(fin.fail()){
		cerr << "Warning: cannot find the PARAM_FILE '" << filename << "'." << endl;
		foutLog << "Warning: cannot find the PARAM_FILE '" << filename << "'." << endl;
		create_paramfile(filename);
		return flag;
	}
	string str;
	while(!fin.eof()){
		fin>>str;
		if(str[0]=='#'){
			getline(fin, str);
		}else if(str.compare("STUDY_FILE")==0){
			fin>>str;
			if(str[0]!='#'){
				if(STUDY_FILE == default_str){
					STUDY_FILE = str;
				}
			}else{
				getline(fin, str);
			}
		}else if(str.compare("GENO_FILE")==0){
			fin>>str;
			if(str[0]!='#'){
				if(GENO_FILE == default_str){
					GENO_FILE = str;
				}
			}else{
				getline(fin, str);
			}			
		}else if(str.compare("COORD_FILE")==0){
			fin>>str;
			if(str[0]!='#'){
				if(COORD_FILE == default_str){
					COORD_FILE = str;
				}
			}else{
				getline(fin, str);
			}
		}else if(str.compare("EXCLUDE_LIST")==0){
			fin>>str;
			if(str[0]!='#'){
				if(EXCLUDE_LIST == default_str){
					EXCLUDE_LIST = str;
				}
			}else{
				getline(fin, str);
			}				
		}else if(str.compare("OUT_PREFIX")==0){
			fin>>str;
			if(str[0]!='#'){
				if(OUT_PREFIX == default_str){
					OUT_PREFIX = str;
				}
			}else{
				getline(fin, str);
			}
		}else if(str.compare("DIM")==0){
			fin>>str;
			if(str[0]!='#'){
				if(is_int(str) && atoi(str.c_str())>0){
					if(DIM == default_int){
						DIM = atoi(str.c_str());
					}
				}else{
					if(DIM != default_int){
						cerr<< "Warning: DIM in the parameter file is not a positive integer." <<endl;
						foutLog<< "Warning: DIM in the parameter file is not a positive integer." <<endl;
					}else{
						DIM = default_int-1;
					}
				}
			}else{
				getline(fin, str);
			}
		}else if(str.compare("DIM_HIGH")==0){
			fin>>str;
			if(str[0]!='#'){
				if(is_int(str) && atoi(str.c_str())>=0){
					if(DIM_HIGH == default_int){
						DIM_HIGH = atoi(str.c_str());
					}
				}else{
					if(DIM_HIGH != default_int){
						cerr<< "Warning: DIM_HIGH in the parameter file is not a positive integer." <<endl;
						foutLog<< "Warning: DIM_HIGH in the parameter file is not a positive integer." <<endl;
					}else{
						DIM_HIGH = default_int-1;
					}
				}
			}else{
				getline(fin, str);
			}			
		}else if(str.compare("MIN_LOCI")==0){
			fin>>str;
			if(str[0]!='#'){
				if(is_int(str) && atoi(str.c_str())>0){
					if(MIN_LOCI == default_int){
						MIN_LOCI = atoi(str.c_str());
					}
				}else{
					if(MIN_LOCI != default_int){
						cerr<< "Warning: MIN_LOCI in the parameter file is not a positive integer." <<endl;
						foutLog<< "Warning: MIN_LOCI in the parameter file is not a positive integer." <<endl;
					}else{
						 MIN_LOCI = default_int-1;
					}
				}
			}else{
				getline(fin, str);
			}
		}else if(str.compare("REF_SIZE")==0){
			fin>>str;
			if(str[0]!='#'){
				if(is_int(str) && atoi(str.c_str())>0){
					if(REF_SIZE == default_int){
						REF_SIZE = atoi(str.c_str());
					}
				}else{
					if(REF_SIZE != default_int){
						cerr<< "Warning: REF_SIZE in the parameter file is not a positive integer." <<endl;
						foutLog<< "Warning: REF_SIZE in the parameter file is not a positive integer." <<endl;
					}else{
						 REF_SIZE = default_int-1;
					}
				}
			}else{
				getline(fin, str);
			}									
		}else if(str.compare("FIRST_IND")==0){
			fin>>str;
			if(str[0]!='#'){
				if(is_int(str) && atoi(str.c_str())>0){
					if(FIRST_IND == default_int){
						FIRST_IND = atoi(str.c_str());
					}
				}else{
					if(FIRST_IND != default_int){
						cerr<< "Warning: FIRST_IND in the parameter file is not a positive integer." <<endl;
						foutLog<< "Warning: FIRST_IND in the parameter file is not a positive integer." <<endl;
					}else{
						 FIRST_IND = default_int-1;
					}
				}
			}else{
				getline(fin, str);
			}
		}else if(str.compare("LAST_IND")==0){
			fin>>str;
			if(str[0]!='#'){
				if(is_int(str) && atoi(str.c_str())>0){
					if(LAST_IND == default_int){
						LAST_IND = atoi(str.c_str());
					}
				}else{
					if(LAST_IND != default_int){
						cerr<< "Warning: LAST_IND in the parameter file is not a positive integer." <<endl;
						foutLog<< "Warning: LAST_IND in the parameter file is not a positive integer." <<endl;
					}else{
						 LAST_IND = default_int-1;
					}
				}
			}else{
				getline(fin, str);
			}
		}else if(str.compare("TRIM_PROP")==0){
			fin>>str;
			if(str[0]!='#'){
				if(is_numeric(str) && atof(str.c_str())>=0 && atof(str.c_str())<=1){
					if(TRIM_PROP == default_double){
						TRIM_PROP = atof(str.c_str());
					}
				}else{
					if(TRIM_PROP != default_double){
						cerr<< "Warning: TRIM_PROP in the parameter file is not between 0 and 1." <<endl;
						foutLog<< "Warning: TRIM_PROP in the parameter file is not between 0 and 1." <<endl;
					}else{
						TRIM_PROP  = default_double-1;
					}
				}
			}else{
				getline(fin, str);
			}			
		}else if(str.compare("MASK_PROP")==0){
			fin>>str;
			if(str[0]!='#'){
				if(is_numeric(str) && atof(str.c_str())>=0 && atof(str.c_str())<=1){
					if(MASK_PROP == default_double){
						MASK_PROP = atof(str.c_str());
					}
				}else{
					if(MASK_PROP != default_double){
						cerr<< "Warning: MASK_PROP in the parameter file is not between 0 and 1." <<endl;
						foutLog<< "Warning: MASK_PROP in the parameter file is not between 0 and 1." <<endl;
					}else{
						MASK_PROP  = default_double-1;
					}
				}
			}else{
				getline(fin, str);
			}			
		}else if(str.compare("ALPHA")==0){
			fin>>str;
			if(str[0]!='#'){
				if(is_numeric(str) && atof(str.c_str())>=0 && atof(str.c_str())<=1){
					if(ALPHA == default_double){
						ALPHA = atof(str.c_str());
					}
				}else{
					if(ALPHA != default_double){
						cerr<< "Warning: ALPHA in the parameter file is not between 0 and 1." <<endl;
						foutLog<< "Warning: ALPHA in the parameter file is not between 0 and 1." <<endl;
					}else{
						ALPHA  = default_double-1;
					}
				}
			}else{
				getline(fin, str);
			}			
		}else if(str.compare("THRESHOLD")==0){
			fin>>str;
			if(str[0]!='#'){
				if(is_numeric(str) && atof(str.c_str())>=0){
					if(THRESHOLD == default_double){
						THRESHOLD = atof(str.c_str());
					}
				}else{
					if(THRESHOLD != default_double){
						cerr<< "Warning: THRESHOLD in the parameter file is not a positive number." <<endl;
						foutLog<< "Warning: THRESHOLD in the parameter file is not a positive number." <<endl;
					}else{
						THRESHOLD  = default_double-1;
					}
				}
			}else{
				getline(fin, str);
			}
		}else if(str.compare("PROCRUSTES_SCALE")==0){
			fin>>str;
			if(str[0]!='#'){
				if(str=="0" || str=="1"){
					if(PROCRUSTES_SCALE == default_int){
						PROCRUSTES_SCALE = atoi(str.c_str());
					}
				}else{
					if(PROCRUSTES_SCALE != default_int){
						cerr<< "Warning: PROCRUSTES_SCALE in the parameter file is not 0 or 1." <<endl;
						foutLog<< "Warning: PROCRUSTES_SCALE in the parameter file is not 0 or 1." <<endl;
					}else{
						PROCRUSTES_SCALE  = default_int-1;
					}
				}
			}else{
				getline(fin, str);
			}
		}else if(str.compare("RANDOM_SEED")==0){
			fin>>str;
			if(str[0]!='#'){
				if(is_int(str) && atoi(str.c_str())>=0){
					if(RANDOM_SEED == default_int){
						RANDOM_SEED = atoi(str.c_str());
					}
				}else{
					if(RANDOM_SEED != default_int){
						cerr<< "Warning: RANDOM_SEED in the parameter file is not a non-negative integer." <<endl;
						foutLog<< "Warning: RANDOM_SEED in the parameter file is not a non-negative integer." <<endl;
					}else{
						RANDOM_SEED = default_int-1;
					}
				}
			}else{
				getline(fin, str);
			}
		}else if(str.compare("KNN_ZSCORE")==0){
			fin>>str;
			if(str[0]!='#'){
				if(is_int(str) && atoi(str.c_str())>2){
					if(KNN_ZSCORE == default_int){
						KNN_ZSCORE = atoi(str.c_str());
					}
				}else{
					if(KNN_ZSCORE != default_int){
						cerr<< "Warning: KNN_ZSCORE in the parameter file is not an integer >2." <<endl;
						foutLog<< "Warning: KNN_ZSCORE in the parameter file is not an integer >2." <<endl;
					}else{
						 KNN_ZSCORE = default_int-1;
					}
				}
			}else{
				getline(fin, str);
			}			
		}else if(str.compare("NUM_THREADS")==0){
			fin>>str;
			if(str[0]!='#'){
				if(is_int(str) && atoi(str.c_str())>0){
					if(NUM_THREADS == default_int){
						NUM_THREADS = atoi(str.c_str());
					}
				}else{
					if(NUM_THREADS != default_int){
						cerr<< "Warning: NUM_THREADS in the parameter file is not a positive integer." <<endl;
						foutLog<< "Warning: NUM_THREADS in the parameter file is not a positive integer." <<endl;
					}else{
						NUM_THREADS = default_int-1;
					}
				}
			}else{
				getline(fin, str);
			}	
		}		
	}
	fin.close();
	return flag;
}
//################# Function to print parameters in execution  ##################
void print_configuration(){
	cout <<endl << "Parameter values used in execution:" <<endl;
	cout << "-------------------------------------------------" << endl;
	cout << "STUDY_FILE (-s)" << "\t" << STUDY_FILE <<endl;
	cout << "GENO_FILE (-g)" << "\t" << GENO_FILE << endl;
	if(COORD_FILE.compare(default_str)!=0){
		cout << "COORD_FILE (-c)" << "\t" << COORD_FILE << endl;
	}
	cout << "OUT_PREFIX (-o)" << "\t" << OUT_PREFIX << endl;		
	cout << "DIM (-k)" << "\t" << DIM << endl;
	if(DIM_HIGH != 0){
		cout << "DIM_HIGH (-K)" << "\t" << DIM_HIGH << endl;
	}else{
		cout << "ALPHA (-a)" << "\t" << ALPHA << endl;
	}
	cout << "THRESHOLD (-t)" << "\t" << THRESHOLD << endl;					
	cout << "MIN_LOCI (-l)" << "\t" << MIN_LOCI << endl;
	cout << "FIRST_IND (-x)" << "\t" << FIRST_IND << endl;
	cout << "LAST_IND (-y)" << "\t" << LAST_IND << endl;
	if(REF_SIZE != REF_INDS){
		cout << "REF_SIZE (-N)" << "\t" << REF_SIZE << endl;
	}
	if(TRIM_PROP > 0){
		cout << "TRIM_PROP (-M)" << "\t" << TRIM_PROP << endl;
	}
	if(MASK_PROP > 0){
		cout << "MASK_PROP (-m)" << "\t" << MASK_PROP << endl;
	}
	if(EXCLUDE_LIST.compare(default_str)!=0){
		cout << "EXCLUDE_LIST (-ex)" << "\t" << EXCLUDE_LIST << endl;
	}
	if(PROCRUSTES_SCALE > 0){
		cout << "PROCRUSTES_SCALE (-rho)" << "\t" << PROCRUSTES_SCALE << endl;
	}
	cout << "KNN_ZSCORE (-knn)" << "\t" << KNN_ZSCORE << endl;
	cout << "RANDOM_SEED (-seed)" << "\t" << RANDOM_SEED << endl;
	cout << "NUM_THREADS (-nt)" << "\t" << NUM_THREADS << endl;
	cout << "-------------------------------------------------" << endl; 

	foutLog <<endl << "Parameter values used in execution:" <<endl;
	foutLog << "-------------------------------------------------" << endl;
	foutLog << "STUDY_FILE (-s)" << "\t" << STUDY_FILE <<endl;
	foutLog << "GENO_FILE (-g)" << "\t" << GENO_FILE << endl;
	if(COORD_FILE.compare(default_str)!=0){
		foutLog << "COORD_FILE (-c)" << "\t" << COORD_FILE << endl;
	}
	foutLog << "OUT_PREFIX (-o)" << "\t" << OUT_PREFIX << endl;		
	foutLog << "DIM (-k)" << "\t" << DIM << endl;
	if(DIM_HIGH != 0){
		foutLog << "DIM_HIGH (-K)" << "\t" << DIM_HIGH << endl;
	}else{
		foutLog << "ALPHA (-a)" << "\t" << ALPHA << endl;
	}
	foutLog << "THRESHOLD (-t)" << "\t" << THRESHOLD << endl;		
	foutLog << "MIN_LOCI (-l)" << "\t" << MIN_LOCI << endl;	
	foutLog << "FIRST_IND (-x)" << "\t" << FIRST_IND << endl;
	foutLog << "LAST_IND (-y)" << "\t" << LAST_IND << endl;
	if(REF_SIZE != REF_INDS){
		foutLog << "REF_SIZE (-N)" << "\t" << REF_SIZE << endl;
	}
	if(TRIM_PROP > 0){
		foutLog << "TRIM_PROP (-M)" << "\t" << TRIM_PROP << endl;
	}
	if(MASK_PROP > 0){
		foutLog << "MASK_PROP (-m)" << "\t" << MASK_PROP << endl;
	}
	if(EXCLUDE_LIST.compare(default_str)!=0){
		foutLog << "EXCLUDE_LIST (-ex)" << "\t" << EXCLUDE_LIST << endl;
	}
	if(PROCRUSTES_SCALE > 0){
		foutLog << "PROCRUSTES_SCALE (-rho)" << "\t" << PROCRUSTES_SCALE << endl;
	}
	foutLog << "KNN_ZSCORE (-knn)" << "\t" << KNN_ZSCORE << endl;
	foutLog << "RANDOM_SEED (-seed)" << "\t" << RANDOM_SEED << endl;
	foutLog << "NUM_THREADS (-nt)" << "\t" << NUM_THREADS << endl;
	foutLog << "-------------------------------------------------" << endl; 
}
//################# Function to check parameter values  ##################
int check_parameters(){
	int flag = 1;
	if(GENO_FILE.compare(default_str)==0){
		cerr << "Error: GENO_FILE (-g) is not specified." << endl;
		foutLog << "Error: GENO_FILE (-g) is not specified." << endl;
		flag = 0;
	}	
	if(STUDY_FILE.compare(default_str)==0){
		cerr << "Error: STUDY_FILE (-s) is not specified." << endl;
		foutLog << "Error: STUDY_FILE (-s) is not specified." << endl;
		flag = 0;
	}
	if(DIM==default_int){
		cerr << "Error: DIM (-k) is not specified." << endl;
		foutLog << "Error: DIM (-k) is not specified." << endl;
		flag = 0;
	}else if(DIM<1){ 
		cerr << "Error: invalid value for DIM (-k)." << endl;
		foutLog << "Error: invalid value for DIM (-k)." << endl;
		flag = 0;
	}else if(REF_SIZE!=default_int && DIM>=REF_SIZE){
		cerr << "Error: invalid value for DIM (-k)." << endl;
		cerr << "DIM must be smaller than REF_SIZE." << endl;
		foutLog << "Error: invalid value for DIM (-k)." << endl;
		foutLog << "DIM must be smaller than REF_SIZE." << endl;
		flag = 0;
	}else if(LOCI!=default_int && DIM>=LOCI){
		cerr << "Error: invalid value for DIM (-k)." << endl;
		cerr << "DIM must be smaller than the total number of shared loci." << endl;
		foutLog << "Error: invalid value for DIM (-k)." << endl;
		foutLog << "DIM must be smaller than the total number of shared loci." << endl;
		flag = 0;
	}else if(NUM_PCS!=default_int && DIM>NUM_PCS){
		cerr << "Error: invalid value for DIM (-k)." << endl;
		cerr << "DIM cannot be greater than the number of PCs in the COORD_FILE." << endl;
		foutLog << "Error: invalid value for DIM (-k)." << endl;
		foutLog << "DIM cannot be greater than the number of PCs in the COORD_FILE." << endl;
		flag = 0;
	}
	if(DIM_HIGH==default_int){
		cerr << "Error: DIM_HIGH (-K) is not specified." << endl;
		foutLog << "Error: DIM_HIGH (-K) is not specified." << endl;
		flag = 0;
	}else if(DIM_HIGH<DIM && DIM_HIGH!=0){ 
		cerr << "Error: invalid value for DIM_HIGH (-K)." << endl;
		cerr << "DIM_HIGH cannot be smaller than DIM." << endl;
		foutLog << "Error: invalid value for DIM_HIGH (-K)." << endl;
		foutLog << "DIM_HIGH cannot be smaller than DIM." << endl;
		flag = 0;
	}else if(REF_SIZE!=default_int && DIM_HIGH>=REF_SIZE){
		cerr << "Error: invalid value for DIM_HIGH (-K)." << endl;
		cerr << "DIM_HIGH must be smaller than REF_SIZE." << endl;
		foutLog << "Error: invalid value for DIM_HIGH (-K)." << endl;
		foutLog << "DIM_HIGH must be smaller than REF_SIZE." << endl;
		flag = 0;
	}else if(LOCI!=default_int && DIM_HIGH>=LOCI){
		cerr << "Error: invalid value for DIM_HIGH (-K)." << endl;
		cerr << "DIM_HIGH must be smaller than the total number of shared loci." << endl;
		foutLog << "Error: invalid value for DIM_HIGH (-K)." << endl;
		foutLog << "DIM_HIGH must be smaller than the total number of shared loci." << endl;
		flag = 0;		
	}	
	if(MIN_LOCI < 1){
		cerr << "Error: invalid value for MIN_LOCI (-l)." << endl;
		foutLog << "Error: invalid value for MIN_LOCI (-l)." << endl;
		flag = 0;	
	}else if(MIN_LOCI>LOCI && LOCI!=default_int){
		cerr << "Error: invalid value for MIN_LOCI (-l)." << endl;
		cerr << "MIN_LOCI cannot be greater than the total number of shared loci." << endl;
		foutLog << "Error: invalid value for MIN_LOCI (-l)." << endl;
		foutLog << "MIN_LOCI cannot be greater than the total number of shared loci." << endl;
		flag = 0;
	}
	if(REF_SIZE < 0){
		cerr << "Error: invalid value for REF_SIZE (-N)." << endl;
		foutLog << "Error: invalid value for REF_SIZE (-N)." << endl;
		flag = 0;	
	}else if(REF_SIZE<=DIM_HIGH){
		cerr << "Error: invalid value for REF_SIZE (-N)." << endl;
		cerr << "REF_SIZE must be greater than DIM_HIGH." << endl;
		foutLog << "Error: invalid value for REF_SIZE (-N)." << endl;
		foutLog << "REF_SIZE must be greater than DIM_HIGH." << endl;
		flag = 0;
	}else if(REF_SIZE>REF_INDS){
		cerr << "Error: invalid value for REF_SIZE (-N)." << endl;
		cerr << "REF_SIZE cannot be greater than the number of individuals in the GENO_FILE." << endl;
		foutLog << "Error: invalid value for REF_SIZE (-N)." << endl;
		foutLog << "REF_SIZE cannot be greater than the number of individuals in the GENO_FILE." << endl;
		flag = 0;	
	}			
	if(FIRST_IND < 0){
		cerr << "Error: invalid value for FIRST_IND (-x)." << endl;
		foutLog << "Error: invalid value for FIRST_IND (-x)." << endl;
		flag = 0;
	}else if(FIRST_IND>INDS && INDS!=default_int){
		cerr << "Error: invalid value for FIRST_IND (-x)." << endl;
		cerr << "FIRST_IND cannot be greater than the number of individuals in the STUDY_FILE." << endl;
		foutLog << "Error: invalid value for FIRST_IND (-x)." << endl;
		foutLog << "FIRST_IND cannot be greater than the number of individuals in the STUDY_FILE." << endl;
		flag = 0;
	}
	if(LAST_IND<0 && LAST_IND!=default_int){
		cerr << "Error: invalid value for LAST_IND (-y)." << endl;
		foutLog << "Error: invalid value for LAST_IND (-y)." << endl;
		flag = 0;	
	}else if(LAST_IND<FIRST_IND && FIRST_IND!=default_int && LAST_IND!=default_int){
		cerr << "Error: invalid value for LAST_IND (-y)." << endl;
		cerr << "LAST_IND cannot be smaller than FIRST_IND." << endl;
		foutLog << "Error: invalid value for LAST_IND (-y)." << endl;
		foutLog << "LAST_IND cannot be smaller than FIRST_IND." << endl;
		flag = 0;
	}else if(LAST_IND>INDS && INDS!=default_int){
		LAST_IND = INDS;
	}
	if(TRIM_PROP < 0 || TRIM_PROP >= 1){
		cerr << "Error: invalid value for TRIM_PROP (-M)." << endl;
		foutLog << "Error: invalid value for TRIM_PROP (-M)." << endl;
		foutLog << "Error: invalid value for TRIM_PROP (-M)." << endl;
		flag = 0;
	}		
	if(MASK_PROP < 0 || MASK_PROP >= 1){
		cerr << "Error: invalid value for MASK_PROP (-m)." << endl;
		foutLog << "Error: invalid value for MASK_PROP (-m)." << endl;
		flag = 0;
	}
	if(ALPHA != 0.2 && ALPHA != 0.15 && ALPHA != 0.1 && ALPHA != 0.05 && ALPHA != 0.01 && ALPHA != 0.005 && ALPHA != 0.001){
		cerr << "Error: invalid value for ALPHA (-a)." << endl;
		cerr << "Current version only allows ALPHA to be 0.001, 0.005, 0.01, 0.05, 0.1, 0.15, or 0.2." << endl;
		foutLog << "Error: invalid value for ALPHA (-a)." << endl;
		foutLog << "Current version only allows ALPHA to be 0.001, 0.005, 0.01, 0.05, 0.1, 0.15, or 0.2." << endl;
		flag = 0;
	}
	if(THRESHOLD <= 0){
		cerr << "Error: invalid value for THRESHOLD (-t)." << endl;
		foutLog << "Error: invalid value for THRESHOLD (-t)." << endl;
		flag = 0;
	}
	if(PROCRUSTES_SCALE!=0 && PROCRUSTES_SCALE!=1){
		cerr << "Error: invalid value for PROCRUSTES_SCALE (-rho)." << endl;
		foutLog << "Error: invalid value for PROCRUSTES_SCALE (-rho)." << endl;
		flag = 0;
	}
	if(KNN_ZSCORE < 3){
		cerr << "Error: invalid value for KNN_ZSCORE (-knn)." << endl;
		cerr << "KNN_ZSCORE need to be an integer greater than 2." << endl;
		foutLog << "Error: invalid value for KNN_ZSCORE (-knn)." << endl;
		foutLog << "KNN_ZSCORE need to be an integer greater than 2." << endl;
		flag = 0;	
	}else if(KNN_ZSCORE>REF_SIZE){
		cerr << "Error: invalid value for KNN_ZSCORE (-knn)." << endl;
		cerr << "KNN_ZSCORE cannot be greater than REF_SIZE." << endl;
		foutLog << "Error: invalid value for KNN_ZSCORE (-knn)." << endl;
		foutLog << "KNN_ZSCORE cannot be greater than REF_SIZE." << endl;
		flag = 0;	
	}	
	if(RANDOM_SEED < 0){
		cerr << "Error: invalid value for RANDOM_SEED (-seed)." << endl;
		foutLog << "Error: invalid value for RANDOM_SEED (-seed)." << endl;
		flag = 0;
	}
	if(NUM_THREADS < 1){
		cerr << "Error: invalid value for NUM_THREADS (-nt)." << endl;
		foutLog << "Error: invalid value for NUM_THREADS (-nt)." << endl;
		flag = 0;
	}	
	//============================================================================
	return flag;
}
