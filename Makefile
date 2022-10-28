BUILD_DIR=build
MAKE_FLAGS=-j
TEST_FLAGS=-j 50 --output-on-failure

.PHONY: all debug release coverage doc clean test

all:
	mkdir -p $(BUILD_DIR) && cd $(BUILD_DIR) && $(MAKE) $(MAKE_FLAGS) || echo "Type either \"make debug\" or \"make release\"!"

debug:
	mkdir -p $(BUILD_DIR) && cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug .. && $(MAKE) $(MAKE_FLAGS)

release:
	mkdir -p $(BUILD_DIR) && cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release .. && $(MAKE) $(MAKE_FLAGS)

coverage: clean
	mkdir -p $(BUILD_DIR) && cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Coverage .. && $(MAKE) $(MAKE_FLAGS)

doc:
	cd $(BUILD_DIR) && $(MAKE) $(MAKE_FLAGS) doc

test:
	cd $(BUILD_DIR) && ctest $(TEST_FLAGS)

check:
	cd $(BUILD_DIR) && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .. && cppcheck --project=compile_commands.json --quiet --error-exitcode=1

clean:
	cd $(BUILD_DIR) && rm -rf *
	rm -rf doc
