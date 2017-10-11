#!/bin/bash

if [ ! -d "${QNX_HOST}" ]; then
  echo "QNX_HOST must be set to the path of the QNX host toolchain."
  exit 1
fi

if [ ! -d "${QNX_TARGET}" ]; then
  echo "QNX_TARGET must be set to the path of the QNX target toolchain."
  exit 1
fi

if [ "${QCONF_OVERRIDE}" != "" ]; then
  cp -p $QCONF_OVERRIDE /tmp/owbqsk$$.mk
  echo "all:" >>/tmp/owbqsk$$.mk
  echo '	echo $(INSTALL_ROOT_nto)' >>/tmp/owbqsk$$.mk
  STAGE_DIR=`make -s -f /tmp/owbqsk$$.mk`
  rm /tmp/owbqsk$$.mk
fi

if [ "${STAGE_DIR}" == "" ]; then
  echo Staging directory could not be determined. Using NDK.
else
  echo Using staging directory: ${STAGE_DIR}
fi

if [ "${1}" == "clean" ]; then
  make -f Makefile clean
  exit 1
fi

if [ "${1}" == "x86" ]; then
  CPU="${1}"
  CPUDIR="${CPU}"
  CPUTYPE="${CPU}"
  BUSUFFIX="${CPU}"
  CPU_CFLAGS=""
else
  echo Building for arm
  CPU="arm"
  CPUDIR="${CPU}le-v7"
  CPUTYPE="${CPU}v7le"
  BUSUFFIX="${CPU}v7"
  if [ "${2}" == "" ]; then
    CPU_VER="cortex-a9"
  else
    CPU_VER="${2}"
  fi
  CPU_CFLAGS="-mtune=${CPU_VER} -mfpu=vfpv3"
  #CPU_CFLAGS="-mtune=${CPU_VER} -mfpu=vfpv3 -mfloat-abi=hard" can't get to work
  #CPU_CFLAGS="-mtune=${CPU_VER} -mfpu=vfpv3-d16" for Tegra2
fi

QNX_TOOL_DIR="${QNX_HOST}/usr/bin"
QNX_COMPILER="${QNX_TOOL_DIR}/ntoarmv7-gcc"
QNX_COMPILER="${QNX_TOOL_DIR}/qcc"
QNX_TOOL_PREFIX="${QNX_TOOL_DIR}/nto${BUSUFFIX}"

if [ "${STAGE_DIR}" == "" ]; then 
  QNX_LIB="${QNX_TARGET}/${CPUDIR}/lib"
  QNX_USR_LIB="${QNX_TARGET}/${CPUDIR}/usr/lib"
  QNX_INC="${QNX_TARGET}/usr/include"
else
  QNX_LIB="${STAGE_DIR}/${CPUDIR}/lib"
  QNX_USR_LIB="${STAGE_DIR}/${CPUDIR}/usr/lib"
  QNX_INC="${STAGE_DIR}/usr/include"
fi

COMP_PATHS=" \
  -Wl,-rpath-link,${QNX_LIB} \
  -Wl,-rpath-link,${QNX_USR_LIB} \
  -L${QNX_LIB} \
  -L${QNX_USR_LIB} \
  -I${QNX_INC}"

export CC="${QNX_COMPILER}"
export CFLAGS="-Vgcc_nto${CPUTYPE} -g -Wformat -Wformat-security -Werror=format-security -Wl,-z,relro -fPIE -pie ${COMP_PATHS} ${CPU_CFLAGS}"
#export CFLAGS="-Vgcc_nto${CPUTYPE} -g -Wformat -Wformat-security -Werror=format-security -Wl,-z,relro -fPIE -D__QNXNTO65__ ${COMP_PATHS} ${CPU_CFLAGS}" for QNX650
export CXX="${QNX_COMPILER}"
#export CXXFLAGS="-Vgcc_nto${CPUTYPE}_cpp-ne -g -lang-c++ -Wformat -Wformat-security -Werror=format-security -Wl,-z,relro -fPIE -pie ${COMP_PATHS} ${CPU_CFLAGS}"
export CXXFLAGS="-Vgcc_nto${CPUTYPE}_cpp-ne -g -lang-c++ -Wformat -Wformat-security -Werror=format-security -Wl,-z,relro -fPIE -Wl,--export-dynamic ${COMP_PATHS} ${CPU_CFLAGS}"
export AR="${QNX_TOOL_PREFIX}-ar"
export LINK="${QNX_COMPILER}"
export LDFLAGS="${CXXFLAGS} -lcrypto -lssl"
export RANLIB="${QNX_TOOL_PREFIX}-ranlib"

# The set of GYP_DEFINES to pass to gyp.
export GYP_DEFINES="OS=qnx want_separate_host_toolset=0"
#export GYP_GENERATORS="make-linux"

CONFIGURE_OPTIONS=""

if [ "${CPU}" != "x86" ]; then
  #CONFIGURE_OPTIONS="--dest-cpu=arm --with-arm-float-abi=softfp --without-snapshot" for Tegra2
  CONFIGURE_OPTIONS="--dest-cpu=arm --dest-os=qnx --with-arm-float-abi=softfp --without-snapshot --without-dtrace"
else
  CONFIGURE_OPTIONS="--dest-cpu=ia32"
fi

./configure --shared-openssl --shared-zlib ${CONFIGURE_OPTIONS}

if [ "${1}" == "test" ]; then
  make test
else
  make -j4
fi

