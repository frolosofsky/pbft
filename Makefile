CXX = clang++
EXE = pbft
TEST = pbft_tests

.PHONY: run test clean

$(EXE): pbft_types.cpp main.cpp
	$(CXX) $^ -o $@ -std=c++14 -Wall -Wextra -pedantic -g

$(TEST): pbft_types.cpp pbft_tests.cpp
	$(CXX) $^ -o $@ -std=c++14 -Wall -Wextra -pedantic -g

run: $(EXE)
	@ ./$(EXE)

test: $(TEST)
	@ ./$(TEST) && echo "Passed" || echo "Failed"

clean:
	rm -f $(EXE) $(TEST)
