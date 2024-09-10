#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

#include "./cppunit.h"

int run_unittest(void) {
	auto& registry = CppUnit::TestFactoryRegistry::getRegistry();
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(registry.makeTest());
	auto success = runner.run("", false);
	return success ? 0 : -1;
}
