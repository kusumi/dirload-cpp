#include <sstream>
#include <filesystem>
#include <exception>

#include <ctime>

#include "./util.h"

namespace {
// This function
// * resolves symlink by default
// * does not resolve symlink when lexical=true
// * works with non existent path
std::filesystem::path canonicalize_path_impl(const std::string& f,
	bool lexical) {
	if (lexical)
		return std::filesystem::path(f).lexically_normal();
	else
		return std::filesystem::weakly_canonical(f);
}

[[maybe_unused]]
std::string canonicalize_path(const std::string& f, bool lexical) {
	return std::string(canonicalize_path_impl(f, lexical));
}
} // namespace

std::string get_abspath(const std::string& f, bool lexical) {
	return std::filesystem::absolute(canonicalize_path_impl(f, lexical));
}

std::string get_dirpath(const std::string& f, bool lexical) {
	return canonicalize_path_impl(f, lexical).parent_path();
}

std::string get_basename(const std::string& f, bool lexical) {
	return canonicalize_path_impl(f, lexical).filename();
}

bool is_abspath(const std::string& f) {
	return canonicalize_path_impl(f, true).is_absolute();
}

std::string join_path(const std::string& f1, const std::string& f2,
	bool lexical) {
	return canonicalize_path_impl(f1, lexical).append(f2);
}

bool is_linux(void) {
#ifdef __linux__
	return true;
#else
	return false;
#endif
}

bool is_windows(void) {
#ifdef _WIN32
	return true;
#else
	return false;
#endif
}

char get_path_separator(void) {
	return std::filesystem::path::preferred_separator;
}

namespace {
FileType get_mode_type(const std::filesystem::file_type& t) {
	switch (t) {
	case std::filesystem::file_type::directory:
		return FileType::Dir;
	case std::filesystem::file_type::regular:
		return FileType::Reg;
	case std::filesystem::file_type::block:
		[[fallthrough]];
	case std::filesystem::file_type::character:
		return FileType::Device;
	case std::filesystem::file_type::symlink:
		return FileType::Symlink;
	default:
		return FileType::Unsupported;
	}
}
} // namespace

FileType get_raw_file_type(const std::string& f) {
	try {
		return get_mode_type(std::filesystem::symlink_status(f).type());
	} catch (std::filesystem::filesystem_error& e) {
		return FileType::Unsupported;
	}
}

FileType get_file_type(const std::string& f) {
	try {
		return get_mode_type(std::filesystem::status(f).type());
	} catch (std::filesystem::filesystem_error& e) {
		return FileType::Unsupported;
	}
}

// std::filesystem::exists can't be used as it resolves symlink
bool path_exists(const std::string& f) {
	try {
		switch (std::filesystem::symlink_status(f).type()) {
		case std::filesystem::file_type::none:
			[[fallthrough]];
		case std::filesystem::file_type::not_found:
			return false;
		default:
			return true;
		}
	} catch (std::filesystem::filesystem_error& e) {
		return false;
	}
}

bool is_dot_path(const std::string& f) {
	return get_basename(f, true).starts_with(".") ||
		f.find("/.") != std::string::npos;
}

bool is_dir_writable(const std::string& f) {
	if (get_raw_file_type(f) != FileType::Dir)
		throw std::invalid_argument(f);

	std::ostringstream ss;
	ss << "dirload_write_test_" << get_time_string();
	auto x = join_path(f, ss.str());

	std::error_code ec;
	std::filesystem::create_directory(x, ec);
	if (ec.value())
		return false; // assume readonly
	std::filesystem::remove(x, ec);
	if (ec.value())
		return false; // assume readonly
	return true; // read+write
}

std::vector<std::string>
remove_dup_string(const std::vector<std::string>& input) {
	std::vector<std::string> l;
	for (const auto& a : input) {
		auto exists = false;
		for (const auto& b : l)
			if (a == b)
				exists = true;
		if (!exists)
			l.push_back(a);
	}
	return l;
}

std::string get_time_string(void) {
	time_t t;
	time(&t);
	char buf[256];
	strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", localtime(&t));
	return std::string(buf);
}

std::mt19937& get_random_engine(void) {
	static std::random_device seed_gen;
	static std::mt19937 engine;
	static auto init = false;

	if (!init) {
		engine = std::mt19937(seed_gen());
		init = true;
	}
	return engine;
}

