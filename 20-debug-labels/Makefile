BIN=debug-labels
all: ${BIN}

${BIN}: ${BIN}.cpp
	clang++ -g $< `llvm-config --cxxflags --ldflags --system-libs --libs core native` -o $@

clean:
	rm -f ${BIN}
