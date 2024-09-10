#include <iostream>
#include <sstream>
#include <array>
#include <vector>
#include <string>
#include <exception>

#include <cstdlib>
#include <cstring>
#include <csignal>
#include <cassert>

#include <getopt.h>

#include "./cppunit.h"
#include "./dir.h"
#include "./flist.h"
#include "./global.h"
#include "./log.h"
#include "./stat.h"
#include "./thread.h"
#include "./util.h"
#include "./worker.h"

extern char* optarg;
extern int optind;

volatile sig_atomic_t interrupted;

namespace opt {
	unsigned long num_set = 1;
	unsigned long num_reader;
	unsigned long num_writer;
	long num_repeat = -1;
	long time_minute;
	long time_second;
	long monitor_int_minute;
	long monitor_int_second;
	bool stat_only;
	bool ignore_dot;
	bool follow_symlink;
	unsigned long read_buffer_size = 1 << 16;
	long read_size = -1;
	unsigned long write_buffer_size = 1 << 16;
	long write_size = -1;
	bool random_write_data;
	long num_write_paths = 1 << 10;
	bool truncate_write_paths;
	bool fsync_write_paths;
	bool dirsync_write_paths;
	bool keep_write_paths;
	bool clean_write_paths;
	std::string write_paths_base("x");
	std::vector<WritePathsType> write_paths_type =
		{WritePathsType::Dir, WritePathsType::Reg};
	PathIter path_iter = PathIter::Ordered;
	std::string flist_file;
	bool flist_file_create;
	bool force;
	bool verbose;
	bool debug;
} // namespace opt