Timer::Timer(long duration, long frequency):
	_time_begin(std::chrono::steady_clock::now()),
	_duration(duration),
	_frequency(frequency),
	_counter(0) {
}

bool Timer::elapsed(void) {
	if (_duration == 0)
		return false; // consider 0 as unused
	_counter++;
	if (_frequency == 0 || _counter % _frequency == 0)
		return std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::steady_clock::now() - _time_begin).count()
			>= _duration;
	else
		return false;
}

void Timer::reset(void) {
	_time_begin = std::chrono::steady_clock::now();
}

#ifdef CONFIG_CPPUNIT
#include <tuple>
#include <thread>

#include <cppunit/TestAssert.h>

#include "./cppunit.h"

void UtilTest::test_canonicalize_path(void) {
	const std::vector<std::tuple<std::string, std::string>> path_list{
		{"/", "/"},
		{"/////", "/"},
		{"/..", "/"},
		{"/../", "/"},
		{"/root", "/root"},
		{"/root/", "/root"},
		{"/root/..", "/"},
		{"/root/../dev", "/dev"},
	};
	for (const auto& x : path_list) {
		const auto [input, output] = x;
		CPPUNIT_ASSERT_EQUAL_MESSAGE(input,
			canonicalize_path(input, false), output);
	}
}

void UtilTest::test_get_abspath(void) {
	const std::vector<std::tuple<std::string, std::string>> path_list{
		{"/", "/"},
		{"/////", "/"},
		{"/..", "/"},
		{"/../", "/"},
		{"/root", "/root"},
		{"/root/", "/root"},
		{"/root/..", "/"},
		{"/root/../dev", "/dev"},
		{"/does/not/exist", "/does/not/exist"},
		{"/does/not/./exist", "/does/not/exist"},
		{"/does/not/../NOT/exist", "/does/NOT/exist"},
	};
	for (const auto& x : path_list) {
		const auto [input, output] = x;
		CPPUNIT_ASSERT_EQUAL_MESSAGE(input, get_abspath(input, false),
			output);
	}
}

void UtilTest::test_get_dirpath(void) {
	const std::vector<std::tuple<std::string, std::string>> path_list{
		{"/root", "/"},
		{"/root/", "/"},
		{"/root/../dev", "/"},
		{"/does/not/exist", "/does/not"},
		{"/does/not/./exist", "/does/not"},
		{"/does/not/../NOT/exist", "/does/NOT"},
	};
	for (const auto& x : path_list) {
		const auto [input, output] = x;
		CPPUNIT_ASSERT_EQUAL_MESSAGE(input, get_dirpath(input, false),
			output);
	}
}

void UtilTest::test_get_basename(void) {
	const std::vector<std::tuple<std::string, std::string>> path_list{
		{"/root", "root"},
		{"/root/", "root"},
		{"/root/../dev", "dev"},
		{"/does/not/exist", "exist"},
		{"/does/not/./exist", "exist"},
		{"/does/not/../NOT/exist", "exist"},
	};
	for (const auto& x : path_list) {
		const auto [input, output] = x;
		CPPUNIT_ASSERT_EQUAL_MESSAGE(input, get_basename(input, false),
			output);
	}
}

void UtilTest::test_is_abspath(void) {
	const std::vector<std::tuple<std::string, bool>> path_list{
		{"/", true},
		{"/////", true},
		{"/..", true},
		{"/../", true},
		{"/root", true},
		{"/root/", true},
		{"/root/..", true},
		{"/root/../dev", true},
		{"/does/not/exist", true},
		{"/does/not/../NOT/exist", true},
		{"xxx", false},
		{"does/not/exist", false},
	};
	for (const auto& x : path_list) {
		const auto [input, output] = x;
		CPPUNIT_ASSERT_EQUAL_MESSAGE(input, is_abspath(input), output);
	}
}

void UtilTest::test_is_windows(void) {
	CPPUNIT_ASSERT(!is_windows());
}

void UtilTest::test_get_path_separator(void) {
	CPPUNIT_ASSERT_EQUAL(get_path_separator(), '/');
}

void UtilTest::test_get_raw_file_type(void) {
	const std::vector<std::string> dir_list{
		".",
		"..",
		"/",
		"/dev",
	};
	for (const auto& f : dir_list)
		CPPUNIT_ASSERT_EQUAL_MESSAGE(f, get_raw_file_type(f),
			FileType::Dir);

	const std::vector<std::string> invalid_list{
		"",
		"516e7cb4-6ecf-11d6-8ff8-00022d09712b",
	};
	for (const auto& f : invalid_list)
		CPPUNIT_ASSERT_EQUAL_MESSAGE(f, get_raw_file_type(f),
			FileType::Unsupported);
}

