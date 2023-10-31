LLVM_BASE_TAG:=bookworm
LLVM_VERSION:=17
ARM_GNU_VERSION:=12.3.rel1
TEMP_DIR:=/tmp

# Make sure the target "base" runs once at the start of the makefile before any
# other target runs. Do this by including "base" - which will not exist on a
# virgin system so the "base" rule will be executed to create the "base" include
# file before anything else runs.
.PHONY: base
$(TEMP_DIR)/base:
	apt-get update --fix-missing
	apt-get -y upgrade
	apt-get install -y --no-install-recommends \
		g++-11 \
		g++ \
		build-essential \
		cmake \
		wget \
		xz-utils \
		ca-certificates \
		libusb-1.0-0-dev \
		debhelper \
		libglib2.0-0 \
		pkg-config \
		kmod \
		linux-headers-$(shell uname -r) \
		linux-modules-$(shell uname -r) \
		git \
		usbutils
	touch $(TEMP_DIR)/base

-include $(TEMP_DIR)/base

.PHONY: all
all: clang ceedling arm-gcc open-ocd

.PHONY: clang
clang:
	apt-get update --fix-missing
	apt-get -y upgrade
	apt-get install -y --no-install-recommends gnupg2 \
                                               gnupg-agent
    curl --fail --silent --show-error --location https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
	echo "deb http://apt.llvm.org/$(LLVM_BASE_TAG)/ llvm-toolchain-$(LLVM_BASE_TAG)-$(LLVM_VERSION) main" >> /etc/apt/sources.list.d/llvm.list
	apt-get update --fix-missing
	apt-get -y upgrade
	apt-get install -y --no-install-recommends clang-format-$(LLVM_VERSION) \
                                               clang-tidy-$(LLVM_VERSION)

.PHONY: ceedling
ceedling:
	apt-get update --fix-missing
	apt-get -y upgrade
	apt-get -y satisfy "ruby (>=1.9.2)"
	gem install ceedling

.PHONY: arm-gcc
arm-gcc:
	wget -O "$(TEMP_DIR)/arm-gnu-toolchain-$(ARM_GNU_VERSION)-x86_64-arm-none-eabi.tar.xz" https://developer.arm.com/-/media/Files/downloads/gnu/$(ARM_GNU_VERSION)/binrel/arm-gnu-toolchain-$(ARM_GNU_VERSION)-x86_64-arm-none-eabi.tar.xz
	mkdir /opt/gcc-arm-none-eabi
	tar -xf "$(TEMP_DIR)/arm-gnu-toolchain-$(ARM_GNU_VERSION)-x86_64-arm-none-eabi.tar.xz" --strip-components=1 -C /opt/gcc-arm-none-eabi
	echo 'export PATH=$$PATH:/opt/gcc-arm-none-eabi/bin' | tee -a /etc/profile.d/gcc-arm-none-eabi.sh
	# The following required for GDB
	# 	- curseswn5 required by GDB
	# 	- add-apt-repository required for the add-apt-respository command
	# 	- add-apt-respository required to install Python3.8
	apt-get update --fix-missing
	apt-get -y upgrade
	apt-get install -y --no-install-recommends libncursesw5 software-properties-common
	add-apt-repository -y ppa:deadsnakes/ppa
	apt install -y python3.8

.PHONY: open-ocd
open-ocd:
	git clone https://git.code.sf.net/p/openocd/code "$(TEMP_DIR)/openocd-code"
	cd "$(TEMP_DIR)/openocd-code" && ./bootstrap && ./configure --enable-stlink && make -j 4 && make install
