########################################################################################################################

.PHONY: default
default:
	@echo "Make what? Try 'make help'!"
	@exit 1

# Defaults
BUILD_TYPE = Release

# User vars
-include config.mk

FP_USE_ROS1=
FP_USE_ROS2=
ifneq ($(MAKECMDGOALS),)
ifneq ($(MAKECMDGOALS),help)
ifneq ($(MAKECMDGOALS),pre-commit)
ifneq ($(MAKECMDGOALS),ci)
ifneq ($(MAKECMDGOALS),distclean)
ifneq ($(MAKECMDGOALS),doc)
    ifeq ($(INSTALL_PREFIX),)
        $(error Please provide a INSTALL_PREFIX. Try 'make help'!)
    endif
    ifneq ($(ROS_PACKAGE_PATH),)
        FP_USE_ROS1=yes
    else ifeq ($(ROS_VERSION),2)
        FP_USE_ROS2=yes
    else
        $(info No ROS_PACKAGE_PATH (ROS1) and no ROS_VERSION (ROS2) found)
    endif
endif
endif
endif
endif
endif
endif

.PHONY: help
help:
	@echo "Usage:"
	@echo
	@echo "    make <target> INSTALL_PREFIX=... [BUILD_TYPE=Release|Debug] [VERBOSE=1]"
	@echo
	@echo "Where possible <target>s are:"
	@echo
	@echo "    clean               Clean build directory"
	@echo "    cmake               Configure packages"
	@echo "    build               Build packages"
	@echo "    test                Run tests"
	@echo "    install             Install packages (into INSTALL_PREFIX path)"
	@echo
	@echo "Typically you want to do something like this:"
	@echo
	@echo "     source /opt/ros/noetic/setup.bash"
	@echo "     make install INSTALL_PREFIX=~/fpsdk"
	@echo
	@echo "Notes:"
	@echo
	@echo "- ROS_PACKAGE_PATH can be provided through catkin/ros env (recommended) or on the command line"
	@echo "- Command line variables can be stored into a config.mk file, which is automatically loaded"
	@echo "- When changing the INSTALL_PREFIX or config.mk, a 'make clean' will be required"
	@echo "- 'make ci' runs the CI (more or less) like on Github. INSTALL_PREFIX and BUILD_TYPE have no effect here."
	@echo

########################################################################################################################

TOUCH      := touch
MKDIR      := mkdir
ECHO       := echo
RM         := rm
CMAKE      := cmake
DOXYGEN    := doxygen

ifeq ($(VERBOSE),1)
V =
V1 =
V2 =
V12 =
RM += -v
MV += -v
CP += -v
MKDIR += -v
else
ZIP += -q
UNZIP += -q
V = @
V1 = > /dev/null
V2 = 2> /dev/null
V12 = 2>&1 > /dev/null
endif

fancyterm := true
ifeq ($(TERM),dumb)
fancyterm := false
endif
ifeq ($(TERM),)
fancyterm := false
endif
ifneq ($(MSYSTEM),)
fancyterm := false
endif
ifeq ($(fancyterm),true)
HLW="\\e[1m"
HLO="\\e[m"
else
HLW=
HLO=
endif

# Disable all built-in rules
.SUFFIXES:

########################################################################################################################

CMAKE_ARGS_BUILD :=
CMAKE_ARGS := -DCMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX)
CMAKE_ARGS += -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
ifneq ($(ROS_PACKAGE_PATH),)
  CMAKE_ARGS += -DROS_PACKAGE_PATH=$(ROS_PACKAGE_PATH)
endif

ifneq ($(VERBOSE),0)
  CMAKE_ARGS_BUILD += --verbose
endif

ifeq ($(GITHUB_WORKSPACE),)
  CMAKE_ARGS_BUILD = --parallel 4
else
  CMAKE_ARGS_BUILD = --parallel 1
endif

BUILD_DIR = build/$(BUILD_TYPE)

# "All-in-one" targets
.PHONY: clean
clean:
	$(V)$(RM) -rf $(BUILD_DIR)

.PHONY: distclean
distclean:
	$(V)$(RM) -rf install build fpsdk

# ----------------------------------------------------------------------------------------------------------------------

.PHONY: cmake
cmake: $(BUILD_DIR)/.make-cmake

