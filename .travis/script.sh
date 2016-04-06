#!/usr/bin/env bash

set -ev

if [[ -n "${RELEASE}" ]]; then
  ./install.py -r --prefix=./install --threads 2
elif [[ "$CXX" = "g++" ]]; then
  ./install.py -p --prefix=./install --threads 2
else
  ./install.py -d --prefix=./install --threads 2
fi

./.travis/run_tests.sh

[[ -z "${RELEASE}" && "$CXX" = "g++" ]] || exit 0
# Check for memory leaks with Valgrind
valgrind --tool=memcheck --leak-check=full --show-leak-kinds=definite \
  --errors-for-leak-kinds=definite --error-exitcode=127 \
  --track-fds=yes \
  scram_tests \
  --gtest_filter=-*Death*:*Baobab*:*IncorrectFtaInputs:GateTest.Cycle \
  || [[ $? -ne 127 ]]

# Submit coverage of C++
coveralls --exclude tests  --exclude ./build/CMakeFiles/ \
  --exclude ./build/lib/ --exclude ./build/gui/ \
  --exclude ./build/bin/  --exclude install/include/ \
  --exclude gui/ \
  --exclude src/error.cc \
  --exclude src/env.cc \
  --exclude src/version.cc \
  --exclude src/logger.cc \
  --exclude-pattern .*/src/.*\.h$ \
  > /dev/null

# Submit coverage of Python
codecov > /dev/null

# Check documentation coverage
doxygen ./.travis/doxygen.conf > /dev/null 2> doc_errors.txt
# Deletion of compiler generated default functions
sed -i '/=delete/d' doc_errors.txt
# Doxygen 1.8.6 can't deal with C++11 initializer list in constructor.
sed -i '/expression\.cc/d' doc_errors.txt
if [[ -s doc_errors.txt ]]; then
  echo "Documentation errors:" >&2
  cat doc_errors.txt >&2
  exit 1
fi

# Lizard function complexity printout for C++ and Python
lizard -w -L 60 -a 5 -EIgnoreAssert src/* || echo "TODO: Fix the C++ complexity"
lizard -w -L 60 -a 5 scripts/*.py

# C++ linting
cpplint --repository=../ --quiet src/* tests/* 2> style.txt \
  || echo "TODO: Fix the C++ code"
# Clean false positives and noise
sed -i '/Found C system header after C\+\+/d' style.txt
sed -i '/Found C system header after other header/d' style.txt
sed -i '/Include the directory when naming/d' style.txt
sed -i '/^Ignoring/d' style.txt
sed -i '/^Skipping/d' style.txt
sed -i '/stream&/d' style.txt
sed -i '/is an unapproved C++11 header/d' style.txt
sed -i '/env\.cc:26/d' style.txt  # Unpredictable line length.
if [[ -s style.txt ]]; then
  echo "Style errors:" >&2
  cat style.txt >&2
  exit 1
fi
