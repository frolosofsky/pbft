CXX = g++
CXX_FLAGS = -std=c++14 -Wall -Wextra -pedantic -Werror -g
EXE = pbft
TEST = pbft_tests

ifeq ($(CXX),clang++)
CXX_FLAGS := $(CXX_FLAGS) -Wimplicit-fallthrough
endif

.PHONY: run test clean docker-build docker-run

$(EXE): pbft_types.cpp main.cpp
	$(CXX) $^ -o $@ $(CXX_FLAGS)

$(TEST): pbft_types.cpp pbft_tests.cpp
	$(CXX) $^ -o $@ $(CXX_FLAGS)

run: $(EXE)
	@ ./$(EXE)

test: $(TEST)
	@ ./$(TEST) && echo "Passed" || echo "Failed"

clean:
	rm -f $(EXE) $(TEST)

docker-build:
	docker build . -t sfrolov/pbft-ubuntu:16.04 --rm --force-rm

docker-run: docker-build
	docker run --rm -t sfrolov/pbft-ubuntu:16.04 make CXX=g++ -C /tmp/pbft-src test run
