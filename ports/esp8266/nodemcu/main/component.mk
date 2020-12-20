#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the 
# src/ directory, compile them and link them into lib(subdirectory_name).a 
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#
COMPONENT_ADD_LDFLAGS += -L$(BUILD_DIR_BASE) -lapp
COMPONENT_PRIV_INCLUDEDIRS += \
	../../../../external/libmcu/include \
	../../../../external/libmcu/include/libmcu/posix \
	../../../../external/libmcu/examples \
	../../../../
COMPONENT_EMBED_TXTFILES := \
	../../../../AmazonRootCA1.pem \
	../../../../certificate.pem.crt \
	../../../../private.pem.key
