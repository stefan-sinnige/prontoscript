# Setup for Pronto Script automated tests. Define the '.js' extension as a test
# case extension and invoke the prontoscript executable.
TEST_EXTENSIONS = .js
JS_LOG_COMPILER = ../js/src/prontoscript -m ./modules

# Define all the test scripts
TESTS = \
	json-list.js

# Create the './modules' directory that contains the required modules for the
# test cases. Start a test server that the test-cases can connect to.
CONTRIB = ../contrib
BUILT_SOURCES = modules http-server

modules:
	@if [[ ! -f $(CONTRIB)/philips/com.philips.HttpLibrary.js ]]; then \
      echo "******************************************************************";\
      echo "Due to licensing restrictions, the Philips Pronto Script libraries";\
      echo "are not bundled in this repository. They would need to be supplied";\
      echo "and copied from your own Philips/Marantz development environment";\
      echo "to '$(CONTRIB)/philips/'."; \
      echo ""; \
      echo "Refer to the Philips Pronto Script Development Guide."; \
      echo "******************************************************************";\
      exit 1; \
    fi
	@mkdir -p $@
	@cp -p $(CONTRIB)/json/*.js $@
	@cp -p $(CONTRIB)/philips/*.js $@

.PHONY: http-server
http-server:
	@server/httpserver.py > /dev/null 2>&1 &

# vi: ts=4 noexpandtab:
