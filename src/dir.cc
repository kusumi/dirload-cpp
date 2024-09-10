#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

#include <cstring>
#include <cerrno>
#include <cassert>

#include <unistd.h>
#include <fcntl.h>

#include "./dir.h"
#include "./global.h"
#include "./util.h"
#include "./worker.h"

const unsigned long MAX_BUFFER_SIZE = 128lu * 1024;

namespace {
const std::string WRITE_PATHS_PREFIX = "dirload";

int read_file(const std::string&, XThread&);
int write_file(const std::string&, const std::string&, XThread&, const Dir&);
int create_inode(const std::string&, const std::string&, WritePathsType);
int fsync_inode(const std::string&);
std::string get_write_paths_base(void);
}

ThreadDir::ThreadDir(unsigned long rbufsiz, unsigned long wbufsiz):
	_read_buffer(std::vector<char>(rbufsiz, 0)),
	_write_buffer(std::vector<char>(wbufsiz, 0x41)),
	_write_paths{},
	_write_paths_counter(0) {
}

Dir::Dir(bool random):
	_random_write_data{},
	_write_paths_ts{} {
	if (random) {
		// doubled
		for (unsigned long i = 0; i < MAX_BUFFER_SIZE * 2; i++)
			_random_write_data.push_back(get_random(
				static_cast<unsigned char>(32),
				static_cast<unsigned char>(128)));
	}
	_write_paths_ts = get_time_string();
}

unsigned long cleanup_write_paths(const std::vector<const ThreadDir*>& tdv) {
	std::vector<std::string> l;
	for (const auto& tdir : tdv)
		tdir->splice_write_paths(l);
	auto num_remain = 0lu;
	if (opt::keep_write_paths) {
		num_remain += static_cast<unsigned long>(l.size());
	} else {
		unlink_write_paths(l, -1);
		num_remain += static_cast<unsigned long>(l.size());
	}
	return num_remain;
}

int unlink_write_paths(std::vector<std::string>& l, long count) {
	auto lsize = static_cast<long>(l.size());
	auto n = lsize; // unlink all by default
	if (count > 0) {
		n = count;
		if (n > lsize)
			n = lsize;
	}
	std::cout << "Unlink " << n << " write paths" << std::endl;
	std::sort(l.begin(), l.end());

	while (n > 0) {
		auto& f = l[l.size() - 1];
		switch (get_raw_file_type(f)) {
		case FileType::Dir:
			[[fallthrough]];
		case FileType::Reg:
			assert(path_exists(f));
			[[fallthrough]];
		case FileType::Symlink:
			// don't resolve symlink (test symlink itself, not target)
			if (!path_exists(f))
				continue;
			if (std::filesystem::remove(f))
				l.pop_back();
			n--;
			break;
		default:
			return -EINVAL;
		}
	}
	return 0;
}

namespace {
void assert_file_path(const std::string& f) {
	// must always handle file as abs
	assert(is_abspath(f));

	// file must not end with "/"
	assert(!f.ends_with('/'));
}
}

int read_entry(const std::string& f, XThread& thr) {
	assert_file_path(f);
	auto t = get_raw_file_type(f);

	// stats by dirwalk itself are not counted
	thr.get_mut_stat().inc_num_stat();

	// ignore . entries if specified
	if (opt::ignore_dot && t != FileType::Dir && is_dot_path(f))
		return 0;

	// beyond this is for file read
	if (opt::stat_only)
		return 0;

	// find target if symlink
	std::string x;
	if (t == FileType::Symlink) {
		x = std::filesystem::read_symlink(f);
		thr.get_mut_stat().add_num_read_bytes(x.size());
		if (!is_abspath(x)) {
			x = join_path(get_dirpath(f), x);
			assert(is_abspath(x));
		}
		t = get_file_type(x); // update type
		thr.get_mut_stat().inc_num_stat(); // count twice for symlink
		assert(t != FileType::Symlink); // symlink chains resolved
		if (!opt::follow_symlink)
			return 0;
	} else {
		x = f;
	}

	switch (t) {
	case FileType::Reg:
		return read_file(f, thr);
	case FileType::Dir:
		[[fallthrough]];
	case FileType::Device:
		[[fallthrough]];
	case FileType::Unsupported:
		return 0;
	case FileType::Symlink:
		std::cout << x << " is symlink" << std::endl;
		assert(false);
		break;
	}
	return 0;
}

