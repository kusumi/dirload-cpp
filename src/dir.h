#ifndef SRC_DIR_H_
#define SRC_DIR_H_

#include <vector>
#include <tuple>
#include <string>

extern const unsigned long MAX_BUFFER_SIZE;

class ThreadDir {
	public:
	ThreadDir(unsigned long, unsigned long);
	static ThreadDir newread(unsigned long bufsiz) {
		return ThreadDir(bufsiz, 0);
	}
	static ThreadDir newwrite(unsigned long bufsiz) {
		return ThreadDir(0, bufsiz);
	}

	std::tuple<char*, size_t> get_read_buffer(void) {
		return {_read_buffer.data(), _read_buffer.size()};
	}
	std::tuple<char*, size_t> get_write_buffer(void) {
		return {_write_buffer.data(), _write_buffer.size()};
	}

	unsigned long get_num_write_paths(void) const {
		return _write_paths.size();
	}
	void push_write_paths(const std::string& f) {
		_write_paths.push_back(f);
	}
	void splice_write_paths(std::vector<std::string>& l) const {
		l.insert(l.end(), _write_paths.begin(), _write_paths.end());
	}

	unsigned long get_write_paths_counter(void) const {
		return _write_paths_counter;
	}
	void inc_write_paths_counter(void) {
		_write_paths_counter++;
	}

	private:
	std::vector<char> _read_buffer;
	std::vector<char> _write_buffer;
	std::vector<std::string> _write_paths;
	unsigned long _write_paths_counter;
};

class Dir {
	public:
	explicit Dir(bool);
	const unsigned char* get_random_write_data(void) const {
		return _random_write_data.empty() ? NULL : _random_write_data.data();
	}
	const std::string& get_write_paths_ts(void) const {
		return _write_paths_ts;
	}

	private:
	std::vector<unsigned char> _random_write_data;
	std::string _write_paths_ts;
};

unsigned long cleanup_write_paths(const std::vector<const ThreadDir*>&);
int unlink_write_paths(std::vector<std::string>&, long);
class XThread;
int read_entry(const std::string&, XThread&);
int write_entry(const std::string&, XThread&, const Dir&);
std::vector<std::string> collect_write_paths(const std::vector<std::string>&);
#endif // SRC_DIR_H_
