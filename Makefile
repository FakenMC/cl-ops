# Check if required directories are defined by a higher priority Makefile
ifndef OBJDIR
export OBJDIR := ${CURDIR}/obj
endif
ifndef BUILDDIR
export BUILDDIR := ${CURDIR}/bin
endif
ifndef CF4OCL_DIR
export CF4OCL_DIR := $(abspath ${CURDIR}/cf4ocl)
endif
ifndef CF4OCL_INCDIR
export CF4OCL_INCDIR := $(abspath $(CF4OCL_DIR)/lib)
endif
ifndef CF4OCL_OBJDIR
export CF4OCL_OBJDIR := $(OBJDIR)
endif

# Check if the required OpenCL locations are defined by a higher priority 
# Makefile, if not define them.

# Macros required by a specific OpenCL implementation, e.g. -DATI_OS_LINUX
# for AMDAPPSDK on Linux.
ifndef CLMACROS
export CLMACROS :=
endif

# The location of the OpenCL headers. In Debian/Ubuntu you can install 
# the package opencl-headers, so that CLINCLUDES remains empty. Otherwise
# you should specify the location, e.g. -I$$AMDAPPSDKROOT/include for 
# AMDAPPSDK on Linux.
ifndef CLINCLUDES
export CLINCLUDES := -I$(AMDAPPSDKROOT)/include
endif

# The location of libOpenCL.so (Linux/Unix) or OpenCL.dll (Windows).
# If you have it in your LD_LIBRARY_PATH (Linux) you can leave CLLIBDIR empty.
# You can leave the location empty if you have installed the ocl-icd-opencl-dev 
# package (Debian/Ubuntu). For AMDAPPSDK on Linux, one would use
# -L$$AMDAPPSDKROOT/lib/x86_64.
ifndef CLLIBDIR
CLLIBDIR := -L$(AMDAPPSDKROOT)/lib/x86_64
endif

# Call "make" on the following folders
SUBDIRS = src $(CF4OCL_DIR)

# Phony targets
.PHONY: all $(SUBDIRS) clean mkdirs getutils

# Targets and rules
all: $(SUBDIRS)

$(SUBDIRS): mkdirs
	$(MAKE) -C $@
     
$(CF4OCL_DIR): getutils

src: $(CF4OCL_DIR)

getutils:
	test -d $(CF4OCL_DIR) || git clone https://github.com/FakenMC/cf4ocl.git

mkdirs:
	mkdir -p $(BUILDDIR)
	mkdir -p $(OBJDIR)
	
clean:
	rm -rf $(OBJDIR) $(BUILDDIR)


     
