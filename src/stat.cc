#include <iostream>
#include <iomanip>
#include <sstream>
#include <array>

#include <cassert>

#include "./stat.h"

ThreadStat::ThreadStat(bool is_reader):
	_is_reader(is_reader),
	_input_path{},
	_time_begin(std::chrono::steady_clock::now()),
	_time_end(_time_begin),
	_num_repeat(0),
	_num_stat(0),
	_num_read(0),
	_num_read_bytes(0),
	_num_write(0),
	_num_write_bytes(0),
	_done(false) {
}

bool ThreadStat::sec_elapsed(long d) const {
	if (d <= 0)
		return false;
	return time_elapsed<std::chrono::seconds>().count() > d;
}

void print_stat(const std::vector<ThreadStat>& tsv) {
	std::vector<const ThreadStat*> v;
	for (const auto& ts : tsv)
		v.push_back(&ts);
	print_stat(v);
}

void print_stat(const std::vector<const ThreadStat*>& tsv) {
	// repeat
	auto width_repeat = std::string("repeat").size();
	for (const auto& ts : tsv) {
		auto s = std::to_string(ts->get_num_repeat());
		if (s.size() > width_repeat)
			width_repeat = s.size();
	}

	// stat
	auto width_stat = std::string("stat").size();
	for (const auto& ts : tsv) {
		auto s = std::to_string(ts->get_num_stat());
		if (s.size() > width_stat)
			width_stat = s.size();
	}

	// read
	auto width_read = std::string("read").size();
	for (const auto& ts : tsv) {
		auto s = std::to_string(ts->get_num_read());
		if (s.size() > width_read)
			width_read = s.size();
	}

	// read[B]
	auto width_read_bytes = std::string("read[B]").size();
	for (const auto& ts : tsv) {
		auto s = std::to_string(ts->get_num_read_bytes());
		if (s.size() > width_read_bytes)
			width_read_bytes = s.size();
	}

	// write
	auto width_write = std::string("write").size();
	for (const auto& ts : tsv) {
		auto s = std::to_string(ts->get_num_write());
		if (s.size() > width_write)
			width_write = s.size();
	}

	// write[B]
	auto width_write_bytes = std::string("write[B]").size();
	for (const auto& ts : tsv) {
		auto s = std::to_string(ts->get_num_write_bytes());
		if (s.size() > width_write_bytes)
			width_write_bytes = s.size();
	}

	// sec
	std::vector<double> num_sec;
	for (const auto& ts : tsv) {
		auto sec = static_cast<double>(
			ts->time_diff<std::chrono::milliseconds>().count());
		num_sec.push_back(sec / 1000);
	}
	auto width_sec = std::string("sec").size();
	std::vector<std::string> num_sec_str;
	for (const auto& x : num_sec) {
		std::ostringstream ss;
		ss << std::fixed << std::setprecision(2) << x;
		auto s = ss.str();
		if (s.size() > width_sec)
			width_sec = s.size();
		num_sec_str.push_back(s);
	}

	// MiB/sec
	std::vector<double> num_mibs;
	for (size_t i = 0; i < tsv.size(); i++) {
		auto mib = static_cast<double>(tsv[i]->get_num_read_bytes() +
			tsv[i]->get_num_write_bytes()) / (1 << 20);
		num_mibs.push_back(mib / num_sec[i]);
	}
	auto width_mibs = std::string("MiB/sec").size();
	std::vector<std::string> num_mibs_str;
	for (const auto& x : num_mibs) {
		std::ostringstream ss;
		ss << std::fixed << std::setprecision(2) << x;
		auto s = ss.str();
		if (s.size() > width_mibs)
			width_mibs = s.size();
		num_mibs_str.push_back(s);
	}

	// path
	auto width_path = std::string("path").size();
	for (const auto& ts : tsv) {
		auto& s = ts->get_input_path();
		assert(!s.empty());
		if (s.size() > width_path)
			width_path = s.size();
	}

	// index
	auto nlines = tsv.size();
	auto width_index = 1lu;
	auto n = nlines;
	if (n > 0) {
		n--; // gid starts from 0
		width_index = std::to_string(n).size();
	}

	auto slen = 0lu;
	std::cout << std::string(1 + width_index + 1, ' ');
	slen += 1 + width_index + 1;
	std::cout << std::left << std::setw(6) << "type";
	std::cout << " ";
	slen += 6 + 1;
	const std::array<std::string, 9> ls{
		"repeat",
		"stat",
		"read",
		"read[B]",
		"write",
		"write[B]",
		"sec",
		"MiB/sec",
		"path",
	};
	const std::array<unsigned long, 9> lw{
		width_repeat,
		width_stat,
		width_read,
		width_read_bytes,
		width_write,
		width_write_bytes,
		width_sec,
		width_mibs,
		width_path,
	};
	for (size_t i = 0; i < ls.size(); i++) {
		std::cout << std::left << std::setw(static_cast<int>(lw[i]))
			<< ls[i];
		slen += lw[i];
		if (i != ls.size() - 1) {
			std::cout << " ";
			slen += 1;
		}
	}
	std::cout << std::endl;
	std::cout << std::string(slen, '-') << std::endl;

	for (auto i = 0; i < static_cast<int>(nlines); i++) {
		const auto& p = tsv[i];
		// index (left align)
		std::cout << "#" << std::left
			<< std::setw(static_cast<int>(width_index)) << i << " ";
		// type
		if (p->is_reader())
			std::cout << "reader ";
		else
			std::cout << "writer ";
		// repeat
		std::cout << std::right << std::setw(static_cast<int>(lw[0]))
			<< p->get_num_repeat() << " ";
		// stat
		std::cout << std::right << std::setw(static_cast<int>(lw[1]))
			<< p->get_num_stat() << " ";
		// read
		std::cout << std::right << std::setw(static_cast<int>(lw[2]))
			<< p->get_num_read() << " ";
		// read[B]
		std::cout << std::right << std::setw(static_cast<int>(lw[3]))
			<< p->get_num_read_bytes() << " ";
		// write
		std::cout << std::right << std::setw(static_cast<int>(lw[4]))
			<< p->get_num_write() << " ";
		// write[B]
		std::cout << std::right << std::setw(static_cast<int>(lw[5]))
			<< p->get_num_write_bytes() << " ";
		// sec
		std::cout << std::right << std::setw(static_cast<int>(lw[6]))
			<< num_sec_str[i] << " ";
		// MiB/sec
		std::cout << std::right << std::setw(static_cast<int>(lw[7]))
			<< num_mibs_str[i] << " ";
		// path (left align)
		std::cout << std::left << std::setw(static_cast<int>(lw[8]))
			<< p->get_input_path() << " ";
		std::cout << std::endl;
	}
	std::cout << std::flush;
}

