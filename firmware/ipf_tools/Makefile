# Desktop build of the converter and its tests (Linux/macOS).
#   make            -> build/ipf2hfe, build/verify, build/glue_test
#   make check      -> convert + verify tests/synth.ipf
#   make check IPF=path/to/game.ipf
# Sources live in the sketch: ../Gotek_P4_JC1060/src/ipf
SRC := ../Gotek_P4_JC1060/src/ipf
CXX ?= g++
CXXFLAGS ?= -O2 -std=gnu++17 -I$(SRC)
B := build
CAPS_SRC := $(wildcard $(SRC)/capsimg/*.cpp)
CAPS_OBJ := $(patsubst $(SRC)/capsimg/%.cpp,$(B)/caps/%.o,$(CAPS_SRC))
IPF ?= $(B)/synth.ipf

all: $(B)/ipf2hfe $(B)/verify $(B)/glue_test

$(B)/caps/%.o: $(SRC)/capsimg/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -w -c $< -o $@

$(B)/ipf2hfe.o: $(SRC)/ipf2hfe.cpp $(SRC)/ipf2hfe.h
	@mkdir -p $(B)
	$(CXX) $(CXXFLAGS) -Wall -c $< -o $@

$(B)/ipf2hfe: tools/ipf2hfe_cli.cpp $(B)/ipf2hfe.o $(CAPS_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(B)/verify: tests/verify.cpp $(CAPS_OBJ)
	$(CXX) $(CXXFLAGS) -w $^ -o $@

$(B)/glue_test: tests/glue_test.cpp $(SRC)/gti_ipf.h $(B)/ipf2hfe.o $(CAPS_OBJ)
	$(CXX) $(CXXFLAGS) -Itests/shim -I$(SRC) tests/glue_test.cpp $(B)/ipf2hfe.o $(CAPS_OBJ) -o $@

$(B)/synth.ipf: tests/mkipf.py
	@mkdir -p $(B)
	python3 tests/mkipf.py $@

check: all $(IPF)
	./$(B)/ipf2hfe "$(IPF)" $(B)/check.hfe
	./$(B)/verify "$(IPF)" $(B)/check.hfe -q

clean:
	rm -rf $(B)

.PHONY: all check clean