void UtilTest::test_get_file_type(void) {
	const std::vector<std::string> dir_list{
		".",
		"..",
		"/",
		"/dev",
	};
	for (const auto& f : dir_list)
		CPPUNIT_ASSERT_EQUAL_MESSAGE(f, get_file_type(f),
			FileType::Dir);

	const std::vector<std::string> invalid_list{
		"",
		"516e7cb4-6ecf-11d6-8ff8-00022d09712b",
	};
	for (const auto& f : invalid_list)
		CPPUNIT_ASSERT_EQUAL_MESSAGE(f, get_file_type(f),
			FileType::Unsupported);
}

void UtilTest::test_path_exists(void) {
	const std::vector<std::string> dir_list{
		".",
		"..",
		"/",
		"/dev",
	};
	for (const auto& f : dir_list)
		CPPUNIT_ASSERT_EQUAL_MESSAGE(f, path_exists(f), true);

	const std::vector<std::string> invalid_list{
		"",
		"516e7cb4-6ecf-11d6-8ff8-00022d09712b",
	};
	for (const auto& f : invalid_list)
		CPPUNIT_ASSERT_EQUAL_MESSAGE(f, path_exists(f), false);
}

void UtilTest::test_is_dot_path(void) {
	// XXX commented out paths behave differently vs dirload
	const std::vector<std::string> dot_list{
		"/.",
		"/..",
		//"./", // XXX
		"./.",
		"./..",
		//".",
		//"..",
		".git",
		"..git",
		"/path/to/.",
		"/path/to/..",
		"/path/to/.git/xxx",
		"/path/to/.git/.xxx",
		"/path/to/..git/xxx",
		"/path/to/..git/.xxx",
	};
	for (const auto& f : dot_list)
		CPPUNIT_ASSERT_MESSAGE(f, is_dot_path(f));

	const std::vector<std::string> non_dot_list{
		"/",
		"xxx",
		"xxx.",
		"xxx..",
		"/path/to/xxx",
		"/path/to/xxx.",
		"/path/to/x.xxx.",
		"/path/to/git./xxx",
		"/path/to/git./xxx.",
		"/path/to/git./x.xxx.",
	};
	for (const auto& f : non_dot_list)
		CPPUNIT_ASSERT_MESSAGE(f, !is_dot_path(f));
}

void UtilTest::test_is_dir_writable(void) {
	if (!is_linux())
		return;

	const std::vector<std::string> writable_list{
		"/tmp",
	};
	for (const auto& f : writable_list)
		CPPUNIT_ASSERT_MESSAGE(f, is_dir_writable(f));

	const std::vector<std::string> unwritable_list{
		"/proc",
	};
	for (const auto& f : unwritable_list)
		CPPUNIT_ASSERT_MESSAGE(f, !is_dir_writable(f));

	const std::vector<std::string> invalid_list{
		"/proc/vmstat",
		"516e7cb4-6ecf-11d6-8ff8-00022d09712b",
	};
	for (const auto& f : invalid_list)
		try {
			is_dir_writable(f);
			CPPUNIT_FAIL(f);
		} catch (const std::exception& e) {
		}
}