#ifdef CONFIG_CPPUNIT
#include <thread>

#include <cppunit/TestAssert.h>

#include "./cppunit.h"

void StatTest::test_newread(void) {
	auto ts = ThreadStat::newread();
	CPPUNIT_ASSERT(ts.is_reader());
	CPPUNIT_ASSERT(ts.get_input_path().empty());
	CPPUNIT_ASSERT_EQUAL(ts.get_num_repeat(), 0lu);
	CPPUNIT_ASSERT_EQUAL(ts.get_num_stat(), 0lu);
	CPPUNIT_ASSERT_EQUAL(ts.get_num_read(), 0lu);
	CPPUNIT_ASSERT_EQUAL(ts.get_num_read_bytes(), 0lu);
	CPPUNIT_ASSERT_EQUAL(ts.get_num_write(), 0lu);
	CPPUNIT_ASSERT_EQUAL(ts.get_num_write_bytes(), 0lu);
}

void StatTest::test_newwrite(void) {
	auto ts = ThreadStat::newwrite();
	CPPUNIT_ASSERT(!ts.is_reader());
	CPPUNIT_ASSERT(ts.get_input_path().empty());
	CPPUNIT_ASSERT_EQUAL(ts.get_num_repeat(), 0lu);
	CPPUNIT_ASSERT_EQUAL(ts.get_num_stat(), 0lu);
	CPPUNIT_ASSERT_EQUAL(ts.get_num_read(), 0lu);
	CPPUNIT_ASSERT_EQUAL(ts.get_num_read_bytes(), 0lu);
	CPPUNIT_ASSERT_EQUAL(ts.get_num_write(), 0lu);
	CPPUNIT_ASSERT_EQUAL(ts.get_num_write_bytes(), 0lu);
}

