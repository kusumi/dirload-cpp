#ifndef SRC_WORKER_H_
#define SRC_WORKER_H_

#include <vector>
#include <tuple>
#include <string>
#include <memory>

#include "./dir.h"
#include "./global.h"
#include "./stat.h"
#include "./thread.h"

typedef std::vector<const ThreadStat*> thread_monitor_arg;
typedef std::tuple<XThread*, Dir*, std::string, std::vector<std::string>>
	thread_worker_arg;

class XThread {
	public:
	XThread(unsigned int, ThreadDir&&, ThreadStat&&);
	static std::unique_ptr<XThread> newread(unsigned long gid,
		unsigned long bufsiz) {
		return std::make_unique<XThread>(gid,
			ThreadDir::newread(bufsiz),
			ThreadStat::newread());
	}
	static std::unique_ptr<XThread> newwrite(unsigned long gid,
		unsigned long bufsiz) {
		return std::make_unique<XThread>(gid,
			ThreadDir::newwrite(bufsiz),
			ThreadStat::newwrite());
	}

	unsigned long get_gid(void) const {
		return _gid;
	}
	bool is_reader(void) const {
		return _gid < opt::num_reader;
	}
	bool is_writer(void) const {
		return !is_reader();
	}
	bool is_write_done(void) const;

	const ThreadDir& get_dir(void) const {
		return _dir;
	}
	ThreadDir& get_mut_dir(void) {
		return _dir;
	}
	const ThreadStat& get_stat(void) const {
		return _stat;
	}
	ThreadStat& get_mut_stat(void) {
		return _stat;
	}

	unsigned long get_num_complete(void) const {
		return _num_complete;
	}
	unsigned long get_num_interrupted(void) const {
		return _num_interrupted;
	}
	unsigned long get_num_error(void) const {
		return _num_error;
	}

	void inc_num_complete(void) {
		_num_complete++;
	}
	void inc_num_interrupted(void) {
		_num_interrupted++;
	}
	void inc_num_error(void) {
		_num_error++;
	}

	int thread_create_worker(thread_worker_arg*);
	int thread_join(void) {
		return _thread.join();
	}

	private:
	unsigned long _gid;
	ThreadDir _dir;
	ThreadStat _stat;
	Thread _thread;
	unsigned long _num_complete;
	unsigned long _num_interrupted;
	unsigned long _num_error;
};

typedef std::tuple<unsigned long, unsigned long, unsigned long, unsigned long,
	std::vector<ThreadStat>> dispatch_res;
int dispatch_worker(const std::vector<std::string>&, dispatch_res&);
#endif // SRC_WORKER_H_