void UtilTest::test_remove_dup_string(void) {
	const std::vector<std::vector<std::string>> uniq_ll{
		{""},
		{"/path/to/xxx"},
		{"/path/to/xxx", "/path/to/yyy"},
		{"xxx1", "xxx2"},
		{"xxx1", "xxx2", "xxx3"},
		{"xxx1", "xxx2", "xxx3", "xxx4"},
		{"xxx1", "xxx2", "xxx3", ""},
		{"a", "b", "c", "d", "e", "f"},
	};
	for (const auto& l : uniq_ll) {
		const auto x = remove_dup_string(l);
		for (size_t i = 0; i < x.size(); i++)
			for (size_t j = 0; j < x.size(); j++) {
				std::ostringstream ss;
				ss << i << ": " << x[i] << " vs "
					<< j << ": " << x[j];
				CPPUNIT_ASSERT_MESSAGE(ss.str(),
					!(i != j && x[i] == x[j]));
			}

		std::ostringstream ss;
		ss << l.size() << " != " << x.size();
		CPPUNIT_ASSERT_EQUAL_MESSAGE(ss.str(), l.size(), x.size());

		for (size_t i = 0; i < x.size(); i++) {
			std::ostringstream ss;
			ss << i << ": " << x[i] << " != " << l[i];
			CPPUNIT_ASSERT_EQUAL_MESSAGE(ss.str(), x[i], l[i]);
		}
	}

	const std::vector<std::vector<std::string>> dup_ll{
		{"", ""},
		{"", "", ""},
		{"/path/to/xxx", "/path/to/xxx"},
		{"xxx1", "xxx2", "xxx1"},
		{"xxx1", "xxx2", "xxx1", "xxx1"},
		{"xxx1", "xxx1", "xxx2", "xxx1"},
		{"xxx1", "xxx2", "xxx1", "xxx2"},
		{"a", "b", "c", "d", "e", "f", "a", "b", "c", "d", "e", "f"},
	};
	for (const auto& l : dup_ll) {
		const auto x = remove_dup_string(l);
		for (size_t i = 0; i < x.size(); i++)
			for (size_t j = 0; j < x.size(); j++) {
				std::ostringstream ss;
				ss << i << ": " << x[i] << " vs "
					<< j << ": " << x[j];
				CPPUNIT_ASSERT_MESSAGE(ss.str(),
					!(i != j && x[i] == x[j]));
			}

		std::ostringstream ss;
		ss << l.size() << " <= " << x.size();
		CPPUNIT_ASSERT_MESSAGE(ss.str(), l.size() > x.size());

		const auto xx = remove_dup_string(x);
		ss.clear();
		ss << x.size() << " != " << xx.size();
		CPPUNIT_ASSERT_EQUAL_MESSAGE(ss.str(), x.size(), xx.size());

		for (size_t i = 0; i < x.size(); i++) {
			std::ostringstream ss;
			ss << i << ": " << x[i] << " != " << xx[i];
			CPPUNIT_ASSERT_EQUAL_MESSAGE(ss.str(), x[i], xx[i]);
		}
	}
}

void UtilTest::test_get_random(void) {
	for (auto i = 1; i < 10000; i++) {
		auto x = get_random(0, i);
		CPPUNIT_ASSERT_MESSAGE(std::to_string(i), !(x < 0 || x >= i));
	}
	for (auto i = 1; i < 10000; i++) {
		auto x = get_random(-i, 0);
		CPPUNIT_ASSERT_MESSAGE(std::to_string(i), !(x < -i || x >= 0));
	}
}

void UtilTest::test_timer1(void) {
	auto timer = Timer(0, 0); // unused
	CPPUNIT_ASSERT(!timer.elapsed());
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	CPPUNIT_ASSERT(!timer.elapsed());
	CPPUNIT_ASSERT(!timer.elapsed());
	timer.reset();
	CPPUNIT_ASSERT(!timer.elapsed());

	timer = Timer(1, 0);
	CPPUNIT_ASSERT(!timer.elapsed());
	std::this_thread::sleep_for(std::chrono::seconds(1));
	CPPUNIT_ASSERT(timer.elapsed());
	CPPUNIT_ASSERT(timer.elapsed());
	timer.reset();
	CPPUNIT_ASSERT(!timer.elapsed());

	timer = Timer(2, 0);
	CPPUNIT_ASSERT(!timer.elapsed());
	std::this_thread::sleep_for(std::chrono::seconds(1));
	CPPUNIT_ASSERT(!timer.elapsed());
	CPPUNIT_ASSERT(!timer.elapsed());
	timer.reset();
	CPPUNIT_ASSERT(!timer.elapsed());
}

void UtilTest::test_timer2(void) {
	auto timer = Timer(0, 1000); // unused
	std::this_thread::sleep_for(std::chrono::seconds(1));
	CPPUNIT_ASSERT(!timer.elapsed());
	std::this_thread::sleep_for(std::chrono::seconds(1));
	CPPUNIT_ASSERT(!timer.elapsed());

	timer = Timer(1, 1000);
	std::this_thread::sleep_for(std::chrono::seconds(1));
	CPPUNIT_ASSERT(!timer.elapsed());
	std::this_thread::sleep_for(std::chrono::seconds(1));
	CPPUNIT_ASSERT(!timer.elapsed());
}

CPPUNIT_TEST_SUITE_REGISTRATION(UtilTest);
#endif
