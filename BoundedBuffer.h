#ifndef BoundedBuffer_h
#define BoundedBuffer_h

#include <stdio.h>
#include <queue>
#include <string>
#include <thread>
#include <mutex>
#include <assert.h>
#include <vector>
#include <condition_variable>
#include <string.h>
//#include "Semaphore.h"

using namespace std;

class BoundedBuffer
{
private:
	int cap; // max number of items in the buffer
	queue<vector<char>> q;	/* the queue of items in the buffer. Note
	that each item a sequence of characters that is best represented by a vector<char> because: 
	1. An STL std::string cannot keep binary/non-printables
	2. The other alternative is keeping a char* for the sequence and an integer length (i.e., the items can be of variable length), which is more complicated.*/

	// add necessary synchronization variables (e.g., sempahores, mutexes) and variables
	mutex m;
	condition_variable data_available;  // for wait by the pop, signaled by push functions
	condition_variable slot_available;

public:

	BoundedBuffer(int _cap){
		cap = _cap;
	}
	~BoundedBuffer(){
		
	}

	void push(vector<char> data){
		// follow the class lecture pseudocode
				
		//1. Perform necessary waiting (by calling wait on the right semaphores and mutexes),
		//2. Push the data onto the queue
		//3. Do necessary unlocking and notification
		
		//1. Wait until there is room in the queue (i.e., queue lengh is less than cap)
		unique_lock<mutex> l(m);
		slot_available.wait (l, [this]{return q.size() < cap;});
		
		
		//2. Then push the vector at the end of the queue, watch out for racecondition
		q.push (data);
		l.unlock ();
		
		//3. Wake up pop() threads 
		data_available.notify_one ();
		
	}

	vector<char> pop(){
		//1. Wait using the correct sync variables 
		//2. Pop the front item of the queue. 
		//3. Unlock and notify using the right sync variables
		//4. Return the popped vector

		//1. Wait until the queue has at least 1 item
		unique_lock<mutex> l (m);
		data_available.wait (l, [this]{return q.size() > 0;});
		
		//2. pop the front item of the queue. The popped item is a vector<char>, watch out for race condition
		vector<char> d = q.front ();
		q.pop ();
			
		//3. wake up any potentially sleeping push() function
		l.unlock ();

		slot_available.notify_one ();
		
		//4. Return the vector's length to the caller so that he knows many bytes were popped
		return d;
	}
};

#endif /* BoundedBuffer_ */
