include ../../Makefile.include
#DEPS= ../../Makefile.include Makefile

LIBNAME=xbmc-visualization-spectrum
VERSION=0a88c73c5e2d266adbe346ff15e5e0904b1e047e
GIT_DIR=$(TARBALLS_LOCATION)/$(LIBNAME).git
BASE_URL=git://github.com/davilla/visualization.spectrum.git
DYLIB=$(PLATFORM)/visualization.spectrum/spectrum.so
XBMC_ADDONSDIR=$(XBMCROOT)/addons
ADDON=visualization.spectrum

#tell git to use the addons repo rather than xbmc's repo
export GIT_DIR
export GIT_WORK_TREE=$(PLATFORM)

# configuration settings
CONFIGURE=$(CMAKE) . -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=1 -DPACKAGE_ZIP=1

all: .installed-$(PLATFORM)

$(GIT_DIR)/HEAD:
	cd $(TARBALLS_LOCATION); git clone --bare $(BASE_URL) $(LIBNAME).git

$(GIT_DIR)/current/$(VERSION): $(GIT_DIR)/HEAD $(DEPS)
	git rev-list -1 $(VERSION) >/dev/null || git fetch origin "+refs/heads/*:refs/remotes/origin/*"
	git rev-list -1 $(VERSION) >/dev/null
	rm -rf $(GIT_DIR)/current; mkdir -p $(GIT_DIR)/current
	touch $@

$(PLATFORM)/CMakeLists.txt: $(GIT_DIR)/current/$(VERSION)
	rm -rf $(PLATFORM); mkdir -p $(PLATFORM)
	git checkout $(VERSION) -- .

$(PLATFORM)/Makefile: $(PLATFORM)/CMakeLists.txt
	cd $(PLATFORM); $(CONFIGURE)

$(DYLIB): $(PLATFORM)/Makefile
	make -C $(PLATFORM)
	touch $@

.installed-$(PLATFORM): $(DYLIB)
	make -C $(PLATFORM) package
	touch $@

clean:
	-make -C $(PLATFORM) clean
	rm -f .installed-$(PLATFORM)

distclean::
	rm -rf $(PLATFORM) .installed-$(PLATFORM)
	rm -rf $(GIT_DIR)/current