namespace {
int read_file(const std::string& f, XThread& thr) {
	auto [buf, bufsiz] = thr.get_mut_dir().get_read_buffer();
	auto resid = opt::read_size; // negative resid means read until EOF
	if (resid == 0) {
		resid = get_random<long>(0, bufsiz) + 1;
		assert(resid > 0);
		assert(resid <= static_cast<long>(bufsiz));
	}
	assert(resid == -1 || resid > 0);

	// start read
	std::ifstream ifs;
	ifs.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	ifs.open(f, std::ifstream::binary);

	while (1) {
		// cut read size if > positive residual
		auto n = static_cast<std::streamsize>(bufsiz);
		if (resid > 0)
			if (n > resid)
				n = resid;

		ifs.readsome(buf, n); // read sets failbit on EOF
		auto siz = ifs.gcount();
		thr.get_mut_stat().inc_num_read();
		thr.get_mut_stat().add_num_read_bytes(siz);
		if (siz == 0)
			break;

		// end if positive residual becomes <= 0
		if (resid > 0) {
			resid -= siz;
			if (resid <= 0) {
				if (opt::debug)
					assert(resid == 0);
				break;
			}
		}
	}
	return 0;
}
} // namespace

int write_entry(const std::string& f, XThread& thr, const Dir& dir) {
	assert_file_path(f);
	auto t = get_raw_file_type(f);

	// stats by dirwalk itself are not counted
	thr.get_mut_stat().inc_num_stat();

	// ignore . entries if specified
	if (opt::ignore_dot && t != FileType::Dir && is_dot_path(f))
		return 0;

	switch (t) {
	case FileType::Dir:
		return write_file(f, f, thr, dir);
	case FileType::Reg:
		return write_file(get_dirpath(f), f, thr, dir);
	case FileType::Device:
		[[fallthrough]];
	case FileType::Symlink:
		[[fallthrough]];
	case FileType::Unsupported:
		return 0;
	}
	return 0;
}

