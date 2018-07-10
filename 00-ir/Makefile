BIN=ir
all: ${BIN}

${BIN}: ${BIN}.cpp
	clang++ -g $< `llvm-config --cxxflags --ldflags --system-libs --libs core` -o $@

clean:
	rm -f ${BIN}
