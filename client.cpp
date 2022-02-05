#include "common.h"
#include "TCPRequestChannel.h"
#include "BoundedBuffer.h"
#include "HistogramCollection.h"
#include <sys/wait.h>
using namespace std;

void patient_thread_function(int pno, int n, BoundedBuffer* request_buffer){
    /* What will the patient threads do? */
	DataRequest d(pno, 0.00, 1);
	for(int i = 0; i < n; i++){
		//cout << "hi" << endl;
		vector<char> data((char*) &d, (char*) &d + sizeof(DataRequest));//(char*) &d + sizeof(DataRequest));
		request_buffer->push(data);
		d.seconds += 0.004;
		//cout << "hi" << endl;
	}

}
struct Response{
	int person;
	int ecg;
};

void worker_thread_function(BoundedBuffer* request_buffer, TCPRequestChannel* chans, BoundedBuffer* response_buffer, int bufcap){
    /*
		Functionality of the worker threads	
    */
   char buf[1024];
   char rep[bufcap];
   vector<char> buffs;
   while(true){
	   buffs = request_buffer->pop();
	   memcpy(buf, buffs.data(), buffs.size());
	   REQUEST_TYPE_PREFIX* m = (REQUEST_TYPE_PREFIX*) buf;
	   
	   if(*m == DATA_REQ_TYPE){
		    //cout << "hi" << endl;
		    double reply;
			DataRequest* d((DataRequest*) &buf);
			chans->cwrite (&d, sizeof (DataRequest)); // question
			
			chans->cread (&reply, sizeof(double)); //answer
			Response r{d->person, reply};
			vector<char> responses((char*) &r, (char*) &r + sizeof(r));
			
			response_buffer->push(responses);
	   }
	   if(*m == FILE_REQ_TYPE){
		    //cout << "wat" << endl;
			FileRequest* f = (FileRequest*) buf;
			string filename = (char*) (f + 1);
			int ln = sizeof(FileRequest) + filename.size() + 1; 
			chans->cwrite (buf, ln); // question
			chans->cread (rep, bufcap);

		
			string fn = "received/" + filename;
			ofstream of ("received/" + filename);
			of.write(rep, f->length);
			of.close();
			//FILE* fo = fopen(fn.c_str(), "r+");
			//fseek(fo, f->offset, SEEK_SET);
			//fwrite(rep, 1, f->length, fo);
			//fclose(fo);
			
	   }
	   if(*m == QUIT_REQ_TYPE){
		    //cout <<"hi" << endl;
    		chans->cwrite (m, sizeof (Request));
			delete chans;
		    break;
	   }
   }
}
void histogram_thread_function (BoundedBuffer* response_buffer, HistogramCollection* hc){
    /*
		Functionality of the histogram threads	
    */
	char buf[1024];
	vector<char> buffs;
	while(true){
		buffs = response_buffer->pop();
	    memcpy(buf, buffs.data(), buffs.size());	
		Response* r = (Response*) buf;
		if(r->person == -1){
			break;
		}
		hc->update(r->person, r->ecg);

	}
}

void file_thread_function (string filename, BoundedBuffer* request_buffer, TCPRequestChannel* chan, int bufcap){
	FileRequest fm (0,0);
	int len = sizeof (FileRequest) + filename.size()+1;
	char buf2 [1024];
	memcpy (buf2, &fm, sizeof (FileRequest));
	strcpy (buf2 + sizeof (FileRequest), filename.c_str());
	chan->cwrite (buf2, len);  
	int64 filelen;
	chan->cread (&filelen, sizeof(int64));
	//cout <<"hi" << endl;
	cout << "File length is: " << filelen << " bytes" << endl;

	filename = "received/" + filename;
	FILE* fo = fopen(filename.c_str(),"w");
	fseek(fo, filelen, SEEK_SET);
	//cout <<"hi" << endl;
	fclose(fo);

	//cout <<"hi" << endl;
	int64 i = filelen;
	FileRequest* f = (FileRequest*) buf2;
	while(i > 0){
		f->length = min(i, (int64) bufcap);
		vector<char> files((char*) buf2, (char*) buf2 + sizeof(buf2));
		request_buffer->push(files);
		i -= f->length;
		f->offset += f->length;
	}
	//cout <<"hi" << endl;
}

