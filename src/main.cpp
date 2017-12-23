#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "parser.h"
#include "generator.h"

using std::cout;
using std::cerr;
using std::endl;
using std::fstream;

std::string get_basename(const std::string filename) {
	std::string::size_type ld = filename.find_last_of('.');
	std::string::size_type ls = filename.find_last_of("/\\");
	if (ls != std::string::npos && ld != std::string::npos && ls < ld) {
		return filename.substr(ls + 1, ld - (ls + 1));
	} else if (ls != std::string::npos) {
		return filename.substr(ls + 1);
	} else {
		return filename.substr(0, ld);
	}
	return "";
}

std::string get_basepath(const std::string filename) {
	std::string::size_type ls = filename.find_last_of("/\\");
	if (ls != std::string::npos) {
		return filename.substr(0, ls + 1);
	}
	return "";
}

int main(int argc, char* argv[]) {

	documentgen gen;
	documentgenparser parser;

	if (argc != 2) {
		cout << "usage: dbgenpp [inputfile]" << endl << endl;
		return 1;
	}

	std::string inputfile = argv[1];

	std::string prefix = get_basename(inputfile);
	if (prefix.empty()) {
		cerr << "unable to determine short name from input file name" << endl;
		return 3;
	}

	std::string basepath = get_basepath(inputfile);

	if (!parser.parse_dbgen(inputfile.c_str(), &gen)) {
		return 2;
	}

	std::string outputheaderfile = basepath + prefix + "_types.h";
	std::string outputimplfile = basepath + prefix + "_types_cpp.h";

	fstream outf;
	outf.open(outputimplfile.c_str(), std::ios::trunc | std::ios::out);
	gen.generate_document_implementation(prefix, outf);
	outf.close();

	outf.open(outputheaderfile.c_str(), std::ios::trunc | std::ios::out);
	gen.generate_document_header(prefix, outf);
	outf.close();

	return 0;
}
