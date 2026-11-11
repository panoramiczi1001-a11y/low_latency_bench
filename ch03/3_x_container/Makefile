CXX      := g++
CXXFLAGS := -g -std=c++17
SRCS     := 3malloc.cpp 3ptr.cpp 3vecres.cpp 3vector.cpp
NAMES    := $(SRCS:.cpp=)
OPTS     := O0 O2
TEST_BIN := $(foreach s,$(NAMES),$(foreach o,$(OPTS),test_$(s)_$(o)))

TEST_SRC ?= test_p99.cpp   # 换成 $(TEST_SRC) 即为原始单值版本
N        ?= 100
ROUNDS   ?= 20000
REP      ?= 100
RESULT   := results/results.txt

.PHONY: all clean run perf record

all: $(TEST_BIN)

test_%_O0: $(TEST_SRC) %.cpp
	$(CXX) $(CXXFLAGS) -O0 -c $(TEST_SRC) -o test_$*_O0.o
	$(CXX) $(CXXFLAGS) -O0 -Dmain=orig_main -c $*.cpp -o $*_O0.o
	$(CXX) test_$*_O0.o $*_O0.o -o $@

test_%_O2: $(TEST_SRC) %.cpp
	$(CXX) $(CXXFLAGS) -O2 -c $(TEST_SRC) -o test_$*_O2.o
	$(CXX) $(CXXFLAGS) -O2 -Dmain=orig_main -c $*.cpp -o $*_O2.o
	$(CXX) test_$*_O2.o $*_O2.o -o $@

run: all
	@mkdir -p results
	@echo "=== timing (n=$(N), rounds=$(ROUNDS), rep=$(REP)) ==="
	@for b in $(TEST_BIN); do \
		printf "%-22s " $$b; \
		./$$b $(N) $(ROUNDS) $(REP); \
	done

perf: all
	@echo "=== perf stat (n=$(N), rounds=200, rep=50) ==="
	@for b in $(TEST_BIN); do \
		echo "--- $$b ---"; \
		perf stat -e cycles \
			./$$b $(N) 200 50 2>&1 | grep -E 'cycles' || true; \
	done

record: all
	@mkdir -p results
	@echo "=== timing (n=$(N), rounds=$(ROUNDS), rep=$(REP)) ===" > $(RESULT)
	@for b in $(TEST_BIN); do \
		printf "%-22s " $$b >> $(RESULT); \
		./$$b $(N) $(ROUNDS) $(REP) >> $(RESULT) 2>&1; \
	done
	@echo "" >> $(RESULT)
	@echo "=== perf stat (n=$(N), rounds=200, rep=50) ===" >> $(RESULT)
	@for b in $(TEST_BIN); do \
		echo "--- $$b ---" >> $(RESULT); \
		perf stat -e cycles \
			./$$b $(N) 200 50 >> $(RESULT) 2>&1 || true; \
	done
	@echo "done: $(RESULT)"

clean:
	rm -f $(TEST_BIN)
	rm -f *.o
	rm -rf asm