namespace {
const std::array<int, 3> _version{0, 4, 0};
std::vector<std::string> _what;

std::string get_version_string() {
	std::ostringstream ss;
	ss << _version[0] << "." << _version[1] << "." << _version[2];
	return ss.str();
}

void print_version() {
	std::cout << get_version_string() << std::endl;
}

void sigint_handler([[maybe_unused]] int n) {
	interrupted = 1;
}

void atexit_handler(void) {
	cleanup_log();
	for (const auto& s : _what)
		std::cout << s << std::endl;
}

void print_build_options(void) {
	std::cout << "Build options:" << std::endl
#ifdef DEBUG
		<< "  debug" << std::endl
#endif
#ifdef CONFIG_STDTHREAD
		<< "  stdthread" << std::endl
#endif
#ifdef CONFIG_CPPUNIT
		<< "  cppunit" << std::endl
#endif
		;
}

void usage(const std::string& arg) {
	std::cout << "Usage: " << arg << " [options] <paths>" << std::endl
		<< "Options:" << std::endl
		<< "  --num_set - Number of sets to run (default 1)"
		<< std::endl
		<< "  --num_reader - Number of reader threads" << std::endl
		<< "  --num_writer - Number of writer threads" << std::endl
		<< "  --num_repeat - Exit threads after specified iterations "
		<< "if > 0 (default -1)" << std::endl
		<< "  --time_minute - Exit threads after sum of this and "
		<< "--time_second option if > 0" << std::endl
		<< "  --time_second - Exit threads after sum of this and "
		<< "--time_minute option if > 0" << std::endl
		<< "  --monitor_interval_minute - Monitor threads every sum of "
		<< "this and --monitor_interval_second option if > 0"
		<< std::endl
		<< "  --monitor_interval_second - Monitor threads every sum of "
		<< "this and --monitor_interval_minute option if > 0"
		<< std::endl
		<< "  --stat_only - Do not read file data" << std::endl
		<< "  --ignore_dot - Ignore entries start with ." << std::endl
		<< "  --follow_symlink - Follow symbolic links for read unless "
		<< "directory" << std::endl
		<< "  --read_buffer_size - Read buffer size (default 65536)"
		<< std::endl
		<< "  --read_size - Read residual size per file read, "
		<< "use < read_buffer_size random size if 0 (default -1)"
		<< std::endl
		<< "  --write_buffer_size - Write buffer size (default 65536)"
		<< std::endl
		<< "  --write_size - Write residual size per file write, "
		<< "use < write_buffer_size random size if 0 (default -1)"
		<< std::endl
		<< "  --random_write_data - Use pseudo random write data"
		<< std::endl
		<< "  --num_write_paths - Exit writer threads after creating "
		<< "specified files or directories if > 0 (default 1024)"
		<< std::endl
		<< "  --truncate_write_paths - ftruncate(2) write paths for "
		<< "regular files instead of write(2)" << std::endl
		<< "  --fsync_write_paths - fsync(2) write paths" << std::endl
		<< "  --dirsync_write_paths - fsync(2) parent directories of "
		<< "write paths" << std::endl
		<< "  --keep_write_paths - Do not unlink write paths after "
		<< "writer threads exit" << std::endl
		<< "  --clean_write_paths - Unlink existing write paths and exit"
		<< std::endl
		<< "  --write_paths_base - Base name for write paths (default x)"
		<< std::endl
		<< "  --write_paths_type - File types for write paths "
		<< "[d|r|s|l] (default dr)" << std::endl
		<< "  --path_iter - <paths> iteration type "
		<< "[walk|ordered|reverse|random] (default ordered)"
		<< std::endl
		<< "  --flist_file - Path to flist file" << std::endl
		<< "  --flist_file_create - Create flist file and exit"
		<< std::endl
		<< "  --force - Enable force mode" << std::endl
		<< "  --verbose - Enable verbose print" << std::endl
		<< "  --debug - Enable debug mode" << std::endl
		<< "  -v, --version - Print version and exit" << std::endl
		<< "  -h, --help - Print usage and exit" << std::endl;
}

int handle_long_option(const std::string& name, const std::string& arg) {
	if (name == "num_set") {
		opt::num_set = std::stoul(arg);
	} else if (name == "num_reader") {
		opt::num_reader = std::stoul(arg);
	} else if (name == "num_writer") {
		opt::num_writer = std::stoul(arg);
	} else if (name == "num_repeat") {
		opt::num_repeat = std::stol(arg);
		if (opt::num_repeat == 0 || opt::num_repeat < -1)
			opt::num_repeat = -1;
	} else if (name == "time_minute") {
		opt::time_minute = std::stol(arg);
	} else if (name == "time_second") {
		opt::time_second = std::stol(arg);
	} else if (name == "monitor_interval_minute") {
		opt::monitor_int_minute = std::stol(arg);
	} else if (name == "monitor_interval_second") {
		opt::monitor_int_second = std::stol(arg);
	} else if (name == "stat_only") {
		opt::stat_only = true;
	} else if (name == "ignore_dot") {
		opt::ignore_dot = true;
	} else if (name == "follow_symlink") {
		opt::follow_symlink = true;
	} else if (name == "read_buffer_size") {
		opt::read_buffer_size = std::stoul(arg);
		if (opt::read_buffer_size > MAX_BUFFER_SIZE) {
			std::cout << "Invalid read buffer size "
				<< opt::read_buffer_size << std::endl;
			return -1;
		}
	} else if (name == "read_size") {
		opt::read_size = std::stol(arg);
		if (opt::read_size < -1) {
			opt::read_size = -1;
		} else if (opt::read_size > static_cast<long>(MAX_BUFFER_SIZE)) {
			std::cout << "Invalid read size "
				<< opt::read_size << std::endl;
			return -1;
		}
	} else if (name == "write_buffer_size") {
		opt::write_buffer_size = std::stoul(arg);
		if (opt::write_buffer_size > MAX_BUFFER_SIZE) {
			std::cout << "Invalid write buffer size "
				<< opt::write_buffer_size << std::endl;
			return -1;
		}
	} else if (name == "write_size") {
		opt::write_size = std::stol(arg);
		if (opt::write_size < -1) {
			opt::write_size = -1;
		} else if (opt::write_size > static_cast<long>(MAX_BUFFER_SIZE)) {
			std::cout << "Invalid write size "
				<< opt::write_size << std::endl;
			return -1;
		}
	} else if (name == "random_write_data") {
		opt::random_write_data = true;
	} else if (name == "num_write_paths") {
		opt::num_write_paths = std::stol(arg);
		if (opt::num_write_paths < -1)
			opt::num_write_paths = -1;
	} else if (name == "truncate_write_paths") {
		opt::truncate_write_paths = true;
	} else if (name == "fsync_write_paths") {
		opt::fsync_write_paths = true;
	} else if (name == "dirsync_write_paths") {
		opt::dirsync_write_paths = true;
	} else if (name == "keep_write_paths") {
		opt::keep_write_paths = true;
	} else if (name == "clean_write_paths") {
		opt::clean_write_paths = true;
	} else if (name == "write_paths_base") {
		opt::write_paths_base = arg;
		if (opt::write_paths_base.empty()) {
			std::cout << "Empty write paths base" << std::endl;
			return -1;
		}
		try {
			opt::write_paths_base = std::string(std::stoul(arg), 'x');
			std::cout << "Using base name " << opt::write_paths_base
				<< " for write paths" << std::endl;
		} catch (const std::exception& e) {
		}
	} else if (name == "write_paths_type") {
		if (arg.empty()) {
			std::cout << "Empty write paths type" << std::endl;
			return -1;
		}
		opt::write_paths_type.clear();
		for (auto& x : arg) {
			switch (x) {
			case 'd':
				opt::write_paths_type.push_back(
					WritePathsType::Dir);
				break;
			case 'r':
				opt::write_paths_type.push_back(
					WritePathsType::Reg);
				break;
			case 's':
				opt::write_paths_type.push_back(
					WritePathsType::Symlink);
				break;
			case 'l':
				opt::write_paths_type.push_back(
					WritePathsType::Link);
				break;
			default:
				std::cout << "Invalid write paths type "
					<< x << std::endl;
				return -1;
			}
		}
	} else if (name == "path_iter") {
		if (arg == "walk") {
			opt::path_iter = PathIter::Walk;
		} else if (arg == "ordered") {
			opt::path_iter = PathIter::Ordered;
		} else if (arg == "reverse") {
			opt::path_iter = PathIter::Reverse;
		} else if (arg == "random") {
			opt::path_iter = PathIter::Random;
		} else {
			std::cout << "Invalid path iteration type " << arg
				<< std::endl;
			return -1;
		}
	} else if (name == "flist_file") {
		opt::flist_file = arg;
	} else if (name == "flist_file_create") {
		opt::flist_file_create = true;
	} else if (name == "force") {
		opt::force = true;
	} else if (name == "verbose") {
		opt::verbose = true;
	} else if (name == "debug") {
		opt::debug = true;
	} else {
		return -1;
	}
	return 0;
}
} // namespace

