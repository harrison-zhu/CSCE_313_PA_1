/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Harrison Zhu
	UIN: 734001635
	Date: 9/26/2025
*/
#include "common.h"
#include "FIFORequestChannel.h"

using namespace std;


int main (int argc, char *argv[]) {
	int opt;
	int p = -1;
	double t = -1.0;
	int e = -1;
	int m = MAX_MESSAGE;
	bool new_channel = false;
	vector<FIFORequestChannel*> channels;
	
	string filename = "";
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'm':
				m = atoi (optarg);
				break;
			case 'c':
				new_channel = true;
				break;
		}
	}

	// convert int m into cstring
	string m_string = to_string(m);
	int length = m_string.size() + 1;
	char* m_cstr = new char[length];
	strcpy(m_cstr, m_string.c_str());

	char* server_cmd[] = {(char*) "./server", (char*) "-m", m_cstr, NULL};

	// fork to make child process that calls server
	pid_t pid = fork();
	if (pid < 0) {
		cerr << "Fork creation was unseccessful." << endl;
	} else if (pid == 0) {
		execvp(server_cmd[0], server_cmd);
		cerr << "Exec for server command failed" << endl;
	}

	// delete cstring on heap
	delete[] m_cstr;

	// create client side channel
    FIFORequestChannel cont_chan("control", FIFORequestChannel::CLIENT_SIDE);
	channels.push_back(&cont_chan);

	// create new channel if specified
	if (new_channel) {
		// send request to server
		MESSAGE_TYPE msg = NEWCHANNEL_MSG;
    	cont_chan.cwrite(&msg, sizeof(MESSAGE_TYPE));

		// create variable to hold name
		char new_channel_name[30];
		cont_chan.cread(&new_channel_name, m);

		// create new channel and push to vector
		FIFORequestChannel* new_chan = new FIFORequestChannel(new_channel_name, FIFORequestChannel::CLIENT_SIDE);
		channels.push_back(new_chan);
	}

	FIFORequestChannel chan = *channels.back();

	// process data request only if flags are valid
	if (p > 0 && t >= 0 && (e == 1 || e == 2)) { // single data point
		// example data point request
		char buf[MAX_MESSAGE]; // 256
		datamsg x(p, t, e);
		
		memcpy(buf, &x, sizeof(datamsg));
		chan.cwrite(buf, sizeof(datamsg)); // question
		double reply;
		chan.cread(&reply, sizeof(double)); //answer
		cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
	} else if (p > 0 && t < 0 && (e != 1 && e != 2)) { // get first 1000 lines of patient
		// open file and create file for writing
		ofstream file;
		file.open("received/x1.csv");
		// loop over first 1000 lines
		for(int i = 0; i < 1000; i++) {
			// start at t = 0
			// increment by 0.004
			t = i * 0.004;

			// send request for ecg1
			char buf1[MAX_MESSAGE]; // 256
			datamsg x(p, t, 1);
			memcpy(buf1, &x, sizeof(datamsg));
			chan.cwrite(buf1, sizeof(datamsg)); // question
			double reply1;
			chan.cread(&reply1, sizeof(double)); //answer

			// send request for ecg2
			char buf2[MAX_MESSAGE]; // 256
			datamsg y(p, t, 2);
			memcpy(buf2, &y, sizeof(datamsg));
			chan.cwrite(buf2, sizeof(datamsg)); // question
			double reply2;
			chan.cread(&reply2, sizeof(double)); //answer

			// write line to received/x1.csv
			file << t << "," << reply1 << "," << reply2 << endl;
		}
		// close file
		file.close();
	}

	// request file only if flag has filename
	if (filename != "") {
		// get filename size
		filemsg fm(0, 0);
		string fname = filename;
		
		int len = sizeof(filemsg) + (fname.size() + 1);
		char* buf = new char[len];
		memcpy(buf, &fm, sizeof(filemsg));
		strcpy(buf + sizeof(filemsg), fname.c_str());
		chan.cwrite(buf, len);  // I want the file length;
		__int64_t filelength;
		chan.cread(&filelength, sizeof(__int64_t));

		delete[] buf;

		cout << "File length is: " << filelength << " bytes" << endl;

		// get contents of file
		// create copy of file in received
		ofstream file;
		file.open("received/" + filename);

		// may loop multiple times to get entire content
		for(int i = 0; i < filelength; i += m) {
			// buffer length
			int resp_len;
			if (filelength - i == filelength % m) {
				cout << "end reached" << endl;
				resp_len = filelength % m;
			} else {
				resp_len = m;
			}

			// request the server to write in file data
			filemsg fm(i, resp_len);
			string fname = filename;
			
			int len = sizeof(filemsg) + (fname.size() + 1);
			char* bufreq = new char[len];
			memcpy(bufreq, &fm, sizeof(filemsg));
			strcpy(bufreq + sizeof(filemsg), fname.c_str());
			chan.cwrite(bufreq, len);

			// read file data and write into new file
			char* bufresp = new char[m];
			chan.cread(bufresp, m);
			file.write(bufresp, resp_len);

			// delete buffers created in heap
			delete[] bufreq;
			delete[] bufresp;
		}

		// close the file
		file.close();
	}

	// close and delete new channel if needed
	if (new_channel) {
		FIFORequestChannel* new_ch = channels.back();
		MESSAGE_TYPE msg = QUIT_MSG;
    	(*new_ch).cwrite(&msg, sizeof(MESSAGE_TYPE));
		delete new_ch;
		channels.pop_back();
	}
	
	// closing the channel
    MESSAGE_TYPE msg = QUIT_MSG;
    chan.cwrite(&msg, sizeof(MESSAGE_TYPE));
	// wait(NULL);
}
