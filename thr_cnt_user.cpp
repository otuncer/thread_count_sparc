#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <unistd.h>

#include "constants.h"

using namespace std;


#define FILE_NAME "/devices/pseudo/thr_cnt@0:thr_cnt"

int main(int argc, char* argv[]) {
	int duration_sec = 30;
	
	//replace duration with argument if available
	char *buf = new char[16 * sizeof(int)];
	if(argc > 1) {
		strcpy(buf, argv[1]);
		duration_sec = atoi(buf);
	}
	
	int nsamples = duration_sec * THR_CNT_SAMPLING_RATE;
	cerr << "nsamples=" << nsamples << endl;
	delete [] buf;
	buf = new char[nsamples * sizeof(int)];
	
	vector<int> thread_cnt(nsamples, -1);
	ifstream devfile(FILE_NAME);
	devfile.read(buf, nsamples * sizeof(int));
	memcpy(&thread_cnt[0], buf, nsamples * sizeof(int));

	//write to stdout
	for(int i = 0; i < nsamples; i++)
		cout << thread_cnt[i] << endl;
	
	delete [] buf;

	return 0;
}

