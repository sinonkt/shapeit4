////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Olivier Delaneau, University of Lausanne
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
////////////////////////////////////////////////////////////////////////////////

#include <phaser/phaser_header.h>

void phaser::declare_options() {
	bpo::options_description opt_base ("Basic options");
	opt_base.add_options()
			("help", "Produce help message")
			("seed", bpo::value<int>()->default_value(15052011), "Seed of the random number generator")
			("thread,T", bpo::value<int>()->default_value(1), "Number of thread used");

	bpo::options_description opt_input ("Input files");
	opt_input.add_options()
			("input,I", bpo::value< string >(), "Genotypes to be phased in VCF/BCF format")
			("reference,H", bpo::value< string >(), "Reference panel of haplotypes in VCF/BCF format")
			("scaffold,S", bpo::value< string >(), "Scaffold of haplotypes in VCF/BCF format")
			("map,M", bpo::value< string >(), "Genetic map")
			("region,R", bpo::value< string >(), "Target region")
			("use-PS", bpo::value<double>(), "Informs phasing using PS field from read based phasing");

	bpo::options_description opt_mcmc ("MCMC parameters");
	opt_mcmc.add_options()
			("mcmc-iterations", bpo::value<string>()->default_value("5b,1p,1b,1p,1b,1p,5m"), "Iteration scheme of the MCMC")
			("mcmc-prune", bpo::value<double>()->default_value(0.999), "Pruning threshold in genotype graphs");

	bpo::options_description opt_pbwt ("PBWT parameters");
	opt_pbwt.add_options()
			("pbwt-modulo", bpo::value< double >()->default_value(0.025), "Storage frequency of PBWT indexes in cM (i.e. 0.025 means storage every 0.025 cM)")
			("pbwt-depth", bpo::value< int >()->default_value(4), "Depth of PBWT indexes to condition on")
			("pbwt-mac", bpo::value< int >()->default_value(2), "Minimal Minor Allele Count at which PBWT is evaluated")
			("pbwt-mdr", bpo::value< double >()->default_value(0.050), "Maximal Missing Data Rate at which PBWT is evaluated");
	
	bpo::options_description opt_ibd2 ("IBD2 parameters");
	opt_ibd2.add_options()
			("ibd2-length", bpo::value< double >()->default_value(3), "Minimal size of IBD2 tracks for building copying constraints")
			("ibd2-maf", bpo::value< double >()->default_value(0.01), "Minimal Minor Allele Frequency for variants to be considered in the IBD2 mapping")
			("ibd2-mdr", bpo::value< double >()->default_value(0.050), "Maximal Missing data rate for variants to be considered in the IBD2 mapping")
			("ibd2-count", bpo::value< int >()->default_value(150), "Minimal number of filtered variants in IBD2 tracks")
			("ibd2-output", bpo::value< string >(), "Output all IBD2 constraints in the specified file (useful for debugging!)");

	bpo::options_description opt_hmm ("HMM parameters");
	opt_hmm.add_options()
			("window,W", bpo::value<double>()->default_value(2.5), "Minimal size of the phasing window in cM")
			("effective-size", bpo::value<int>()->default_value(15000), "Effective size of the population");

	bpo::options_description opt_output ("Output files");
	opt_output.add_options()
			("output,O", bpo::value< string >(), "Phased haplotypes in VCF/BCF format")
			("log", bpo::value< string >(), "Log file");

	descriptions.add(opt_base).add(opt_input).add(opt_mcmc).add(opt_pbwt).add(opt_ibd2).add(opt_hmm).add(opt_output);
}

void phaser::parse_command_line(vector < string > & args) {
	try {
		bpo::store(bpo::command_line_parser(args).options(descriptions).run(), options);
		bpo::notify(options);
	} catch ( const boost::program_options::error& e ) { cerr << "Error parsing command line arguments: " << string(e.what()) << endl; exit(0); }

	if (options.count("help")) { cout << descriptions << endl; exit(0); }

	if (options.count("log") && !vrb.open_log(options["log"].as < string > ()))
		vrb.error("Impossible to create log file [" + options["log"].as < string > () +"]");

	vrb.title("SHAPEIT");
	vrb.bullet("Author        : Olivier DELANEAU, University of Lausanne");
	vrb.bullet("Contact       : olivier.delaneau@gmail.com");
	vrb.bullet("Version       : 4.1.1");
	vrb.bullet("Run date      : " + tac.date());
}

