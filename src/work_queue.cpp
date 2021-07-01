#include <deque>
#include <pthread.h>
#include <sys/socket.h>

using namespace std;

struct work_queue {
	deque<int> jobQueue;
	pthread_mutex_t jobMutex;

	int addJob(int n) {
		pthread_mutex_lock(&this->jobMutex);
		jobQueue.push_back(n);
		size_t len = jobQueue.size();
		pthread_mutex_unlock(&this->jobMutex);
		return len;
	}

	bool removeJob(int *job) {
		pthread_mutex_lock(&this->jobMutex);
		bool success = !this->jobQueue.empty();
		if (success) {
			*job = this->jobQueue.front();
			this->jobQueue.pop_front();
		}
		pthread_mutex_unlock(&this->jobMutex);
		return success;
	}
};