void add_exception(const std::exception &e) {
	global_lock();
	_what.push_back(e.what());
	global_unlock();
}

int main(int argc, char** argv) {
	const auto* progname_ptr = argv[0];
	std::string progname(progname_ptr);

	int i, c;
	option lo[] = {
		{ "num_set", 1, nullptr, 0 },
		{ "num_reader", 1, nullptr, 0 },
		{ "num_writer", 1, nullptr, 0 },
		{ "num_repeat", 1, nullptr, 0 },
		{ "time_minute", 1, nullptr, 0 },
		{ "time_second", 1, nullptr, 0 },
		{ "monitor_interval_minute", 1, nullptr, 0 },
		{ "monitor_interval_second", 1, nullptr, 0 },
		{ "stat_only", 0, nullptr, 0 },
		{ "ignore_dot", 0, nullptr, 0 },
		{ "follow_symlink", 0, nullptr, 0 },
		{ "read_buffer_size", 1, nullptr, 0 },
		{ "read_size", 1, nullptr, 0 },
		{ "write_buffer_size", 1, nullptr, 0 },
		{ "write_size", 1, nullptr, 0 },
		{ "random_write_data", 0, nullptr, 0 },
		{ "num_write_paths", 1, nullptr, 0 },
		{ "truncate_write_paths", 0, nullptr, 0 },
		{ "fsync_write_paths", 0, nullptr, 0 },
		{ "dirsync_write_paths", 0, nullptr, 0 },
		{ "keep_write_paths", 0, nullptr, 0 },
		{ "clean_write_paths", 0, nullptr, 0 },
		{ "write_paths_base", 1, nullptr, 0 },
		{ "write_paths_type", 1, nullptr, 0 },
		{ "path_iter", 1, nullptr, 0 },
		{ "flist_file", 1, nullptr, 0 },
		{ "flist_file_create", 0, nullptr, 0 },
		{ "force", 0, nullptr, 0 },
		{ "verbose", 0, nullptr, 0 },
		{ "debug", 0, nullptr, 0 },
		{ "version", 0, nullptr, 'v' },
		{ "help", 0, nullptr, 'h' },
		{ nullptr, 0, nullptr, 0 },
	};

	while ((c = getopt_long(argc, argv, "vhxX", lo, &i)) != -1) {
		switch (c) {
		case 0:
			try {
				if (handle_long_option(lo[i].name,
					std::string(optarg ? optarg : ""))
					== -1) {
					usage(progname);
					exit(1);
				}
			} catch (const std::exception& e) {
				std::cout << lo[i].name << ": " << e.what()
					<< std::endl;
				exit(1);
			}
			break;
		case 'v':
			print_version();
			exit(1);
		default:
		case 'h':
			usage(progname);
			exit(1);
		case 'x': // hidden
			print_build_options();
			exit(0);
		case 'X': // hidden
			exit(-run_unittest());
		}
	}
	argv += optind;
	argc -= optind;

	// adjust getopt results
	opt::time_second += opt::time_minute * 60;
	opt::time_minute = 0;
	opt::monitor_int_second += opt::monitor_int_minute * 60;
	opt::monitor_int_minute = 0;
	// using flist file means not walking input directories
	if (!opt::flist_file.empty() && opt::path_iter == PathIter::Walk) {
		opt::path_iter = PathIter::Ordered;
		std::cout << "Using flist, force --path_iter=ordered" << std::endl;
	}

	if (is_windows()) {
		std::cout << "Windows unsupported" << std::endl;
		exit(1);
	}

	auto s = get_path_separator();
	if (s != '/') {
		std::cout << "Invalid path separator " << s << std::endl;
		exit(1);
	}

	if (argc == 0) {
		usage(progname);
		exit(1);
	}

	init_log(progname_ptr);

	// only allow directories since now that write is supported
	std::vector<std::string> input;
	for (auto i = 0; i < argc; i++) {
		auto absf = get_abspath(std::string(argv[i]));
		assert(!absf.ends_with('/'));
		if (get_raw_file_type(absf) != FileType::Dir) {
			std::cout << absf << " not directory" << std::endl;
			exit(1);
		}
		if (!opt::force) {
			auto count = 0;
			for (const auto& x : absf)
				if (x == '/')
					count += 1;
			// /path/to/dir is allowed, but /path/to is not
			if (count < 3) {
				std::cout << absf
					<< " not allowed, use --force option to proceed"
					<< std::endl;
				exit(1);
			}
		}
		input.push_back(absf);
		xlog("input[%d]: %s", i, absf.c_str());
	}

	// and the directories should be writable
	if (opt::debug && opt::num_writer > 0)
		for ([[maybe_unused]] const auto& f : input)
			xlog("%s writable %s", f.c_str(),
				is_dir_writable(f) ? "true" : "false");

	// create flist and exit
	if (opt::flist_file_create) {
		if (opt::flist_file.empty()) {
			std::cout << "Empty flist file path" << std::endl;
			exit(1);
		}
		auto ret = create_flist_file(input, opt::flist_file,
			opt::ignore_dot, opt::force);
		if (ret < 0) {
			std::cout << strerror(-ret) << std::endl;
			exit(1);
		}
		std::cout << opt::flist_file << std::endl;
		exit(0);
	}
	// clean write paths and exit
	if (opt::clean_write_paths) {
		auto l = collect_write_paths(input);
		auto a = l.size();
		auto ret = unlink_write_paths(l, -1);
		if (ret < 0) {
			std::cout << strerror(-ret) << std::endl;
			exit(1);
		}
		auto b = l.size();
		assert(a >= b);
		std::cout << "Unlinked " << (a - b) << " / " << a
			<< " write paths" << std::endl;
		if (b != 0) {
			std::cout << b << " / " << a << " write paths remaining"
				<< std::endl;
			exit(1);
		}
		exit(0);
	}

	if (atexit(atexit_handler)) {
		perror("atexit");
		exit(1);
	}

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigint_handler;
	if (sigemptyset(&sa.sa_mask) == -1) {
		perror("sigemptyset");
		exit(1);
	}
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	// ready to dispatch workers
	for (unsigned long i = 0; i < opt::num_set; i++) {
		if (opt::num_set != 1) {
			std::cout << std::string(80, '=') << std::endl;
			std::ostringstream ss;
			ss << "Set " << (i + 1) << "/" << opt::num_set;
			auto s = ss.str();
			std::cout << s << std::endl;
			xlog("%s", s.c_str());
		}
		try {
			dispatch_res result;
			auto ret = dispatch_worker(input, result);
			if (ret < 0) {
				std::cout << strerror(-ret) << std::endl;
				exit(1);
			}
			auto [_ignore, num_interrupted, num_error, num_remain,
				tsv] = result;
			if (num_interrupted > 0)
				std::cout << num_interrupted << " worker"
					<< (num_interrupted > 1 ? "s" : "")
					<< " interrupted" << std::endl;
			if (num_error > 0)
				std::cout << num_error << " worker"
					<< (num_error > 1 ? "s" : "")
					<< " failed" << std::endl;
			if (num_remain > 0)
				std::cout << num_remain << " write path"
					<< (num_remain > 1 ? "s" : "")
					<< " remaining" << std::endl;
			print_stat(tsv);
			if (num_interrupted > 0)
				break;
		} catch (const std::exception& e) {
			add_exception(e);
			exit(1);
		}
		if (opt::num_set != 1 && i != opt::num_set - 1)
			std::cout << std::endl;
	}

	return 0;
}
