#ifndef SRC_GLOBAL_H_
#define SRC_GLOBAL_H_

#include <vector>
#include <string>
#include <exception>

#include <cstdint>
#include <csignal>

enum class WritePathsType {
	Dir,
	Reg,
	Symlink,
	Link, // hardlink
};

enum class PathIter {
	Walk,
	Ordered,
	Reverse,
	Random,
};

extern volatile sig_atomic_t interrupted;

// readonly after getopt
namespace opt {
	extern unsigned long num_set;
	extern unsigned long num_reader;
	extern unsigned long num_writer;
	extern long num_repeat;
	extern long time_minute;
	extern long time_second;
	extern long monitor_int_minute;
	extern long monitor_int_second;
	extern bool stat_only;
	extern bool ignore_dot;
	extern bool follow_symlink;
	extern unsigned long read_buffer_size;
	extern long read_size;
	extern unsigned long write_buffer_size;
	extern long write_size;
	extern bool random_write_data;
	extern long num_write_paths;
	extern bool truncate_write_paths;
	extern bool fsync_write_paths;
	extern bool dirsync_write_paths;
	extern bool keep_write_paths;
	extern bool clean_write_paths;
	extern std::string write_paths_base;
	extern std::vector<WritePathsType> write_paths_type;
	extern PathIter path_iter;
	extern std::string flist_file;
	extern bool flist_file_create;
	extern bool force;
	extern bool verbose;
	extern bool debug;
} // namespace opt

void add_exception(const std::exception&);
#endif // SRC_GLOBAL_H_
