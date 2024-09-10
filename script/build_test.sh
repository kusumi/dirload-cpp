#!/bin/bash

if [ "${WARNING_LEVEL}" = "" ]; then
	WARNING_LEVEL=3
fi
WARNING="-Dwarning_level=${WARNING_LEVEL}"

BUILDTYPE_RELEASE="-Dbuildtype=release" # default debug
STDTHREAD="-Dstdthread=true" # default false
CPPUNIT="-Dcppunit=true" # default false

MESON_BUILD=./meson.build
if [ ! -f ${MESON_BUILD} ]; then
	echo "XXX No such file ${MESON_BUILD}"
	exit 1
fi

BUILDDIR=./build
clean_builddir() {
	rm -rf ${BUILDDIR}
}

exec_cmd() {
	cmd=$1
	echo ${cmd}
	${cmd}
	if [ $? -ne 0 ]; then
		echo "XXX \"${cmd}\" failed"
		exit 1
	fi
}

assert_bin() {
	a=$1
	b=$2
	c=$3

	f=${BUILDDIR}/src/dirload-cpp
	echo ${f}
	if [ ! -f ${f} ]; then
		echo "XXX No such file ${f}"
		exit 1
	fi
	${f} -x || exit 1

	sym=openlog
	nm ${f} | grep ${sym} >/dev/null
	res=$?
	if [ "${a}" = ${BUILDTYPE_RELEASE} ]; then
		if [ ${res} -eq 0 ]; then
			echo "XXX Illegal ${sym}"
			exit 1
		fi
	else
		if [ ${res} -ne 0 ]; then
			echo "XXX Missing ${sym}"
			exit 1
		fi
	fi

	sym=pthread_create
	nm ${f} | grep ${sym} >/dev/null
	res=$?
	if [ "${b}" = ${STDTHREAD} ]; then
		if [ ${res} -eq 0 ]; then
			echo "XXX Illegal ${sym}"
			exit 1
		fi
	else
		if [ ${res} -ne 0 ]; then
			echo "XXX Missing ${sym}"
			exit 1
		fi
	fi

	sym=CppUnit
	nm ${f} | grep ${sym} >/dev/null
	res=$?
	if [ "${c}" = ${CPPUNIT} ]; then
		if [ ${res} -ne 0 ]; then
			echo "XXX Illegal ${sym}"
			exit 1
		fi
		${f} -X || exit 1 # run cppunit
	else
		if [ ${res} -eq 0 ]; then
			echo "XXX Missing ${sym}"
			exit 1
		fi
	fi
}

for a in "" ${BUILDTYPE_RELEASE}; do
	for b in "" ${STDTHREAD}; do
		for c in "" ${CPPUNIT}; do
			echo "========================================"
			clean_builddir
			exec_cmd "meson setup ${BUILDDIR} -Dwerror=true ${WARNING} ${a} ${b} ${c}"
			exec_cmd "ninja -C ${BUILDDIR}"
			assert_bin "${a}" "${b}" "${c}"
		done
	done
done
clean_builddir

echo "success"
