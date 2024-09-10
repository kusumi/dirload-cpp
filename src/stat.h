#ifndef SRC_STAT_H_
#define SRC_STAT_H_

#include <vector>
#include <string>
#include <chrono>

class ThreadStat {
	public:
	explicit ThreadStat(bool);
	static ThreadStat newread(void) {
		return ThreadStat(true);
	}
	static ThreadStat newwrite(void) {
		return ThreadStat(false);
	}

	bool is_reader(void) const {
		return _is_reader;
	}
	bool is_ready(void) const {
		return !_input_path.empty();
	}

	const std::string& get_input_path(void) const {
		return _input_path;
	}
	std::chrono::steady_clock::time_point get_time_begin(void) const {
		return _time_begin;
	}
	std::chrono::steady_clock::time_point get_time_end(void) const {
		return _time_end;
	}
	unsigned long get_num_repeat(void) const {
		return _num_repeat;
	}
	unsigned long get_num_stat(void) const {
		return _num_stat;
	}
	unsigned long get_num_read(void) const {
		return _num_read;
	}
	unsigned long get_num_read_bytes(void) const {
		return _num_read_bytes;
	}
	unsigned long get_num_write(void) const {
		return _num_write;
	}
	unsigned long get_num_write_bytes(void) const {
		return _num_write_bytes;
	}
	bool is_done(void) const {
		return _done;
	}

	void set_input_path(const std::string& f) {
		_input_path = f;
	}
	void set_time_begin(void) {
		_time_begin = std::chrono::steady_clock::now();
	}
	void set_time_end(void) {
		_time_end = std::chrono::steady_clock::now();
	}
	void inc_num_repeat(void) {
		_num_repeat++;
	}
	void inc_num_stat(void) {
		_num_stat++;
	}
	void inc_num_read(void) {
		_num_read++;
	}
	void add_num_read_bytes(unsigned long siz) {
		_num_read_bytes += siz;
	}
	void inc_num_write(void) {
		_num_write++;
	}
	void add_num_write_bytes(unsigned long siz) {
		_num_write_bytes += siz;
	}
	void set_done(void) {
		_done = true;
	}

	template <class T> T time_diff(void) const {
		return std::chrono::duration_cast<T>(_time_end - _time_begin);
	}
	template <class T> T time_elapsed(void) const {
		return std::chrono::duration_cast<T>(
			std::chrono::steady_clock::now() - _time_begin);
	}
	bool sec_elapsed(long) const;

	private:
	bool _is_reader;
	std::string _input_path;
	std::chrono::steady_clock::time_point _time_begin;
	std::chrono::steady_clock::time_point _time_end;
	unsigned long _num_repeat;
	unsigned long _num_stat;
	unsigned long _num_read;
	unsigned long _num_read_bytes;
	unsigned long _num_write;
	unsigned long _num_write_bytes;
	bool _done;
};

void print_stat(const std::vector<ThreadStat>&);
void print_stat(const std::vector<const ThreadStat*>&);

#ifdef CONFIG_CPPUNIT
#include <cppunit/TestFixture.h>
#include <cppunit/TestSuite.h>
#include <cppunit/extensions/HelperMacros.h>

class StatTest: public CPPUNIT_NS::TestFixture {
	public:
	CPPUNIT_TEST_SUITE(StatTest);
	CPPUNIT_TEST(test_newread);
	CPPUNIT_TEST(test_newwrite);
	CPPUNIT_TEST(test_set_time);
	CPPUNIT_TEST(test_time_elapsed);
	CPPUNIT_TEST(test_inc_num_repeat);
	CPPUNIT_TEST(test_inc_num_stat);
	CPPUNIT_TEST(test_inc_num_read);
	CPPUNIT_TEST(test_add_num_read_bytes);
	CPPUNIT_TEST(test_inc_num_write);
	CPPUNIT_TEST(test_add_num_write_bytes);
	CPPUNIT_TEST_SUITE_END();

	private:
	void test_newread(void);
	void test_newwrite(void);
	void test_set_time(void);
	void test_time_elapsed(void);
	void test_inc_num_repeat(void);
	void test_inc_num_stat(void);
	void test_inc_num_read(void);
	void test_add_num_read_bytes(void);
	void test_inc_num_write(void);
	void test_add_num_write_bytes(void);
};
#endif
#endif // SRC_STAT_H_
