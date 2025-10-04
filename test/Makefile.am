# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the ProntoScript re-implementation October 4, 2025.
#
# The Initial Developer of the Original Code is Stefan Sinnige.
# Portions created by the Initial Developer are Copyright (C) 2025
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK ***** */

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
LIB = ../lib
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
	@cp -p $(LIB)/*.js $@

.PHONY: http-server
http-server:
	@server/httpserver.py > /dev/null 2>&1 &

# vi: ts=4 noexpandtab:
