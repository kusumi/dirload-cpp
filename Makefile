WERROR		?= true # default false
WARNING_LEVEL	?= 3 # 0,1,2,3,everything (default 1)
BUILDTYPE	?= release # default debug
CPPUNIT		?= false # default false

BUILDDIR	?= build

default:
	meson setup ${BUILDDIR} -Dwerror=${WERROR} -Dwarning_level=${WARNING_LEVEL} -Dbuildtype=${BUILDTYPE} -Dcppunit=${CPPUNIT}
	ninja -C ${BUILDDIR}
stdthread:
	meson setup ${BUILDDIR} -Dwerror=${WERROR} -Dwarning_level=${WARNING_LEVEL} -Dbuildtype=${BUILDTYPE} -Dcppunit=${CPPUNIT} -Dstdthread=true
	ninja -C ${BUILDDIR}
install:
	ninja -C ${BUILDDIR} install
uninstall:
	ninja -C ${BUILDDIR} uninstall
clean:
	rm -rf ${BUILDDIR}
test:
	./build/src/dirload-cpp -X