namespace {
int write_file(const std::string& d, const std::string& f, XThread& thr,
	const Dir& dir) {
	if (thr.is_write_done())
		return 0;

	// construct a write path
	// XXX too long (easily hits ENAMETOOLONG with walk)
	std::ostringstream ss;
	ss << get_write_paths_base() << "_gid" << thr.get_gid() << "_"
		<< dir.get_write_paths_ts() << "_"
		<< thr.get_dir().get_write_paths_counter();
	auto newb = ss.str();
	thr.get_mut_dir().inc_write_paths_counter();
	auto newf = join_path(d, newb);

	// create an inode
	auto i = get_random<int>(0,
		static_cast<int>(opt::write_paths_type.size()));
	auto t = opt::write_paths_type[i];
	auto ret = create_inode(f, newf, t);
	if (ret < 0)
		return ret;
	if (opt::fsync_write_paths) {
		auto ret = fsync_inode(newf);
		if (ret < 0)
			return ret;
	}
	if (opt::dirsync_write_paths) {
		auto ret = fsync_inode(d);
		if (ret < 0)
			return ret;
	}

	// register the write path, and return unless regular file
	thr.get_mut_dir().push_write_paths(newf);
	if (t != WritePathsType::Reg) {
		thr.get_mut_stat().inc_num_write();
		return 0;
	}

	auto [buf, bufsiz] = thr.get_mut_dir().get_write_buffer();
	auto resid = opt::write_size; // negative resid means no write
	if (resid < 0) {
		thr.get_mut_stat().inc_num_write();
		return 0;
	} else if (resid == 0) {
		resid = get_random<long>(0, bufsiz) + 1;
		assert(resid > 0);
		assert(resid <= static_cast<long>(bufsiz));
	}
	assert(resid > 0);

	// path based truncate unlinke Rust or Go
	if (opt::truncate_write_paths) {
		std::filesystem::resize_file(newf, resid);
		thr.get_mut_stat().inc_num_write();
		if (opt::fsync_write_paths) {
			auto ret = fsync_inode(newf);
			if (ret < 0)
				return ret;
		}
		return 0;
	}

	// start write
	std::ofstream ofs;
	ofs.exceptions(std::ofstream::failbit | std::ofstream::badbit);
	ofs.open(newf, std::ofstream::binary);

	while (1) {
		// cut write size if > residual
		auto n = static_cast<std::streamsize>(bufsiz);
		if (n > resid)
			n = resid;
		if (opt::random_write_data) {
			auto rbuf = dir.get_random_write_data();
			auto i = get_random<int>(0, static_cast<int>(bufsiz));
			memcpy(buf, rbuf + i, bufsiz);
		}

		auto pos = ofs.tellp();
		ofs.write(buf, n);
		auto siz = ofs.tellp() - pos;
		assert(siz >= 0);
		thr.get_mut_stat().inc_num_write();
		thr.get_mut_stat().add_num_write_bytes(siz);

		// end if residual becomes <= 0
		resid -= siz;
		if (resid <= 0) {
			if (opt::debug)
				assert(resid == 0);
			break;
		}
	}

	if (opt::fsync_write_paths)
		ofs.flush(); // not fsync
	return 0;
}

void create_regular_file(const std::string& f) {
	std::ofstream ofs;
	ofs.exceptions(std::ofstream::failbit | std::ofstream::badbit);
	ofs.open(f);
}

int create_inode(const std::string& oldf, const std::string& newf,
	WritePathsType t) {
	if (t == WritePathsType::Link) {
		if (get_raw_file_type(oldf) == FileType::Reg) {
			std::filesystem::create_hard_link(oldf, newf);
			return 0;
		}
		t = WritePathsType::Dir; // create a directory instead
	}
	std::error_code ec;
	switch (t) {
	case WritePathsType::Dir:
		if (!std::filesystem::create_directory(newf, ec))
			return -ec.value();
		break;
	case WritePathsType::Reg:
		create_regular_file(newf);
		break;
	case WritePathsType::Symlink:
		std::filesystem::create_symlink(oldf, newf);
		break;
	default:
		break;
	}
	return 0;
}

int fsync_inode(const std::string& f) {
	auto fd = open(f.c_str(), O_RDONLY);
	if (fd < 0)
		return -errno;
	auto ret = fsync(fd);
	if (ret < 0) {
		auto error = errno;
		close(fd);
		return -error;
	}
	close(fd);
	return 0;
}

std::string get_write_paths_base(void) {
	std::ostringstream ss;
	ss << WRITE_PATHS_PREFIX << "_" << opt::write_paths_base;
	return ss.str();
}
} // namespace

std::vector<std::string>
collect_write_paths(const std::vector<std::string>& input) {
	const auto b = get_write_paths_base();
	std::vector<std::string> l;
	for (const auto& f : remove_dup_string(input))
		for (const auto& e : std::filesystem::recursive_directory_iterator(f)) {
			auto x = e.path();
			switch (get_raw_file_type(x)) {
			case FileType::Dir:
				[[fallthrough]];
			case FileType::Reg:
				[[fallthrough]];
			case FileType::Symlink:
				// don't resolve symlink (test symlink itself, not target)
				if (get_basename(x, true).starts_with(b))
					l.push_back(x);
				break;
			default:
				break;
			}
		}
	return l;
}
