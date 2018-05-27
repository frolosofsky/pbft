CXX = clang++
CXX_FLAGS = -std=c++14 -Wall -Wextra -pedantic -Werror -Wimplicit-fallthrough -g
EXE = pbft
TEST = pbft_tests

.PHONY: run test clean

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
