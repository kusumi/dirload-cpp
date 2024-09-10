#ifndef SRC_UTIL_H_
#define SRC_UTIL_H_

#include <vector>
#include <string>
#include <chrono>
#include <random>

#include <cassert>

enum class FileType {
	Dir,
	Reg,
	Device,
	Symlink,
	Unsupported,
};

class Timer {
	public:
	Timer(long, long);
	bool elapsed(void);
	void reset(void);

	private:
	std::chrono::steady_clock::time_point _time_begin;
	long _duration;
	long _frequency;
	long _counter;
};

std::string get_abspath(const std::string&, bool=false);
std::string get_dirpath(const std::string&, bool=false);
std::string get_basename(const std::string&, bool=false);
bool is_abspath(const std::string&);
std::string join_path(const std::string&, const std::string&, bool=false);
bool is_linux(void);
bool is_windows(void);
char get_path_separator(void);
FileType get_raw_file_type(const std::string&);
FileType get_file_type(const std::string&);
bool path_exists(const std::string&);
bool is_dot_path(const std::string&);
bool is_dir_writable(const std::string&);
std::vector<std::string> remove_dup_string(const std::vector<std::string>&);
std::string get_time_string(void);
std::mt19937& get_random_engine(void);

template <class T> T get_random(T beg, T end) {
	assert(beg < end);
	end--; // caller assumes [beg, end)

	std::uniform_int_distribution<T> dist(beg, end);
	return dist(get_random_engine());
}

#ifdef CONFIG_CPPUNIT
#include <cppunit/TestFixture.h>
#include <cppunit/TestSuite.h>
#include <cppunit/extensions/HelperMacros.h>

class UtilTest: public CPPUNIT_NS::TestFixture {
	public:
	CPPUNIT_TEST_SUITE(UtilTest);
	CPPUNIT_TEST(test_canonicalize_path);
	CPPUNIT_TEST(test_get_abspath);
	CPPUNIT_TEST(test_get_dirpath);
	CPPUNIT_TEST(test_get_basename);
	CPPUNIT_TEST(test_is_abspath);
	CPPUNIT_TEST(test_is_windows);
	CPPUNIT_TEST(test_get_path_separator);
	CPPUNIT_TEST(test_get_raw_file_type);
	CPPUNIT_TEST(test_get_file_type);
	CPPUNIT_TEST(test_path_exists);
	CPPUNIT_TEST(test_is_dot_path);
	CPPUNIT_TEST(test_is_dir_writable);
	CPPUNIT_TEST(test_remove_dup_string);
	CPPUNIT_TEST(test_get_random);
	CPPUNIT_TEST(test_timer1);
	CPPUNIT_TEST(test_timer2);
	CPPUNIT_TEST_SUITE_END();

	private:
	void test_canonicalize_path(void);
	void test_get_abspath(void);
	void test_get_dirpath(void);
	void test_get_basename(void);
	void test_is_abspath(void);
	void test_is_windows(void);
	void test_get_path_separator(void);
	void test_get_raw_file_type(void);
	void test_get_file_type(void);
	void test_path_exists(void);
	void test_is_dot_path(void);
	void test_is_dir_writable(void);
	void test_remove_dup_string(void);
	void test_get_random(void);
	void test_timer1(void);
	void test_timer2(void);
};
#endif
#endif // SRC_UTIL_H_
