if BUILD_DOCS

doxyfile.stamp: doxygen.cfg
	$(DOXYGEN) $^
	cd latex && $(MAKE)
	echo Timestamp > $@

CLEANFILES = doxyfile.stamp

all-local: doxyfile.stamp

endif
