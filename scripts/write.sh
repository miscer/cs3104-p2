#!/bin/bash

TEST_DATA=testing

REAL_FILE=./write.txt
TEST_FILE=$1/write.txt

echo ${TEST_DATA} > ${REAL_FILE}
echo ${TEST_DATA} > ${TEST_FILE}

cmp --silent ${REAL_FILE} ${TEST_FILE}

if [ $? -ne 0 ]; then
  echo "FAILED write test"
  exit -1
fi

echo "PASSED write test"