int main(int argc, char *argv[]){
	//cout << "hi" << endl;
	int opt;
	int p = 1;
	int e = 1;
	int n = 1;
	int h = 10;
	string port;
	string host;
	int w =1;
	string bcap = "256";
	int bufcap = MAX_MESSAGE;
	string filename = "";
	int b = 10; // size of bounded buffer, note: this is different from another variable buffercapacity/m
	// take all the arguments first because some of these may go to the server
	while ((opt = getopt(argc, argv, "f:n:p:w:b:m:h:r:")) != -1) {
		switch (opt) {
			case 'f':
				filename = optarg;
				break;
			case 'n':
				n = atoi(optarg);
				//cout << n << endl;

				break;
			case 'p':
				p = atoi(optarg);
				//cout << p << endl;
				break;
			case 'w':
				w = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
				break;
			case 'm':
				bcap = optarg;
				bufcap = atoi(optarg);
				break;
			case 'h':
				host = optarg;
				break;
			case 'r':
				port = optarg;
				break;
		}
	}

	
	TCPRequestChannel* chan = new TCPRequestChannel (host, port);
	BoundedBuffer request_buffer(b);
	BoundedBuffer response_buffer(p);
	HistogramCollection hc;

	vector<thread> ws;
	vector<thread> ps;
	vector<thread> hs;
	TCPRequestChannel* chans[w];
	//cout << "hi" << endl;
	

	for(int i = 0; i < p; i++){
		//cout << "hi" << endl;
		Histogram* h = new Histogram (10, -2.0, 2.0);
		hc.add(h);
	}

    /* Start all threads here */
	for(int i=0; i<w; i++){
		//cout << "hi" << endl;
		chans[i] = new TCPRequestChannel(host, port);
	}
	//cout << p << endl;
	struct timeval start, end;
    gettimeofday (&start, 0);
	if(filename == ""){
		for (int i=0; i< p; i++){
			//cout << p << endl;
			ps.push_back(thread (patient_thread_function, i + 1, n, &request_buffer));
		}

		for (int i=0; i< w; i++)
			ws.push_back (thread (worker_thread_function, &request_buffer, chans[i], &response_buffer, 0));
		//cout << "hi" << endl;

		for (int i=0; i< h; i++)
			hs.push_back(thread (histogram_thread_function, &response_buffer, &hc));
		
		for (int i=0; i<p; i++)
			ps [i].join();


		//cout << "hi" << endl;
		for (int i=0; i< w; i++){
			REQUEST_TYPE_PREFIX q = QUIT_REQ_TYPE;
			vector<char> quit((char*) &q, (char*) &q + sizeof(REQUEST_TYPE_PREFIX));
			request_buffer.push(quit);

		}

		for(int i =0; i < w; i++){
			ws[i].join();
		}

		Response r{-1, 0};
		vector<char> resp((char*) &r, (char*) &r + sizeof(Response));

		for(int i = 0; i < h; i++){
			response_buffer.push(resp);
		}

		for(int i = 0; i < h; i++){
			hs[i].join();
		}	

	}else{
		
		thread fs (file_thread_function, filename, &request_buffer, chan, bufcap);

		for (int i=0; i< w; i++)
			ws.push_back (thread (worker_thread_function, &request_buffer, chans[i], &response_buffer, bufcap));

		fs.join();
		//cout <<"hi" << endl;
		for (int i=0; i< w; i++){
			REQUEST_TYPE_PREFIX q = QUIT_REQ_TYPE;
			vector<char> quit((char*) &q, (char*) &q + sizeof(REQUEST_TYPE_PREFIX));
			request_buffer.push(quit);

		}

		for(int i =0; i < w; i++){
			ws[i].join();
		}
		

	}	
	

	/* Join all threads here */
    gettimeofday (&end, 0);
	//cout << "hi" << endl;

    // print the results and time difference
	//cout << "hi" << endl;
	hc.print (); 
    int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)/(int) 1e6;
    int usecs = (int)(end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)%((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;
	
	// closing the channel    

    Request q (QUIT_REQ_TYPE);
    chan->cwrite (&q, sizeof (Request));
	// client waiting for the server process, which is the child, to terminate
	//wait(0);
	//cout << "hi" << endl;

	cout << "Client process exited" << endl;
	delete chan;
}
