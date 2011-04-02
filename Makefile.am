SUBDIRS=src

INSTALLDIRS=addon

install-data-local:
	@for dir in $(INSTALLDIRS); do \
	  cp -r $$dir/* $(prefix); \
        done
