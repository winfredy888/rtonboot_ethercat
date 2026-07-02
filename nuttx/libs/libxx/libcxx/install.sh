#!/bin/bash

usage="USAGE: $0 <full path to the NuttX directory>"

# Get the single, required command line argument

nuttx_path=$1
if [ -z "${nuttx_path}" ]; then
  echo "ERROR: Missing path to the NuttX directory"
  echo $usage
  exit 1
fi

# Lots of sanity checking so that we do not do anything too stupid

if [ ! -d src ]; then
  echo "ERROR: Directory src/ does not exist in this directory"
  echo "       Please CD into the libcxx directory and try again"
  echo $usage
  exit 1
fi

if [ ! -d include ]; then
  echo "ERROR: Directory include/ does not exist in this directory"
  echo "       Please CD into the libc++ directory and try again"
  echo $usage
  exit 1
fi

if [ ! -d machine ]; then
  echo "ERROR: Directory machine/ does not exist in this directory"
  echo "       Please CD into the libc++ directory and try again"
  echo $usage
  exit 1
fi

if [ ! -d "${nuttx_path}" ]; then
  echo "ERROR: Directory ${nuttx_path} does not exist"
  echo $usage
  exit 1
fi

if [ ! -f "${nuttx_path}/Makefile" ]; then
  echo "ERROR: No Makefile in directory ${nuttx_path}"
  echo $usage
  exit 1
fi

libxx_srcdir=${nuttx_path}/libs/libxx

if [ ! -d "${libxx_srcdir}" ]; then
  echo "ERROR: Directory ${libxx_srcdir} does not exist"
  echo $usage
  exit 1
fi

if [ ! -f "${libxx_srcdir}/Makefile" ]; then
  echo "ERROR: No Makefile in directory ${libxx_srcdir}"
  echo $usage
  exit 1
fi

libcxx_srcdir=${libxx_srcdir}/libcxx

if [ -d "${libcxx_srcdir}" ]; then
  echo "ERROR: Directory ${libcxx_srcdir} already exists"
  echo "       Please remove the  ${libcxx_srcdir} directory and try again"
  echo $usage
  exit 1
fi

nuttx_incdir=${nuttx_path}/include

if [ ! -d "${nuttx_incdir}" ]; then
  echo "ERROR: Directory ${nuttx_incdir} does not exist"
  echo $usage
  exit 1
fi

nuttxcxx_incdir=${nuttx_incdir}/cxx

if [ ! -d "${nuttxcxx_incdir}" ]; then
  echo "ERROR: Directory ${nuttxcxx_incdir} does not exist"
  echo $usage
  exit 1
fi

libcxx_incdir=${nuttx_incdir}/libcxx

if [ -d "${libcxx_incdir}" ]; then
  echo "ERROR: Directory ${libcxx_incdir} already exists"
  echo "       Please remove the  ${libcxx_incdir} directory and try again"
  echo $usage
  exit 1
fi

machine_incdir=${nuttx_incdir}/machine

if [ -d "${machine_incdir}" ]; then
  echo "ERROR: Directory ${machine_incdir} already exists"
  echo "       Please remove the  ${machine_incdir} directory and try again"
  echo $usage
  exit 1
fi

# Installation

echo "Installing LLVM/libcxx in the NuttX source tree"

filelist=`find src -type f`

for file in $filelist; do
  if [ "${file##*.}" = "cpp" ]; then
    endfile="`basename ${file} .cpp`.cxx"
  else
    endfile=${file}
  fi
  install -D ${file} ${libxx_srcdir}/libcxx/${endfile#src/}
done

mkdir -p ${libcxx_incdir}

filelist=`find include -type f`

for file in $filelist; do
  install -D ${file} ${nuttx_path}/include/libcxx/${file#include/}
done

filelist=`find machine -type f`

for file in $filelist; do
  install -D ${file} ${nuttx_path}/include/${file}
done

echo "Installation suceeded"
echo ""