deps = $(sort $(wildcard Makefile doc/* Doxyfile \
    fpcommon/* fpcommon/*/* fpcommon/*/*/* fpcommon/*/*/*/* \
    fpros1/* fpros1/*/* fpros1/*/*/* fpros1/*/*/*/* \
    fpros2/* fpros2/*/* fpros2/*/*/* fpros2/*/*/*/* \
    fpapps/* fpapps/*/* fpapps/*/*/* fpapps/*/*/*/*))

$(BUILD_DIR)/.make-cmake: $(deps)
	@echo "$(HLW)***** Configure ($(BUILD_TYPE)) *****$(HLO)"
	$(V)$(CMAKE) -B $(BUILD_DIR) $(CMAKE_ARGS)
	$(V)$(TOUCH) $@

# ----------------------------------------------------------------------------------------------------------------------

.PHONY: build
build: $(BUILD_DIR)/.make-build

$(BUILD_DIR)/.make-build: $(BUILD_DIR)/.make-cmake
	@echo "$(HLW)***** Build ($(BUILD_TYPE)) *****$(HLO)"
	$(V)$(CMAKE) --build $(BUILD_DIR)  $(CMAKE_ARGS_BUILD)
	$(V)$(TOUCH) $@

# ----------------------------------------------------------------------------------------------------------------------

.PHONY: install
install: $(BUILD_DIR)/.make-install

$(BUILD_DIR)/.make-install: $(BUILD_DIR)/.make-build
	@echo "$(HLW)***** Install ($(BUILD_TYPE)) *****$(HLO)"
	$(V)$(CMAKE) --install $(BUILD_DIR)
	$(V)$(TOUCH) $@

# ----------------------------------------------------------------------------------------------------------------------

.PHONY: test
test: $(BUILD_DIR)/.make-build
	@echo "$(HLW)***** Test ($(BUILD_TYPE)) *****$(HLO)"
	$(V)(cd $(BUILD_DIR)/fpcommon && ctest)
ifneq ($(FP_USE_ROS1),)
	$(V)(cd $(BUILD_DIR)/fpros1 && ctest)
endif

# ----------------------------------------------------------------------------------------------------------------------

.PHONY: doc
doc: $(BUILD_DIR)/.make-doc

$(BUILD_DIR)/.make-doc: $(BUILD_DIR)/.make-cmake
	@echo "$(HLW)***** Doc ($(BUILD_TYPE)) *****$(HLO)"
	$(V)( \
            cat Doxyfile; \
            echo "PROJECT_NUMBER = $$(cat $(BUILD_DIR)/FP_VERSION_STRING || echo 'unknown revision')"; \
            echo "OUTPUT_DIRECTORY = $(BUILD_DIR)"; \
        ) | $(DOXYGEN) -
	$(V)$(TOUCH) $@

# ----------------------------------------------------------------------------------------------------------------------

.PHONY: ci
ci: $(BUILD_DIR)/.ci-bookworm $(BUILD_DIR)/.ci-noetic $(BUILD_DIR)/.ci-humble $(BUILD_DIR)/.ci-jazzy

$(BUILD_DIR)/.ci-bookworm: $(deps)
	@echo "$(HLW)***** CI (bookworm) *****$(HLO)"
ifeq ($(FPSDK_IMAGE),)
	$(V)docker/docker.sh run bookworm-ci ./docker/ci.sh
	$(V)$(TOUCH) $@
else
	@echo "This ($@) should not run inside Docker!"
	@false
endif

$(BUILD_DIR)/.ci-noetic: $(deps)
	@echo "$(HLW)***** CI (noetic) *****$(HLO)"
ifeq ($(FPSDK_IMAGE),)
	$(V)docker/docker.sh run noetic-ci ./docker/ci.sh
	$(V)$(TOUCH) $@
else
	@echo "This ($@) should not run inside Docker!"
	@false
endif

$(BUILD_DIR)/.ci-humble: $(deps)
	@echo "$(HLW)***** CI (humble) *****$(HLO)"
ifeq ($(FPSDK_IMAGE),)
	$(V)docker/docker.sh run humble-ci ./docker/ci.sh
	$(V)$(TOUCH) $@
else
	@echo "This ($@) should not run inside Docker!"
	@false
endif

$(BUILD_DIR)/.ci-jazzy: $(deps)
	@echo "$(HLW)***** CI (jazzy) *****$(HLO)"
ifeq ($(FPSDK_IMAGE),)
	$(V)docker/docker.sh run jazzy-ci ./docker/ci.sh
	$(V)$(TOUCH) $@
else
	@echo "This ($@) should not run inside Docker!"
	@false
endif



# ----------------------------------------------------------------------------------------------------------------------

.PHONY: pre-commit
pre-commit:
	@echo "$(HLW)***** pre-commit checks *****$(HLO)"
	$(V)pre-commit run --all-files --hook-stage manual || return 1

########################################################################################################################
