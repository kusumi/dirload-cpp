#ifndef SRC_CPPUNIT_H_
#define SRC_CPPUNIT_H_

#ifdef CONFIG_CPPUNIT
int run_unittest(void);
#else
#include <iostream>

#include <cerrno>

static inline int run_unittest(void) {
	std::cout << "cppunit unsupported" << std::endl;
	return -EOPNOTSUPP;
}
#endif
#endif // SRC_CPPUNIT_H_
