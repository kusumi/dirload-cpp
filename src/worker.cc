#include <iostream>
#include <sstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <exception>

#include <cerrno>
#include <cassert>

#include "./flist.h"
#include "./log.h"
#include "./thread.h"
#include "./util.h"
#include "./worker.h"

XThread::XThread(unsigned int gid, ThreadDir&& dir, ThreadStat&& stat):
	_gid(gid),
	_dir(dir),
	_stat(stat),
	_thread{},
	_num_complete(0),
	_num_interrupted(0),
	_num_error(0) {
}

bool XThread::is_write_done(void) const {
	if (!is_writer() || opt::num_write_paths <= 0)
		return false;
	else
		return get_dir().get_num_write_paths() >=
			static_cast<unsigned long>(opt::num_write_paths);
}

namespace {
int setup_flist_impl(const std::vector<std::string>& input,
	std::vector<std::vector<std::string>>& fls) {
	assert(fls.empty());
	for ([[maybe_unused]] const auto& f : input)
		fls.push_back({});

	if (!opt::flist_file.empty()) {
		// load flist from flist file
		assert(opt::path_iter != PathIter::Walk);
		std::cout << "flist_file " << opt::flist_file << std::endl;
		for (const auto& s : load_flist_file(opt::flist_file)) {
			std::ostringstream ss;
			auto found = false;
			for (size_t i = 0; i < input.size(); i++) {
				const auto& f = input[i];
				if (s.starts_with(f)) {
					fls[i].push_back(s);
					found = true;
					// no break, s can exist in multiple fls[i]
				}
				if (!found) {
					ss << f;
					if (i != input.size() - 1)
						ss << ", ";
				}
			}
			if (!found) {
				std::cout << s << " has no prefix in "
					<< ss.str() << std::endl;
				return -EINVAL;
			}
		}
	} else {
		// initialize flist by walking input directories
		for (size_t i = 0; i < input.size(); i++) {
			const auto& f = input[i];
			const auto l = init_flist(f, opt::ignore_dot);
			std::cout << l.size() << " files scanned from " << f
				<< std::endl;
			fls[i] = l;
		}
	}

	// don't allow empty flist as it results in spinning loop
	for (size_t i = 0; i < fls.size(); i++) {
		const auto& fl = fls[i];
		if (!fl.empty()) {
			std::cout << "flist " << input[i] << " " << fl.size()
				<< std::endl;
		} else {
			std::cout << "empty flist " << input[i] << std::endl;
			return -EINVAL;
		}
	}

	return 0;
}

int setup_flist(const std::vector<std::string>& input,
	std::vector<std::vector<std::string>>& fls) {
	fls.clear();
	// setup flist for non-walk iterations
	if (opt::path_iter == PathIter::Walk) {
		for (const auto& f : input)
			std::cout << "Walk " << f << std::endl;
		return 0;
	} else {
		auto ret = setup_flist_impl(input, fls);
		if (ret < 0)
			return ret;
		assert(input.size() == fls.size());
		return 0;
	}
}

void debug_print_complete(const XThread& thr, int repeat) {
	std::ostringstream ss;
	ss << get_thread_id() << " #" << thr.get_gid() << " "
		<< (thr.is_reader() ? "reader" : "writer")
		<< " complete - repeat " << repeat << " iswritedone "
		<< thr.is_write_done();
	auto s = ss.str();
	xlog("%s", s.c_str());
	if (opt::debug)
		std::cout << s << std::endl;
}

void print_exception(const XThread& thr, const std::exception &e) {
	std::ostringstream ss;
	ss << get_thread_id() << " #" << thr.get_gid() << " "
		<< (thr.is_reader() ? "reader" : "writer") << " - " << e.what();
	auto s = ss.str();
	xlog("%s", s.c_str());
	std::cout << s << std::endl;
}

EXTERN_C_BEGIN
void* monitor_handler_impl(const std::vector<const ThreadStat*>& statv) {
	auto timer = Timer(opt::monitor_int_second, 0);
	assert(statv.size() > 0);

	while (1) {
		if (timer.elapsed()) {
			auto done = true;
			// ignore possible race
			for (const auto& stat : statv) {
				auto* p = const_cast<ThreadStat*>(stat);
				if (!p->is_done()) {
					done = false;
					p->set_time_end();
				}
			}
			if (done)
				break; // all threads done
			print_stat(statv);
			timer.reset();
		}
		if (interrupted)
			break;
		if (statv[0]->sec_elapsed(opt::time_second))
			break;
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	return nullptr;
}

void* monitor_handler(void* arg) {
	try {
		auto statv = *reinterpret_cast<thread_monitor_arg*>(arg);
		return monitor_handler_impl(statv);
	} catch (const std::exception& e) {
		add_exception(e);
		return nullptr;
	}
}

void* worker_handler_impl(XThread& thr, const Dir& dir,
	const std::string& input_path, const std::vector<std::string>& fl) {
	auto d = opt::time_second;
	auto repeat = 0;

	// assert thr
	assert(thr.get_num_complete() == 0);
	assert(thr.get_num_interrupted() == 0);
	assert(thr.get_num_error() == 0);

	thr.get_mut_stat().set_input_path(input_path);

	while (1) {
		// either walk or select from input path
		if (opt::path_iter == PathIter::Walk) {
			for (const auto& x :
				std::filesystem::recursive_directory_iterator(
				input_path)) {
				auto f = std::string(x.path());
				assert(f.starts_with(input_path));
				int ret;
				if (thr.is_reader())
					ret = read_entry(f, thr);
				else
					ret = write_entry(f, thr, dir);
				if (ret < 0) {
					thr.inc_num_error();
					return nullptr;
				}
				if (interrupted) {
					thr.inc_num_interrupted();
					break;
				}
				if (thr.get_stat().sec_elapsed(d)) {
					debug_print_complete(thr, repeat);
					thr.inc_num_complete();
					break;
				}
			}
		} else {
			for (size_t i = 0; i < fl.size(); i++) {
				auto idx = -1;
				switch (opt::path_iter) {
				case PathIter::Ordered:
					idx = static_cast<int>(i);
					break;
				case PathIter::Reverse:
					idx = static_cast<int>(
						fl.size() - 1 - i);
					break;
				case PathIter::Random:
					idx = get_random<int>(0,
						static_cast<int>(fl.size()));
					break;
				default:
					break;
				}
				if (idx == -1) {
					thr.inc_num_error();
					return nullptr;
				}
				const auto& f = fl[idx];
				assert(f.starts_with(input_path));
				int ret;
				if (thr.is_reader())
					ret = read_entry(f, thr);
				else
					ret = write_entry(f, thr, dir);
				if (ret < 0) {
					thr.inc_num_error();
					return nullptr;
				}
				if (interrupted) {
					thr.inc_num_interrupted();
					break;
				}
				if (thr.get_stat().sec_elapsed(d)) {
					debug_print_complete(thr, repeat);
					thr.inc_num_complete();
					break;
				}
			}
		}
		// return if interrupted or complete
		if (thr.get_num_interrupted() > 0 || thr.get_num_complete() > 0)
			return nullptr; // not break
		// otherwise continue until num_repeat if specified
		thr.get_mut_stat().inc_num_repeat();
		repeat++;
		if (opt::num_repeat > 0 && repeat >= opt::num_repeat)
			break; // usually only readers break from here
		if (thr.is_writer() && thr.is_write_done())
			break;
	}

	if (thr.is_reader()) {
		assert(opt::num_repeat > 0);
		assert(repeat >= opt::num_repeat);
	}
	debug_print_complete(thr, repeat);
	thr.inc_num_complete();

	return nullptr;
}

void* worker_handler(void* arg) {
	auto [thr, dir, input_path, fl] =
		*reinterpret_cast<thread_worker_arg*>(arg);
	try {
		auto ret = worker_handler_impl(*thr, *dir, input_path, fl);
		thr->get_mut_stat().set_done();
		thr->get_mut_stat().set_time_end();
		return ret;
	} catch (const std::exception& e) {
		add_exception(e);
		thr->inc_num_error();
		print_exception(*thr, e);
		return nullptr;
	}
}
EXTERN_C_END

int thread_create_monitor(Thread& thread, thread_monitor_arg* arg) {
	return thread.create(monitor_handler, arg);
}
} // namespace

int XThread::thread_create_worker(thread_worker_arg* arg) {
	std::get<0>(*arg) = this;
	return _thread.create(worker_handler, arg);
}

int dispatch_worker(const std::vector<std::string>& input,
	dispatch_res& result) {
	for (const auto& f : input)
		assert(is_abspath(f));
	assert(opt::time_minute == 0);
	assert(opt::monitor_int_minute == 0);

	// number of readers and writers are 0 by default
	if (opt::num_reader == 0 && opt::num_writer == 0) {
		result = {0, 0, 0, 0, {}};
		return 0;
	}

	// initialize dir
	Dir dir(opt::random_write_data);

	// initialize thread structure
	auto num_thread = opt::num_reader + opt::num_writer;
	std::vector<std::unique_ptr<XThread>> thrv;
	for (unsigned long i = 0; i < num_thread; i++)
		if (i < opt::num_reader)
			thrv.push_back(XThread::newread(i,
				opt::read_buffer_size));
		else
			thrv.push_back(XThread::newwrite(i,
				opt::write_buffer_size));
	assert(thrv.size() == num_thread);

	// setup flist
	std::vector<std::vector<std::string>> fls;
	auto ret = setup_flist(input, fls);
	if (ret < 0)
		return ret;
	if (opt::path_iter == PathIter::Walk)
		assert(fls.empty());
	else
		assert(!fls.empty());

	// initialize thread argument
	thread_monitor_arg marg;
	std::vector<thread_worker_arg> argv;
	for (unsigned long i = 0; i < num_thread; i++) {
		const auto& thr = thrv[i];
		const auto& input_path = input[thr->get_gid() % input.size()];
		const auto& fl = fls.empty() ? std::vector<std::string>{} :
			fls[thr->get_gid() % fls.size()];
		marg.push_back(&thr->get_mut_stat());
		argv.push_back({nullptr, &dir, input_path, fl});
	}

	// create threads
	Thread mthr;
	if (opt::monitor_int_second > 0) {
		auto ret = thread_create_monitor(mthr, &marg);
		if (ret) {
			xlog("monitor create failed %d", ret);
			return ret;
		}
		xlog("%s", "monitor created");
	}
	for (unsigned long i = 0; i < num_thread; i++) {
		const auto& thr = thrv[i];
		auto ret = thr->thread_create_worker(&argv[i]);
		if (ret) {
			xlog("#%lu create failed %d", thr->get_gid(), ret);
			return ret;
		}
		thr->get_mut_stat().set_time_begin();
		xlog("#%lu created", thr->get_gid());
	}

	// join threads
	for (const auto& thr : thrv) {
		auto ret = thr->thread_join();
		if (ret) {
			xlog("#%lu join failed %d", thr->get_gid(), ret);
			return ret;
		}
		xlog("#%lu joined", thr->get_gid());
	}
	if (opt::monitor_int_second > 0) {
		auto ret = mthr.join();
		if (ret) {
			xlog("monitor join failed %d", ret);
			return ret;
		}
		xlog("%s", "monitor joined");
	}

	// collect result
	unsigned long num_complete = 0;
	unsigned long num_interrupted = 0;
	unsigned long num_error = 0;
	for (const auto& thr : thrv) {
		num_complete += thr->get_num_complete();
		num_interrupted += thr->get_num_interrupted();
		num_error += thr->get_num_error();
	}
	assert(num_complete + num_interrupted + num_error == num_thread);

	std::vector<const ThreadDir*> tdv;
	std::vector<ThreadStat> tsv;
	for (const auto& thr : thrv) {
		tdv.push_back(&thr->get_dir());
		tsv.push_back(thr->get_stat());
	}
	result = {num_complete, num_interrupted, num_error,
		cleanup_write_paths(tdv), tsv};
	return 0;
}