void phaser::check_options() {
	if (!options.count("input"))
		vrb.error("You must specify one input file using --input");

	if (!options.count("region"))
		vrb.error("You must specify a region or chromosome to phase using --region");

	if (!options.count("output"))
		vrb.error("You must specify a phased output file with --output");

	if (options.count("seed") && options["seed"].as < int > () < 0)
		vrb.error("Random number generator needs a positive seed value");

	if (options.count("thread") && options["thread"].as < int > () < 1)
		vrb.error("You must use at least 1 thread");

	if (!options["thread"].defaulted() && !options["seed"].defaulted())
		vrb.warning("Using multi-threading prevents reproducing a run by specifying --seed");

	if (!options["effective-size"].defaulted() && options["effective-size"].as < int > () < 1)
		vrb.error("You must specify a positive effective size");

	if (!options["window"].defaulted() && (options["window"].as < double > () < 0.5 || options["window"].as < double > () > 10))
		vrb.error("You must specify a window size comprised between 0.5 and 10 cM");

	parse_iteration_scheme(options["mcmc-iterations"].as < string > ());
}

void phaser::verbose_files() {
	vrb.title("Files:");
	vrb.bullet("Input VCF     : [" + options["input"].as < string > () + "]");
	if (options.count("reference")) vrb.bullet("Reference VCF : [" + options["reference"].as < string > () + "]");
	if (options.count("scaffold")) vrb.bullet("Scaffold VCF  : [" + options["scaffold"].as < string > () + "]");
	if (options.count("map")) vrb.bullet("Genetic Map   : [" + options["map"].as < string > () + "]");
	vrb.bullet("Output VCF    : [" + options["output"].as < string > () + "]");
	if (options.count("log")) vrb.bullet("Output LOG    : [" + options["log"].as < string > () + "]");
}

void phaser::verbose_options() {
	vrb.title("Parameters:");
	vrb.bullet("Seed    : " + stb.str(options["seed"].as < int > ()));
	vrb.bullet("Threads : " + stb.str(options["thread"].as < int > ()) + " threads");
	vrb.bullet("MCMC    : " + get_iteration_scheme());
	vrb.bullet("PBWT    : Depth of PBWT neighbours to condition on: " + stb.str(options["pbwt-depth"].as < int > ()));
	vrb.bullet("PBWT    : Store indexes at variants [MAC>=" + stb.str(options["pbwt-mac"].as < int > ()) + " / MDR<=" + stb.str(options["pbwt-mdr"].as < double > ()) + " / Dist=" + stb.str(options["pbwt-modulo"].as < double > ()) + " cM]");
	vrb.bullet("HMM     : K is variable / min W is " + stb.str(options["window"].as < double > (), 2) + "cM / Ne is "+ stb.str(options["effective-size"].as < int > ()));
	if (options.count("map")) vrb.bullet("HMM     : Recombination rates given by genetic map");
	else vrb.bullet("HMM     : Constant recombination rate of 1cM per Mb");
	if (options.count("use-PS")) vrb.bullet("HMM     : Inform phasing using VCF/PS field / Error rate of PS field is " + stb.str(options["use-PS"].as < double > ()));
#ifdef __AVX2__
	vrb.bullet("HMM     : AVX2 optimization active");
#else
	vrb.bullet("HMM     : !AVX2 optimization inactive!");
#endif
	vrb.bullet("IBD2    : length>=" + stb.str(options["ibd2-length"].as < double > (), 2) + "cM [N>="+ stb.str(options["ibd2-count"].as < int > ()) + " / MAF>=" + stb.str(options["ibd2-maf"].as < double > (), 3) + " / MDR<=" + stb.str(options["ibd2-mdr"].as < double > (), 3) + "]");
	if (options.count("ibd2-output")) vrb.bullet("IBD2    : write IBD2 tracks in [" +  options["ibd2-output"].as < string > () + "]");

}