void StatTest::test_set_time(void) {
	auto ts = ThreadStat::newread();
	CPPUNIT_ASSERT_EQUAL(std::chrono::duration_cast<std::chrono::seconds>(
		ts.get_time_end() - ts.get_time_begin()).count(), 0l);

	CPPUNIT_ASSERT_EQUAL(std::chrono::duration_cast<std::chrono::seconds>(
		ts.get_time_begin() - ts.get_time_end()).count(), 0l);

	ts.set_time_begin();
	std::this_thread::sleep_for(std::chrono::seconds(1));
	ts.set_time_end();
	CPPUNIT_ASSERT(ts.time_diff<std::chrono::seconds>().count() >= 1l);

	ts.set_time_begin();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	ts.set_time_end();
	CPPUNIT_ASSERT(ts.time_diff<std::chrono::milliseconds>().count()
		>= 100l);

	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	ts.set_time_end();
	CPPUNIT_ASSERT(ts.time_diff<std::chrono::milliseconds>().count()
		>= 200l);
}

void StatTest::test_time_elapsed(void) {
	auto ts = ThreadStat::newread();
	ts.set_time_begin();

	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	CPPUNIT_ASSERT(ts.time_elapsed<std::chrono::milliseconds>().count()
		>= 100);
	CPPUNIT_ASSERT_EQUAL(ts.time_elapsed<std::chrono::seconds>().count(),
		0l);
	CPPUNIT_ASSERT(!ts.sec_elapsed(1));

	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	CPPUNIT_ASSERT(ts.time_elapsed<std::chrono::milliseconds>().count()
		>= 200);
	CPPUNIT_ASSERT_EQUAL(ts.time_elapsed<std::chrono::seconds>().count(),
		0l);
	CPPUNIT_ASSERT(!ts.sec_elapsed(1));
}

void StatTest::test_inc_num_repeat(void) {
	auto ts = ThreadStat::newread();
	ts.inc_num_repeat();
	CPPUNIT_ASSERT_EQUAL(ts.get_num_repeat(), 1lu);
	ts.inc_num_repeat();
	CPPUNIT_ASSERT_EQUAL(ts.get_num_repeat(), 2lu);
}

void StatTest::test_inc_num_stat(void) {
	auto ts = ThreadStat::newread();
	ts.inc_num_stat();
	CPPUNIT_ASSERT_EQUAL(ts.get_num_stat(), 1lu);
	ts.inc_num_stat();
	CPPUNIT_ASSERT_EQUAL(ts.get_num_stat(), 2lu);
}

void StatTest::test_inc_num_read(void) {
	auto ts = ThreadStat::newread();
	ts.inc_num_read();
	CPPUNIT_ASSERT_EQUAL(ts.get_num_read(), 1lu);
	ts.inc_num_read();
	CPPUNIT_ASSERT_EQUAL(ts.get_num_read(), 2lu);
}

void StatTest::test_add_num_read_bytes(void) {
	auto ts = ThreadStat::newread();
	const auto siz = 1234lu;
	ts.add_num_read_bytes(siz);
	CPPUNIT_ASSERT_EQUAL(ts.get_num_read_bytes(), siz);
	ts.add_num_read_bytes(siz);
	CPPUNIT_ASSERT_EQUAL(ts.get_num_read_bytes(), siz * 2);
	ts.add_num_read_bytes(0);
	CPPUNIT_ASSERT_EQUAL(ts.get_num_read_bytes(), siz * 2);
}

void StatTest::test_inc_num_write(void) {
	auto ts = ThreadStat::newwrite();
	ts.inc_num_write();
	CPPUNIT_ASSERT_EQUAL(ts.get_num_write(), 1lu);
	ts.inc_num_write();
	CPPUNIT_ASSERT_EQUAL(ts.get_num_write(), 2lu);
}

void StatTest::test_add_num_write_bytes(void) {
	auto ts = ThreadStat::newwrite();
	const auto siz = 1234lu;
	ts.add_num_write_bytes(siz);
	CPPUNIT_ASSERT_EQUAL(ts.get_num_write_bytes(), siz);
	ts.add_num_write_bytes(siz);
	CPPUNIT_ASSERT_EQUAL(ts.get_num_write_bytes(), siz * 2);
	ts.add_num_write_bytes(0);
	CPPUNIT_ASSERT_EQUAL(ts.get_num_write_bytes(), siz * 2);
}

CPPUNIT_TEST_SUITE_REGISTRATION(StatTest);
#endif
